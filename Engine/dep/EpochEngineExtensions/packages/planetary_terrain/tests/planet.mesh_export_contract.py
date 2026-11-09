"""Exercise the actual optional C++ producer; all data stays in its build tree."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile


def verify_ocean_vertices(dry_bytes, wet_bytes, recipe):
    """Separate sea samples, sealing skirts and clipped shoreline vertices.

    The producer retains each dry terrain block, appends its matching water
    grid/skirt, then appends shoreline crossings. Clipped crossings belong on
    rendered triangle edges/ground planes, not on an ideal radius-ten sphere.
    See water.planetary_shoreline_consumer.cpp for the shared owner's additional
    time/parent-morph checks against those same geometric relationships.
    """
    patches = recipe['patches']
    assert recipe['mode'] == 'planet' and len(patches) == 6
    dry_count, dry_index_count = struct.unpack_from('<II', dry_bytes, 40)
    wet_count, wet_index_count = struct.unpack_from('<II', wet_bytes, 40)
    grid = (recipe['cells_per_tile'] + 1) ** 2
    layer, remainder = divmod(dry_count, len(patches))
    assert remainder == 0 and layer in (grid, grid + 4 * recipe['cells_per_tile']), 'Unexpected source grid/skirt topology'
    raw = wet_bytes[48:48 + wet_count * 44]
    bases = []
    for patch in range(len(patches)):
        start = 48 + patch * layer * 44
        terrain = dry_bytes[start:start + layer * 44]
        offset = raw.find(terrain)
        assert offset >= 0 and offset % 44 == 0, 'Ocean changed its input terrain'
        assert raw.find(terrain, offset + 1) == -1, 'Ambiguous terrain block'
        bases.append(offset // 44)
    assert bases == sorted(bases) and bases[0] == 0
    bases.append(wet_count)
    dry_indices = struct.unpack_from(f'<{dry_index_count}I', dry_bytes, 48 + dry_count * 44)
    wet_indices = struct.unpack_from(f'<{wet_index_count}I', wet_bytes, 48 + wet_count * 44)
    positions = [struct.unpack_from('<fff', wet_bytes, 48 + i * 44) for i in range(wet_count)]
    normals = [struct.unpack_from('<fff', wet_bytes, 60 + i * 44) for i in range(wet_count)]
    def sub(a, b):
        return tuple(x - y for x, y in zip(a, b))
    def dot(a, b):
        return sum(x * y for x, y in zip(a, b))
    def cross(a, b):
        return (a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0])
    sea_samples = shoreline = surface_crossings = 0
    for patch, (base, end) in enumerate(zip(bases, bases[1:])):
        water = base + layer
        clipped = water + layer
        assert clipped <= end, 'Ocean lost its original water grid/skirt'
        for i in range(water, end):
            assert all(math.isfinite(v) for v in positions[i] + normals[i]), 'Nonfinite ocean vertex'
            assert abs(math.sqrt(dot(normals[i], normals[i])) - 1) <= 1e-5, 'Opaque ocean normal is invalid'
            color = struct.unpack_from('<fff', wet_bytes, 48 + i * 44 + 32)
            assert max(abs(a - b) for a, b in zip(color, (0.015, 0.085, 0.12))) < 1e-7
        for i in range(water, water + grid):
            assert abs(math.sqrt(dot(positions[i], positions[i])) - 10) <= 1e-5, 'Original sea sample left sea level'
            sea_samples += 1
        dry_base = patch * layer
        triangles = []
        for t in range(0, dry_index_count, 3):
            ids = dry_indices[t:t+3]
            if any(dry_base <= i < dry_base + layer for i in ids):
                assert all(dry_base <= i < dry_base + layer for i in ids), 'Source crosses patch ranges'
                triangles.append(tuple(i - dry_base for i in ids))
        assert triangles, 'Source contains no ground triangles'
        edges = []
        for ids in triangles:
            surface = all(i < grid for i in ids)
            origin = positions[base + ids[0]]
            normal = cross(sub(positions[base + ids[1]], origin), sub(positions[base + ids[2]], origin))
            length = math.sqrt(dot(normal, normal))
            assert length > 0
            for a, b in zip(ids, ids[1:] + ids[:1]):
                av, bv = positions[water + a], positions[water + b]
                direction = sub(bv, av)
                denominator = dot(direction, direction)
                if denominator > 0:
                    edges.append((av, direction, denominator, surface, origin, normal, length))
        for i in range(clipped, end):
            q = positions[i]
            matches = []
            for av, direction, denominator, surface, origin, normal, length in edges:
                delta = sub(q, av)
                fraction = dot(delta, direction) / denominator
                if not -1e-5 <= fraction <= 1 + 1e-5:
                    continue
                residual = tuple(delta[axis] - fraction * direction[axis] for axis in range(3))
                # Same float-artifact bounds as the existing C++ shoreline contract.
                if dot(residual, residual) > 4e-12:
                    continue
                matches.append((surface, abs(dot(normal, sub(q, origin))) / length))
            assert matches, 'Shoreline vertex left every original water edge'
            surface_errors = [error for surface, error in matches if surface]
            if surface_errors:
                assert min(surface_errors) <= 2e-6, 'Surface shoreline misses the rendered ground plane'
                surface_crossings += 1
            shoreline += 1
        for t in range(0, wet_index_count, 3):
            ids = wet_indices[t:t+3]
            if any(base <= i < end for i in ids):
                assert all(base <= i < end for i in ids), 'Ocean crosses patch ranges'
    assert sea_samples and shoreline and surface_crossings, 'Fixture did not exercise sea samples and clipped shores'
    return {'sea_samples': sea_samples, 'shoreline_vertices': shoreline, 'surface_crossings': surface_crossings}


def verify_body_inventory(output):
    """Ranges must describe the final artifact, including clipped water."""
    data = (output / 'terrain.epoch_mesh').read_bytes()
    vertex_count, index_count = struct.unpack_from('<II', data, 40)
    frame = (output / 'terrain.frame').read_text()
    inventory = (output / 'terrain.bodies').read_text()
    prefix = 'epoch.planet.body-inventory.v1\n' + frame
    assert inventory.startswith(prefix), 'Body inventory belongs to a different frame'
    lines = inventory[len(prefix):].splitlines()
    count = int(lines.pop(0).removeprefix('bodies='))
    assert count == (6 if 'mode=universe\n' in frame else 1)
    names = ('body', 'parent', 'name', 'center_meters', 'radius_meters',
             'first_vertex', 'vertices', 'first_index', 'indices', 'minimum', 'maximum')
    assert len(lines) == count * len(names)
    vertex_at = index_at = 0
    for at in range(count):
        block = [line.split('=', 1) for line in lines[at*len(names):(at+1)*len(names)]]
        assert [key for key, value in block] == list(names)
        body = dict(block)
        vertices, indices = int(body['vertices']), int(body['indices'])
        assert int(body['first_vertex']) == vertex_at and int(body['first_index']) == index_at
        assert vertices >= 3 and indices >= 3 and indices % 3 == 0
        assert vertex_at + vertices <= vertex_count and index_at + indices <= index_count
        positions = [struct.unpack_from('<fff', data, 48 + 44*i)
                     for i in range(vertex_at, vertex_at + vertices)]
        for bound, fn in (('minimum', min), ('maximum', max)):
            assert [float(value) for value in body[bound].split(',')] == [fn(p[axis] for p in positions) for axis in range(3)]
        for index in struct.unpack_from(f'<{indices}I', data, 48 + 44*vertex_count + 4*index_at):
            assert vertex_at <= index < vertex_at + vertices, 'Body references another body mesh'
        vertex_at += vertices
        index_at += indices
    assert vertex_at == vertex_count and index_at == index_count, 'Body ranges omit final artifact data'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--producer', type=Path, required=True)
    parser.add_argument('--scratch', type=Path, required=True)
    args = parser.parse_args()
    args.scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mesh-export-', dir=args.scratch) as scratch:
        for mode in ('classic', 'tiled', 'hemisphere', 'planet', 'universe'):
            output = Path(scratch) / mode
            invocation = [str(args.producer), mode, str(output), '73']
            run = subprocess.run(invocation, capture_output=True, timeout=30)
            if run.returncode:
                raise AssertionError(f'{mode}: producer returned {run.returncode}')
            recipe = json.loads((output / 'terrain.preview.json').read_text())
            data = (output / 'terrain.epoch_mesh').read_bytes()
            magic, version = struct.unpack_from('<II', data)
            vertices, indices = struct.unpack_from('<II', data, 40)
            if (magic, version) != (0x48534D45, 1):
                raise AssertionError('Unexpected Engine artifact format')
            if vertices != recipe['vertices'] or indices != recipe['indices']:
                raise AssertionError('Recipe and artifact disagree')
            if len(data) != 48 + vertices * 44 + indices * 4:
                raise AssertionError('Unbounded or truncated artifact')
            verify_body_inventory(output)
            if recipe['preview_only'] is not True or recipe['meters_to_preview_units'] <= 0:
                raise AssertionError('Overview must retain an explicit metric conversion')
            for index in range(vertices):
                position = struct.unpack_from('<fff', data, 48 + index * 44)
                radius_limit = 10.1 if mode == 'planet' else 10.00005
                if sum(value * value for value in position) > radius_limit * radius_limit:
                    raise AssertionError('Overview exceeds its declared radius')
            if mode == 'planet':
                dry_planet = data
                if abs(recipe['meters_to_preview_units'] - 10 / 6371000) > 1e-20:
                    raise AssertionError('Static planet does not share streaming body-radius scale')
            digest = hashlib.sha256(data).digest()
            retried = subprocess.run(invocation, capture_output=True, timeout=30)
            if retried.returncode != 6 or hashlib.sha256((output / 'terrain.epoch_mesh').read_bytes()).digest() != digest:
                raise AssertionError('Existing authored destination was overwritten')
        explicit_dry = Path(scratch) / 'explicit-dry'
        dry_run = subprocess.run([str(args.producer), 'planet', str(explicit_dry), '73', '16', '4', '0', '128', '0'],
                                 capture_output=True, timeout=30)
        if dry_run.returncode or (explicit_dry / 'terrain.epoch_mesh').read_bytes() != dry_planet:
            raise AssertionError('Explicit false changed legacy terrain-only bytes')
        if 'ocean_surface' in json.loads((explicit_dry / 'terrain.preview.json').read_text()):
            raise AssertionError('Terrain-only metadata schema changed')
        wet_output = Path(scratch) / 'opaque-ocean'
        wet_run = subprocess.run([str(args.producer), 'planet', str(wet_output), '73', '16', '4', '0', '128', '1'],
                                 capture_output=True, timeout=30)
        if wet_run.returncode:
            raise AssertionError(f'Opaque ocean failed: {wet_run.returncode}')
        wet_recipe = json.loads((wet_output / 'terrain.preview.json').read_text())
        wet_bytes = (wet_output / 'terrain.epoch_mesh').read_bytes()
        if wet_recipe.get('ocean_surface') is not True or wet_bytes == dry_planet:
            raise AssertionError('Ocean option has no distinct artifact/settings identity')
        verify_body_inventory(wet_output)
        # The reported failure used this seed and Moon-sized primary body.
        # Universe mode also catches wrong offsets after the wet primary body.
        for mode in ('planet', 'universe'):
            output = Path(scratch) / f'moon-sized-wet-{mode}'
            cells = '16' if mode == 'planet' else '4'
            run = subprocess.run([str(args.producer), mode, str(output), '1337', cells, '4', '0', '128', '1',
                                  '--reference-radius-meters', '1737000'], capture_output=True, timeout=30)
            assert run.returncode == 0, (mode, run.returncode)
            verify_body_inventory(output)
        verify_ocean_vertices(dry_planet, wet_bytes, wet_recipe)
        # The clipping check must reject geometry corruption, including the old
        # spherical "fix" that passes a radius-only check but tears the coast.
        sea = (struct.unpack_from('<II', dry_planet, 40)[0]
               // len(wet_recipe['patches']))
        shore = 2 * sea
        original_sea = struct.unpack_from('<fff', wet_bytes, 48 + sea * 44)
        original_shore = struct.unpack_from('<fff', wet_bytes, 48 + shore * 44)
        shore_radius = math.sqrt(sum(value * value for value in original_shore))
        corruptions = (
            ('sea radius', sea, 0, tuple(value * 1.001 for value in original_sea)),
            ('sea normal', sea, 12, (0., 0., 0.)),
            ('nonfinite sea', sea, 0, (math.nan, original_sea[1], original_sea[2])),
            ('shore offset', shore, 0, (original_shore[0] + .01,
                                       original_shore[1] + .02, original_shore[2] - .01)),
            ('spherical shore projection', shore, 0,
             tuple(value * 10 / shore_radius for value in original_shore)),
        )
        for label, index, field, values in corruptions:
            corrupted = bytearray(wet_bytes)
            struct.pack_into('<fff', corrupted, 48 + index * 44 + field, *values)
            try:
                verify_ocean_vertices(dry_planet, corrupted, wet_recipe)
            except AssertionError:
                continue
            raise AssertionError(f'Ocean geometry check accepted {label} corruption')
        flat = Path(scratch) / 'unsupported-flat-ocean'
        invalid = subprocess.run([str(args.producer), 'classic', str(flat), '73', '4', '1', '0', '128', '1'],
                                 capture_output=True, timeout=10)
        if invalid.returncode == 0 or flat.exists():
            raise AssertionError('Unimplemented flat water was silently accepted')
        for body in range(1, 6):
            unsupported = Path(scratch) / f'unsupported-body-ocean-{body}'
            invalid = subprocess.run([str(args.producer), 'planet', str(unsupported), '73', '4', '1', str(body), '128', '1'],
                                     capture_output=True, timeout=10)
            if invalid.returncode == 0 or unsupported.exists():
                raise AssertionError('Unsupported selected-body ocean was silently accepted')
        rejected = subprocess.run([str(args.producer), 'unknown', str(Path(scratch) / 'bad')],
                                  capture_output=True, timeout=10)
        if rejected.returncode != 2 or (Path(scratch) / 'bad').exists():
            raise AssertionError('Unknown mode admitted')
        # Project authoring settings must reach the real generator, rather than
        # merely being saved in UI state while the defaults are used.
        custom = Path(scratch) / 'project-settings'
        invocation = [str(args.producer), 'tiled', str(custom), '4294967295', '8', '3', '2', '256.25']
        run = subprocess.run(invocation, capture_output=True, timeout=30)
        if run.returncode:
            raise AssertionError(f'Project generation failed: exit{run.returncode}')
        settings = json.loads((custom / 'terrain.preview.json').read_text())
        expected = {'mode': 'tiled', 'seed': 4294967295, 'cells_per_tile': 8,
                    'tiles_per_axis': 3, 'body_index': 2, 'tile_span_meters': 256.25}
        if any(settings[key] != value for key, value in expected.items()) or len(settings['patches']) != 9:
            raise AssertionError('Saved project settings were not used')
        for field, value in ((4, '0'), (5, '4294967295'), (6, '99'), (7, 'nan')):
            invalid = invocation.copy()
            invalid[2] = str(Path(scratch) / f'invalid-{field}')
            invalid[field] = value
            refused = subprocess.run(invalid, capture_output=True, timeout=10)
            if refused.returncode == 0 or Path(invalid[2]).exists():
                raise AssertionError('Invalid project settings produced output')
        # Original analytic world/moon centers must move with explicit time.
        centers = []
        for instant in (0, 12345.125):
            timed_output = Path(scratch) / f'universe-time-{instant}'
            run = subprocess.run([str(args.producer), 'universe', str(timed_output), '73',
                                  '--seconds', str(instant)], capture_output=True, timeout=30)
            assert run.returncode == 0
            meta = json.loads((timed_output / 'terrain.preview.json').read_text())
            assert meta['seconds'] == instant
            positions = {p['body']: p['body_center'] for p in meta['patches']}
            assert len(positions) == 6
            centers.append(positions)
            moon = [a-b for a,b in zip(positions[5], positions[0])]
            assert abs(math.sqrt(sum(v*v for v in moon)) - 42000000) < 1e-5
        assert all(centers[0][body] != centers[1][body] for body in range(6))
        old_universe = (Path(scratch) / 'universe' / 'terrain.epoch_mesh').read_bytes()
        assert (Path(scratch) / 'universe-time-0' / 'terrain.epoch_mesh').read_bytes() == old_universe
        assert (Path(scratch) / 'universe-time-12345.125' / 'terrain.epoch_mesh').read_bytes() != old_universe
        for invalid in ('-1', 'nan', 'inf', '1000000000000', '1e999'):
            dest = Path(scratch) / ('invalid-time-' + invalid)
            run = subprocess.run([str(args.producer), 'planet', str(dest), '--seconds', invalid], capture_output=True, timeout=30)
            assert run.returncode == 2 and not dest.exists()

        large_output = Path(scratch) / 'explicit-large-snapshot'
        large = [str(args.producer), 'tiled', str(large_output), '1337', '32', '8', '0', '128']
        rejected = subprocess.run(large, capture_output=True, timeout=30)
        if rejected.returncode != 4 or large_output.exists():
            raise AssertionError('Default snapshot byte budget silently expanded')
        accepted = subprocess.run(large + ['--mesh-bytes', str(8 * 1024 * 1024)], capture_output=True, timeout=30)
        if accepted.returncode:
            raise AssertionError(f'Explicit large static snapshot refused: {accepted.returncode}')
        raw = (large_output / 'terrain.epoch_mesh').read_bytes()
        vertex_count, index_count = struct.unpack_from('<II', raw, 40)
        if not (4 * 1024 * 1024 < len(raw) <= 8 * 1024 * 1024 and vertex_count > 65536 and index_count > 196608):
            raise AssertionError('Explicit large snapshot did not exercise the new admitted limits')
        long_parent = Path(scratch) / ('long-source-' + 'x' * 75) / ('long-project-' + 'y' * 75)
        long_parent.mkdir(parents=True)
        long_output = long_parent / 'overview'
        generated = subprocess.run([str(args.producer), 'classic', str(long_output), '73'],
                                   capture_output=True, timeout=30)
        if generated.returncode or not (long_output / 'terrain.epoch_mesh').is_file():
            raise AssertionError(f'Long project path failed: exit{generated.returncode}')


if __name__ == '__main__':
    main()
