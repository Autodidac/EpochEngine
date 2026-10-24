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
output = fixture / "tone.wav"
subprocess.run([str(exe), str(output)], check=True)
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
retry = subprocess.run([str(exe), str(output)])
if retry.returncode == 0 or hashlib.sha256(output.read_bytes()).hexdigest() != before:
    raise SystemExit("Existing output was overwritten")
receipt = {"file": str(output), "bytes": len(data), "sample_rate": 48000,
           "channels": 2, "bits_per_sample": 16, "nonzero": True,
           "no_overwrite": True, "sha256": before, "retry_exit": retry.returncode}
(fixture / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
print(json.dumps(receipt))
