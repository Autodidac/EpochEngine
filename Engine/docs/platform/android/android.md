# Android bring-up

Android 12 (API 31, x86_64) is the retained emulator. Calculator portrait
touch passed on Android 12 and on the former Android 16 emulator, which also
verified the software keyboard. The ARM64 phone test remains open.

## Verified toolchain and emulator

The Windows checkout-local tool set uses OpenJDK 17.0.21.1, Android API 36,
NDK r30 `30.0.16248370`, Build Tools `36.0.0` and ADB. The NDK compiled and
linked an API 26 C++23 consumer of production `render.math` for `x86_64` and
`arm64-v8a`. The x86_64 executable also ran successfully on the API 36
emulator. The arm64 executable has build proof only.

The signed `org.epochengine.tests.androidcapability` probe APK installed on
the checkout's `Epoch_Android_36` emulator. It presented at 1080×2400 through
GLES 3.0 on SwiftShader, presented again after returning from Home, and
presented at 2400×1080 after rotating.
[APK and emulator receipts](../../../../Artifacts/Reports/android-local-sdk-bootstrap-1118/capability/).

## First Android milestone

The first mobile milestone should stay deliberately narrow:

- one **single-context** runtime path
- one stable mobile renderer/backend path
- one input/touch ownership path
- one packaging/install story
- one asset/shader/script resolution path rooted from the installed runtime

The saved Calculator must stay editable in Epoch and produce a standalone
APK from the same admitted project sources and authored GUI. The first
package exercises the manual keypad and project assets. AI and terrain remain
optional. Desktop multicontext stays outside this milestone.

