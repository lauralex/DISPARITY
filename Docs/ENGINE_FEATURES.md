# DISPARITY Engine Feature Guide

This document is the practical test map for the current Visual Studio 2022 C++20/DirectX 11 engine.

## Editor

- Press `F1` to show or hide the Dear ImGui editor.
- Drag panels into the main dockspace, or pull them outside the main window to test multi-viewport support.
- Use `Hierarchy` to select the player or a scene object.
- Selected objects render with a bright outline in the main viewport.
- Press `Tab` to release the mouse, then left-click the main viewport to pick objects. Hold `Ctrl` while picking or selecting in `Hierarchy` to multi-select. Picks try GPU object-ID/depth readback for scene objects, the player, and gizmo handles, then fall back to stable-ID CPU OBB tests.
- Use `Copy`, `Paste`, `Duplicate`, and `Delete` in the `Hierarchy` panel or `Edit` menu. Keyboard shortcuts are `Ctrl+C`, `Ctrl+V`, `Ctrl+D`, and `Delete`.
- The `Viewport` panel enables an editor camera. Use `Frame Selection`, `Frame Player`, right-drag look, arrow/Page keys, or right-drag plus `WASD`/`Q`/`E` to inspect the scene without moving the player.
- The viewport preview includes a compact diagnostics HUD over the renderer-owned texture. It shows active camera/render mode, bloom/TAA state, last GPU-picked object/depth/stale age, readback latency/cache state, high-resolution capture tile/resolve state, and small object-ID/depth debug thumbnail swatches. Use `Viewport > Viewport HUD` to enable, pin, or hide individual HUD rows.
- The viewport texture also has an overlaid toolbar. Use `Cam` to toggle editor/game camera, `Dbg` to cycle post debug views, `Cap` to queue the capture workflow, `ID` and `Depth` to toggle object-ID/depth rows, and `HUD` to hide or restore the diagnostics overlay.
- Editor preferences autosave under `Saved/Editor/editor_preferences.json`. They currently remember viewport HUD rows/pinning, viewport-toolbar visibility, editor-camera enablement, transform precision/pivot/orientation, and the Profiler command-history filter.
- Use `Inspector` to edit transforms/materials. The `Transform Gizmo` section provides small move, scale, and yaw buttons without adding another third-party dependency, and the viewport draws camera-scaled 3D gizmo handles at the current selection pivot. Choose translate/rotate/scale and world/local space in `Viewport`, then left-drag an axis marker, rotate ring, or translucent translate-plane handle; hold `Shift` while dragging for snapping. Gizmo handles brighten on hover or drag.
- Use `Inspector > Transform Precision` to set the gizmo nudge step and inspect the active pivot/orientation mode used by the transform handles.
- Use `Inspector > Prefab Overrides` on a selected scene object to compare against `Assets/Prefabs/Beacon.dprefab`, apply the current object back to the prefab, or revert object mesh/material/scale while preserving world position and stable ID.
- Use `Ctrl+Z` and `Ctrl+Y`, or the `DISPARITY` menu, to test undo/redo. The profiler shows recent command labels and includes a command-history filter for long editor sessions.
- The `Profiler` panel now includes `Production Readiness v25`, a forty-row table that shows the current runtime/editor readiness state for editor preferences, viewport toolbar coverage, gizmo metadata, asset pipeline readiness, render roadmap items, capture/VFX, sequencer/audio, and production automation followups.

## Showcase Mode

- The first playable scene now includes an animated procedural DISPARITY rift in front of the player: HDR rings, orbiting shards, spires, dynamic colored point lights, and stronger bloom response.
- Press `F7`, use `DISPARITY > Showcase Mode`, or click `Start Showcase` in the renderer panel to hide the editor and enter the cinematic orbit camera.
- Showcase mode temporarily forces tone mapping, bloom, SSAO, anti-aliasing, temporal AA, clustered lighting, and higher color grading values, then restores the previous renderer settings when disabled.
- Press `F7` again to return to the previous editor visibility and gameplay camera state.

