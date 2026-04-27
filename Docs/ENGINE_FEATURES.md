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
- Use `Inspector` to edit transforms/materials. The `Transform Gizmo` section provides small move, scale, and yaw buttons without adding another third-party dependency, and the viewport draws camera-scaled 3D gizmo handles at the current selection pivot. Choose translate/rotate/scale and world/local space in `Viewport`, then left-drag an axis marker, rotate ring, or translucent translate-plane handle; hold `Shift` while dragging for snapping. Gizmo handles brighten on hover or drag.
- Use `Inspector > Prefab Overrides` on a selected scene object to compare against `Assets/Prefabs/Beacon.dprefab`, apply the current object back to the prefab, or revert object mesh/material/scale while preserving world position and stable ID.
- Use `Ctrl+Z` and `Ctrl+Y`, or the `DISPARITY` menu, to test undo/redo. The profiler shows recent command labels.

## Showcase Mode

- The first playable scene now includes an animated procedural DISPARITY rift in front of the player: HDR rings, orbiting shards, spires, dynamic colored point lights, and stronger bloom response.
- Press `F7`, use `DISPARITY > Showcase Mode`, or click `Start Showcase` in the renderer panel to hide the editor and enter the cinematic orbit camera.
- Showcase mode temporarily forces tone mapping, bloom, SSAO, anti-aliasing, temporal AA, clustered lighting, and higher color grading values, then restores the previous renderer settings when disabled.
- Press `F7` again to return to the previous editor visibility and gameplay camera state.

## Trailer And Photo Mode

- Press `F8`, use `DISPARITY > Trailer/Photo Mode`, or click `Start Trailer` in the renderer panel to play authored camera shots from `Assets/Cinematics/Showcase.dshot`.
- Trailer mode interpolates position, target, focus, depth-of-field strength, lens dirt, letterbox, easing, renderer pulse, audio cue, and bookmark metadata so the same vertical-slice camera move is repeatable for recording and verification.
- Use the `Shot Director` panel to add/edit/capture/reload/save v3 `.dshot` keys while the game is running.
- Press `F9` or click `Capture PPM+PNG` in the viewport panel to capture the presented frame, then write `Saved/Captures/DISPARITY_photo_source.ppm`, `Saved/Captures/DISPARITY_photo_source.png`, and a 2x upscaled `Saved/Captures/DISPARITY_photo_2x.ppm`.
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
- The renderer now owns dedicated editor GPU targets for viewport color, object IDs, and object depth. Scene objects, player meshes, and gizmo handles write IDs/depth to those targets, and the editor can read back the hovered pixel for GPU-backed picking. The `Viewport` panel presents the dedicated viewport texture through ImGui, and runtime reports include object-ID readback ring request/completion/latency diagnostics.
- Materials can request double-sided rendering. The DX11 path switches those draws to a no-cull rasterizer state and the scene shader flips back-face normals, keeping imported single-surface meshes like the sample triangle visible during rotation.
- The `Profiler` panel shows frame timings, job worker count, renderer draw-call counters, GPU frame timing once DX11 timestamp queries warm up, and compiled render-graph schedule/resource lifetime diagnostics. The render graph view also reports per-pass GPU timings, culled passes, graph callback execution, pending capture requests, read/write transition diagnostics, alias candidates, transient allocation slots, and alias reuse.
- If bloom/SSAO/AA/DOF/lens differences are hard to spot in the final image, use `Renderer > Post debug`: `Bloom` isolates bright bleed, `SSAO mask` shows contact darkening, `AA edges` shows the edge detector, `Depth` visualizes the depth buffer, `DOF` shows the circle-of-confusion mask, and `Lens dirt` isolates the dirt/bloom response.
- Bloom became more visible because the scene shader no longer clamps lighting before the HDR post pass.
- Scene materials now include emissive color and emissive intensity, and the rift uses those channels for glow alongside its animated point lights.

## Audio

