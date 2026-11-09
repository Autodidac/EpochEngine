"""Actual native producer/cache, with no renderer or model process."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile


def receipt(path, ocean_surface=False, seconds=None):
    lines = (path / 'terrain.stream').read_text().splitlines()
    assert lines[0] == ('epoch.planet.stream.v3' if seconds is not None else ('epoch.planet.stream.v2' if ocean_surface else 'epoch.planet.stream.v1'))
    result = dict(line.split('=', 1) for line in lines[1:])
    if seconds is not None:
        assert lines[-2] == f'ocean_surface={int(ocean_surface)}'
        assert lines[-1].startswith('simulation_seconds=') and float(result['simulation_seconds']) == float(seconds)
    elif ocean_surface:
        assert result.get('ocean_surface') == '1'
    else:
        assert 'ocean_surface' not in result
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--producer', type=Path, required=True)
    parser.add_argument('--scratch', type=Path, required=True)
    args = parser.parse_args()
    args.scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='stream-export-', dir=args.scratch) as temporary:
        root = Path(temporary)
        cache = root / 'cache'
        sequence = 0

        def step(camera, maximum_new=8, expected=0, ocean_surface=False, cache_override=None, mesh_bytes=None, seconds=None, **overrides):
            nonlocal sequence
            sequence += 1
            output = root / f'step-{sequence}'
            values = dict(seed='1337', cells='4', body='0', height='1080', fov='1.0471975511965976',
                          level='12', leaves='96', memory=str(64 * 1024 * 1024))
            values.update(overrides)
            command = [str(args.producer), '--stream-step-preview', str(output), str(cache_override or cache),
                       values['seed'], values['cells'], values['body'], *(str(x) for x in camera),
                       values['height'], values['fov'], values['level'], values['leaves'], values['memory'],
                       str(maximum_new), str(sequence)]
            if ocean_surface:
                command.append('1')
            if mesh_bytes is not None:
                command.extend(('--mesh-bytes', str(mesh_bytes)))
            if seconds is not None:
                command.extend(('--seconds', str(seconds)))
            result = subprocess.run(command, capture_output=True, timeout=30)
            assert result.returncode == expected, f'exit{result.returncode}: {command}'
            if expected == 0:
                timing = dict(line.split('=', 1) for line in (output / 'terrain.performance').read_text().splitlines()[1:])
                worker_limit = int(timing['worker_limit'])
                assert 1 <= int(timing['peak_active_workers']) <= worker_limit <= 4
                assert int(timing['worker_jobs_started']) >= int(timing['peak_active_workers'])
                assert float(timing['worker_run_ms']) > 0
            return output, receipt(output, ocean_surface, seconds) if result.returncode == 0 else None, command

        cold, r, _ = step((0, 2500, 0), maximum_new=2)
        assert r['snapshot_ready'] == '0' and r['built'] == '2' and not (cold / 'terrain.epoch_mesh').exists()
        distant, r, repeated = step((0, 2500, 0))
        assert r['snapshot_ready'] == '1' and r['reused'] == '2' and r['draw'] == '6' and r['max_level'] == '0'
        digest = hashlib.sha256((distant / 'terrain.epoch_mesh').read_bytes()).digest()
        failed = subprocess.run(repeated, capture_output=True, timeout=30)
        assert failed.returncode == 6 and hashlib.sha256((distant / 'terrain.epoch_mesh').read_bytes()).digest() == digest
        # The authored fallback and the first streamed root cut must agree in
        # actual geometry, not merely carry plausible metadata scale values.
        static = root / 'static-planet'
        generated = subprocess.run([str(args.producer), 'planet', str(static), '1337', '4', '1', '0', '128'],
                                   capture_output=True, timeout=30)
        assert generated.returncode == 0
        static_recipe = json.loads((static / 'terrain.preview.json').read_text())
        distant_recipe = json.loads((distant / 'terrain.preview.json').read_text())
        assert static_recipe['meters_to_preview_units'] == distant_recipe['meters_to_preview_units']
        static_bytes = (static / 'terrain.epoch_mesh').read_bytes()
        distant_bytes = (distant / 'terrain.epoch_mesh').read_bytes()
        surface_vertices, patch_vertices = 25, 41
        for face in range(6):
            for index in range(surface_vertices):
                a = struct.unpack_from('<fff', static_bytes, 48 + (face * surface_vertices + index) * 44)
                b = struct.unpack_from('<fff', distant_bytes, 48 + (face * patch_vertices + index) * 44)
                assert max(abs(x - y) for x, y in zip(a, b)) < 1e-5, 'Fallback/stream root geometry changed scale'
                assert 9.9 < math.sqrt(sum(value * value for value in a)) < 10.1
        near = None
        for _ in range(64):
            near, r, _ = step((0, 10.001, 0), maximum_new=8)
            assert r['snapshot_ready'] == '1' and int(r['built']) <= 8
            assert int(r['resident_bytes']) < int(r['maximum_bytes'])
            if r['pending'] == '0':
                break
        assert r['pending'] == '0' and int(r['max_level']) > 0 and int(r['draw']) > 6
        recipe = json.loads((near / 'terrain.preview.json').read_text())
        assert abs(recipe['meters_to_preview_units'] - 10 / 6371000) < 1e-20
        assert recipe['meters_to_preview_units'] == static_recipe['meters_to_preview_units']
        raw = (near / 'terrain.epoch_mesh').read_bytes()
        vertices, indices = struct.unpack_from('<II', raw, 40)
        assert vertices == int(r['vertices']) and indices == int(r['indices'])
        assert len(raw) == 48 + vertices * 44 + indices * 4
        for offset in range(vertices):
            xyz = struct.unpack_from('<fff', raw, 48 + offset * 44)
            assert all(abs(value) < 100 for value in xyz)
            if offset % patch_vertices < surface_vertices:
                assert 9.9 < math.sqrt(sum(value * value for value in xyz)) < 10.1, 'Near detail changed the body radius'
        idle, r, _ = step((0, 10.001, 0))
        assert r['built'] == '0' and int(r['reused']) > 6
        assert (idle / 'terrain.epoch_mesh').read_bytes() == (near / 'terrain.epoch_mesh').read_bytes()
        near_cache_hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in cache.iterdir()}
        timed_near, timed_near_receipt, _ = step((0, 10.001, 0), seconds=12345.125)
        assert timed_near_receipt['built'] == '0' and timed_near_receipt['pending'] == '0'
        assert (timed_near / 'terrain.epoch_mesh').read_bytes() == (near / 'terrain.epoch_mesh').read_bytes()
        assert near_cache_hashes == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in cache.iterdir()}
        far, r, _ = step((0, 2500, 0))
        assert r['resident'] == '6' and r['draw'] == '6' and r['built'] == '0'
        assert len(list(cache.glob('*.epoch_patch'))) == 6
        _, _, _ = step((0, 10, 0), expected=3, memory=str(1024 * 1024))
        _, _, _ = step((0, 10, 0), expected=3, body='99')
        _, _, _ = step((0, 'nan', 0), expected=3)
        # Cache identity cannot be silently repurposed across seed/settings.
        _, _, _ = step((0, 2500, 0), expected=9, seed='1338')
        patch = next(cache.glob('*.epoch_patch'))
        saved = patch.read_bytes()
        patch.write_bytes(saved[:40])
        _, _, _ = step((0, 2500, 0), expected=9)
        patch.write_bytes(saved)
        incomplete = patch.with_name(patch.name + '.pending')
        incomplete.write_bytes(b'interrupted patch write')
        _, r, _ = step((0, 2500, 0))
        assert r['built'] == '0' and not incomplete.exists()
        # The same source/body/cells with a different ocean option must not
        # silently repurpose or delete the existing terrain-only patch cache.
        cache_hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in cache.iterdir()}
        step((0, 2500, 0), ocean_surface=True, expected=9)
        assert cache_hashes == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in cache.iterdir()}
        water_cache = root / 'ocean-cache'
        water_output, wet, _ = step((0, 2500, 0), ocean_surface=True, cache_override=water_cache)
        assert wet['snapshot_ready'] == '1' and wet['draw'] == '6'
        assert (water_cache / 'cache.epoch_planet_stream').read_text().endswith('ocean_surface=1\n')
        wet_data = (water_output / 'terrain.epoch_mesh').read_bytes()
        assert wet_data != (far / 'terrain.epoch_mesh').read_bytes()
        assert json.loads((water_output / 'terrain.preview.json').read_text())['ocean_surface'] is True
        water_vertices = 0
        for index in range(int(wet['vertices'])):
            color = struct.unpack_from('<fff', wet_data, 48 + index * 44 + 32)
            if max(abs(a - b) for a, b in zip(color, (0.015, 0.085, 0.12))) < 1e-7:
                point = struct.unpack_from('<fff', wet_data, 48 + index * 44)
                # Skirts are deliberately below sea level; their surface grid
                # endpoints still include the actual radius-ten ocean.
                if abs(math.sqrt(sum(x*x for x in point)) - 10) < 1e-5:
                    water_vertices += 1
        assert water_vertices > 0
        repeated_water, wet2, _ = step((0, 2500, 0), ocean_surface=True, cache_override=water_cache)
        assert wet2['built'] == '0' and int(wet2['reused']) == 6
        assert (repeated_water / 'terrain.epoch_mesh').read_bytes() == wet_data
        wet_hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in water_cache.iterdir()}
        step((0, 2500, 0), cache_override=water_cache, expected=9)
        assert wet_hashes == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in water_cache.iterdir()}
        step((0, 10, 0), ocean_surface=True, cache_override=water_cache, cells='64', expected=2)
        step((0, 10, 0), ocean_surface=True, cache_override=water_cache, memory=str(1024 * 1024), expected=3)
        for body in range(1, 6):
            step((0, 2500, 0), ocean_surface=True, cache_override=water_cache, body=str(body), expected=3)
        # The orbital clock preserves cached terrain; the optional Water
        # implementation samples the same absolute time into disposable output.
        for wet_option in (False, True):
            timed_cache = root / f'timed-{wet_option}'
            baseline, base, base_command = step((0, 2500, 0), ocean_surface=wet_option, cache_override=timed_cache)
            baseline_mesh = (baseline / 'terrain.epoch_mesh').read_bytes()
            hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in timed_cache.iterdir()}
            initial_recipe = json.loads((baseline / 'terrain.preview.json').read_text())
            sampled_times = {}
            for instant in (0, 12345.125, 89999.75):
                timed, value, command = step((0, 2500, 0), ocean_surface=wet_option,
                    cache_override=timed_cache, seconds=instant, mesh_bytes=4 * 1024 * 1024)
                assert value['built'] == '0' and value['reused'] == '6' and value['pending'] == '0'
                sampled_mesh = (timed / 'terrain.epoch_mesh').read_bytes()
                sampled_times[instant] = sampled_mesh
                assert (sampled_mesh == baseline_mesh) == (not wet_option or instant == 0)
                meta = json.loads((timed / 'terrain.preview.json').read_text())
                assert meta['seconds'] == instant
                assert (meta['origin_meters'] == initial_recipe['origin_meters']) == (instant == 0)
                assert hashes == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in timed_cache.iterdir()}
            revisited, _, _ = step((0, 2500, 0), ocean_surface=wet_option,
                cache_override=timed_cache, seconds=0, mesh_bytes=4 * 1024 * 1024)
            assert (revisited / 'terrain.epoch_mesh').read_bytes() == baseline_mesh
            for invalid in ('-1', 'nan', 'inf', '1000000000000', '1e999'):
                step((0, 2500, 0), ocean_surface=wet_option, cache_override=timed_cache, seconds=invalid, expected=2)
            duplicate = root / f'duplicate-time-{wet_option}'
            bad = base_command.copy(); bad[2] = str(duplicate)
            run = subprocess.run(bad + ['--seconds', '0', '--seconds', '1'], capture_output=True, timeout=30)
            assert run.returncode == 2 and not duplicate.exists()
            assert hashes == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in timed_cache.iterdir()}
            # Both explicit option orders reach exactly the same producer.
            reversed_output = root / f'reversed-options-{wet_option}'
            reversed_args = base_command.copy(); reversed_args[2] = str(reversed_output)
            run = subprocess.run(reversed_args + ['--seconds', '12345.125', '--mesh-bytes', str(4 * 1024 * 1024)], capture_output=True, timeout=30)
            assert run.returncode == 0
            receipt(reversed_output, wet_option, 12345.125)
            assert (reversed_output / 'terrain.epoch_mesh').read_bytes() == sampled_times[12345.125]

        # Exercise the actual Editor default leaf/cell/memory settings, not
        # merely a roomy synthetic low-cell request. This used to stop at LOD2
        # because five invisible bodies consumed thirty of its forty-eight leaves.
        default_cache = root / 'editor-default-cache'
        default_args = dict(cells='16', leaves='48', memory=str(16 * 1024 * 1024),
                            level='10', height='720', ocean_surface=True, cache_override=default_cache)
        for _ in range(16):
            default_output, default, _ = step((0, 10.0008, 0), **default_args)
            assert int(default['built']) <= 8
            assert int(default['resident_bytes']) <= int(default['maximum_bytes'])
            if default['pending'] == '0':
                break
        assert default['pending'] == '0' and int(default['max_level']) > 2
        assert int(default['draw']) <= 48 and int(default['indices']) <= 196608
        assert int(default['vertices']) <= 65536
        per_face = [0.0] * 6
        for patch in json.loads((default_output / 'terrain.preview.json').read_text())['patches']:
            x, y, face, level = patch['tile']
            per_face[face] += 1 / (4 ** level)
        assert all(abs(area - 1) < 1e-10 for area in per_face), 'Default streaming lost a cube face'
        settled_output, settled, _ = step((0, 10.0008, 0), **default_args)
        assert settled['built'] == '0' and (settled_output / 'terrain.epoch_mesh').read_bytes() == (default_output / 'terrain.epoch_mesh').read_bytes()
        default_hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in default_cache.iterdir()}
        step((0, 10.0008, 0), **dict(default_args, memory=str(4 * 1024 * 1024)), expected=3)
        assert default_hashes == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in default_cache.iterdir()}
        # Older valid wet64 settings still fit the4MiB snapshot envelope;
        # their selected-body CPU cache needs explicit headroom now.
        old64_cache = root / 'old64-cache'
        for _ in range(8):
            _, old64, _ = step((0, 10.0008, 0), **dict(default_args, leaves='64', memory=str(32 * 1024 * 1024), cache_override=old64_cache), maximum_new=32)
            if old64['pending'] == '0':
                break
        assert old64['pending'] == '0' and int(old64['indices']) > 196608
        assert 48 + int(old64['vertices']) * 44 + int(old64['indices']) * 4 <= 4 * 1024 * 1024
        detailed_cache = root / 'detailed-cache'
        detailed_args = dict(cells='16', leaves='384', memory=str(128 * 1024 * 1024),
                             level='18', height='1080', ocean_surface=True, cache_override=detailed_cache)
        step((10.0008, 0, 0), **detailed_args, expected=2)  # Default snapshot budget must not silently grow.
        assert not detailed_cache.exists()
        for _ in range(24):
            detailed_output, detailed, _ = step((10.0008, 0, 0), **detailed_args,
                                              mesh_bytes=32 * 1024 * 1024, maximum_new=32)
            assert int(detailed['resident_bytes']) <= int(detailed['maximum_bytes'])
            if detailed['pending'] == '0':
                break
        assert detailed['pending'] == '0' and int(detailed['draw']) == 384 and int(detailed['max_level']) > 2
        detailed_size = (detailed_output / 'terrain.epoch_mesh').stat().st_size
        assert 4 * 1024 * 1024 < detailed_size <= 32 * 1024 * 1024
        assert int(detailed['vertices']) > 65536 and int(detailed['indices']) > 196608
        detailed_hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in detailed_cache.iterdir()}
        for invalid_bytes in (0, 65 * 1024 * 1024, 2**64 - 1):
            step((10.0008, 0, 0), **detailed_args, mesh_bytes=invalid_bytes, expected=2)
        step((10.0008, 0, 0), **dict(detailed_args, memory=str(64 * 1024 * 1024)),
             mesh_bytes=32 * 1024 * 1024, expected=3)
        assert detailed_hashes == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in detailed_cache.iterdir()}
        settled_output, settled, _ = step((10.0008, 0, 0), **detailed_args, mesh_bytes=32 * 1024 * 1024)
        assert settled['built'] == '0' and (settled_output / 'terrain.epoch_mesh').read_bytes() == (detailed_output / 'terrain.epoch_mesh').read_bytes()

        # A larger bounded batch amortizes cache/snapshot work without changing
        # the final admitted geometry, memory limit or deterministic patch order.
        batch_cache = root / 'batch-cache'
        batch_args = dict(detailed_args, cache_override=batch_cache)
        for _ in range(8):
            batch_output, batch, _ = step((10.0008, 0, 0), **batch_args,
                                         mesh_bytes=32 * 1024 * 1024, maximum_new=128)
            assert 0 <= int(batch['built']) <= 128
            assert int(batch['resident_bytes']) <= int(batch['maximum_bytes'])
            if batch['pending'] == '0':
                break
        assert batch['pending'] == '0'
        assert (batch_output / 'terrain.epoch_mesh').read_bytes() == (detailed_output / 'terrain.epoch_mesh').read_bytes()
        batch_hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in batch_cache.iterdir()}
        for invalid_batch in (0, 129, 2**32 - 1):
            output, _, _ = step((10.0008, 0, 0), **batch_args, mesh_bytes=32 * 1024 * 1024,
                                maximum_new=invalid_batch, expected=2)
            assert not output.exists()
        assert batch_hashes == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in batch_cache.iterdir()}

        # Unknown files are never deleted as if they were generated cache.
        sentinel = cache / 'authored.txt'
        sentinel.write_text('retain this')
        interrupted = next(cache.glob('*.epoch_patch')).with_suffix('.epoch_patch.pending')
        interrupted.write_bytes(b'retain until the entire directory passes admission')
        before_refusal = {p.name: p.read_bytes() for p in cache.iterdir() if p.is_file()}
        _, _, _ = step((0, 2500, 0), expected=9)
        assert before_refusal == {p.name: p.read_bytes() for p in cache.iterdir() if p.is_file()}
        sentinel.unlink()
        # Even a valid patch name cannot turn a directory into patch input.
        # Failed admission must not clean the otherwise obsolete pending file.
        invalid_patch = cache / 'tile-0-0-1-0-0.epoch_patch'
        invalid_patch.mkdir()
        _, _, _ = step((0, 2500, 0), expected=9)
        assert invalid_patch.is_dir()
        assert {k: v for k, v in before_refusal.items() if k != 'authored.txt'} == {
            p.name: p.read_bytes() for p in cache.iterdir() if p.is_file()}
        invalid_patch.rmdir()
        _, _, _ = step((0, 2500, 0))
        assert not interrupted.exists()


if __name__ == '__main__':
    main()