## Trailer And Photo Mode

- Press `F8`, use `DISPARITY > Trailer/Photo Mode`, or click `Start Trailer` in the renderer panel to play authored camera shots from `Assets/Cinematics/Showcase.dshot`.
- Trailer mode interpolates position, target, focus, depth-of-field strength, lens dirt, letterbox, easing, renderer pulse, audio cue, bookmark, spline, timeline-lane, renderer-track, audio-track, thumbnail, clip-lane, nested-sequence, hold-time, and shot-role metadata so the same vertical-slice camera move is repeatable for recording and verification.
- Use the `Shot Director` panel to add/edit/capture/reload/save v6 `.dshot` keys while the game is running. Camera keys can use Catmull-Rom or linear interpolation, editable easing curves, renderer/audio timeline tracks, thumbnail tint metadata, generated shot thumbnails, clip lanes, nested sequence labels, hold durations, shot roles, and non-modal preview scrubbing.
- Press `F9` or click `Capture PPM+PNG` in the viewport panel to capture the presented frame, then write `Saved/Captures/DISPARITY_photo_source.ppm`, `Saved/Captures/DISPARITY_photo_source.png`, a 2x `Saved/Captures/DISPARITY_photo_2x.ppm`, and `Saved/Captures/DISPARITY_photo_2x.dcapture.json`. The high-resolution path now flows through graph-owned offscreen capture diagnostics and an async worker; the v2 manifest records scale, tile count, MSAA samples, tile jitter, and a tent-like resolve filter used by the current 2x proof.
- The renderer panel exposes depth of field, lens dirt, vignette, film grain, letterbox, title-safe guides, and presentation pulse controls for public demo capture.
- Automatic generated cue tones are off by default for clean capture with the current WinMM prototype backend. Enable `Audio Mixer > Cinematic cue tones` only when you want to test the temporary generated cues.

## Assets And Prefabs

- Edit `Assets/Scenes/Prototype.dscene`, `Assets/Scripts/Prototype.dscript`, `Assets/Prefabs/Beacon.dprefab`, or the player material files while the game is running.
- Keep `Hot reload assets` enabled in the `Assets` panel and wait about half a second, or press `F5` to force scene/script reload.
- Select a scene object, then use `Apply Selection To Beacon Prefab` to write that object shape/material back to `Assets/Prefabs/Beacon.dprefab`.
- Select a scene object, then use `Save Selection As Runtime Prefab` to write a prefab snapshot under `Saved`.
- Use `Rescan Database` to rebuild the in-editor asset database. It classifies assets, shows dirty cooked state, tracks glTF buffer/image references, material texture-slot references, script prefab references, prefab parent/child references, and reports dependency graph nodes/edges.
- Use `Cook Dirty Assets` to write metadata cache files under `Saved/CookedAssets` through the engine job system.
- Use `Export glTF Materials` to convert loaded glTF material metadata into `.dmat` material assets under `Assets/Materials/Imported`, including base-color, normal, metallic-roughness, emissive, and occlusion texture-slot metadata.
- Import settings use `.dimport` files. `Assets/ImportSettings/Assets/Meshes/SampleTriangle.gltf.dimport` is the sample mesh import settings file.

## glTF

- `Assets/Meshes/SampleTriangle.gltf` is loaded at startup.
- The loader creates renderer meshes for every primitive, tracks primitive material indices, binds base-color textures when present, honors glTF `doubleSided` materials, instantiates mesh nodes into the scene, reads skin joint/weight attributes, stores inverse bind matrices, and parses animation sampler keyframes.
- The `Assets` panel shows mesh, primitive, material, node, skin, and animation counts.
- `glTF animation playback` previews parsed animation channels when an imported asset includes them.

## Rendering

