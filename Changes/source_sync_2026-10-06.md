# October 6 EngCoder endpoint source checkpoint

The operator's source-only request is completed on GitHub and the Site. This
receipt does not replace source exports or create a runtime release.

- Engine commit: `94f8e47ae2e052ed5b8907b3b469ef7b5e67c5f2`.
- Tree: `12252fc933bd36e8d6683e4edef7ce0f94e346c8`, 1,067 committed files.
- Source version remains 0.90.34; runtime Windows/Linux 0.90.33 and macOS
  0.89.30 are unchanged. No tag, dependency release or stable-branch rewrite.
- Saved loopback endpoints, EngCoder 0.6.5 complete-response compatibility,
  endpoint-bound session credentials and collapsible route references use the
  existing Model Settings/GUI. EngCoder remains external; Responses and full
  agent tasks are not implemented execution transports or source bypasses.
- Original EngCoder source/archive and all Epoch styling/window ownership are
  preserved. No external UI/window stack, server or training job was imported
  or started. Full UI/agent integration and native self-coding remain unfinished.

## Verification

Production Windows Debug Editor build and the build-safe pure engine contracts
passed with existing host vcpkg installed dependencies. Tests cover endpoint
normalization, persistence and malformed profiles, credential isolation and
streamed/complete request construction. Unrelated operator deletions and local
caches were not staged. Exact Git ZIP/TAR exports agree member-for-member and
byte-for-byte; Git enumeration uses NUL delimiters for historical Unicode paths.
Logs/exports remain local under `build/endpoint-20261006/`, not public payload.

Hosted MSBuild run 37536547555 passed. In CMake run 37536547523, Windows MSVC,
Windows cpp26 and headless Linux Clang/GCC passed at receipt time; the full
Linux Clang engine lane remained in progress. Do not infer its result from
earlier checkpoints. Native model inference, sandbox build/docked PID comparison,
Keep/Choose and successor continuity were not tested in this pass.

## Private Site admission complete

Site owner independently regenerated the Git exports and verified both freshly
encrypted archives by live GET/hash/byte comparison and exact offline decryption
before atomic paired activation. No plaintext public Site source was enabled.

- Site v111 commit: `4f10aea41c4b8a3bcafddf00896fb63f6660c789`.
- Deployment: `appgdep_6ac570ddab708191bf4eff090e0149f5` succeeded.
- Project: https://epoch.adamrushford.chatgpt.site/projects/epoch-engine
- Windows ID: `epoch-engine-v0.90.34-windows-x64-94f8e47ae2e0`.
  Plaintext 61,799,509 bytes, SHA-256
  `2726396e89b0f268b3dbe68748009c601b588e2ac8554a0e5fc666439060eec4`;
  ciphertext 61,799,525 bytes, SHA-256
  `4844857bb9b38b99e58e2ee4af7eaa4b078bb251dfa8b3efc7250cf8c72ea4e2`.
- Linux ID: `epoch-engine-v0.90.34-linux-x64-94f8e47ae2e0`.
  Plaintext 61,262,360 bytes, SHA-256
  `9d4acbafbd75ac8f466b6890428c8d949d02011b64e9bbff5a68d7113ea56cf3`;
  ciphertext 61,262,376 bytes, SHA-256
  `6593d78d0de3485689694e9dcfe5de7c18c22d5295f3f93607c3c10890f2fecc`.

Previous private rows are inactive/recoverable, not deleted. Existing KEK,
signing key and device policy were unchanged. Anonymous/forged private/admin
requests remain denied; public plaintext source remains unavailable. All nine
existing runtime objects were independently re-downloaded and matched their
original bytes/hashes; runtime metadata and signature were verified unchanged.

Site build and 88/88 contracts pass. Lint has zero errors and ten existing
warnings. Its production audit reports two high-severity advisories
(sharp/librsvg and source-map-js); this bounded source-only mutation did not
repair them and does not claim the Site security-clean. The owner retained
nonsecret admission/activation/runtime-boundary receipts in its own workspace.

A terminal CR/LF confirmation issue prevented the first upload from starting;
the owner corrected only that helper and regenerated fresh in-memory keys for
the successful pair. No first-attempt upload or activation occurred. Task-owned
Site staging was removed; original exports and hosted history were preserved.
