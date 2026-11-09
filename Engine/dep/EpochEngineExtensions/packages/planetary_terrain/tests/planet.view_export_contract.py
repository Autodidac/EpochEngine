"""Actual CPU producer: legacy cone and rolled footprint receipts/admission."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--producer', type=Path, required=True)
    parser.add_argument('--scratch', type=Path, required=True)
    parser.add_argument('--result', type=Path)
    args = parser.parse_args()
    args.scratch.mkdir(parents=True, exist_ok=True)
    receipts = []
    with tempfile.TemporaryDirectory(prefix='view-export-', dir=args.scratch) as temporary:
        root = Path(temporary)
        sequence = 0

        def run(flag, packet, expected=0, clouds=False):
            nonlocal sequence
            sequence += 1
            output, cache = root / f'output-{sequence}', root / ('cloud-cache' if clouds else f'cache-{sequence}')
            command = [str(args.producer), '--stream-step-preview', str(output), str(cache),
                       '1337', '4', '0', '10.0001', '0', '0', '1080', '1.0471975511965976',
                       '20', '384', str(128 * 1024 * 1024), '6', str(sequence),
                       '--mesh-bytes', str(32 * 1024 * 1024),
                       '--patch-transport-v1', 'sha256:' + 'a' * 64 + ':' + 'b' * 64,
                       '--metric-frame-v1', flag, packet]
            if clouds:
                command.append("--cloud-layer-v1")
            completed = subprocess.run(command, capture_output=True, timeout=30)
            if expected:
                assert completed.returncode != 0, command
                assert not output.exists() and not cache.exists(), 'invalid basis mutated output/cache'
                return
            assert completed.returncode == 0, (command, completed.stdout, completed.stderr)
            if clouds:
                encoded = (output / 'terrain.epoch_planet_patch').read_bytes()
                assert struct.unpack_from('<II', encoded) == (0x544c5050, 4)
                assert (cache / 'cloud.epoch_mesh').is_file()
            lines = (output / 'terrain.stream').read_text().splitlines()
            receipt = dict(line.split('=', 1) for line in lines[1:])
            assert int(receipt['resident_bytes']) <= int(receipt['maximum_bytes']) == 128 * 1024 * 1024
            assert int(receipt['built']) <= 6 and int(receipt['draw']) <= 384
            assert int(receipt['max_level']) <= 20
            receipts.append({'schema': lines[0], 'receipt': receipt})
            return lines, receipt

        lines, legacy = run('--view-cone-v1', '-1,0,0,1.7777777777777777')
        assert lines[0] == 'epoch.planet.stream.v8'
        assert [line.split('=', 1)[0] for line in lines[-4:]] == [
            'view_forward_x', 'view_forward_y', 'view_forward_z', 'view_aspect']
        for up in ('0,1,0', '0,0,1'):
            lines, footprint = run('--view-footprint-v1', '-1,0,0,1.7777777777777777,' + up)
            assert lines[0] == 'epoch.planet.stream.v9'
            assert [line.split('=', 1)[0] for line in lines[-7:]] == [
                'view_forward_x', 'view_forward_y', 'view_forward_z', 'view_aspect',
                'view_up_x', 'view_up_y', 'view_up_z']
            assert [float(footprint['view_up_' + axis]) for axis in 'xyz'] == [float(x) for x in up.split(',')]
        run('--view-cone-v1', '-1,0,0,1.7777777777777777', clouds=True)
        cloud_cache = root / 'cloud-cache' / 'cloud.epoch_mesh'
        cloud_before = (cloud_cache.stat().st_mtime_ns, cloud_cache.read_bytes())
        run('--view-cone-v1', '-1,0,0,1.7777777777777777', clouds=True)
        assert (cloud_cache.stat().st_mtime_ns, cloud_cache.read_bytes()) == cloud_before, 'cloud geometry regenerated'
        for packet in ('-1,0,0,1,0,0,0', '-1,0,0,1,1,0,0', '-1,0,0,1,0,nan,0',
                       '-1,0,0,1,0,1', '-1,0,0,1,0,1,0,1', '0,0,0,1,0,1,0', '-1,0,0,0,0,1,0'):
            run('--view-footprint-v1', packet, expected=1)
        run('--view-cone-v1', '-1,0,0,1,0,1,0', expected=1)
    if args.result:
        args.result.write_text(json.dumps({'passed': True, 'producer_sha256': hashlib.sha256(args.producer.read_bytes()).hexdigest(),
                                          'receipts': receipts, 'invalid_packets_refused': 8,
                                          'gpu_or_native_acceptance': False}, indent=2) + '\n')


if __name__ == '__main__':
    main()