- The renderer remains DirectX 11 and shader-model 5.
- The `Renderer` panel exposes VSync, tone mapping, shadow maps, broader CSM-style shadow coverage, clustered light contribution, bloom, SSAO, anti-aliasing, temporal AA, exposure, shadow strength, bloom threshold/strength, SSAO strength, AA strength, temporal blend, saturation, contrast, depth of field, lens dirt, cinematic overlay, vignette, letterbox, title-safe guides, film grain, presentation pulse, and post debug views.
- The scene uses a directional light, shadow map, and eight point lights, including animated rift lights.
- Post-processing uses the HDR scene texture, temporal history texture, and depth SRV.
- The renderer now owns dedicated editor GPU targets for viewport color, object IDs, and object depth. Scene objects, player meshes, and gizmo handles write IDs/depth to those targets, and the editor queues non-blocking readbacks from a staging ring for GPU-backed picking. The `Viewport` panel presents the dedicated viewport texture through ImGui, overlays live GPU-pick/capture diagnostics, and runtime reports include object-ID readback request/completion/pending/busy/latency diagnostics, hover-cache samples, latency histogram buckets, stale-readback age, and last picked object display.
- Materials can request double-sided rendering. The DX11 path switches those draws to a no-cull rasterizer state and the scene shader flips back-face normals, keeping imported single-surface meshes like the sample triangle visible during rotation.
- The `Profiler` panel shows frame timings, job worker count, renderer draw-call counters, GPU frame timing once DX11 timestamp queries warm up, and compiled render-graph schedule/resource lifetime diagnostics. The render graph view also reports per-pass GPU timings, culled passes, graph callback execution, graph resource binding hits/misses, pending capture requests, graph-owned high-resolution capture target/tile/MSAA/pass diagnostics, read/write transition diagnostics, alias candidates, transient allocation slots, and alias reuse.
- If bloom/SSAO/AA/DOF/lens differences are hard to spot in the final image, use `Renderer > Post debug`: `Bloom` isolates bright bleed, `SSAO mask` shows contact darkening, `AA edges` shows the edge detector, `Depth` visualizes the depth buffer, `DOF` shows the circle-of-confusion mask, and `Lens dirt` isolates the dirt/bloom response.
- Bloom became more visible because the scene shader no longer clamps lighting before the HDR post pass.
- Scene materials now include emissive color and emissive intensity, and the rift uses those channels for glow alongside its animated point lights.

## Audio

- The `Audio Mixer` panel exposes Master/SFX/UI/Music/Ambience buses.
- Use `Low Tone`, `High Tone`, and `Spatial` to test generated bus-routed tones.
- `F2` plays the UI notification tone.
- The mixer shows the active backend, per-bus send values, simple peak meters, active generated voice counts, production-style mixer voice counts, streamed music layers, spatial emitters, attenuation curves, meter updates, and content pulse counts.
- The mixer detects XAudio2 runtime availability and exposes a preference switch. When XAudio2 initializes, generated tones use source voices through a dynamically loaded XAudio2 backend; WinMM remains the fallback path.
- The mixer and runtime report expose audio analysis values: peak, RMS, beat envelope, active voices, and whether generated/streamed content has driven the analysis surface.
- `Cinematic cue tones` is opt-in; showcase/trailer visual beat pulses still run when this is off. Generated cues prefer XAudio2 when available and otherwise reuse a persistent WinMM output handle with a short silent pre-roll so cue playback is not dependent on another app keeping the Windows audio endpoint awake.
- Use `Store Snapshot` and `Recall` to test mixer snapshot capture/restore without playing content.
- `AudioSystem::StreamMusic` and `PlayWaveFileAsync` provide WinMM-backed async/looped wave playback hooks for future content, while the public surface now has listener orientation and production-style snapshot/meter APIs for the future XAudio2 backend.

