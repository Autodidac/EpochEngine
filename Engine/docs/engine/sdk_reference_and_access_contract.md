# Private SDK Reference And Access Contract

Status: planned, not implemented or published. This document defines one finite
SDK delivery gate; it does not assert that Doxygen covers the Engine, that a
private SDK is already hosted, or that existing source access grants SDK access.

`Changes/active_pass.md` owns the current acceptance result and
`Changes/roadmap.md` owns ordering. Complete self-coding and candidate selection,
the current EpochGui/Extensions objectives, loadable generated projects, and the
EpochSimEngine/EpochSpaceEngine demo/library integrations before this final
same-day SDK milestone. The September 6 deadline does not waive build, access,
licensing or publication evidence. This mission is not substitute work for a
blocked self-coding gate.

## Existing Evidence And Required Audit

The tracked `Engine/Doxyfile` currently generates HTML from `Engine/include`
headers matching `*.h` and `*.hpp`. It enables `EXTRACT_ALL`, omits a project
version, and has no module-interface input or complete SDK assembly contract.
That configuration is a starting point, not a demonstrated C++23 module
reference or a usable external SDK.

The source updater already has an identity/device authorization path documented
in `runtime_and_editor_workflows.md` and implemented under `updater.source_access`.
Reuse the applicable identity and device enrollment mechanisms rather than
creating another login or storing a second set of credentials. Existing native
source authorization and website admin sign-in are different consumers; neither
is evidence of an SDK-specific capability today.

Before implementation, inspect the current Site routes, protected storage,
owner/admin checks, device grants and session lifecycle in the Site task. Record
what is implemented, partially connected and missing. Historical deployment
notes are not proof of current authorization. Do not invent route names or
capability claims and then treat those names as implemented contracts.

## Deliverable And Product Boundary

Deliver one owner-only, version-bound Epoch Engine SDK with:

- an API reference for the admitted public C++23 modules and headers;
- readable manuals for installation, project profiles, dependencies, builds,
  debugging, errors, resource ownership and supported context lifecycles;
- external sample projects for a CLI application, a basic native-window
  application and a GUI application, with Build/Run/Stop/Reopen instructions;
- a module/import and header dependency map, supported toolchain/platform
  matrix, required redistributables and exact licensing notices;
- immutable package and per-file manifests, sizes, hashes and validation
  receipts bound to the same accepted Engine source commit and tree.

This is an Engine SDK, not another editor or a dynamic Engine-plugin system.
Reusable software profiles consume the stable platform/context base without
constructing the editor. EpochGui remains included in non-CLI Engine application
profiles; CLI profiles retain the core runtime without accidentally acquiring
GUI, native-window or renderer dependencies.
Extensions supply project add-ons and their declared libraries, not plugins
which enable supposedly absent core Engine features.

Admit public interfaces deliberately. The presence of a file under
`Engine/modules` does not make its module an external supported API. Classify
public interfaces, compatibility facades, internal implementation and
editor-only surfaces before generation. Document compatibility and deprecation
without exporting private implementation merely to make every page resolve.
The naming authority remains `source_naming_architecture.md`.

Refresh and prove the software/context base after the major current repairs,
then freeze its exact accepted checkpoint before broad game-engine-specific
expansion. SDK sample and compatibility tests use that recorded base. A new
game-specific feature must not silently advance the frozen base or expand the
small CLI/native-window profiles.

## Reference Generation And Shared Metadata

Use a pinned, recorded Doxygen toolchain and a reviewed input inventory. Verify
the selected generator against the actual C++23 module syntax, exported
partitions, re-exports, templates and public headers used by Epoch. If an
adapter is required, retain exact declaration/source mapping and test its
output against compiled public examples; do not publish invented signatures or
silently drop unsupported declarations.

Generate the API reference and manuals from the accepted committed tree, not
from live caches or a partially edited sandbox. Classify generator warnings:
missing public declarations, broken cross-references and incorrect signatures
fail the documentation gate. Internal-only exclusions may be intentional and
must be recorded. Suppressing all warnings or enabling `EXTRACT_ALL` is not API
coverage evidence.

