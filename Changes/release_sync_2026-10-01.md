# October 1 source, dependency and release continuation

## October 2 completed GitHub publication

This outcome supersedes the earlier preparation/remaining-gate notes below.
GitHub v0.90.33 is public/latest at
https://github.com/Autodidac/EpochEngine/releases/tag/v0.90.33 , published
04:42:27 UTC. Its immutable tag and all source/package receipts bind to
`f33d3d6758abf402747870df4f8887e7887a2035`, tree
`86a7f4b72d60026d58f08f4dbc4ff29c0acde5c6`, 1066 tracked files. SHA-256 of the
unprefixed exact-commit git archive TAR is
`acdb8f240b60c69cad4bfde6b16201acdf5c1e7f1bad09c63b31430ab05cbab9`.
No working-tree bytes are included. Main/stable were synchronized at f33 before
release; a following docs-only checkpoint does not replace release source.

- Hosted CMake: https://github.com/Autodidac/EpochEngine/actions/runs/36963049417
  (all five jobs pass; full Clang job 110700717125 compiles 1963 steps, 62/62
  tests, bounded software OpenGL smoke and package checks pass).
- Hosted MSBuild: https://github.com/Autodidac/EpochEngine/actions/runs/36963049433
  (pass). Local Windows Debug/Release/pure checks, 15 headless tests and 311
  packaged DLL imports pass; known logger/optimization warnings remain visible.
- Linux release uses the downloaded hosted Ubuntu 22.04 artifact, not the local
  GLIBCXX_3.4.32 dependency payload. Independent archive/ABI/dependency/version/
  pure-contract audit confirms GLIBC <=2.35 and GLIBCXX <=3.4.30.
- All 14 uploaded asset digests and sizes were compared with admitted local
  files; release is not draft and is GitHub latest. Inputs/provenance/sidecars
  are in `C:/tmp/epoch_release_v0.90.33_20261001`; complete proof logs are in
  `build/import-20260930` (ignored, not release payload).

Final artifact SHA-256:

| Artifact | SHA-256 |
| --- | --- |
| Windows runtime ZIP | `14b068ebc9399af8e5a8322214a1c5e18574f44c275fb65cdc5365b9fa8b5a1c` |
| Linux runtime TAR.GZ | `407c43dc6765fc035b6a1877fbe0ec54c8662b3ea5f8621faa94f2d999e75552` |
| Source ZIP | `c1fa005462e2dd20eb5cb72ee37d142fd53a62f22f44162f5f0d693a18049ae4` |
| Source TAR.GZ | `3e4fcbb1466e74d81ecd70e5d5159ef27e910ef7913a536e9eabc37eb5c4db30` |
| Windows receipt JSON | `15b29e7a67062410815870795b3df701aea282611720d25d07314d9817a22dbd` |
| Linux receipt JSON | `6363fc4ed7e0082d250f80244f8b54e92658aea83fb16c67aa66dd48ae4ccbd2` |

EpochGui main remains synchronized at c52e91293749a7a34959bfc042c582c3ac182504
with 11/11 Windows Debug/Release and Linux Debug CPU checks. Extensions is
unchanged at eccef139f982a9e6b64295cd9c8b976905ef0990; richer package source was
not overwritten by descriptor catalogs. Historical release refs/bytes and
macOS 0.89.30 remain unchanged.

Site chat confirmed successful v108 publication: source
`b7087c209cf8a16500de1a3b234e1716ae28bdc7`, deployment
`appgdep_6abf3cb942b08191a0972914974e8d52`. Nine public objects passed
independent HTTP 200/no-redirect readback, exact hash/size, MIME, attachment,
immutable caching and nosniff checks before activation. Runtime/receipt sidecars
match their exact names and bodies. The Site's runtime-only checksum manifest is
418 bytes, SHA-256 `44826564b00be032fbded699d3025597f65488316211a6c4a0386d2246829672`;
the native combined source/runtime manifest was not substituted or rewritten.
Both private source archives were re-downloaded as ciphertext and decrypted
offline to these exact source hashes; 1066 files match the committed Git export.
Atomic paired Windows/Linux activation and source-version 0.90.33 were verified.
Pinned Ed25519 integrity signature verifies; anonymous/forged private/admin
access remains denied and consumed one-shot upload grants are closed. Old release
objects and inactive private rows are retained, macOS/key/device policy unchanged.
Site checks pass 86/86 and production build; pre-existing dev-tool audit and four
TypeScript issues remain separate, not falsely claimed repaired. Site made no
Engine GitHub writes.

October 3 AI source follow-up is a later checkpoint, not replacement release
bytes: stage-specific prompts and bounded streamed-response assembly now have
production build/pure-canary proof. Native GPU/Qwen/candidate PID/docked choice/
successor acceptance remains operator-owned and unverified. Project Assistant
regression still needs its exact request/result. The release cannot claim either
native AI behavior from CPU checks or the hosted software-renderer smoke.