## Asset Cooking

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\CookDisparityAssets.ps1 -Configuration Debug -BinaryPackages
```

The cook writes per-source metadata plus `Saved/CookedAssets/manifest.dcook`. With `-BinaryPackages`, it writes deterministic `.dassetbin` source bundles under `Saved/CookedAssets/Binary`, structured `DSGLBPK2` `.dglbpack` manifests for glTF/glB sources under `Saved/CookedAssets/Optimized`, and package hashes in the manifest. glTF packages include mesh/primitive/material/node/skin/animation counts, accessor and buffer-view metadata, embedded/external buffer hashes, import settings, and dependencies. Runtime verification now loads the optimized package manifest as a cooked runtime resource and validates dependency invalidation coverage.

Review cooked packages with:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\ReviewCookedPackages.ps1
```

## Packaging

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\PackageDisparity.ps1 -Configuration Release -IncludeSymbols -CreateArchive
```

The packaged build is written to `dist/DISPARITY-Release`. Packages include `package_manifest.json`; passing `-IncludeSymbols` copies PDBs under `Symbols`, and `-CreateArchive` writes a zip artifact under `dist`.

Symbol indexing and installer payload generation are separate production checks:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\IndexDisparitySymbols.ps1 -PackagePath .\dist\DISPARITY-Release
powershell -ExecutionPolicy Bypass -File .\Tools\PublishDisparitySymbols.ps1 -PackagePath .\dist\DISPARITY-Release
powershell -ExecutionPolicy Bypass -File .\Tools\CreateDisparityInstaller.ps1 -PackagePath .\dist\DISPARITY-Release -CreateArchive
powershell -ExecutionPolicy Bypass -File .\Tools\CreateDisparityBootstrapper.ps1 -PackagePath .\dist\DISPARITY-Release
```

These write `Symbols/symbols_manifest.json`, a symbol-server publish plan, `dist/SymbolServer/symbol_publish_manifest.json`, `dist/Installer/DISPARITY-SetupManifest.json`, `dist/Installer/DISPARITY-BootstrapperPlan.json`, `dist/Installer/DISPARITY-InstallerBootstrapper.cmd`, and an installer payload zip for later replacement with a real bootstrapper executable.

Release readiness review checks the assembled package, installer/bootstrapper output, symbols, OBS profile, runtime schema manifest, and the v25 production batch manifest:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\ReviewReleaseReadiness.ps1
```

The review writes `Saved/Release/release_readiness_manifest.json` and is part of the full verification gate.

Crash upload staging is local for now:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\CollectCrashReports.ps1 -CreateArchive
powershell -ExecutionPolicy Bypass -File .\Tools\UploadCrashReports.ps1 -DryRun
```

The scripts scan `Saved/CrashLogs`, write `Saved/CrashUploads/crash_upload_manifest.json`, can bundle staged reports, can dry-run the eventual transport step, and retry authenticated uploads with backoff when an endpoint is provided.

Trailer launch preset generation is separate from packaging:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\LaunchTrailerCapture.ps1 -Configuration Release -Packaged -NoLaunch
powershell -ExecutionPolicy Bypass -File .\Tools\GenerateObsSceneProfile.ps1
```

The scripts write `Saved/Trailer/trailer_launch_preset.json` and `Saved/Trailer/OBS/DISPARITY-Trailer-Scene.json` with executable, working directory, capture output, recording hotkey metadata, OBS scene/profile metadata, and recording approval metadata. Omit `-NoLaunch` to start the selected build.

Interactive CI planning is generated by:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\GenerateInteractiveCiPlan.ps1
```

The plan lands in `Saved/CI/interactive_ci_plan.json` and describes the interactive GPU runner, packaged smoke gate, artifacts, runtime suites, and trailer/OBS capture expectations.

## Verification

