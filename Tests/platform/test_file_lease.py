"""Exercise real native file leases and crash retirement in separate processes."""
import argparse
import errno
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--helper', type=Path, required=True)
    parser.add_argument('--scratch', type=Path, required=True)
    parser.add_argument('--result', type=Path, required=True)
    args = parser.parse_args()
    args.scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='file-lease-', dir=args.scratch) as temporary:
        root = Path(temporary).resolve()
        assert root.is_relative_to(args.scratch.resolve())
        lock = root / 'cache.lock'

        def run(mode, target, expected):
            result = subprocess.run([str(args.helper), mode, str(target)], capture_output=True, timeout=10)
            assert result.returncode == expected, (mode, str(target), result.returncode, expected)

        run('--try', lock, 0)
        assert lock.is_file() and lock.stat().st_size == 0
        run('--move', lock, 0)
        ready = root / 'held.txt'
        child = subprocess.Popen([str(args.helper), '--hold', str(lock), str(ready)],
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 5
            while not ready.exists() and child.poll() is None and time.monotonic() < deadline:
                time.sleep(0.01)
            assert ready.read_text() == 'held\n', 'Lease holder never became ready'
            run('--try', lock, 3)
            # This exact Popen handle owns the test helper; no Engine/model runs.
            child.kill()
            child.wait(timeout=5)
            run('--try', lock, 0)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait(timeout=5)
            if child.stdin:
                child.stdin.close()
            if child.stdout:
                child.stdout.close()
            if child.stderr:
                child.stderr.close()
        nonempty = root / 'authored.txt'
        nonempty.write_bytes(b'retain this source')
        run('--try', nonempty, 4)
        assert nonempty.read_bytes() == b'retain this source'
        directory = root / 'directory.lock'
        directory.mkdir()
        run('--try', directory, 4)
        fifo_checked = False
        fifo_skip_reason = 'not applicable on Windows'
        if os.name != 'nt':
            fifo = root / 'not-a-regular-file'
            try:
                os.mkfifo(fifo)
            except OSError as error:
                if error.errno not in {errno.EOPNOTSUPP, errno.ENOTSUP}:
                    raise
                fifo_skip_reason = 'fixture filesystem does not support FIFO (EOPNOTSUPP)'
                print('FIFO refusal case skipped: ' + fifo_skip_reason)
            else:
                run('--try', fifo, 4)
                fifo_checked = True
                fifo_skip_reason = ''
        run('--try', root / 'directory.lock' / '..' / 'cache.lock', 4)
        hardlink = root / 'hardlink.lock'
        os.link(lock, hardlink)
        run('--try', hardlink, 4)
        hardlink.unlink()
        link = root / 'redirect.lock'
        link.symlink_to(lock)
        run('--try', link, 4)
        outside = root / 'other-owned-fixture'
        outside.mkdir()
        parent_link = root / 'redirect-parent'
        parent_link.symlink_to(outside, target_is_directory=True)
        run('--try', parent_link / 'cache.lock', 4)
        assert not (outside / 'cache.lock').exists()
        parent_link.unlink()
        junction_checked = False
        if os.name == 'nt':
            junction = root / 'junction-parent'
            created = subprocess.run(['cmd.exe', '/d', '/c', 'mklink', '/J', str(junction), str(outside)],
                                     capture_output=True, timeout=10)
            assert created.returncode == 0
            try:
                run('--try', junction / 'cache.lock', 4)
                assert not (outside / 'cache.lock').exists()
                junction_checked = True
            finally:
                junction.rmdir()
        args.result.write_text(json.dumps(dict(native_cpu_pass=True, two_process_busy=True,
            forced_exit_releases=True, move_only=True, nonempty_untouched=True,
            hardlink_refused=True, symlink_refused=True, parent_symlink_refused=True,
            fifo_refused=fifo_checked, fifo_skip_reason=fifo_skip_reason,
            junction_refused=junction_checked, renderer_started=False), indent=2) + '\n')


if __name__ == '__main__':
    main()