## Bounded scope

Continue the operator's 0.90.33 import, prepare Windows/Linux release packages,
push the focused repaired source to GitHub, synchronize the explicitly requested
stable branch by fast-forward, and give the Site chat exact publication evidence.
Do not recreate old product plans, rewrite history, overwrite optional package
source with descriptor catalogs, or start operator-owned native runtime tests.

## Implemented repairs

- Validate recovered structured read paths before admission; traversal remains
  rejected even if other supplied paths are valid. Explicit read records may
  recover their own path but cannot invent checkout authority.
- Count actual line-start navigation entries, not the repository map's PATH
  heading or symbol metadata.
- Restarting the same stopped objective retains its plan and accepted selection
  checkpoints; starting a different objective clears the old mission plan.
- Restore first-party ownership-dot naming guidance in source proposals.
- Match pure fixtures to the imported cumulative repository-search behavior,
  active model budgets, bounded reasoning retries and host-first triage. Keep
  unsafe selector, source canary, stale result and terminal failure checks.
- Avoid the MSVC neuromorphic imported-inline-body compiler crash by compiling
  the four unchanged contract functions out of line in their owning modules.
- Give bottom-dock tests the GUI implementation that owns exported vtables,
  without colliding with the full engine's GUI target. Give standalone proposal
  codec tests their explicit logger/time module and implementation dependencies.
- Pin hosted vcpkg preparation to the tracked manifest baseline rather than an
  older registry tag. The local Linux lane uses a separate native vcpkg checkout;
  the operator's Windows vcpkg installation was not rewritten.
- Resolve Windows package notices from the same normal-or-nested dependency
  layout selected by MSBuild; reject missing notices before staging. Generated
  staging targets are checked against their dedicated output root.
- Build Debug+Release sequentially with inherited caller dependency properties
  and a fresh solution reference map for each configuration. The outer combined
  map cannot accidentally supply Debug module interfaces to Release compilation.

## Proven / in progress

- Source names: 610 files pass.
- MSBuild Debug Editor: passed; pure engine contracts exit 0, October 1 19:24.
  Evidence: `build/import-20260930/contract-repair-debug.log` and
  `x64/Debug/logs/Engine.Editor.SelfTest.log`.
- MSBuild Release Editor: passed; explicitly waited pure contract process exits
  0 at 19:27. Evidence: `build/import-20260930/contract-repair-release.log`,
  `release-contract-stdout.log`, `release-contract-stderr.log` in the same folder.
- Windows CMake headless: 15/15 pass in `build/release-headless-20261001`, including
  neuromorphic CPU contracts after removing the ineffective caller workarounds.
- Combined Debug+Release solution: passed after excluding the outer reference
  map. Evidence: `build/import-20260930/dual-build-fixed.log`. The earlier failure
  transcript records Release compilation importing Debug IFCs; no STL source or
  iterator ABI workaround was added.
- Windows initial package inventory/version/notices check: passed. Final bytes
  must be restaged after platform version authority is aligned with this release.
- EpochGui 0.89.32: 11/11 Windows Debug, 11/11 Windows Release, 11/11 Linux Clang
  Debug CPU tests. Standalone GitHub main:
  `c52e91293749a7a34959bfc042c582c3ac182504` (additive sync plus portable test fix).
- EpochEngineExtensions: unchanged at
  `eccef139f982a9e6b64295cd9c8b976905ef0990`. No other first-party dependency import
  changes were found; third-party vendored dependencies are not upstream patches.
- Linux full Release: passed; all 62 CPU/contract tests pass. Toolchain is Clang 22.1.8/CMake 4.4 on Ubuntu
  22.04.5, vcpkg registry baseline `5f96cd15fd745122cf27e0524606d6c1efc5fd07`.
  Initial dependency installation completed all 69 packages. The first compile
  reached the standalone missing logger dependency; its CMake ownership and a
  subsequent GUI alias configuration error are repaired. Full transcript:
  `build/import-20260930/linux-release-build-fixed.log`; tests:
  `build/import-20260930/linux-release-tests.log`.
- Windows/Linux packaged version pins are now 0.90.33 for this authorized new
  release pass; macOS remains 0.89.30. Both production rebuilds pass after changing
  those pins: `final-dual-build.log` and `linux-release-final-build.log` in the
  evidence folder. Final Windows Debug/Release pure checks exit 0; final Linux
  CTest is 62/62. A version constant alone is not publication evidence.
- Final Windows import-table audit resolves all 311 DLL imports against the
  staged payload or Windows system libraries. Linux staging proves `$ORIGIN/lib`,
  packaged SFML/Vulkan resolution without build-cache paths and GLIBC <= 2.35.
  Both staged archives include assets, notices and license/component hashes and
  exclude runtime logs/cache. Proof: `windows-dependency-resolution.log`,
  `windows-final-package-proof.log`, `linux-final-package-proof.log`.
