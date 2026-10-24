#!/usr/bin/env python3
"""Audit staged Linux release bytes without extracting or publishing them."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import tarfile


def audit(archive):
    archive = Path(archive)
    match = re.fullmatch(r"epoch_linux_x64_v([0-9]+\.[0-9]+\.[0-9]+)\.tar\.gz", archive.name)
    if not match:
        raise ValueError("Invalid Linux runtime archive name")
    version = match[1]
    with archive.open('rb') as source:
        digest = hashlib.file_digest(source, 'sha256').hexdigest()
    expected = digest + '  ' + archive.name
    if Path(str(archive)+'.sha256').read_text(encoding='ascii').strip() != expected:
        raise ValueError("Archive checksum mismatch")
    receipt = json.loads(Path(str(archive)+'.build-validation.json').read_text(encoding='utf-8'))
    expected_keys = {'schema','edition','version','artifact','sha256','configuration',
                     'version_check','engine_contract','shared_libraries','archive_inventory',
                     'runtime_smoke','runtime_renderer','hardware_parity'}
    if set(receipt) != expected_keys or receipt['schema'] != 'epoch.linux-package-validation.v1':
        raise ValueError("Unexpected validation receipt")
    if (receipt['version'] != version or receipt['artifact'] != archive.name
            or receipt['sha256'] != digest or receipt['configuration'] != 'Clang-Release'
            or receipt['edition'] not in ('epoch.public.github.v1','epoch.development.site.v1')
            or receipt['hardware_parity'] is not False):
        raise ValueError("Artifact identity/edition mismatch")
    for key in ('version_check','engine_contract','shared_libraries','archive_inventory'):
        if receipt[key] != 'pass': raise ValueError("Incomplete build/package validation")
    if (receipt['runtime_smoke'],receipt['runtime_renderer']) not in (
            ('pass','software_opengl'),('pass','opengl'),('not_run','not_run')):
        raise ValueError("Inconsistent native smoke evidence")
    prefix = archive.name.removesuffix('.tar.gz')
    allowed = {'epoch','epoch-distribution.json','assets','lib','licenses','LICENSE','README.md',
               'THIRD_PARTY_NOTICES.txt','THIRD_PARTY_COMPONENTS.json'}
    required = {prefix+'/'+s for s in ('epoch','epoch-distribution.json',
        'assets/fonts/Roboto-Regular.ttf','LICENSE','THIRD_PARTY_NOTICES.txt','THIRD_PARTY_COMPONENTS.json')}
    names = set()
    total = 0
    with tarfile.open(archive, 'r:gz') as package:
        for member in package:
            path = PurePosixPath(member.name)
            if (path.is_absolute() or '..' in path.parts or '\\' in member.name
                    or not path.parts or path.parts[0] != prefix or member.name in names):
                raise ValueError("Unsafe/duplicate archive path")
            names.add(member.name)
            total += member.size
            if len(names) > 100000 or total > 10*1024**3: raise ValueError("Archive exceeds bounds")
            if len(path.parts)>1 and path.parts[1] not in allowed:
                raise ValueError("Unadmitted runtime payload")
            if any(p.casefold() in {'logs','cache','.git','.env','projects','epochselfcoding'} for p in path.parts):
                raise ValueError("Private or generated runtime data")
            if member.issym() or member.islnk():
                target = PurePosixPath(member.linkname)
                resolved = posixpath.normpath(str(path.parent/target) if member.issym() else str(target))
                if target.is_absolute() or not resolved.startswith(prefix+'/'):
                    raise ValueError("Archive link escapes package")
            elif not member.isdir() and not member.isfile():
                raise ValueError("Non-file runtime archive entry")
            if member.name == prefix+'/epoch':
                header = package.extractfile(member).read(20)
                if len(header)!=20 or header[:6] != b'\x7fELF\x02\x01' or header[18:20] != b'\x3e\x00':
                    raise ValueError("Runtime is not an ELF x86-64 binary")
            if member.name == prefix+'/epoch-distribution.json':
                if member.size > 4096: raise ValueError("Oversized edition marker")
                identity = json.load(package.extractfile(member))
                if identity != {'schema':'epoch.distribution.v1','edition':receipt['edition'],'version':version}:
                    raise ValueError("Packaged edition marker mismatch")
    if not required <= names: raise ValueError("Required runtime payload missing")
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.archive)
    print('linux_package_audit.result=pass smoke='+result['runtime_smoke'])