- The `Audio Mixer` panel exposes Master/SFX/UI/Music/Ambience buses.
- Use `Low Tone`, `High Tone`, and `Spatial` to test generated bus-routed tones.
- `F2` plays the UI notification tone.
- The mixer shows the active backend, per-bus send values, simple peak meters, and active generated voice counts.
- The mixer detects XAudio2 runtime availability and exposes a preference switch while v16 keeps WinMM as the actual playback path.
- The mixer and runtime report expose audio analysis values: peak, RMS, beat envelope, active voices, and whether generated/streamed content has driven the analysis surface.
- `Cinematic cue tones` is opt-in; showcase/trailer visual beat pulses still run when this is off, but repeated generated WinMM tones are suppressed to avoid glitches. Generated tones reuse a persistent WinMM output handle and write a short silent pre-roll so cue playback is not dependent on another app keeping the Windows audio endpoint awake.
- Use `Store Snapshot` and `Recall` to test mixer snapshot capture/restore without playing content.
- `AudioSystem::StreamMusic` and `PlayWaveFileAsync` provide WinMM-backed async/looped wave playback hooks for future content, while the public surface now has listener orientation and production-style snapshot/meter APIs for the future XAudio2 backend.

## Asset Cooking

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\CookDisparityAssets.ps1 -Configuration Debug -BinaryPackages
```

The cook writes per-source metadata plus `Saved/CookedAssets/manifest.dcook`. With `-BinaryPackages`, it writes deterministic `.dassetbin` source bundles under `Saved/CookedAssets/Binary`, deterministic `.dglbpack` optimized-package placeholders for glTF/glB sources under `Saved/CookedAssets/Optimized`, and package hashes in the manifest. Metadata now includes declared import-setting, glTF URI, material texture-slot, script prefab, and prefab parent/child dependencies plus a cook payload label. These packages still need a runtime-optimized mesh/material/animation layout, but the deterministic package and dependency records are now in place.

## Packaging

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\PackageDisparity.ps1 -Configuration Release -IncludeSymbols -CreateArchive
```

The packaged build is written to `dist/DISPARITY-Release`. Packages include `package_manifest.json`; passing `-IncludeSymbols` copies PDBs under `Symbols`, and `-CreateArchive` writes a zip artifact under `dist`.

Symbol indexing and installer payload generation are separate production checks:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\IndexDisparitySymbols.ps1 -PackagePath .\dist\DISPARITY-Release
powershell -ExecutionPolicy Bypass -File .\Tools\CreateDisparityInstaller.ps1 -PackagePath .\dist\DISPARITY-Release -CreateArchive
```

These write `Symbols/symbols_manifest.json`, `dist/Installer/DISPARITY-SetupManifest.json`, and an installer payload zip for later replacement with a real bootstrapper.

Crash upload staging is local for now:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\CollectCrashReports.ps1 -CreateArchive
powershell -ExecutionPolicy Bypass -File .\Tools\UploadCrashReports.ps1 -DryRun
```

The scripts scan `Saved/CrashLogs`, write `Saved/CrashUploads/crash_upload_manifest.json`, can bundle staged reports, and can dry-run the eventual transport step.

Trailer launch preset generation is separate from packaging:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\LaunchTrailerCapture.ps1 -Configuration Release -Packaged -NoLaunch
```

The script writes `Saved/Trailer/trailer_launch_preset.json` with executable, working directory, capture output, and recording hotkey metadata. Omit `-NoLaunch` to start the selected build.

## Verification

Use the full local gate before calling the repository healthy:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\VerifyDisparity.ps1
```

The verification script runs `git diff --check`, a Dear ImGui literal ID conflict check, warning-free Debug and Release builds, MSVC static analysis, all shader entry-point compiles, asset cook manifest/binary package generation, crash upload manifest/upload dry runs, baseline review, baseline approval manifest generation, Debug window smoke, every runtime verification suite, Release packaging with symbols/archive, symbol indexing, installer payload generation, packaged window smoke, every packaged runtime verification suite, and a performance-history summary against committed baselines.