- Linux's xcb-util-m4 vcpkg recipe intentionally supplies an empty copyright file.
  The collector preserves its exact bytes/hash and explicitly infers no license
  terms instead of crashing on null PowerShell text output. No notice was dropped.

## Remaining exact gates

October 2: 18d998e5 is on both remote branches. Hosted full Clang compilation
passes all 1963 steps; 61/62 tests pass. The remaining script compiler contract
selects an old Clang 14 alias, rejecting -std=c++23 although 22 is installed.
This is reproduced locally. Production discovery now prefers modern versioned
drivers through the existing path resolver without overriding explicit request
selection; the old-first-PATH regression passes with 22. Both full Linux outputs
again pass 62/62 and Windows dual builds pass. Final package/pure checks and a
focused follow-up commit/hosted confirmation are next. No release exists yet.

Latest stop: follow-up `51c515ea` passes MSBuild and four CMake lanes, but the
full Linux compile failed at missing direct standard headers with hosted
libstdc++12. The clean alternate full-engine build uses extracted 12 headers
without changing the local system; missing vector/optional/span/algorithm/array
ownership is repaired in its production units. The complete clean build and
62/62 CPU checks pass in `linux-stdlib12-final-build.log`. Final Windows dual
build and pure checks pass, plus 15/15 short-TEMP headless checks. Normal Linux
production full Release build and its 62/62 CPU checks also pass. Windows
restaging and all 311 import checks pass.
Do not publish the prepared 51c515ea packages. Rebuild both normal production
outputs and regenerate exact-commit source, receipts and provenance after final
proof. Site remains on HOLD; there is still no 0.90.33 GitHub release.

The local Linux libraries require GLIBCXX_3.4.32 and are not admitted for the
Ubuntu 22.04 baseline. Packaging now gates both GLIBC <= 2.35 and GLIBCXX <=
3.4.30. Use the new exact-commit hosted Ubuntu 22.04 artifact after all hosted
checks pass; audit that archive and bind its receipt to actual hosted build/test
and package evidence. Windows uses its final locally built/staged artifact.

Hosted follow-up: candidate `278687d1219159662192722b3d8869934a7e7ede`
passed MSBuild run `36944581337`; CMake run `36944581988` passed Linux GCC/Clang
headless checks but failed the two Windows fixture lanes and full Linux
dependency installation. Windows fixtures now canonicalize only their trusted
host TEMP parent, preserving candidate alias/reparse/traversal rejection.
Local Windows checks pass 15/15 with DOS short-name TEMP outside the checkout
(`windows-headless-short-temp-tests.log`); Linux passes 62/62 after rebuilding
the affected fixtures (`linux-hosted-fixture-repair.log`). The hosted Linux
earliest error is pthread-stubs requiring `autoconf-archive`; the workflow now
installs it. Do not bypass these failures or admit the previous candidate.
Bind the final receipts/exports to the focused follow-up commit and wait for
the new hosted result. Native runtime acceptance remains operator-owned.

1. Local source repair, production rebuilds, pure/CPU contracts and clean package
   staging are complete. Keep the exact evidence; stop on any later regression.
2. Bind release receipts to the final committed tree and actual artifact bytes.
   Do not confuse source version with platform-packaged authority. Renderer
   smoke is not run locally; record skipped native pixels explicitly.
   Receipt `source_tree_sha256` is SHA-256 of the unprefixed TAR emitted by
   `git archive --format=tar <exact source commit>` (tracked export attributes,
   no working-tree bytes). Source ZIP/TAR exports carry `EpochEngine-0.90.33/`;
   their own archive hashes are separate from that tree-export digest.
3. Push focused Engine main, fast-forward the requested stable branch without
   force, confirm exact remote SHAs, then publish only admitted release assets.
   Hosted CI must confirm the repaired candidate before publication. Do not
   rewrite historical tags or substitute new bytes into historical releases.
   Send exact files, sizes, hashes, source revisions and remaining acceptance
   gaps to Site chat `01a03f60-0009-7ab2-b0cf-679ccfd9a78d`.
4. Operator acceptance remains: real Qwen request, sandbox compile, separate
   candidate PID embedded beside the parent, Keep/Choose, and a successor build
   continuing from the chosen sandbox without changing the original source.
   No such native test or live-source promotion occurred in this pass.

The current prepared binaries are not a claim that all AI/editor/renderer
features are runtime-verified. Existing duplicate logger and selected MSVC
optimization-override warnings remain visible. Operator workspace deletions and
`epochengine69.zip` remain untouched and excluded from focused source staging.
Candidate storage retention/cleanup, native docking and live-source promotion
are not newly proven by this CPU/release pass; preserve their unfinished goals.