Recommended planned shared metadata: one versioned, bounded API/module index
derived from the same reviewed interface inventory. Each admitted record should
identify its symbol/module, owner, visibility, source-relative declaration,
required imports or headers, applicable software profiles, supported platforms,
thread/context ownership, lifecycle constraints, availability/deprecation and
linked examples/manual sections. Bind the index to its schema version, Engine
version, source commit/tree, extraction-tool version and content digest.

Use that index for reference navigation, validated search and optional curated
model research so independent consumers do not invent conflicting API maps.
The index must distinguish implemented/tested support from planned support and
link to the relevant evidence. It is documentation data, not an execution
permit, a list of automatically readable files or a source of model instructions.
The metadata system and its consumers are planned; no generated index is
currently claimed by this contract.

Exclude credentials, environment snapshots, absolute user paths, model exchange
logs, generated projects, candidate sandboxes, dependency caches, build outputs
and local `addons/` from generation and packages. Source listings, XML output,
search indexes and downloadable documentation are private SDK bytes too.

## External Samples And Versioned Packaging

Build the three required sample profiles outside the Engine checkout using only
the staged SDK and declared toolchain/dependency inputs. Prove that they do not
depend on the editor, a developer's current directory, an undeclared checkout
include path or an already-populated build cache. Keep sample writable project
state separate from installed SDK inputs and private authentication state.

For each claimed platform, record configure/build/contract results and the
approved runtime evidence appropriate to that profile. CLI proof must not start
a renderer; native-window and GUI proof must cover initialization, input,
resize, shutdown and owned process/context retirement. Do not infer one
platform's success from another platform's receipt. C++ module build artifacts
are toolchain-specific: document the exact compiler compatibility or supply a
tested rebuild path rather than claiming universal binary-module portability.

The SDK manifest must bind the Engine version, source commit/tree, frozen base
identity, platform/architecture/configuration, compiler and module format,
dependency lock/baseline, license inventory, documentation index and sample
versions. Include exact archive names, sizes and SHA-256 sidecars plus per-file
hashes. Preserve the admitted license terms; do not relabel LicenseRef-MIT-NoSell
as plain MIT. Optional add-on library licensing must be reviewed separately.

The target feature release is v0.90.1, not proof of a published SDK version.
Reconcile current source/platform version authorities before packaging. Never
rename old archives, rewrite historical manifests or imply a newly supported
macOS package without its own accepted evidence. SDK publication is a separate
reviewed release artifact set and must leave existing runtime/private-source
objects and rollback history unchanged unless their own release handoff says
otherwise.

## One Server-Confirmed SDK Authorization

The website and Engine must consume the same project-scoped owner/admin SDK
policy. Authentication establishes identity; the server must separately confirm
that the current subject is allowed to read the Epoch Engine SDK/reference.
An ordinary source entitlement, a paired device, a local admin flag or a hidden
navigation link must not silently become this SDK capability.

Reuse the existing source-update identity/device flow on the native side, with
an explicit SDK audience/capability added only after its server contract is
implemented and tested. Reuse the existing owner/admin website session on the
web side. Bind authorization to the subject, project, intended operation,
artifact/version scope, expiry and applicable session/device identity. Check
revocation server-side. Do not introduce a new listener or an unauthenticated
local bridge to display documentation.

All document, image, stylesheet, script, font, search, index, source-listing,
manifest, archive, sidecar and range-download routes must enforce authorization
before returning private bytes. Protect backing object access as well as the
outer page. A public object URL, pre-rendered bundle or public fallback route
must not bypass the policy. Define and test the handling of redirects,
conditional requests and partially completed downloads.

Private SDK responses must not enter shared CDN/browser/service-worker caches
that can answer another user or an expired session. Use an explicit private,
no-store delivery policy and verify actual response/cache behavior. Do not put
credentials in URLs, referrers, logs, analytics, error text or model prompts.
Retain existing protected credential storage; never ship an owner token/key in
the runtime, website bundle or SDK.

The initial reference is online-only. Do not silently create a persistent
offline private-document mirror. An owner-requested SDK download is an explicit
file export and needs separate clear UI. Revocation can stop future reads; it
cannot retract SDK bytes the owner has already deliberately downloaded. Do not
claim otherwise.

## Readable Engine And Website Entry Points

