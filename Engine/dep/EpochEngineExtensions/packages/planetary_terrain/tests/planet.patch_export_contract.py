"""Actual provider opt-in/legacy transport and admitted hierarchy CPU replay."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--producer', type=Path, required=True)
    parser.add_argument('--consumer', type=Path, required=True)
    parser.add_argument('--scratch', type=Path, required=True)
    args = parser.parse_args()
    args.scratch.mkdir(parents=True, exist_ok=True)
    # tempfile is explicitly checkout-local. Retain failures and all evidence.
    root = Path(tempfile.mkdtemp(prefix='patch-export-', dir=args.scratch))
    source = 'sha256:' + 'a' * 64
    recipe = 'b' * 64

    def step(request, modern=True, extra=()):
        output = root / (f'modern-{request}' if modern else f'legacy-{request}')
        cache = root / ('modern-cache' if modern else 'legacy-cache')
        command = [str(args.producer), '--stream-step-preview', str(output), str(cache),
                   '1337', '4', '0', '0', '11', '0', '1080', '1.0471975511965976',
                   '3', '48', str(64 * 1024 * 1024), '32', str(request)]
        if modern:
            command += ['--patch-transport-v1', source + ':' + recipe]
        command += list(extra)
        result = subprocess.run(command, capture_output=True, timeout=30)
        assert result.returncode == 0, (result.returncode, command, result.stderr)
        lines = (output / 'terrain.stream').read_text().splitlines()
        assert lines[0] == ('epoch.planet.stream.v6' if modern else 'epoch.planet.stream.v1')
        receipt = dict(line.split('=', 1) for line in lines[1:])
        if receipt['snapshot_ready'] == '1':
            if modern:
                assert receipt['patch_file'] == 'terrain.epoch_planet_patch'
                sidecar = (output / receipt['patch_file']).read_bytes()
                assert sidecar[:8] == struct.pack('<II', 0x544c5050, 1)
                assert sidecar[48:119].decode() == source
                assert sidecar[119:183].decode() == recipe
                patches, vertices = struct.unpack_from('<II', sidecar, 183)
                assert vertices == int(receipt['vertices'])
                assert patches >= int(receipt['draw'])
                assert len(sidecar) == 191 + 72 * patches + 12 * vertices + 32
                subprocess.run([str(args.consumer), str(output / 'terrain.epoch_mesh'),
                                str(output / receipt['patch_file']), str(request)], check=True, timeout=30)
            else:
                assert 'patch_file' not in receipt
                assert not (output / 'terrain.epoch_planet_patch').exists()
        return output, receipt

    legacy, old = step(1, False)
    for request in range(1, 9):
        modern, current = step(request)
        if current['pending'] == '0':
            break
    assert current['pending'] == '0'
    assert int(current['draw']) > 6
    # New explicit transport includes the retained ancestors; legacy does not.
    assert int(current['vertices']) > int(old['vertices'])
    # The original argument contract still rejects an unrecognized opt-in.
    invalid = subprocess.run([str(args.producer), '--stream-step-preview', str(root / 'invalid'),
                              str(root / 'invalid-cache'), '1337', '4', '0', '0', '11', '0',
                              '1080', '1.0471975511965976', '3', '48', '67108864', '32', '10',
                              '--patch-transport-v1', 'invalid'], capture_output=True, timeout=30)
    assert invalid.returncode == 2
    # A modern wet hierarchy includes hidden ancestors and clipped coastlines.
    # The selected-leaf estimate alone fits32MiB at576 leaves, while the actual
    # hierarchy used to overflow during assembly and retain an old camera frame.
    # Exercise the exact failed camera/settings through the real codec consumer.
    budget_reductions = 0
    with tempfile.TemporaryDirectory(prefix='snapshot-budget-', dir=args.scratch) as temporary:
        budget_root = Path(temporary)
        for request in range(1, 10):
            output = budget_root / f'output-{request}'
            command = [str(args.producer), '--stream-step-preview', str(output), str(budget_root / 'cache'),
                       '1337', '16', '0', '-15.993193229761141', '6.3980823707129746', '-20.137832859895834',
                       '824', '1.0499999523162842', '20', '576', str(192 * 1024 * 1024), '128', str(request), '1',
                       '--mesh-bytes', str(32 * 1024 * 1024), '--seconds', '4.1833333333333238',
                       '--patch-pixels', '170', '--lod-hysteresis', '0.14999999999999999',
                       '--patch-transport-v1', source + ':' + recipe, '--metric-frame-v1',
                       '--view-footprint-v1', '0.60351672565136361,-0.24143707059294234,0.75991822112813923,1.7815533980582525,0.15015337598378095,0.97041647808737186,0.18906565721244756']
            completed = subprocess.run(command, capture_output=True, timeout=30)
            assert completed.returncode == 0, (completed.returncode, command, completed.stderr)
            current = dict(line.split('=', 1) for line in (output / 'terrain.stream').read_text().splitlines()[1:])
            if current['snapshot_ready'] == '1':
                assert (output / 'terrain.epoch_mesh').stat().st_size <= 32 * 1024 * 1024
                subprocess.run([str(args.consumer), str(output / 'terrain.epoch_mesh'),
                                str(output / 'terrain.epoch_planet_patch'), str(request), '--large-selected'], check=True, timeout=30)
            reduction = output / 'terrain.snapshot-budget'
            if reduction.exists():
                reduced = dict(line.split('=', 1) for line in reduction.read_text().splitlines()[1:])
                assert 6 <= int(reduced['admitted_draw']) < int(reduced['requested_draw']) <= 576
                assert int(reduced['maximum_bytes']) == 32 * 1024 * 1024
                assert current['pending'] == '0' and not (output / 'terrain.failure').exists()
                budget_reductions += 1
                break
            # Only the last output is needed to diagnose a failure.
            import shutil
            shutil.rmtree(output)
        assert budget_reductions, 'The real hierarchy-overflow regression did not exercise coarsening'
    print(json.dumps({'scope': 'CPU provider/codec/hierarchy replay only',
                      'legacy_vertices': int(old['vertices']), 'modern_vertices': int(current['vertices']),
                      'draw': int(current['draw']), 'request': request, 'snapshot_budget_reductions': budget_reductions, 'scratch': str(root)}))


if __name__ == '__main__':
    main()