An exported APK contains the Epoch runtime for its target ABI as a native
library, SDL and its admitted project assets. Android starts each package in
its own app sandbox with its own private files. The on-device Editor runs
supported scenes in its own process. To move a project between installations,
export and import it through the Android Files picker. Standalone compilation
and signing use the desktop toolchain.
[Application sandbox](https://source.android.com/docs/security/app-sandbox),
[NDK packaging](https://developer.android.com/ndk/guides),
[Android Files picker](https://developer.android.com/training/data-storage/shared/documents-files).

The debug-signed Calculator APK `28815F9CB1BC` is 62.6 MiB. It cold-started via
SDL OpenGL ES and presented the real authored GUI. Touch entered `9 ÷ 4 = 2.25`
and ran `2^8 = 256` in the Scientific panel. The basic result survived Home,
resume and portrait-to-landscape rotation. Installed GUI and font hashes matched
the package. The base mobile configuration builds with native AI disabled.
[Calculator run receipt](../../../../Artifacts/Reports/android-local-sdk-bootstrap-1118/calculator-r6-emulator.log),
[verified results](../../../../Artifacts/Reports/android-local-sdk-bootstrap-1118/calculator-r6-results.png).

APK `05F51B8DCF7D` verified the authored portrait stack inside Android's system
bars. Keypad and native-keyboard taps both calculated `2.25` from `9/4`. Back
dismissed the keyboard, retapping reopened it, and Home/resume and rotation
preserved both results in the same process. The emulator's attached keyboard
required enabling its guest on-screen keyboard setting for the test. The test
restored that setting. Android relaunched the Activity once during package asset
replacement. Opening the same installed APK after replacement settled passed.
[Acceptance receipt](../../../../Artifacts/Reports/android-local-sdk-bootstrap-1118/calculator-safe-area-acceptance.json).

The same signed APK on the visible Android 12 emulator verified the installed
APK and authored GUI hashes. Keypad taps displayed `2.25` for `9 ÷ 4`.
Home and resume preserved the display pixels and process ID.
[Android 12 receipt](../../../../Artifacts/Reports/android-calculator-api31-1119/compatibility/compatibility.json).

The same APK recovered after Android Settings covered the Calculator. SDL
recorded `surfaceDestroyed()` and then `surfaceCreated()` under one PID. The
display returned with identical pixels for `31`, and new touches produced `56`.
[Surface receipt](../../../../Artifacts/Reports/calculator-native-recovery-1119/surface-receipt.json),
[display comparison](../../../../Artifacts/Reports/calculator-native-recovery-1119/display-sha256.json).

When SDL reports a lost EGL context with a device reset, Epoch rebuilds the
owned GLES renderer on the same window and rebinds its atlas, GUI and scene
resources. The new renderer needs a fresh successful present. A target-only
reset retains the renderer. An unrecoverable device loss ends the run with a
renderer failure. The reset dispatch passed the mocked SDL CPU fixture and
the x86_64 and ARM64 Release adapters compiled with the NDK. The tracked
Verdant build produced a debug-signed x86_64 APK `7248178D5EE8` with the
reset code and the same 18 declared assets as the prior cloud-and-water run.
ARM64 APK `C43C9EA7C715` includes the same assets, fixed shoreline pin and
renderer recovery code. Both are debug-signed and require API 26. Native
EGL context-loss recovery and physical ARM64 performance remain open.
[Android reset checks](../../../../Artifacts/Reports/android-renderer-reset/README.md),
[x86-64 package](../../../../Artifacts/Reports/android-verdant-reset-1119/README.md),
[ARM64 package](../../../../Artifacts/Reports/verdant-reset-arm64-1119/README.md).

The arithmetic and worked-step CPU contract also passed on the emulator. The
packager's assembly fixture verified signing, native exports, asset integrity
and preservation of saved project files.
[Arithmetic receipt](../../../../Artifacts/Reports/android-calculator-scientific-1118/emulator-run.log),
[package checks](../../../../Artifacts/Reports/android-apk-host-build-1118/README.md).

Keep the existing owners: `EpochPlatformEngine` owns application entry, native
lifecycle and input; the selected renderer owns graphics; EpochGui owns controls;
the project's build/export path assembles the application and assets. Use the
SDL Activity and existing runtime/project admission and asset resolution.
Android CMake excludes desktop X11 and desktop OpenGL linkage. Packaged
project data belongs in `files/epoch/project` with logs in `files/epoch/logs`.
Compiled GUI publication preserves existing artifacts using atomic no-replace
semantics. The project CMake target produces `libmain.so`. The debug packager
assembles it with matching SDL and the declared assets.
[APK build tool](../../../../Tools/build/package_epoch_android_apk.ps1).

## App package and storage boundary

Each exported APK contains its project's native Epoch runtime in the ABI's
`libmain.so`, SDL, and the admitted runtime assets. The installed game or
tool loads that runtime in its own process under its Android application ID
and UID. It runs independently of the Editor and has its own saves. The
separate Editor APK uses the same engine code and plays admitted scenes in
its own process.
Use a distinct application ID for each shipped app and keep it stable across
updates. Android applies the same application sandbox to native code.
[Android application sandbox](https://source.android.com/docs/security/app-sandbox),
[NDK ABI packaging](https://developer.android.com/ndk/guides/abis).

On the Android 12 x86_64 emulator, the signed Calculator and Verdant APKs
ran under distinct app UIDs. Each could read its own saved project. Android
denied reads of the other app's project in both directions. Stopping the
Calculator left Verdant running in the same process.
[App boundary check](../../../../Artifacts/Reports/android-app-boundary-1119/device-37042ea6/result.json).

Workspace edits stay in the Editor's private files. To move a project between
installations, use the system picker to import or export an admitted ZIP.
Android grants access to the selected document. The normal APK requests no
storage permission. The optional Calculator Coach uses a non-exported,
isolated service and receives only the approved model file for inference.
[Android Storage Access Framework](https://developer.android.com/training/data-storage/shared/documents-files).

The Android Editor authors and previews admitted content locally. Use the
desktop build workflow to compile and sign a new standalone APK. The desktop
export and the Android Editor's in-process Play are separate validation paths.
Use the audited Public SDK and a verified linked-runtime inventory for any
public APK. The retained Development debug builds are local test packages.

The saved Calculator also builds for `arm64-v8a`. APK `7C70C43EE21D` is a
66,397,429-byte debug-signed package with 16 KiB ELF and ZIP alignment. Its
minimum is API 26 (Android 8.0), so Android 12 meets the OS requirement.
Its phone run remains open. The host demo runs Android 16 (API 36) on x86_64.
[arm64 package receipt](../../../../Artifacts/Reports/android-calculator-arm64-1119/native-admission.json).

Use a standalone Planetary Terrain project as the graphics benchmark. The
generated Android entry admits a bound saved Game scene in the same SDL host
as GUI tools. It requires authored scene draw work and a successful first
present. The host contract covers rejection of wrong project bindings and
clear-only frames. APK `8962C40E57E6` cold-launched on API 36, framed the
validated baked globe from the untouched starter orbit and presented the
authored surface. The Activity handles `assetsPaths` through the existing SDL
configuration callback. The same process remained foregrounded after that
change during startup. APK `C54B573CA5D3` repeated the native check with
the runtime overlay inside Android's safe area. APK `0025258A0F33` verified
that the first touch retains its target through release. Globe drags orbit the
camera, controls-region drags stay with the GUI, and panel taps still work
after crossing its border. The later [managed flight test](#package-a-playable-flight-start)
exercised cloud crossing on Android 12. Renderer device loss still needs a
native test. Measure water, clouds and model inference in separate runs.
Exercise orbit, approach and ground
views with repeatable camera passes. Record the phone and backend, build
identity, peak memory, sustained frame time and stalls during streaming,
plus battery or thermal data if the device exposes it. The benchmark should
use normal project package activation and recover after suspend and resume.
[Ordinary scene receipt](../../../../Artifacts/Reports/android-dry-terrain-1119/orbit-safe-area/fresh-device-relaunch/present-receipt.json).
[Controls touch receipt](../../../../Artifacts/Reports/android-dry-terrain-1119/orbit-safe-area/controls-touch/touch-receipt.json).
[Touch orbit receipt](../../../../Artifacts/Reports/android-dry-terrain-1119/touch-orbit/fresh-device-relaunch/gestures.json).

The saved dry terrain project builds for `arm64-v8a` with minimum API 26.
Development debug APK `0B5A2006B803` includes the shared shoreline and startup
handoff changes and is 66,332,180 bytes.
Offline readback verified the native runtime, SDL and shared C++ libraries,
APK signing, 16 KiB alignment and zero requested permissions. Phone execution
and mobile water and clouds remain open.
[Current APK](../../../../Artifacts/Reports/android-dry-arm64-planet-start-1119/package-readback.json),
[Shoreline inventory build](../../../../Artifacts/Reports/android-dry-arm64-shoreline-guard-1119/package-readback.json),
[Shoreline merge build](../../../../Artifacts/Reports/android-dry-arm64-shoreline-edge-1119/package-readback.json),
[initial ARM64 baseline](../../../../Artifacts/Reports/android-dry-arm64-1119/package-readback.json),
[OpenGL lighting build](../../../../Artifacts/Reports/android-dry-arm64-rolling-1119/package-readback.json),
[retained shoreline build](../../../../Artifacts/Reports/android-dry-arm64-retained-water-final-1119/package-readback.json).

ARM64 debug APK `3A129FA4F180` packages the saved wet scene's baked mesh
through SDL OpenGL ES and includes the per-vertex shoreline color fix.
All ten project assets matched the reviewed source bytes. The 67,037,283-byte
APK passed signing, 16 KiB alignment and zero-permission checks. The matching
x86-64 Development debug APK `F0827E8DD4BC` passed the same checks at
66,267,226 bytes. Both APKs have minimum API 26. On the Android 12 emulator,
the x86-64 package rendered the baked coastline, reported live water separately,
matched the saved manifest, and returned from Home in the same process. Use
the ARM64 package for the physical Android 12 check and retain the dry APK for
comparison. Animated water and streamed terrain require native provider and
renderer work.
[ARM64 readback](../../../../Artifacts/Reports/awah-8145543b/package-readback.json),
[x86-64 readback](../../../../Artifacts/Reports/awh-605dd019/package-readback.json),
[Android 12 run](../../../../Artifacts/Reports/awh-605dd019/device-87414a03/result.json),
[First wet package](../../../../Artifacts/Reports/aw-1119/package-readback.json),
[Indexed water colors](../../../../Artifacts/Reports/aw-1119/mesh-evidence.json).

The Planetary and FFT archives linked into the existing native application
for both Android ABIs. The link test keeps the generated baked entry.
[x86_64 link](../../../../Artifacts/Reports/android-live-water-link-1119/x86_64/application-link-result.json),
[ARM64 link](../../../../Artifacts/Reports/android-live-water-link-1119/arm64-v8a/application-link-result.json).

Regenerate attached Planetary projects with the current dependency writer
before Android packaging. It links approved source into the app on the
desktop build host. Android excludes the desktop exporter and uses the
saved coastline when live data is unavailable. Signed APK `AE80B63ED0E0`
verified that fallback and Home/resume on Android 12 x86_64.
[Source-attached APK proof](../../../../Artifacts/Reports/and-planet-route-1119/device-7b2f965c/result.json).

For a fixed-camera shoreline build, pass `-FixedShorelineSnapshot <shoreline.json>`
to `Tools/build/build_epoch_android_project.ps1` with the approved Planetary
and FFT sources attached. The build pins the descriptor hash in the project
entry. The signed APK indexes the descriptor, mesh, patch snapshot and
water appearance. At startup the app checks the pin and matches the cut to
the saved project and linked providers, then animates water in the app
process. A missing or mismatched cut uses the saved baked coastline.

Signed x86_64 APK `AF34DCCA10C4` animated the fixed cut on the Android 12
emulator, with Home and resume in the same process. APK `3F6F39226816`
retained the identical native library and rejected a validly packaged
descriptor with the wrong pin. Follow-up APK `37C3D55940D0` used 24-bit
GLES depth on the same view and removed the terrain-skirt band. It kept
water animation, a stable dry reference, and touch controls through
surface recreation in one process. The coast remains coarse. ARM64 APK
`92F78C565DCC` links GLES and passed offline validation with the baked asset
set. Interactive streaming, sustained performance and physical ARM64 execution
need separate proof.
[Animation](../../../../Artifacts/Reports/android-shoreline-runtime-r3-1119/device-c0c890aa/result.json),
[Fallback](../../../../Artifacts/Reports/android-shoreline-refusal-r3-1119/device-5376369f/result.json),
[Depth and lifecycle](../../../../Artifacts/Reports/android-shoreline-depth-1119/device-8f1077d8/result.json),
[ARM64 readback](../../../../Artifacts/Reports/android-shoreline-depth-arm64-1119/readback.json).

The pinned descriptor optionally accepts Boolean `density_clouds`. When true,
the admitted controller supplies the shared analytic recipe to the optional
GLES 3 path. It traces one cloud volume using the sampleable opaque depth
target and composites before SDL overlays. Terrain and clouds use the same
conservatively rounded clip interval. The full viewport is bounded to
4,194,304 pixels and the cloud trace to 16,384. The renderer measures the
first 30 completed SDL frame intervals for this path, excluding background
gaps. Those timings describe the app loop. Use display or device capture to
qualify presentation and sustained mobile cost.

A perspective view with the whole cloud shell ahead of the camera can trace
its padded projected bounds. Resolve and opaque depth use matching native
framebuffer coordinates. Nearby and orthographic views keep a full trace.
The matched 1080 × 2211 orbit used 108 × 151 rays at about 10 px spacing.
Ground, opaque HUD and black space passed pixel comparisons. The orbital
app restored its portrait frame after a landscape rotation. Cloud edges
still show steps. Motion and hardware cost require separate checks.
[Android crop and frame intervals](../../../../Artifacts/Reports/android-verdant-cloud-crop-1119/orientation-and-timing.json).

The cropped orbital resolve eases the ends of each sample interval. It uses
the same four depth-qualified taps and ray count. On the matched Android 12
orbit, brightness curvature dropped at the traced lines and rose between
them. The fixed ground view matched exactly. Large cloud blocks are still
visible, and the timed proof covers a fixed view in the emulator.
[Matched Android resolve](../../../../Artifacts/Reports/android-verdant-cloud-resolve-1119/device-9db59d76/cloud-resolve-pixels.json).

The orbital crop now selects a window of a viewport-anchored ray grid.
Even-index crop starts retain the same fragment-quad footprint for rays in
overlapping windows. The fixed Verdant replay compared both 32 px edge
movements at 1080 × 2211. Crop-induced color and alpha changes fell to zero
across 93,891 sampled overlap pixels. At the parked pose the 1,452-ray
reference error was 16.68 versus 16.48 on a 255-point scale. The initial crop
uses 13,386 trace rays within the 16,384 cap. ARM64 APK `E2F78936CE0BE`
contains the exact orbital pose and terrain cut. The later
[flight package](#package-a-playable-flight-start) verifies movement across
the grid threshold on Android 12. Sustained phone cost still needs a device run.
[CPU replay](../../../../Artifacts/Reports/android-cloud-lattice-1119/replay.log),
[ARM64 package](../../../../Artifacts/Reports/android-cloud-lattice-1119/orbit-verified.json).

The full and cropped passes now use one grid and one trace texture per
drawable size. The crop changes which rays are drawn. Both passes resolve
from those global texels with the same weights. On the November 19 fixed
orbit replay the threshold changed none of the 118,800 sampled overlap
pixels. The parked orbit used 11,392 rays with mean error 18.30 out of 255
against the same full-resolution reference. It previously used 13,386 rays
with error 17.53. Signed ARM64 `EDD64C4DB5C1` passed signature, alignment and
asset readback. A native moving orbit and ground-entry route still need
visual verification and present-time measurements.
[Cloud transition replay](../../../../Artifacts/Reports/android-cloud-window-1119/comparison.json),
[ARM64 readback](../../../../Artifacts/Reports/android-cloud-window-1119/orbit-verified.json).

The same grid presented from signed x86_64 Verdant packages on Android 12
SwiftShader. Orbit and ground preserved their app pixels after Home and
resume. A single 30-interval orbital sample on the same guest measured
75.137 ms before and 62.665 ms after. Orbital cloud edges remain visibly
blocky. The fixed coast retained the controls and dry-land samples while
water advanced from 5.3 to 9.4 s. This establishes fixed-view rendering and
water animation on the emulator. The fixed variants lock the camera. The
mobile flight lane still needs an admitted interactive planetary path.
[November 19 native comparison](../../../../Artifacts/Reports/android-cloud-window-1119/device-35573c00/native-comparison.json).

Use the water-only capture as the comparison. APK `BB061BAACAD4` matched
terrain and HUD controls and retained the sky after surface recreation on
the Android 12 emulator. The fixed view is overcast and visibly banded.
Increasing the grid from 89 × 183 to 178 × 366 retained the broad bands
and increased frame time, so the finer grid stays out of the runtime.
APK `2462BFF3084A` verified the shared clip interval with animated water,
unchanged dry terrain and HUD pixels, and same-process surface recovery.
APK `A592FD40D469` verified those controls again with the shared atmosphere
enabled. After the orbital crop, APK `89738AE861F5` repeated the wet-coast
checks at the full trace, including animated water and surface recovery.
Sustained frame cost and ARM64 hardware are open.
[Native coast receipt](../../../../Artifacts/Reports/android-cloud-depth-stable-1119/device-a55be358/result.json),
[Matched pixels](../../../../Artifacts/Reports/android-cloud-depth-stable-1119/device-a55be358/cloud-depth-stable-pixels.json),
[Atmosphere coast regression](../../../../Artifacts/Reports/android-verdant-sky-1119/coast/device-5f823851/sky-pixels.json),
[Coast after orbital crop](../../../../Artifacts/Reports/android-verdant-cloud-crop-1119/coast/device-6aae968d/sky-pixels.json).

### Package a saved planetary view

Save a paused exploration pose and regenerate the managed Game entry. Pass
`-PackagedPlanetaryPose` to the Android build tool with the approved Planetary
Terrain and FFT sources attached. Include the Game manifest, its saved scene,
`Assets/Worlds/Planet.epoch_planet` and
`Assets/Worlds/Planet.epoch_exploration` in `-ProjectAsset`. The build pins their
exact bytes and verifies the contents of the signed APK before publishing
its local output.

On launch the linked controller restores the packaged body, camera and
weather time. This route currently requires a single wet planet with a cloud
recipe. It draws density clouds and the provider's coarse body mesh by
default. To include a detailed terrain cut, export with `--metric-frame-v1`
and the saved camera's `--view-footprint-v1`. Leave `--patch-transport-v1`
off for this fixed view. Its full mesh contains hidden parent tiles for
desktop transitions. Run until `snapshot_ready=1` and `pending=0` and add
three matching files to `-ProjectAsset` as
`Assets/Worlds/Planet.pose.epoch_mesh`, `Assets/Worlds/Planet.pose.frame`
and `Assets/Worlds/Planet.pose.epoch_planet`. The cut has its own recipe,
and the mesh is bounded to 8 MiB. The build verifies those bytes in the
signed APK. At startup Epoch checks meter scale, body, clock, mesh identity
and the body-relative origin against the restored pose. It checks the
cut's triangles above the restored camera with an outward radial ray. A
refused cut keeps the restored camera and uses the provider's coarse mesh.

Use `-FixedShorelineSnapshot` for animated water. Select one mode per APK.
A changed or missing pose pin, declined provider, or invalid pose leaves
the ordinary saved scene available. The ground, in-cloud and
orbital APKs presented and resumed in one process on Android 12 x86_64.
The fixture uses fixed portrait frames. SDL GLES now draws the shared
atmosphere behind opaque terrain and clouds. The same orbital view gained
an atmospheric limb and black space, and its sampled opaque terrain and
Controls pixels remained identical. The overcast ground and in-cloud poses
still have limited contrast. The selected terrain exports restored with
50,258 vertices for ground and 76,900 for orbit. They preserve visible grass,
the black orbital sky and Controls. Both captures stayed identical across
Home and resume. A prior export with hidden parent tiles crossed above the
ground camera and correctly fell back to coarse terrain, as did a modified
frame. Mobile flight, moving terrain LOD and physical ARM64 remain open.
[Native view results](../../../../Artifacts/Reports/android-verdant-poses-1119/device-2bf3e4ba/result.json),
[Atmosphere comparison](../../../../Artifacts/Reports/android-verdant-sky-1119/device-8faa3d1f/sky-pixels.json),
[Changed-pose fallback](../../../../Artifacts/Reports/android-verdant-poses-1119/device-3ae77113/result.json),
[Ground and orbit camera check](../../../../Artifacts/Reports/android-verdant-camera-guard-1119/device-c2b03bdf/camera-guard-pixels.json),
[Selected terrain captures](../../../../Artifacts/Reports/android-verdant-leaf-only-1119/device-764ca83e/leaf-cut-pixels.json).

### Package a playable flight start

Save a paused flight pose, regenerate the managed Game entry, and use
`-PackagedPlanetaryFlight` with the same authored pose, recipe, manifest,
scene and attached Planetary and FFT inputs. Flight uses the controller's
body mesh. Omit the fixed shoreline and the three optional fixed terrain
cut files. The packager verifies the same signed asset hashes as for a fixed
pose. With Controls closed, hold Ascend or Descend to move vertically.
Releasing or leaving the button ends the hold. Drag the scene away from
controls to look around. The existing exploration controls can resume world
time. Each cold launch restores the signed pose. The orbit-to-ground package
uses the controller body mesh throughout the flight.

In the isolated emulator, compare the `Android density grid presented` rows
before and after descent. Both rows must have the same drawable extent and
lattice. Compare the cloud shell's above, inside and below rows with the
capture timestamps. The renderer logs up to eight initial and changed states
after an SDL present.

On Android 12, x86_64 APK `5D6AEC340C29` held one 89×183 grid while
descending from orbit through the shell, settled after release and looked
up in the original process. The signed starting pose stayed intact.
[Native flight receipt](../../../../Artifacts/Reports/android-flight-look-1119/device-6070872c/result.json),
[scene pixel comparison](../../../../Artifacts/Reports/android-flight-look-1119/device-6070872c/look-pixels.json).

The debug-signed ARM64 flight APK `831B4D82E946` has the same 14 packaged
assets, minimum API 26 and 16 KiB ZIP alignment. Physical Android 12
execution and frame timings remain open.
[ARM64 flight readback](../../../../Artifacts/Reports/android-flight-arm64-1119/matching-flight-assets.json).

For long grazing rays inside the shell, GLES uses the shared near-camera
sample distribution within the 24-bin cap. At the saved in-cloud pose,
Android 12 x86_64 APK `1341EEBA29D1` resumed in the same process with
identical settled scene pixels. The matched CPU-only GLSL comparison
reduced the interior horizontal stripe and retained exact orbit and ground
traces. ARM64 flight APK `768FD79F3D1F` includes this repair and keeps the
same authored assets. Interior shading and physical phone cost remain open.
[Shader comparison](../../../../Artifacts/Reports/verdant-cloud-contours-1119/shader-comparison.json),
[Android resume](../../../../Artifacts/Reports/android-cloud-grazing-1119/device-7acdbb7b/resume-pixels.json),
[current ARM64 readback](../../../../Artifacts/Reports/android-flight-grazing-arm64-1119/matching-flight-assets.json).

### Fly over a packaged shoreline

Regenerate the managed Game entry. Pass `-FixedShorelineSnapshot` with the
reviewed shoreline descriptor and add `-ShorelineFlight`. The package starts
at the signed camera and hovers until the player moves. Hold Ascend or
Descend and drag the scene to look around. The attached FFT Ocean provider
animates the signed terrain cut in the same app process. The cut retains
the detail from its export pose. Moving terrain LOD needs a separate
in-process streaming provider. Omit `-ShorelineFlight` to keep the camera
locked for a shoreline comparison.

On Android 12, the matched x86_64 shoreline packages passed fixed and moving
camera tests with the same signed cut. Touch raised the flight camera from
13 to 65 m and left the fixed camera at 13 m. The water continued sampling
in the flight process after release. The signed ARM64 APK `3B0DBE8851A9`
passed packaging with matching assets. Large shifts at the shallow boundary
are reproducible in a CPU replay of the signed cut. Vertical wave motion over
the shallow shelf explains most of the visible boundary change. The replay
used an estimated stationary camera and best-fit wave phases. Physical device
performance and visual acceptance remain open.
[Paired Android captures](../../../../Artifacts/Reports/android-shore-flight-1119/device-7ccc2e3e/shore-flight.png),
[pixel evidence](../../../../Artifacts/Reports/android-shore-flight-1119/device-7ccc2e3e/verified-pixels.json),
[shoreline replay](../../../../Artifacts/Reports/shore-id-1119/sweep/analysis.json),
[ARM64 readback](../../../../Artifacts/Reports/android-shore-flight-1119/arm64/matching-assets.json).

An experimental shallow-water adjustment was tested on the same Android 12
cut. Over ten held frames per APK, median shoreline travel on the same
screen columns fell from 88 to 56 px. Open-water motion remained measurable
at the same magnitude. The experimental ARM64 APK passed packaging. Its
phone run, visual review, and integration with streamed terrain remain open.
[Native comparison](../../../../Artifacts/Reports/shore-local-1119/device-21dad1ca/native-pixels.json),
[Android captures](../../../../Artifacts/Reports/shore-local-1119/device-21dad1ca/native-coast-comparison.png).

### Package dry flight with terrain streaming

Use `-PackagedPlanetaryFlight` with a saved single-body Planet Game, a dry
recipe with `stream_lod=1`, and a reviewed Planetary source attachment. The
ordinary Game entry pins the project, scene, recipe and starting pose. The
linked provider assembles terrain inside the app from the moving camera. It
reuses the scene scheduler and keeps the last complete cut visible while work
is pending. Dry flight has no FFT dependency. The CPU controller and Android
host compile checks passed. Signed x86_64 APK `E4A733781124` climbed from
143 m to 8.8 km on Android 12 in one process. The two settled terrain cuts
each contained 180 patches, with maximum LOD 14 and 12 respectively. The
fixed shoreline route remains a separate water and cloud check. Wet terrain
streaming follows the recipe below. Physical ARM64 flight needs verification.
[Dry Game preparation](../../../../Artifacts/Reports/lod-assembly-1120/dry-preparation.json),
[Desktop and linked controller proof](../../../../Artifacts/Reports/lod-assembly-1120/linked-host-receipt.json),
[native flight evidence](../../../../Artifacts/Reports/lod-assembly-1120/device-b54765fd/result.json).

### Package wet flight with terrain streaming

For Verdant, use `-PackagedPlanetaryFlight` with a wet single-body recipe,
`stream_lod=1`, and reviewed Planetary and FFT Ocean attachments. The app
builds the complete six-face terrain cut inside the Game process. It uses
the scene's existing FFT worker, shares the presented wave sample with the
terrain worker, and replaces the mesh and water appearance together. The
signed pose fixes the world time. Ocean animation follows its own clock.
The CPU checks passed. Signed APK `93206E2FC84C` ran in one Android 12
x86_64 process from 2 m to 96 m. Nine complete cuts published with 180
patches apiece. The last cut used the 10.45 s wave sample, and animation
then advanced to 11.00 s. Physical ARM64 timing and cloud visuals need
separate checks.
[CPU checks](../../../../Artifacts/Reports/lod-wet-1120/final-cpu-contracts.log),
[signed APK receipt](../../../../Artifacts/Reports/lod-wet-1120/wet-package-check.json),
[native flight](../../../../Artifacts/Reports/lod-wet-1120/device-b5062d7f/result.json).

The GLES cloud pass applies atmospheric haze to distant cloud rays over
clear sky when the sky and cloud views agree on the planet. It reuses the
project's atmospheric thickness, traces the same grid, and preserves the
opaque depth path. Signed x86_64 APK `4848AB9CA7A0` reduced measured coastal
band contrast by 29.5% on the Android 12 emulator against the same saved
pose and assets. The physical phone and cloud interior still need visual
review.
[Native coastal comparison](../../../../Artifacts/Reports/cloud-resolve-1120/device-489b9c31/coast-pixels.json).

The SDL Android scene pass owns opaque depth, sky and cloud composition in
`sdl.android_scene.cpp`. An unsupported viewport or cloud body skips the
current frame's volume. The next eligible frame can draw it in the same GLES
context. Shader or framebuffer failure still disables the pass until reset.
The Android 12 x86_64 coastal replay completed a 180-patch cut and presented
30 cloud frames at 77.1 ms mean and 86.4 ms p95 on SwiftShader. This camera
still shows horizontal bands below the clouds. Physical-device timing is open.
[Coastal replay](../../../../Artifacts/Reports/android-scene-final-1120/device-242efcfc/result.json).

The Android 12 flight with the revised shared density shader crossed the
cloud shell in one process. Its 30 orbital present intervals averaged
80.1 ms (p95 106.0 ms) on SwiftShader. The three captures show broad white
patches over coarse terrain from orbit, nearly uniform grey inside the cloud
and faint horizontal shading when looking up from below. The saved flight
exceeded the mobile wet-terrain profile and drew its fallback surface mesh.
Cloud appearance and physical-phone performance remain open.
[Flight views](../../../../Artifacts/Reports/android-cloud-flight-1121/device-deb10795/flight-contact.png),
[layer and timing records](../../../../Artifacts/Reports/android-cloud-flight-1121/device-deb10795/cloud-readback.json).

## Run the Editor as an Android project

New Project → Epoch Editor creates an ordinary `EditorWorkspace` with its
edition-bound generated entry. The Android build starts the Editor session
on the existing SDL OpenGL ES host and checks the saved Editor document
before presenting. It keeps project and layout settings in app-private
storage. Signed x86_64 debug APK `5E4465D4AF41` saved and reopened
`EditorProject01` on Android 12. **Save & Play** is reachable through the
Project tab overflow. The Editor played an admitted Platformer in-process,
accepted Jump, rendered in landscape, resumed from Home and returned to the
2D workspace. A painted tile remained after Save and reopen. The Platformer
was staged as authored data for this test. Physical ARM64 remains open.
Package builds use the desktop delivery path.
[Native authoring receipt](../../../../Artifacts/Reports/android-editor-project-1119/recheck-6d7fa746/authoring-native-882e84ba/result.json).

### Create a project on Android

Open **Project → New Project** and choose **Game** or **Software / Tool**.
Game starts with a floor, camera, player spawn, input profile and HUD. The
starter's Play settings select first-person view. Drag the scene to look around.
Software / Tool opens its editable interface in the UI workspace. Each new
project uses the packaged Editor's edition and appears in **Available projects**.
When the active project has unsaved edits, create keeps that session open and
selects the new project in the browser. Save your changes before switching.

On Android 12, Development x86_64 APK `F78A2998F4DC` saved both starters and
reopened them with matching authored files. The Game entered in-Editor Play.
The fully saved Tool exported its manifest, scene and GUI with matching hashes.
[Native creation receipt](../../../../Artifacts/Reports/android-editor-create-1119/build-fae8ff8f2b4f/device-48f674fa/result.json).

For an authored Software / Tool project, **Play In Editor** saves the active
GUI, scene and Run settings and previews the compiled GUI in the same Editor
process. Android 12 APK `364CE05CFC96` saved and showed a newly added slider
through Play, then returned to the Editor. The slider's placement clipped at
the portrait edge. Portrait layout, widget interaction and physical ARM64
remain separate checks.
[Tool Play and Return](../../../../Artifacts/Reports/android-editor-create-1119/build-8203febe4941/device-a824d2a5/visual-review.json).

In Play, open **Controls → Return to Editor**. The return action follows the
owning Editor session for both 2D and 3D and restores the authoring guides.
On Android 12 with SDL/GLES, Development APK `0BF876DAD05F` displayed the
starter floor in first-person Play and kept it visible after a drag. Return
restored Scene Preview in the same process with the original authored hashes.
Touch movement requires a separate input check.
[Native Play and Return receipt](../../../../Artifacts/Reports/android-editor-create-1119/build-11e28541e2fc/device-237fe2df/play-starter-result.json).

Android's on-screen square opened Recents and the same Editor process resumed.
[Android Recents receipt](../../../../Artifacts/Reports/android-editor-create-1119/build-11e28541e2fc/device-237fe2df/os-recents-result.json).
On November 19, Windows Emulator 37.2.13 sidebar Overview left both the
Calculator and Android 12 Home in their current views. The sidebar Home
control also left Recents open. Android's on-screen square worked in the same
session and returned to the Calculator with its input intact. Use the
on-screen navigation bar for this emulator. The host input failure remains
open.
[Sidebar comparison](../../../../Artifacts/Reports/android-calculator-api31-1119/compatibility/desktop-sidebar-overview-1119.json).

### Transfer an authored project ZIP

In the Android Editor, open **Project** and choose **Import Project ZIP**.
Select a local ZIP in Android's Files picker. Once the project appears in the
browser, choose **Open Selected** to review it. The archive uses the same
edition as the Editor and starts at `project.epoch.json`, with `worlds/`
and the referenced `Assets/` paths beside it. The initial format covers
saved Game and Tool documents, tile maps, project GUI, input, animation,
audio and compiled textures. Keep the archive under 128 MiB. Builds,
scripts and active extension packages need their desktop workflow.

To export, open the saved Game or Tool from the Editor's **Projects** folder.
Save any edits you want to include, then choose **Export Saved ZIP**. Epoch
prepares the archive from the saved files before opening Android's save picker.
Choose where to create the ZIP. The original project stays in its current
folder, and a recipient can import the ZIP in an Editor of the same edition.

The picker grants access to the selected document. Epoch copies it into
app-private storage, validates it and publishes the complete project. An
existing project keeps its original files. Open and Play remain separate
user actions. On Android 12, signed Development x86_64 APK `300AF00ACE18`
passed Back, incomplete ZIP rejection, first import, duplicate protection,
explicit Open Selected and in-process Play. The 11 authored files matched
the ZIP at import. The saved project reopened after a cold restart.
[Native import receipt](../../../../Artifacts/Reports/android-editor-archive-1119/device-d047954b/result.json).

On Android 12, signed Development x86_64 APK `5EA2DAA9E5A8` exported the
saved Platformer, including a new tile. Canceling the first save left the
project intact. A fresh install of `5CB4655EDBC9` imported the completed ZIP,
matched all 11 files, and showed the edited tile. Physical ARM64 and recovery
after interruption during a provider write remain open.
[Native export and reimport receipt](../../../../Artifacts/Reports/android-editor-export-1119/roundtrip-db949fc5a2ce/device-d65a93a8/result.json).
[Android Storage Access Framework](https://developer.android.com/training/data-storage/shared/documents-files).

## Android Emulator in the Editor

Add an Android target to the existing **Run** workflow and a docked device
display. Use the ordinary saved-project export, install the verified APK on the
selected local virtual device, and track its app and process identity. The
emulator process owns Android. The Editor owns display composition and forwards
input only while the device view has focus. Start and stop the selected session
through the existing run controls and preserve its data and snapshots. Keep
the separate emulator window as an option.

`Tools/build/run_epoch_android_apk.ps1` is the local deployment entry point in
PowerShell 7. It takes the packager's JSON receipt and an explicit emulator
serial. It verifies the APK hash, the guest's supported ABI and the installed
base APK before launching the activity. Use `-Inspect` to read the installed
package identity without installing or launching anything. `-AdbServerPort`
selects the local ADB server.

Qualify the dock with the Calculator first. Capture off the UI thread, reuse
one bounded image surface, keep the latest frame, and suspend capture when the
tab is hidden. Map taps through the displayed image bounds and verify that the
same app, device and orientation are still active. Closing an attached dock
leaves the emulator running. The attach path must use a local transport that
cannot start, stop or replace the existing ADB service.

Android Studio uses its emulator display stream and input API. Review any new
listener through the existing package and host approval rules.
[Android Studio emulator](https://developer.android.com/studio/run/emulator-launch-separate-window),
[Android emulator controller](https://android.googlesource.com/platform/tools/base/+/e50f3a6a1809ad5b2040876123bf3bf04c2db08e/emulator/proto/emulator_controller.proto).

## Repeat the toolchain check

From the checkout root, run the Windows toolchain inventory. The default mode
reports what the SDK contains. `-BuildProbe` requires an NDK and links the
tracked production-module consumer using the selected CMake and Ninja.

```powershell
./Tools/build/check_epoch_android_toolchain.ps1
./Tools/build/check_epoch_android_toolchain.ps1 -BuildProbe -Abi x86_64
./Tools/build/check_epoch_android_toolchain.ps1 -BuildProbe -Abi arm64-v8a
```

Use `-SdkRoot` and `-NdkRoot` for the checkout-local SDK and pinned side-by-side
NDK. The build probe has a fresh local CMake directory and records the result,
including the linked ELF's SHA-256. The standalone `epoch_android_capability`
test target links the same module into the native GLES probe.

## Package a saved project for Android debug

`Tools/build/build_epoch_android_project.ps1` is the Windows host entry for a
saved project. It configures the existing project CMake target for the selected
ABI, builds `epoch_project_runtime`, packages the native libraries and selected
runtime assets, and returns an APK receipt. Supply the installed SDK/JDK and
the dependency prefix for that ABI. Keep the receipt with the APK.
Use `run_epoch_android_apk.ps1` for a separately selected device. It checks
the guest API, ABI, device kind and installed APK identity before starting
the Activity. `-PhysicalDevice` selects an explicit physical-device serial.
Physical-device execution still needs a live acceptance run.

Save the project in the desktop Editor before building. In PowerShell 7, set
`$sdk`, `$jdk`, `$sdlSource`, and `$dependencies` to the installed tools, matching
SDL source and x86_64 Android dependency prefix. For the saved Calculator, use
the following from the checkout root. Set `$calculator` to the saved project,
`$apk` and `$receipt` to new output paths, and `$serial` to the intended emulator.

```powershell
$assets = @('project.epoch.json', 'worlds/tool.epoch', 'Assets/Gui/main.epochgui', 'scripts/tool_bootstrap.ascript.cpp')
./Tools/build/build_epoch_android_project.ps1 -ProjectRoot $calculator -ProjectAsset $assets `
    -SdkRoot $sdk -JdkRoot $jdk -SdlSourceRoot $sdlSource -AndroidDependencies $dependencies `
    -Abi x86_64 -OutputApk $apk | Set-Content -LiteralPath $receipt
./Tools/build/run_epoch_android_apk.ps1 -PackageReceipt $receipt -SdkRoot $sdk -Serial $serial
```

Use `-Abi arm64-v8a` with the ARM64 dependency prefix and a separate APK and
receipt for phones. Android package build and emulator docking are still outside
the Editor's **Run** menu.

Build the project shared library for the selected ABI, then use
`Tools/build/package_epoch_android_apk.ps1` from PowerShell. Pass the saved
project root and its exact runtime files with `ProjectAsset`. The Calculator
uses `project.epoch.json`, `worlds/tool.epoch`, `Assets/Gui/main.epochgui`, and
`scripts/tool_bootstrap.ascript.cpp`. `NativeAppLibrary`, `SdlLibrary`, and
`CxxRuntimeLibrary` are matching ABI files. Point `SdlSourceRoot` at the exact SDL
checkout for that library and pass the checkout-local SDK and JDK roots.

The tool checks the `SDL_main` and `SDL_GetVersion` exports, builds the Activity
DEX, stages canonical ZIP paths, and signs the debug APK. It verifies the
signature, 16 KiB ZIP alignment, runtime bytes and Android permissions before
writing `OutputApk`. The receipt records native library and asset hashes. The
script performs package work only. Emulator or phone deployment is a separate
operation using an explicitly selected transport.

The Activity verifies the bundled asset index and hashes before loading the
native libraries. It publishes a fresh saved project at `files/epoch/project`
and keeps the engine font at `files/epoch/assets/fonts`. Later launches preserve
the existing saved project and check the separate engine-owned assets. A missing
or empty manifest in an existing project stops admission and preserves the data.
The API 31 emulator verified a same-signer upgrade with authored data, a fresh
extraction across reboot, and rejection of an APK for the wrong ABI.
[Upgrade receipt](../../../../Artifacts/Reports/android-calculator-coach-1119/native-proof/durable-negative-proof.json),
[reboot receipt](../../../../Artifacts/Reports/android-calculator-coach-1119/native-proof/durable-reboot.json).

For the Calculator, `-CalculatorCoachArchive` adds the optional isolated CPU
worker. The 409 MiB model is separate. On API 31, the offline worker answered
with the Calculator's evaluator, stopped on request, accepted another question
and rejected wrong model bytes. The follow-up verified Android's picker for the
checked local model. Back cancelled the picker, a wrong-size file published
nothing, and selecting the supported model enabled the next calculation. Host
tests covered low space and copy interruption. Physical ARM64 inference and
actual disk exhaustion on Android remain open.
[Answer evidence](../../../../Artifacts/Reports/android-calculator-coach-picker-1119/device-682e1a69/08-picker-calculation.png),
[ARM64 package receipt](../../../../Artifacts/Reports/android-calculator-coach-picker-1119/package-arm64.json).

## Android platform boundaries

A shipped Epoch game carries the native runtime and its project assets in
its own APK. Android starts the project's activity and loads the native
libraries into that app's process. The desktop editor builds the APK.
The separately installed Android Editor can preview projects inside its
own process. Cross-app imports and exports use Android's Files picker.
[Android application sandbox](https://source.android.com/docs/security/app-sandbox),
[native libraries](https://developer.android.com/ndk/guides/concepts.html).

APK packaging now checks the component boundary before signing and hashes
the compiled manifest again after signing. A standard project exposes one
launcher activity and requests zero permissions. Opting into Calculator
Coach adds its private picker and isolated service. The verified Verdant
package kept all 24 APK entries byte-identical to the emulator-tested
package. The signed ARM64 Coach package passed the manifest, library and
asset checks. Its rebuilt Java code still needs a device run.
[Package proof](../../../../Artifacts/Reports/android-app-boundary-1120/result.json),
[APK entry comparison](../../../../Artifacts/Reports/android-app-boundary-1120/entry-readback.json).

| Surface | Supported path and remaining checks |
| --- | --- |
| Build and deploy | Epoch generates the same project source on desktop. Windows-host builds produced signed APKs for x86_64 and ARM64. The packager checks dependency closure, 16 KiB alignment, permissions and asset hashes. Linux SDK hosting, production signing and physical ARM64 validation remain open. |
| Editor on Android | The packaged Editor can create Game and Tool projects, transfer authored ZIPs, save and reopen projects, and preview the tested Game and authored Tool in-process. APK builds and standalone launch use desktop tools. |
| Storage and transitions | APK assets seed the project on first launch. Saved files, settings and logs live in app-private storage. A same-signer upgrade, guest reboot, and project ZIP transfer through Android's Files picker passed. Low space, process kill during transfer, and abrupt power loss remain open. |
| Communication | Android supports explicit message passing between processes. The optional Calculator Coach uses an isolated Android service. It receives a model descriptor and one question, and the Calculator owns the arithmetic. Project and model data remain separate from installed executable libraries. |
| Rendering and input | The tested backend is SDL OpenGL ES. Keypad, software keyboard, window rotation, resume, surface recreation and terrain touch orbit have emulator evidence. Keep physical GPU, renderer device loss, mobile landscape Editor controls, audio focus and controller hotplug open. |

The Android 12 guest's navigation square opened Recents. The Windows
emulator sidebar failed to open Recents even from Android Home. See the
[navigation comparison](#create-a-project-on-android).

Android SDL now forwards physical keyboard presses and releases for the
owned window through the same gameplay key queries that Project Play uses.
SDL text composition still feeds the authored input field. Focus loss,
backgrounding, device removal and renderer reset discard the held keys.
The Debug and Release host fixtures cover press, repeat, release, foreign
windows and reset. Both API 26 Release SDL adapters compiled with the NDK.
When the window can no longer present, Android discards queued orbit motion.
A suspended GUI contact retains its finger identity until SDL sends release
or cancel. That terminal event clears the press without activating the control.
The host touch fixture verifies this across rapid presentation recovery.
Two debug-signed ARM64 Verdant packages share the new input runtime. APK
`65816A91DF69` preserves all 18 validated coastal assets and the fixed camera
for a water, cloud and resume check. APK `C7EFC4A91AC3` keeps the 14 ordinary
project and font assets and uses the saved scene's camera for keyboard and
interrupted touch checks. Both still need phone execution. Managed-flight
touch and real EGL device loss remain open.
[Host checks](../../../../Artifacts/Reports/dependency-editions-1119/android-key-tests.log),
[x86_64 compile](../../../../Artifacts/Reports/dependency-editions-1119/android-input/x86_64/result.json),
[ARM64 compile](../../../../Artifacts/Reports/dependency-editions-1119/android-input/aarch64/result.json),
[Suspended touch checks](../../../../Artifacts/Reports/android-touch-suspend-1119/final-host.log),
[ARM64 fixed shoreline](../../../../Artifacts/Reports/android-touch-suspend-1119/verified.json),
[ARM64 navigation](../../../../Artifacts/Reports/android-touch-suspend-1119/navigation-verified.json).

The Windows host project archive fixture now uses a single atomic rename in
its private test directory, matching the runtime publication step. Host
checks passed for archive round trips, touch cancellation and pinned model
storage. [Archive check](../../../../Artifacts/Reports/android-project-archive/1795125602504238100/verify.log).

Android's application data rules prevent the desktop pattern of launching a
new executable from writable app storage. Neither full project compilation
on the phone nor launching desktop build tools from the mobile Editor is
part of the current APK route.
[App home execution rules](https://developer.android.com/about/versions/10/behavior-changes-10#execute-permission),
[app-specific storage](https://developer.android.com/training/data-storage/app-specific),
[Android Messenger](https://developer.android.com/reference/android/os/Messenger).

## Success Criteria

- Android package builds from the active source tree
- the packaged runtime can resolve assets, shaders, scripts, and logs without
  cwd assumptions
- one stable mobile backend path presents real content
- app lifecycle and touch/input behave predictably
- rotation, background/foreground and surface loss recover without duplicating
  application state or losing the saved project
- the startup report identifies the actual platform, adapter, graphics tier and
  selection reason, with unknown or unsupported capabilities stated explicitly
- docs say exactly what works and what is still missing
