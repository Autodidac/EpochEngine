"""Verify actual offline output without opening an audio device or replacing files."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import sys
import tempfile

exe = Path(sys.argv[1]).resolve()
base = Path(sys.argv[2]).resolve()
base.mkdir(parents=True, exist_ok=True)
fixture = Path(tempfile.mkdtemp(prefix="wav-contract-", dir=base))
for arguments in ([], ["--help"], ["-h"]):
    help_run = subprocess.run([str(exe), *arguments], cwd=fixture, check=True,
                              capture_output=True, text=True)
    if "Usage: epoch_audio_demo" not in help_run.stdout or any(fixture.iterdir()):
        raise SystemExit("Help must print usage without creating a file")
output = fixture / "tone.wav"
run = subprocess.run([str(exe), str(output)], check=True, capture_output=True, text=True)
if "Wrote one second of stereo PCM16" not in run.stdout:
    raise SystemExit("Expected console confirmation of the output")
data = output.read_bytes()
if len(data) != 192044 or data[:4] != b"RIFF" or data[8:16] != b"WAVEfmt ":
    raise SystemExit("Invalid RIFF size/signature")
if struct.unpack_from("<I", data, 4)[0] != len(data) - 8:
    raise SystemExit("Invalid RIFF length")
if struct.unpack_from("<IHHIIHH", data, 16) != (16, 1, 2, 48000, 192000, 4, 16):
    raise SystemExit("Expected PCM16 stereo 48kHz")
if data[36:40] != b"data" or struct.unpack_from("<I", data, 40)[0] != 192000:
    raise SystemExit("Invalid PCM chunk")
samples = struct.unpack_from("<96000h", data, 44)
if not any(samples) or samples[0::2] != samples[1::2]:
    raise SystemExit("Expected nonzero equal-channel stereo signal")
before = hashlib.sha256(data).hexdigest()
retry = subprocess.run([str(exe), str(output)], capture_output=True, text=True)
if retry.returncode == 0 or "Output already exists" not in retry.stderr or hashlib.sha256(output.read_bytes()).hexdigest() != before:
    raise SystemExit("Existing output was overwritten")
missing = fixture / "missing" / "tone.wav"
refused = subprocess.run([str(exe), str(missing)], capture_output=True, text=True)
if refused.returncode == 0 or "Could not create the WAV file" not in refused.stderr or missing.exists():
    raise SystemExit("Missing output directory must report an error")
receipt = {"file": str(output), "bytes": len(data), "sample_rate": 48000,
           "channels": 2, "bits_per_sample": 16, "nonzero": True,
           "no_overwrite": True, "help_file_free": True, "output_errors_reported": True,
           "sha256": before, "retry_exit": retry.returncode}
(fixture / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
print(json.dumps(receipt))
