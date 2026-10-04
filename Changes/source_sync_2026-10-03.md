# October 3 v0.90.34 source-only synchronization

Completed operator request: publish the AI repair source to GitHub and the Site,
without creating another binary release. This receipt records an already-frozen
source checkpoint; a subsequent documentation-only commit does not rewrite its
archives or imply native AI acceptance.

## Source identity

- GitHub `main` and `multicontext-base-stable` were synchronized at
  `23d62a5d08ccc1944ad8d2a8ac966a63054619fa` before export. Tree:
  `4c0b9074c9d8cb46f4decc12c589ecda8e64220f`, 1,066 committed files.
- This includes AI repair `1a4818247acf6b3b899a3ddd68a7b034da5d0302`:
  phase-specific prompts, bounded worker-owned streamed-response assembly,
  separate reasoning, complete terminal framing before source admission and
  bounded runaway metadata/reasoning recovery. Partial tool arguments never stage.
- Canonical source and source Windows resource metadata: `0.90.34`.
  Windows/Linux packaged authority stays `0.90.33`, macOS stays `0.89.30`.
  Legacy `engine.version` re-exports the canonical module without stale macros.
- Both exports use `EpochEngine-0.90.34/`. ZIP/TAR entry sets and all contents
  match; committed Git blob identities match after tracked export newline
  conversion (94 CRLF-converted files). No dirty operator bytes, generated cache,
  local addon content or submodule worktree bytes entered the exports.

| Export | Bytes | SHA-256 |
| --- | ---: | --- |
| Source ZIP | 61789756 | `32492064d40e36a883f319a1412968e44961385458728f959a78c9f6986f1f7f` |
| Source TAR.GZ | 61252812 | `71a0b0e77083bf5d5ec8628e04500f02b09756a690015ff401cad8fb2ad61080` |

Local original exports and nonsecret manifest: `build/source-0.90.34/` (ignored).

## Verification and limits

- Final source-only Windows Debug Editor build and pure engine contract pass.
  Final full Linux Clang Release rebuild and 62/62 CPU tests pass.
  Version helper self-test, YAML parsing, package-authority guard syntax and
  whitespace checks pass. Logs: `build/import-20260930/source-09034-*`.
- The AI repair parent also passed Windows Debug/Release and both pure checks;
  its normal and hosted-compatible libstdc++12 full Linux builds each pass
  62/62 CPU checks. These are not model inference or GPU/docking evidence.
- Final checkpoint hosted MSBuild run
  https://github.com/Autodidac/EpochEngine/actions/runs/37163826417 passes.
  CMake run https://github.com/Autodidac/EpochEngine/actions/runs/37163826428
  passes all five lanes (Windows MSVC/cpp26, headless Linux Clang and GCC,
  full Linux Clang). Full Clang passed 62/62 CPU tests. Complete transcript:
  `build/import-20260930/source-09034-hosted-linux.log`.
- CI retains build/tests but stages/uploads Linux runtime packages only when
  canonical source equals tracked Linux packaged authority. Source-only 0.90.34
  therefore does not request a new binary smoke/package/release. Hosted authority
  check passed and both binary staging/upload steps were verified skipped.
- No native Qwen -> build -> separate PID -> embedded Keep/Choose -> successor
  qualification occurred in this pass. Project Assistant regression also needs
  exact request/result evidence; the transport repair is not a claim that every
  native workflow is fixed. Runtime testing remains operator-owned.

## Site source admission complete

Site owner chat `01a03f60-0009-7ab2-b0cf-679ccfd9a78d` independently verified fresh
Git exports against both supplied archives and published Site v109:

- Site commit: `4253fcc5e72e070af9870318bc4859e5ddd0ebbd`.
- Successful deployment: `appgdep_6ac19aa5d9008191adb908e14710e392`.
- https://epoch.adamrushford.chatgpt.site/projects/epoch-engine shows private
  source `0.90.34` separately from runtime `0.90.33`.
- Independent native-side HTTP readback of
  https://epoch.adamrushford.chatgpt.site/api/epoch/source-version returned
  HTTP 200 and exactly `0.90.34` after paired activation.
- Both newly encrypted archives were uploaded, downloaded, hashed and decrypted
  byte-exactly to the committed exports before a single atomic paired activation.
  Old source rows are inactive/recoverable, not deleted. Existing KEK, signing
  keys and device policy are unchanged; no grant/decryption secrets enter docs.

| Private ciphertext | Bytes | SHA-256 |
| --- | ---: | --- |
| Windows `epoch-engine-v0.90.34-windows-x64-23d62a5d08cc` | 61789772 | `3ec5582f6dce70a32d866ad6d0ef08c5a11c67683d395edfc0d7c46cc15cedaf` |
| Linux `epoch-engine-v0.90.34-linux-x64-23d62a5d08cc` | 61252828 | `3661705ae12fc7f70cc75bf74d8fda60d43dbcaeadf86d4a797e9ad0457538fc` |

Site reported 87/87 contracts, production build and production dependency audit
pass; lint has zero errors/10 warnings. Existing devtool audit and prior TypeScript
diagnostics are not claimed repaired. Its nonsecret admission/activation/live
receipts remain under `work/release-verification/epoch-source-v0.90.34` in the
Site workspace. Consumed encryption/deployment staging was removed there;
native original archives, keys and historical objects were untouched.

## Runtime and privacy invariants unchanged

Published v0.90.33 GitHub tag remains
`f33d3d6758abf402747870df4f8887e7887a2035`; exact-repository release readback
confirms 14 assets, non-draft/non-prerelease. No v0.90.34 binary release/tag exists
from this pass. The Site independently read back all nine existing runtime,
receipt, sidecar and manifest objects with unchanged sizes/SHA; latest-release
and signed-integrity hashes match immediately before/after source activation.
Pinned Ed25519 authority remains `epoch-release-e76c3921327a2cd0`, canonical
integrity SHA `5f6773d7431fe6cd9d59564c00ddb80e896347589fe6009342ec3c401ce754da`.

Public Engine Git/plaintext source remain 404, anonymous/forged private access
remains 401, consumed grant returns 404. Gui/Extensions/catalog were not mutated
by this engine-owned AI repair. Historical releases/base ancestry remain intact.
Future checks must state exact repository, source/head, release tag, asset listing
and Site channel separately; an empty listing is never universal absence proof.

Next native gate is operator Qwen/docked successor testing plus an exact Project
Assistant failure transcript. Preserve source/data ancestry and stop probes on a
hang/crash. Do not reactivate a binary release from this source-only receipt.