Expose a separate **SDK Reference** destination in Engine About and on the
website only when the current server-confirmed owner/admin SDK capability is
available. Do not bury it inside source-update diagnostics or require another
login form. Native enrollment/reauthentication, when required, remains the
existing identity workflow; never transfer a native bearer in a browser URL.

The reference view must show its Engine/SDK version and offer readable API,
manual, examples and search navigation, usable text selection/copy, wrapping,
keyboard focus, scrolling and narrow/high-DPI layout. A browser destination or
an Engine-owned reference surface must preserve the same content authorization.
Rendering or hiding a menu entry is not the content security boundary.

Represent loading, offline, expired session, revoked access, unavailable version
and request failure plainly. Stop pending loads and clear displayed private
content when authorization expires, is revoked or the user signs out. A cached
successful capability must not keep the view unlocked indefinitely. Recheck at
the point of every protected request without disturbing the active project or
starting a source update as a side effect of reading documentation.

## Documentation Read Access Is Not Model Egress Permission

Owner/admin permission to view SDK documentation does not authorize sending it
to an AI endpoint. Device/source authentication does not authorize model use,
and model selection does not authorize private-document retrieval. Keep these
decisions independent in the host and in the visible UI.

Optional curated SDK research may offer exact versioned metadata or reviewed
documentation excerpts to the selected model only under the applicable explicit
model-egress policy and byte budget. Show the destination, selected evidence and
scope; retain content identities without logging credentials or hidden model
reasoning. Never forward a website cookie, source/device token, whole private
SDK archive or unrelated source as research context. Treat retrieved text as
untrusted evidence, not instructions to expand access or execute commands.

Private research remains subject to the SDK capability as well as model-egress
authorization. Losing either permission cancels pending retrieval/dispatch and
prevents late results from reviving the operation. Public web research, if
implemented, remains separately attributable and must not leak private SDK
queries or excerpts by default. No new automatic retrieval, transport or
background model action is authorized by this planning document.

## Finite Delivery And Acceptance Gate

Complete this batch in this order, recording actual evidence at each step:

1. Audit existing Doxygen, admitted public interfaces, software profiles and
   Site/native identity capabilities. Record the missing wiring and exact
   owner-only policy without changing live release/access state.
2. Implement the shared versioned interface inventory, qualified Doxygen
   generation and manual/sample links. Test module/partition coverage, missing
   declarations, broken links and absence of private operational artifacts.
3. Assemble the staged SDK and build the three external sample profiles with
   recorded dependencies, licensing and per-platform validation receipts.
4. Implement the protected Site SDK capability and content delivery; connect
   the existing native/browser identities without broadening source grants.
5. Add and eye-test the separate readable SDK Reference entry points and
   lifecycle states in both consumers. Test any model research path separately.
6. Publish one accepted immutable SDK/documentation set only after the complete
   security and usability matrix below passes. Preserve rollback material and
   update current metadata atomically through the authorized Site handoff.

Required access tests, including the actual body bytes and cache behavior:

- Owner/admin with a current SDK capability can read the intended version and
  only the operations/artifacts that capability admits.
- Anonymous users, signed-in non-owners and source-authorized users without SDK
  capability receive no SDK page, asset, index, manifest or archive bytes.
- Direct/guessed URLs, backing storage URLs, copied links, redirects, HEAD,
  range and conditional requests cannot bypass authorization or reveal private
  content through error/cache responses.
- Expired, revoked, replayed and wrong-project/version/session/device grants
  are refused; revocation during navigation/download is handled without a stale
  unlocked Engine or website view.
- A second anonymous/non-owner browser after an owner's request cannot receive
  private bytes through CDN, shared browser cache, service worker, search index,
  prefetch or static export. Logout clears transient protected view state.
- Browser/native SDK access does not enable model egress, and source/model
  permissions do not imply SDK access. Cancellation and permission loss prevent
  queued or late private-document dispatch.
- Package extraction, external sample builds, documentation search and denied
  access do not overwrite live source, projects, unrelated caches or credentials.

Retain sanitized test identities, route classes, status/body evidence, exact
artifact hashes, expiry/revocation observations and native/browser eye results.
Do not log authentication secrets. Any failed route, sample, version, licensing
or privacy check keeps publication held. Implementation without these results
remains incomplete; generated HTML alone does not satisfy this contract.