Targeted checks are still useful while iterating:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\SmokeTestDisparity.ps1 -Configuration Debug -ExerciseWindow
powershell -ExecutionPolicy Bypass -File .\Tools\TestImGuiIds.ps1
powershell -ExecutionPolicy Bypass -File .\Tools\RuntimeVerifyDisparity.ps1 -Configuration Debug -Frames 90
.\bin\x64\Debug\DisparityGame.exe --verify-runtime --verify-frames=90
```

Runtime verification writes `Saved/Verification/runtime_verify.txt`, captures `Saved/Verification/runtime_capture.ppm`, runs deterministic input playback, validates capture dimensions/luminance/checksum/nonblank pixels, checks CPU/GPU frame budgets and per-pass render-graph budgets, validates render-graph dispatch order, graph callback execution, transition barriers, transient allocation counts, alias reuse, object-ID readback ring diagnostics, and editor viewport/object-ID/depth target readiness, confirms the imported glTF triangle is double-sided so it does not vanish when rotated, cycles all seven post-debug views, exercises showcase mode and trailer/photo mode with deterministic rift timing, validates the 2x high-resolution capture workflow, validates rift VFX draw counts and VFX system stats, validates beat-pulse counts, validates editor pick/gizmo pick coverage, runs gizmo transform constraint checks, validates async text IO, material texture-slot persistence, prefab variant metadata, shot director easing/bookmarks, animation blending/skinning palette state, audio analysis, and audio snapshot capture/restore, downsamples the frame into a thumbnail, compares it against a suite-specific golden PPM, writes a golden diff thumbnail, and exits non-zero if an invariant fails. Budget defaults can be overridden through `RuntimeVerifyDisparity.ps1` or direct flags such as `--verify-cpu-budget-ms=120`, `--verify-gpu-budget-ms=50`, and `--verify-pass-budget-ms=60`.

Runtime replay and baseline expectations are assetized:

- `Assets/Verification/RuntimeSuites.dverify` defines named runtime suites. The current suites are `Prototype`, `CameraSweep`, `EditorPrecision`, `PostDebug`, `AssetReload`, and `GizmoDrag`.
- `Assets/Verification/Prototype.dreplay` defines frame ranges, movement vectors, and camera drift for deterministic playback.
- `Assets/Verification/CameraSweep.dreplay` adds a camera-heavy deterministic playback path.
- `Assets/Verification/EditorPrecision.dreplay` adds editor-picking coverage for stable-ID object picks and gizmo handle picks.
- `Assets/Verification/PostDebug.dreplay`, `AssetReload.dreplay`, and `GizmoDrag.dreplay` add scenario labels and replay paths for production-style verification coverage.
- `Assets/Verification/*Baseline.dverify` files define expected capture dimensions, average luminance tolerance, nonblack pixel ratio, minimum replay path distance, editor pick counts, gizmo pick counts, post-debug counts, showcase/trailer frame counts, high-resolution capture counts, rift VFX draw counts, beat-pulse counts, async IO/material/prefab/shot/audio-analysis/VFX-system/animation-skinning test counts, render-graph callback/barrier/allocation/alias requirements, editor GPU target requirements, performance budgets, and golden thumbnail tolerances.
- `Assets/Verification/Goldens/*.ppm` stores suite-specific 64x36 golden thumbnails.
- `Assets/Verification/GoldenProfiles/*.dgoldenprofile` stores per-machine or per-adapter golden tolerance defaults. The runtime verification wrapper detects the primary display adapter and uses a matching profile when present, otherwise it falls back to the default profile.
- `Assets/Verification/PerformanceBaselines.dperf` stores committed CPU/GPU/pass thresholds per suite for trend comparison.
- `Tools/CompareCaptureDisparity.ps1` creates or compares capture thumbnails against goldens and writes diff thumbnails.
- `Tools/SummarizePerformanceHistory.ps1` groups recent local runs by suite/executable and reports CPU/GPU deltas plus editor/gizmo/showcase/trailer/high-res/VFX/beat scenario counters, then compares them against committed performance baselines when available.
- `Tools/ReviewVerificationBaselines.ps1` checks runtime baselines for required capture, graph, editor target, and golden fields before running the performance summary.
- `Tools/ApproveVerificationBaseline.ps1` writes `Saved/Verification/baseline_approval.json` with hashes for runtime baselines, performance baselines, golden profiles, and golden thumbnails.
- `Saved/Verification/performance_history.csv` is appended by `RuntimeVerifyDisparity.ps1` so repeated local runs leave a trend trail without committing generated data.

Crashes write a small report to `Saved/CrashLogs`. GitHub Actions runs the static side of `Tools/VerifyDisparity.ps1` on push and pull request; an opt-in `workflow_dispatch` input can run the runtime gate when a runner has an interactive desktop.
