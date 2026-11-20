"""Real scheduler/provider export and v4/v5/v6 codec admission, without a renderer."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--producer', type=Path, required=True)
parser.add_argument('--consumer', type=Path, required=True)
parser.add_argument('--scratch', type=Path, required=True)
args = parser.parse_args()
args.scratch.mkdir(parents=True, exist_ok=True)
run_root = Path(tempfile.mkdtemp(prefix='density-', dir=args.scratch))
receipts = []
for sequence, (flag, body, version) in enumerate(
        (('--cloud-layer-v1', 0, 4), ('--cloud-density-v1', 0, 5),
         ('--cloud-density-v2', 0, 6), ('--cloud-density-v2', 5, 1)), 1):
    output = run_root / f'output-{sequence}'
    cache = run_root / f'cache-{sequence}'
    if output.exists():
        raise RuntimeError('Use a fresh scratch directory; retained evidence will not be overwritten')
    command = [str(args.producer), '--stream-step-preview', str(output), str(cache),
               '1337', '4', str(body), '10.0001', '0', '0', '1080', '1.0471975511965976',
               '3', '48', str(64 * 1024 * 1024), '6', str(sequence),
               '--mesh-bytes', str(32 * 1024 * 1024),
               '--patch-transport-v1', 'sha256:' + 'a' * 64 + ':' + 'b' * 64,
               '--metric-frame-v1', '--view-footprint-v1', '-1,0,0,1.7777777777777777,0,1,0', flag]
    completed = subprocess.run(command, capture_output=True, timeout=30)
    assert completed.returncode == 0, (command, completed.returncode, completed.stdout, completed.stderr)
    receipt = dict(line.split('=', 1) for line in (output / 'terrain.stream').read_text().splitlines()[1:])
    assert receipt['snapshot_ready'] == '1', receipt
    encoded = (output / 'terrain.epoch_planet_patch').read_bytes()
    assert struct.unpack_from('<II', encoded) == (0x544c5050, version)
    if version == 6:
        assert struct.unpack_from('<d', encoded, len(encoded) - 40) == (6_349_751.0,)
    assert (cache / 'cloud.epoch_mesh').exists() == (version == 4)
    subprocess.run([str(args.consumer), str(output / 'terrain.epoch_mesh'),
                    str(output / 'terrain.epoch_planet_patch'), str(sequence)], check=True, timeout=30)
    receipts.append({'transport': flag, 'body': body, 'version': version,
                     'metadata_bytes': len(encoded), 'receipt': receipt})
(run_root / 'result.json').write_text(json.dumps({'pass': True, 'receipts': receipts}, indent=2))
