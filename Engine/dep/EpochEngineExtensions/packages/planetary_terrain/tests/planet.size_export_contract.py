"""Offline producer size/frame contract; no renderer, model or source mutation."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile


def run(command, expected=0):
    result = subprocess.run([str(value) for value in command], capture_output=True, timeout=30)
    assert result.returncode == expected, (result.returncode, command, result.stderr.decode(errors='replace'))


def frame(output):
    raw = (output / 'terrain.frame').read_bytes()
    lines = raw.decode('ascii').splitlines()
    names = ['mode', 'seed', 'body_index', 'cells_per_tile', 'tiles_per_axis', 'tile_span_meters',
             'seconds', 'size_reference_body', 'reference_radius_meters', 'meters_to_preview_units',
             'origin_meters', 'content_key', 'vertices', 'indices']
    assert len(raw) <= 4096 and raw.endswith(b'\n') and lines[0] == 'epoch.planet.mesh-frame.v1'
    assert [line.partition('=')[0] for line in lines[1:]] == names
    values = dict(line.split('=', 1) for line in lines[1:])
    recipe = json.loads((output / 'terrain.preview.json').read_text())
    data = (output / 'terrain.epoch_mesh').read_bytes()
    vertices, indices = struct.unpack_from('<II', data, 40)
    assert len(data) == 48 + 44 * vertices + 4 * indices
    assert vertices == int(values['vertices']) == recipe['vertices']
    assert indices == int(values['indices']) == recipe['indices']
    assert values['content_key'] == recipe['content_key']
    assert float(values['meters_to_preview_units']) == recipe['meters_to_preview_units']
    assert [float(value) for value in values['origin_meters'].split(',')] == recipe['origin_meters']
    for name in ('seed', 'body_index', 'cells_per_tile', 'tiles_per_axis'):
        assert int(values[name]) == recipe[name]
    for name in ('tile_span_meters', 'seconds'):
        assert float(values[name]) == recipe[name]
    assert values['mode'] == recipe['mode']
    for offset in range(vertices):
        point = struct.unpack_from('<fff', data, 48 + 44 * offset)
        assert all(math.isfinite(value) for value in point)
    return values, recipe


def hashes(folder):
    return {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in folder.iterdir() if path.is_file()}


def verify(producer, root, legacy=None):
    base = [producer, 'planet', None, '1337', '4', '4', '0', '128']
    for name, size, reference in [('default', 6371000, 0), ('small', 25000, 0), ('moon-reference', 25000, 5)]:
        output = root / name
        command = base.copy(); command[2] = output
        if name != 'default':
            command += ['--size-reference-body', str(reference), '--reference-radius-meters', str(size)]
        run(command)
        values, recipe = frame(output)
        assert int(values['size_reference_body']) == reference and float(values['reference_radius_meters']) == size
        assert recipe['schema'] == ('epoch.planet.mesh-preview.v1' if name == 'default' else 'epoch.planet.mesh-preview.v2')
        if name != 'default':
            assert recipe['size_reference_body'] == reference and recipe['reference_radius_meters'] == size
        else:
            assert 'reference_radius_meters' not in recipe
        expected_radius = size if reference == 0 else 6371000 * size / 1737000
        assert math.isclose(float(values['meters_to_preview_units']), 10 / expected_radius, rel_tol=1e-15)
        before = hashes(output); run(command, 6); assert hashes(output) == before
        if name == 'default' and legacy:
            previous = root / 'legacy-default'; old = command.copy(); old[0] = legacy; old[2] = previous
            run(old)
            for filename in ('terrain.preview.json', 'terrain.epoch_mesh'):
                assert (output / filename).read_bytes() == (previous / filename).read_bytes(), filename

    if legacy:
        # Existing files remain exact, including all legacy stream versions.
        # The additional typed frame file is intentionally new.
        for mode in ('classic', 'tiled', 'hemisphere', 'universe'):
            for executable, prefix in ((producer, 'new'), (legacy, 'old')):
                run([executable, mode, root / f'{prefix}-{mode}', '1337', '4', '4', '0', '128'])
            for filename in ('terrain.preview.json', 'terrain.epoch_mesh'):
                assert (root / f'new-{mode}' / filename).read_bytes() == (root / f'old-{mode}' / filename).read_bytes()
        for version, options in enumerate(([], ['1'], ['--seconds', '12345.125'],
                ['--patch-pixels', '170', '--lod-hysteresis', '0.2']), 1):
            for executable, prefix in ((producer, 'new'), (legacy, 'old')):
                output = root / f'{prefix}-v{version}'; cache_path = root / f'{prefix}-v{version}-cache'
                run([executable, '--stream-step-preview', output, cache_path, '1337', '4', '0', '0', '2500', '0',
                    '1080', '1.0471975511965976', '12', '96', str(64 * 1024 * 1024), '8', '1', *options])
            assert (root / f'new-v{version}' / 'terrain.stream').read_text().splitlines()[0] == f'epoch.planet.stream.v{version}'
            for filename in ('terrain.preview.json', 'terrain.epoch_mesh', 'terrain.stream'):
                assert (root / f'new-v{version}' / filename).read_bytes() == (root / f'old-v{version}' / filename).read_bytes(), (version, filename)
            assert hashes(root / f'new-v{version}-cache') == hashes(root / f'old-v{version}-cache'), version

    cache = root / 'stream-cache'
    sequence = 0
    def stream(radius='25000', reference='5', expected=0, **extra):
        nonlocal sequence
        sequence += 1
        output = root / f'stream-{sequence}'
        command = [producer, '--stream-step-preview', output, cache, '1337', '4', '0', '0', '2500', '0',
                   '1080', '1.0471975511965976', '12', '96', str(64 * 1024 * 1024), '32', str(sequence),
                   '--size-reference-body', reference, '--reference-radius-meters', radius]
        if extra.get('seconds') is not None:
            command += ['--seconds', str(extra['seconds'])]
        run(command, expected)
        if expected:
            return output, None
        lines = (output / 'terrain.stream').read_text().splitlines()
        assert lines[0] == 'epoch.planet.stream.v5'
        values = dict(line.split('=', 1) for line in lines[1:])
        assert [line.split('=', 1)[0] for line in lines[-6:]] == [
            'ocean_surface', 'simulation_seconds', 'patch_pixels', 'lod_hysteresis',
            'size_reference_body', 'reference_radius_meters']
        assert values['ocean_surface'] == '0' and float(values['simulation_seconds']) == extra.get('seconds', 0)
        assert values['size_reference_body'] == reference and float(values['reference_radius_meters']) == float(radius)
        assert int(values['built']) <= 32 and int(values['resident_bytes']) < int(values['maximum_bytes'])
        if values['snapshot_ready'] == '1':
            frame(output)
        return output, values

    first, initial = stream()
    assert initial['snapshot_ready'] == '1' and initial['draw'] == '6'
    second, repeated = stream()
    assert repeated['built'] == '0' and repeated['reused'] == '6'
    assert (first / 'terrain.epoch_mesh').read_bytes() == (second / 'terrain.epoch_mesh').read_bytes()
    saved = hashes(cache)
    stream(radius='26000', expected=9)
    stream(reference='0', expected=9)
    assert hashes(cache) == saved, 'foreign size mutated admitted cache'
    timed, receipt = stream(seconds=12345.125)
    assert receipt['built'] == '0' and hashes(cache) == saved
    assert (timed / 'terrain.epoch_mesh').read_bytes() == (first / 'terrain.epoch_mesh').read_bytes()
    assert (cache / 'cache.epoch_planet_stream').read_text().splitlines()[0] == 'epoch.planet.stream-cache.v3'
    for index, options in enumerate([
        ['--reference-radius-meters', '249'], ['--reference-radius-meters', '100000001'],
        ['--reference-radius-meters', 'nan'], ['--reference-radius-meters', 'inf'],
        ['--size-reference-body', '6'], ['--reference-radius-meters', '25000', '--reference-radius-meters', '26000'],
        ['--size-reference-body', '0', '--size-reference-body', '5']]):
        output = root / f'invalid-{index}'
        command = base.copy(); command[2] = output
        run(command + options, 2)
        assert not output.exists(), 'invalid size created output'
    return {'pass': True, 'legacy_bytes_compared': legacy is not None, 'static_custom': str(root / 'moon-reference'),
            'stream_custom': str(first), 'stream_timed': str(timed), 'renderer_started': False}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--producer', type=Path, required=True)
    parser.add_argument('--scratch', type=Path, required=True)
    parser.add_argument('--legacy-producer', type=Path)
    parser.add_argument('--retain', type=Path)
    args = parser.parse_args()
    args.scratch.mkdir(parents=True, exist_ok=True)
    if args.retain:
        args.retain.mkdir(parents=True, exist_ok=False)
        result = verify(args.producer, args.retain, args.legacy_producer)
        (args.retain / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    else:
        with tempfile.TemporaryDirectory(prefix='size-export-', dir=args.scratch) as temporary:
            verify(args.producer, Path(temporary), args.legacy_producer)


if __name__ == '__main__':
    main()