Use the full local gate before calling the repository healthy:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\VerifyDisparity.ps1
```

The verification script runs `git diff --check`, a Dear ImGui literal ID conflict check, warning-free Debug and Release builds, MSVC static analysis, all shader entry-point compiles, asset cook manifest/binary package generation, optimized `.dglbpack` package review, crash upload manifest/upload dry runs, baseline review, v25 production manifest review, baseline approval manifest generation with Git signature metadata, signed baseline update approval intent, interactive CI/trailer/OBS plan generation, Debug window smoke, every runtime verification suite, Release packaging with symbols/archive, symbol indexing and symbol publishing, installer payload/bootstrapper generation, release readiness review, packaged window smoke, every packaged runtime verification suite, and a performance-history summary against committed baselines.

Targeted checks are still useful while iterating:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\SmokeTestDisparity.ps1 -Configuration Debug -ExerciseWindow
powershell -ExecutionPolicy Bypass -File .\Tools\TestImGuiIds.ps1
powershell -ExecutionPolicy Bypass -File .\Tools\RuntimeVerifyDisparity.ps1 -Configuration Debug -Frames 90
.\bin\x64\Debug\DisparityGame.exe --verify-runtime --verify-frames=90
```

Runtime verification writes `Saved/Verification/runtime_verify.txt`, captures `Saved/Verification/runtime_capture.ppm`, runs deterministic input playback, validates capture dimensions/luminance/checksum/nonblank pixels, checks CPU/GPU frame budgets and per-pass render-graph budgets, validates render-graph dispatch order, graph callback execution, graph resource binding, transition barriers, transient allocation counts, alias reuse, async object-ID readback ring diagnostics, hover-cache samples, latency histogram buckets, viewport overlay rows, viewport HUD row toggles/debug thumbnails, transform precision state, editor preference save/load round trips, viewport toolbar actions, high-resolution resolve metadata, and editor viewport/object-ID/depth target readiness, confirms the imported glTF triangle is double-sided so it does not vanish when rotated, cycles all seven post-debug views, exercises showcase mode and trailer/photo mode with deterministic rift timing, validates graph-owned high-resolution capture diagnostics plus the async 2x output/manifest workflow, validates rift VFX draw counts plus depth-fade/GPU-simulation/motion-vector/temporal-reprojection/profile counters, validates beat-pulse counts, validates editor pick/gizmo pick coverage, runs gizmo transform constraint checks, validates async text IO, material texture-slot persistence, prefab variant/nested-prefab metadata, cooked package loading/GPU-promotion metadata and dependency invalidation, Shot Director timeline tracks/thumbnails/preview scrubbing/v6 sequencing metadata, animation blending/skinning palette state, XAudio2 backend state, production audio counters, audio analysis, audio meter calibration, audio snapshot capture/restore, release-readiness coverage, and all forty v25 production followup points from `Assets/Verification/V25ProductionBatch.dfollowups`, downsamples the frame into a thumbnail, compares it against a suite-specific golden PPM, writes a golden diff thumbnail, and exits non-zero if an invariant fails. `RuntimeVerifyDisparity.ps1` asserts the report metrics listed in `Assets/Verification/RuntimeReportSchema.dschema`, so missing viewport/capture/v24/v25/v26 production diagnostics fail fast without duplicating schema lists in multiple scripts. Budget defaults can be overridden through `RuntimeVerifyDisparity.ps1` or direct flags such as `--verify-cpu-budget-ms=120`, `--verify-gpu-budget-ms=50`, and `--verify-pass-budget-ms=60`.

Runtime replay and baseline expectations are assetized:

