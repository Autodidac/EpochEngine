# SPDX-License-Identifier: LicenseRef-MIT-NoSell
# Copyright (c) 2026 Adam Rushford
"""Export one committed Extensions source identity; never collect working/build data."""
from __future__ import annotations
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_URL = 'https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git'


def git(*args):
    git_directory = ROOT / '.git'
    if (not git_directory.is_dir() or git_directory.is_symlink() or
            (hasattr(git_directory, 'is_junction') and git_directory.is_junction())):
        raise RuntimeError('Extensions packaging requires its own physical .git directory')
    return subprocess.check_output([
        'git', '--git-dir', str(git_directory), '--work-tree', str(ROOT), *args])


def committed_source():
    if git('remote', 'get-url', 'origin').decode().strip() != REPOSITORY_URL:
        raise RuntimeError('Unexpected source repository')
    if git('status', '--porcelain', '--untracked-files=all').strip():
        raise RuntimeError('Commit the reviewed source before packaging')
    commit = git('rev-parse', 'HEAD').decode().strip()
    files = {}
    with tarfile.open(fileobj=io.BytesIO(git('-c', 'core.autocrlf=false', '-c', 'core.eol=lf',
                                            '-c', 'tar.umask=0022',
                                            'archive', '--format=tar', commit))) as archive:
        for item in archive:
            if item.isdir():
                continue
            name = PurePosixPath(item.name)
            if (not item.isfile() or name.is_absolute() or '..' in name.parts or
                    any(p in {'.git', 'build', 'out', '.ci-engine', '.ci-tools', 'cache', '__pycache__'} for p in name.parts) or
                    name.suffix.lower() in {'.exe', '.dll', '.obj', '.pdb', '.lib', '.gguf', '.jsonl', '.zip'}):
                raise RuntimeError('Non-source entry in committed archive: '+item.name)
            files[item.name] = (archive.extractfile(item).read(), item.mode)
    required = {'LICENSE', 'README.md', 'CMakeLists.txt', 'catalog/extensions.json',
                'demonstrations/tiered_terrain/modules/tiered_terrain.generator.ixx',
                'packages/_api/epoch_package_script_api.hpp'}
    if not required <= files.keys():
        raise RuntimeError('Incomplete optional source owner')
    cmake = files['CMakeLists.txt'][0].decode()
    version = re.search(r'project\(EpochEngineExtensions VERSION ([0-9.]+)', cmake).group(1)
    return commit, version, files


def package(output):
    commit, version, files = committed_source()
    output = output.absolute()
    if output == ROOT or output.is_relative_to(ROOT):
        raise RuntimeError('Package outside the source repository in checkout-local staging')
    for parent in [output, *output.parents]:
        if parent.exists() and (parent.is_symlink() or (hasattr(parent, 'is_junction') and parent.is_junction())):
            raise RuntimeError('Linked output ancestor')
    output.mkdir(parents=True, exist_ok=False)
    prefix = 'EpochEngineExtensions-'+version+'/'
    stem = f'EpochEngineExtensions-{version}-{commit[:8]}-source'
    stamp = int(git('show', '-s', '--format=%ct', commit).strip())
    # A fixed archive timestamp/order makes repeat source exports reproducible.
    with zipfile.ZipFile(output/(stem+'.zip'), 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, (body, mode) in sorted(files.items()):
            entry = zipfile.ZipInfo(prefix+name, date_time=(2026, 1, 1, 0, 0, 0))
            entry.external_attr = (0o100000|mode)<<16
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, body)
    with (output/(stem+'.tar.gz')).open('xb') as raw:
        with gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=stamp) as compressed:
            with tarfile.open(fileobj=compressed, mode='w', format=tarfile.PAX_FORMAT) as archive:
                for name, (body, mode) in sorted(files.items()):
                    entry = tarfile.TarInfo(prefix+name)
                    entry.size, entry.mode, entry.mtime = len(body), mode, stamp
                    archive.addfile(entry, io.BytesIO(body))
    with zipfile.ZipFile(output/(stem+'.zip')) as archive:
        zipped = {n[len(prefix):]:archive.read(n) for n in archive.namelist()}
    with tarfile.open(output/(stem+'.tar.gz'), 'r:gz') as archive:
        tarred = {e.name[len(prefix):]:archive.extractfile(e).read() for e in archive if e.isfile()}
    if zipped != tarred or zipped != {n:b for n,(b,m) in files.items()}:
        raise RuntimeError('ZIP/TAR committed source bodies differ')
    assets = []
    for suffix in ('.zip', '.tar.gz'):
        path = output/(stem+suffix)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        (output/(path.name+'.sha256')).write_text(digest+'  '+path.name+'\n', encoding='utf-8')
        assets.append(dict(name=path.name, bytes=path.stat().st_size, sha256=digest))
    receipt = dict(repository=REPOSITORY_URL, commit=commit, version=version,
                   files=len(files), license_sha256=hashlib.sha256(files['LICENSE'][0]).hexdigest(),
                   contents='Complete committed source only; no history, binaries, models or runtime state', assets=assets)
    (output/'source-package-receipt.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    package(parser.parse_args().output)
