"""Synthetic archive/schema tests; these do not prove a Linux build or smoke."""
import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch
from Tools.build import audit_epoch_linux_package as audit
from Tools.build import verify_public_source as guard
ROOT = Path(__file__).resolve().parents[2]

class LinuxPackageDelivery(unittest.TestCase):
    def fixture(self, folder, extra=None, marker_edition='epoch.public.github.v1'):
        version='0.90.183'; prefix='epoch_linux_x64_v'+version
        archive=Path(folder)/(prefix+'.tar.gz')
        elf=bytearray(20);elf[:6]=b'\x7fELF\x02\x01';elf[18:20]=b'\x3e\x00'
        files={'epoch':bytes(elf),'assets/fonts/Roboto-Regular.ttf':b'fixture',
            'LICENSE':b'fixture','THIRD_PARTY_NOTICES.txt':b'fixture',
            'THIRD_PARTY_COMPONENTS.json':b'[]',
            'epoch-distribution.json':json.dumps({'schema':'epoch.distribution.v1',
                'edition':marker_edition,'version':version}).encode()}
        with tarfile.open(archive,'w:gz') as tar:
            for name,data in files.items():
                entry=tarfile.TarInfo(prefix+'/'+name);entry.size=len(data);tar.addfile(entry,io.BytesIO(data))
            if extra:
                entry=tarfile.TarInfo(prefix+'/'+extra);entry.size=1;tar.addfile(entry,io.BytesIO(b'x'))
        digest=hashlib.sha256(archive.read_bytes()).hexdigest()
        Path(str(archive)+'.sha256').write_text(digest+'  '+archive.name+'\n')
        receipt={'schema':'epoch.linux-package-validation.v1','edition':'epoch.public.github.v1',
            'version':version,'artifact':archive.name,'sha256':digest,'configuration':'Clang-Release',
            'version_check':'pass','engine_contract':'pass','shared_libraries':'pass',
            'archive_inventory':'pass','runtime_smoke':'pass','runtime_renderer':'software_opengl',
            'hardware_parity':False}
        Path(str(archive)+'.build-validation.json').write_text(json.dumps(receipt))
        return archive

    def test_archive_payload_edition_and_evidence_are_fail_closed(self):
        parent=ROOT/'build/contract-tests';parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent,prefix='linux-package-schema-') as folder:
            self.assertEqual(audit.audit(self.fixture(folder))['runtime_renderer'],'software_opengl')
            for bad in ('logs/session.log','cache/private.bin','Engine/private.cpp','assets/../private.txt'):
                with self.subTest(bad=bad),self.assertRaises(ValueError):audit.audit(self.fixture(folder,bad))
            with self.assertRaises(ValueError):audit.audit(self.fixture(folder,marker_edition='epoch.development.site.v1'))
            archive=self.fixture(folder)
            Path(str(archive)+'.sha256').write_text('0'*64+'  '+archive.name)
            with self.assertRaises(ValueError):audit.audit(archive)
            archive=self.fixture(folder)
            receipt=Path(str(archive)+'.build-validation.json');data=json.loads(receipt.read_text())
            data['runtime_smoke']='not_run';receipt.write_text(json.dumps(data))
            with self.assertRaises(ValueError):audit.audit(archive)

    def test_only_reviewed_three_artifact_upload_is_admitted(self):
        workflow=(ROOT/'Tools/build/public_ci_workflow.yml').read_bytes().replace(b'\r\n',b'\n')
        guard.verify_workflow(workflow)
        for old,new in ((b'contents: read',b'contents: write'),
                        (b'epoch_linux_x64_v${{ steps.package.outputs.version }}.tar.gz\n',b'**\n'),
                        (b'include-hidden-files: false',b'include-hidden-files: true')):
            altered=workflow.replace(old,new)
            self.assertNotEqual(altered,workflow)
            # Even a mistakenly refreshed hash cannot broaden the reviewed upload allowlist.
            with patch.object(guard,'WORKFLOW_SHA256',guard.source_digest(guard.WORKFLOW,altered)):
                with self.assertRaises(RuntimeError):guard.verify_workflow(altered)
        self.assertNotIn(b'-SkipRuntimeSmoke',workflow)
        self.assertIn(b'clang Release -- -DBUILD_TESTING=ON',workflow)
        self.assertIn(b'LIBGL_ALWAYS_SOFTWARE=1 xvfb-run',workflow)

if __name__=='__main__':unittest.main()