- `Assets/Verification/RuntimeSuites.dverify` defines named runtime suites. The current suites are `Prototype`, `CameraSweep`, `EditorPrecision`, `PostDebug`, `AssetReload`, and `GizmoDrag`.
- `Assets/Verification/Prototype.dreplay` defines frame ranges, movement vectors, and camera drift for deterministic playback.
- `Assets/Verification/CameraSweep.dreplay` adds a camera-heavy deterministic playback path.
- `Assets/Verification/EditorPrecision.dreplay` adds editor-picking coverage for stable-ID object picks and gizmo handle picks.
- `Assets/Verification/PostDebug.dreplay`, `AssetReload.dreplay`, and `GizmoDrag.dreplay` add scenario labels and replay paths for production-style verification coverage.
- `Assets/Verification/*Baseline.dverify` files define expected capture dimensions, average luminance tolerance, nonblack pixel ratio, minimum replay path distance, editor pick counts, gizmo pick counts, post-debug counts, showcase/trailer frame counts, high-resolution capture counts, rift VFX draw counts, beat-pulse counts, async IO/material/prefab/shot/spline/audio-analysis/XAudio2/VFX-system/GPU-VFX/animation-skinning/GPU-pick-histogram/Shot-Director-thumbnail/graph-high-res/cooked-package/nested-prefab/audio-production/viewport-overlay/high-resolution-resolve/viewport-HUD/transform-precision/command-history/schema-manifest/shot-sequencer/VFX-profile/cooked-GPU/dependency-invalidation/audio-calibration/release-readiness/editor-preference/viewport-toolbar test counts, the `min_v25_production_points=40` gate, render-graph callback/barrier/allocation/alias/resource-binding requirements, editor GPU target requirements, performance budgets, and golden thumbnail tolerances.
- `Assets/Verification/RuntimeReportSchema.dschema` lists required runtime report metrics for wrapper-side schema checks.
- `Assets/Verification/V25ProductionBatch.dfollowups` lists the forty stable v25 followup points that runtime verification must promote to report metrics.
- `Assets/Verification/Goldens/*.ppm` stores suite-specific 64x36 golden thumbnails.
- `Assets/Verification/GoldenProfiles/*.dgoldenprofile` stores per-machine or per-adapter golden tolerance defaults. The runtime verification wrapper detects the primary display adapter and uses a matching profile when present, otherwise it falls back to the default profile.
- `Assets/Verification/PerformanceBaselines.dperf` stores committed CPU/GPU/pass thresholds per suite for trend comparison.
- `Tools/CompareCaptureDisparity.ps1` creates or compares capture thumbnails against goldens and writes diff thumbnails.
- `Tools/SummarizePerformanceHistory.ps1` groups recent local runs by suite/executable and reports CPU/GPU deltas plus editor/gizmo/showcase/trailer/high-res/VFX/beat/viewport-overlay/high-resolve/schema/release/editor-preference/viewport-toolbar scenario counters, then compares them against committed performance baselines when available. Regression checks compare both the previous run and the recent median so isolated max-frame spikes remain visible without immediately failing the gate.
- `Tools/ReviewVerificationBaselines.ps1` checks runtime baselines for required capture, graph, editor target, golden, and v24 coverage fields before running the performance summary.
- `Tools/ReviewProductionBatch.ps1` checks the v25 followup manifest for exactly forty uniquely named points and writes `Saved/Verification/v25_production_batch_review.json`.
- `Tools/ApproveVerificationBaseline.ps1` writes `Saved/Verification/baseline_approval.json` with hashes for runtime baselines, performance baselines, golden profiles, and golden thumbnails plus Git signature status/signing-key metadata. Use `-RequireSignedHead` when approving a golden/performance update locally.
- `Tools/ApproveVerificationUpdate.ps1` writes `Saved/Verification/baseline_update_approval.json` with explicit approver intent, Git signature status, and verification file hashes for baseline/golden changes.
- `Saved/Verification/performance_history.csv` is appended by `RuntimeVerifyDisparity.ps1` so repeated local runs leave a trend trail without committing generated data.

Crashes write a small report to `Saved/CrashLogs`. GitHub Actions runs the static side of `Tools/VerifyDisparity.ps1` on push and pull request; opt-in `workflow_dispatch` inputs can create package artifacts, run packaged smoke, upload artifacts, and run the runtime gate when a runner has an interactive desktop.
