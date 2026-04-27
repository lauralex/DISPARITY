# DISPARITY Agent Context

This file is for future Codex/model sessions working in this repository.

## Project Snapshot

DISPARITY is a native Visual Studio 2022 C++20 project for a simple 3D game engine and the first playable prototype of the game DISPARITY.

Current shape:

- `DISPARITY.sln` is the main solution.
- `DisparityEngine` is a static library.
- `DisparityGame` is a Windows executable that links the engine.
- Renderer backend is DirectX 11.
- Dependency policy now allows the vendored Dear ImGui docking branch in `ThirdParty/imgui`; otherwise prefer Win32, DirectX 11, DirectXMath, WIC/WinMM, and the Windows SDK.
- Geometry includes procedural primitives, procedural terrain, and a glTF 2.0 scene loader path for static mesh primitives, double-sided material metadata, material texture binding, node instancing, skin metadata, joint/weight attributes, and animation sampler data.
- Editor/runtime v20 builds on v19 with graph-owned render-pass callbacks and callback/barrier/resource-handle diagnostics, direct ImGui presentation of the dedicated editor viewport texture, object-ID readback ring diagnostics, v3 `.dshot` easing/renderer-pulse/audio-cue/bookmark metadata, rift VFX system stats, material texture-slot persistence/export, prefab variant/parent/nested dependency metadata, async text IO through the job system, transform blending plus skinning palette metadata, audio analysis values, deterministic `.dglbpack` cook placeholders, committed adapter golden profiles, and a baseline approval manifest script.
- Editor/runtime v19 builds on v18.3 with GPU-backed scene/player/gizmo object-ID and depth readback for viewport picking, double-sided material rendering with back-face normal flipping for imported single-surface showcase meshes, a queued capture path that can write WIC PNG alongside PPM, an in-editor Shot Director panel for `.dshot` keys, prefab override comparison with apply/revert against `Beacon.dprefab`, asset dependency graph display, richer cook dependency metadata, and a trailer launch preset script for repeatable public capture.
- Editor/runtime v18.3 includes docking, multi-viewport panels, undo/redo with command labels, selection outlines, viewport click-picking, Ctrl multi-select, stable-ID/OBB editor picking, dedicated editor viewport/object-ID/object-depth GPU targets, gizmo hover highlighting, copy/paste/duplicate/delete scene-object workflows, an independent editor camera with right-drag WASD/QE fly controls, simple transform gizmo buttons plus camera-scaled 3D translate/rotate/scale handles, torus rotate rings, translucent XY/XZ/YZ translate-plane handles, snapping and world/local space, prefab apply/save workflows, schema v4 scene saves with double-sided material flags, an animated procedural DISPARITY rift set piece, F7 cinematic showcase mode, F8 authored trailer/photo mode, F9 2x PPM capture workflow, emissive material channels, rift particles/ribbons/lightning/fog/lens-dirt/beat-pulse VFX, depth-of-field/title-safe/film-grain presentation controls, visibly stronger post effects with seven debug views, bus-based audio controls, bus sends, simple meters, mixer snapshots, opt-in cinematic cue tones, smoothed generated tone envelopes, persistent WinMM tone output warm-up, spatial listener orientation, XAudio2 availability detection, an asset database with import settings plus deterministic cooked metadata and `.dassetbin` package manifests, a compiled render graph schedule with pass CPU/GPU timings, culled passes, transition diagnostics, alias candidates, resource lifetimes, transient allocation slots, alias reuse, renderer pass-dispatch validation, job system, version reporting, crash logs plus local crash upload manifests/dry-run transport, deterministic runtime self-verification, six-suite assetized replay/baseline/golden verification, showcase/trailer/high-res/VFX/beat coverage counters, deterministic rift timing for runtime captures, frame capture validation, adapter-aware golden profile selection, golden diff thumbnails, committed performance baselines, Dear ImGui duplicate literal ID verification, package manifests/symbols/zip artifacts, symbol indexing, installer payload manifests, stronger window smoke tests, and unified static/runtime verification scripts.

## Important Paths

- `DisparityEngine/Source/Disparity/Disparity.h`: public umbrella include.
- `DisparityEngine/Source/Disparity/Core/`: application loop, input, time, logging, crash handler, version surface, layer hook.
- `DisparityEngine/Source/Disparity/Rendering/Renderer.*`: DirectX 11 renderer, GPU resources, shader setup, draw calls, emissive material constants, HDR post-processing, depth-of-field/lens-dirt/cinematic overlay controls, dedicated editor viewport/object-ID/object-depth targets, object-ID readback ring diagnostics, GPU timing, graph dispatch/callback validation, queued PPM/PNG frame capture, and WIC PNG export.
- `DisparityEngine/Source/Disparity/Rendering/RenderGraph.*`: compiled render graph scaffold with pass dependency scheduling, culled pass tracking, transition diagnostics, alias candidates, resource lifetimes, transient allocation slots, alias reuse, and timing diagnostics.
- `DisparityEngine/Source/Disparity/Assets/`: asset database, material assets, simple JSON, glTF loading, asset hot reload, and tiny prefab IO.
- `DisparityEngine/Source/Disparity/ECS/`: small entity/component registry.
- `DisparityEngine/Source/Disparity/Diagnostics/`: frame profiler.
- `DisparityEngine/Source/Disparity/Editor/`: Dear ImGui wrapper plus legacy title overlay helper.
- `DisparityEngine/Source/Disparity/Animation/`: keyframe, bob, transform blending, and skinning palette helpers.
- `DisparityEngine/Source/Disparity/Audio/`: basic Windows audio hooks plus bus/meter/snapshot/analysis surfaces.
- `DisparityEngine/Source/Disparity/Scripting/`: tiny scene script runner.
- `DisparityEngine/Source/Disparity/Runtime/`: job system scaffold.
- `DisparityEngine/Source/Disparity/Scene/`: camera, material, mesh, procedural primitive factory including cube/plane/terrain/torus helpers, scene object types.
- `DisparityGame/Source/DisparityGame.cpp`: first third-person walking prototype.
- `Assets/Shaders/Basic.hlsl`: runtime-compiled scene shader.
- `Assets/Shaders/PostProcess.hlsl`: runtime-compiled HDR tone-mapping pass.
- `Assets/Cinematics/Showcase.dshot`: authored trailer/photo camera keys for repeatable public capture.
- `Assets/Scenes/Prototype.dscene`: serialized prototype scene.
- `Assets/Verification/RuntimeSuites.dverify`: runtime verification suite manifest.
- `Assets/Verification/Prototype.dreplay`: deterministic runtime verification replay asset.
- `Assets/Verification/CameraSweep.dreplay`: camera-heavy deterministic runtime verification replay asset.
- `Assets/Verification/EditorPrecision.dreplay`: editor stable-ID picking and gizmo picking runtime verification replay asset.
- `Assets/Verification/PostDebug.dreplay`: post-debug scenario runtime verification replay asset.
- `Assets/Verification/AssetReload.dreplay`: asset reload scenario runtime verification replay asset.
- `Assets/Verification/GizmoDrag.dreplay`: gizmo constraint scenario runtime verification replay asset.
- `Assets/Verification/RuntimeBaseline.dverify`: runtime capture/performance baseline asset.
- `Assets/Verification/CameraSweepBaseline.dverify`: camera-heavy runtime capture/performance baseline asset.
- `Assets/Verification/EditorPrecisionBaseline.dverify`: editor precision runtime capture/performance baseline asset.
- `Assets/Verification/PostDebugBaseline.dverify`: post-debug runtime capture/performance/scenario baseline asset.
- `Assets/Verification/AssetReloadBaseline.dverify`: asset reload runtime capture/performance/scenario baseline asset.
- `Assets/Verification/GizmoDragBaseline.dverify`: gizmo drag runtime capture/performance/scenario baseline asset.
- `Assets/Verification/Goldens/*.ppm`: suite-specific golden thumbnails used by runtime verification.
- `Assets/Verification/GoldenProfiles/Default.dgoldenprofile`: default golden comparison tolerance profile.
- `Assets/Verification/PerformanceBaselines.dperf`: committed suite-level performance thresholds for trend comparison.
- `Assets/ImportSettings/Assets/Meshes/SampleTriangle.gltf.dimport`: sample import settings file for the asset database.
- `Assets/Scripts/Prototype.dscript`: tiny prototype scene script.
- `Assets/Prefabs/Beacon.dprefab`: tiny prefab used by the script.
- `Assets/Meshes/SampleTriangle.gltf`: embedded-buffer sample glTF mesh for loader validation.
- `Tools/PackageDisparity.ps1`: Release/Debug packaging helper.
- `Tools/CookDisparityAssets.ps1`: deterministic cooked asset metadata, `.dassetbin` source package, and `.dglbpack` optimized-package-placeholder manifest helper.
- `Tools/LaunchTrailerCapture.ps1`: writes a trailer capture launch preset and optionally starts Debug/Release or packaged DISPARITY for public recording sessions.
- `Tools/CollectCrashReports.ps1`: local crash upload manifest/bundle helper.
- `Tools/UploadCrashReports.ps1`: crash upload dry-run/transport helper.
- `Tools/SmokeTestDisparity.ps1`: launch, window-detect, resize/hotkey, and close runtime smoke helper.
- `Tools/RuntimeVerifyDisparity.ps1`: launches `DisparityGame.exe --verify-runtime`, waits for the runtime PASS/FAIL report, verifies capture output, and fails on non-zero exit.
- `Tools/VerifyDisparity.ps1`: full local verification gate for static and runtime checks.
- `Tools/TestImGuiIds.ps1`: static Dear ImGui literal label check that fails on duplicate widget IDs or empty labels in editor windows.
- `Tools/CompareCaptureDisparity.ps1`: downsamples PPM captures and compares or updates golden thumbnails, including diff thumbnail output.
- `Tools/ReviewVerificationBaselines.ps1`: checks baseline assets for required capture/render-graph/editor-target/golden/v20 coverage fields and runs performance summaries.
- `Tools/ApproveVerificationBaseline.ps1`: writes a local approval manifest with hashes for verification baselines, performance baselines, golden profiles, and golden thumbnails.
- `Tools/SummarizePerformanceHistory.ps1`: prints recent CPU/GPU and scenario trend summaries grouped by suite/executable and compares against committed performance baselines.
- `Tools/IndexDisparitySymbols.ps1`: writes symbol manifests for packaged PDB payloads.
- `Tools/CreateDisparityInstaller.ps1`: writes installer payload manifests and optional installer payload zips.
- `.github/workflows/windows-build.yml`: GitHub Actions static verification through `Tools/VerifyDisparity.ps1 -SkipRuntime`, plus an opt-in `workflow_dispatch` runtime gate for interactive runners.
- `ThirdParty/imgui`: vendored Dear ImGui source and Win32/DX11 backends.
- `Docs/ENGINE_FEATURES.md`: current feature/testing guide.
- `Docs/ROADMAP.md`: next production roadmap.

## Build And Run

Use Visual Studio 2022 or MSBuild:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" .\DISPARITY.sln /m /p:Configuration=Debug /p:Platform=x64
& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" .\DISPARITY.sln /m /p:Configuration=Release /p:Platform=x64
```

Outputs:

- `bin/x64/Debug/DisparityGame.exe`
- `bin/x64/Release/DisparityGame.exe`

The Visual Studio debugger working directory is set to the solution directory so `Assets/Shaders/Basic.hlsl` can be found at runtime. The renderer also searches upward from the executable/current directory for assets.

Runtime verification mode:

```powershell
.\bin\x64\Debug\DisparityGame.exe --verify-runtime --verify-frames=90
```

It writes `Saved/Verification/runtime_verify.txt`, captures `Saved/Verification/runtime_capture.ppm`, loads a `.dreplay` asset, compares against a `.dverify` baseline, exercises deterministic player/camera input playback, scene reload, runtime scene save, renderer post-debug cycling, cinematic showcase mode, authored trailer/photo mode, 2x high-resolution capture, selection cycling, stable-ID object/gizmo picks, gizmo transform constraints, audio mixer snapshots, async text IO, material texture-slot persistence, prefab variant metadata, shot-director easing/bookmarks, audio analysis, animation blending/skinning palette helpers, rift VFX draw/beat-pulse/system counters, draw-call counters, render-graph validation, render-graph allocation/alias/dispatch/callback/barrier diagnostics, object-ID readback ring diagnostics, dedicated editor viewport/object-ID/object-depth target readiness, image stats, and CPU/GPU/pass performance budgets, then exits with code `0` for PASS or `20` for FAIL. `Tools/RuntimeVerifyDisparity.ps1` also compares the capture against the baseline's golden thumbnail through an adapter-specific profile when present or the default golden profile unless `-DisableGoldenComparison` is used.

## Current Controls

- `WASD`: move the player placeholder.
- Mouse: orbit the third-person camera while captured.
- `Tab`: release or recapture the mouse.
- `Esc`: quit.
- `F1`: toggle Dear ImGui editor panels.
- `F2`: play a short generated WinMM-backed audio test tone and show status in the overlay.
- `F3`: cycle selected scene object in the overlay.
- `F5`: reload scene and script; this is only visually obvious after the asset files are edited while the game is running.
- `F6`: save runtime scene snapshot to `Saved/PrototypeRuntime.dscene`.
- `F7`: toggle cinematic showcase mode, hide the editor, boost the post stack, and orbit the animated DISPARITY rift for capture-friendly footage.
- `F8`: toggle authored trailer/photo mode from `Assets/Cinematics/Showcase.dshot`.
- `F9`: queue a source PPM, source PNG, and 2x PPM photo capture under `Saved/Captures`.
- `Audio Mixer > Cinematic cue tones`: optional generated WinMM cue tones for showcase/trailer; default is off to keep capture audio clean until the XAudio2 backend is implemented.
- `Ctrl+Z` / `Ctrl+Y`: undo and redo editor-side scene/player/renderer edits.
- `Ctrl+C` / `Ctrl+V` / `Ctrl+D` / `Delete`: copy, paste, duplicate, or delete the selected scene object.
- When mouse capture is released with `Tab`, left-click the main viewport to pick objects. Hold `Ctrl` while picking or selecting in Hierarchy for multi-selection. The editor now tries GPU object-ID readback first and falls back to CPU ray/OBB picking.
- The `Viewport` panel can enable the independent editor camera; right-drag plus `WASD`/`Q`/`E`, or arrow/Page keys, moves that camera without moving the player.
- Selected objects and the player draw colored, camera-scaled 3D transform handles at the active selection pivot. Choose translate/rotate/scale and world/local space in `Viewport`, then left-drag an X/Y/Z axis marker, rotate ring, or scene-object translate plane; hold `Shift` while dragging to snap.

## Coding Conventions

- Keep the project native VS `.sln`/`.vcxproj` unless the user explicitly asks for CMake.
- Keep C++20, x64 Debug/Release, warning level 4.
- Prefer small, readable engine interfaces over heavy abstractions at this stage.
- Keep DirectX 11 implementation details inside `DisparityEngine`.
- Use DirectXMath for math until a deliberate math-library decision is made.
- Dear ImGui docking branch is intentionally vendored; do not add more external dependencies without user approval.
- Keep Dear ImGui control labels unique within each visible window; use `##StableHiddenId` when two controls intentionally share visible text.
- Use `apply_patch` for manual edits.

## Verified Baseline

On 2026-04-27 after the v20 production followup verification pass:

- `Debug|x64` built successfully with 0 warnings and 0 errors.
- `Release|x64` built successfully with 0 warnings and 0 errors.
- MSVC static analysis completed successfully.
- `Tools/TestImGuiIds.ps1` completed successfully and is wired into `Tools/VerifyDisparity.ps1`.
- `Assets/Shaders/Basic.hlsl` and `Assets/Shaders/PostProcess.hlsl` compiled successfully for `VSMain` and `PSMain` with `fxc`.
- `Tools/SmokeTestDisparity.ps1 -Configuration Debug -ExerciseWindow` launched `DisparityGame.exe`, found the window, resized it, sent basic editor hotkeys, kept it alive for 3 seconds, and closed it cleanly.
- `Tools/VerifyDisparity.ps1` produced PASS Debug runtime reports for the `Prototype`, `CameraSweep`, `EditorPrecision`, `PostDebug`, `AssetReload`, and `GizmoDrag` suites, wrote captures/thumbnails/diff thumbnails, compared them against `Assets/Verification/Goldens/*.ppm` through adapter/default golden profiles, validated stable-ID editor picks, gizmo picks, render-graph allocation/alias/dispatch/callback/barrier diagnostics, dedicated editor viewport/object-ID/object-depth GPU target readiness, ImGui viewport texture presentation, gizmo transform constraints, scene reload/save counters, seven post-debug view counters, showcase/trailer frame counters, high-resolution capture counters, rift VFX stats, audio analysis, audio beat-pulse counters, audio snapshot counters, async IO, material texture-slot persistence, prefab variant metadata, Shot Director v3 metadata, animation blending/skinning palette counters, loaded the replay/baseline assets, and appended `Saved/Verification/performance_history.csv`.
- `Tools/CookDisparityAssets.ps1 -Configuration Debug -BinaryPackages` produced `Saved/CookedAssets/manifest.dcook` plus `.dassetbin` package records with dependency metadata, repo-root dependency resolution, and `.dglbpack` optimized package placeholders for glTF/glB assets.
- `Tools/LaunchTrailerCapture.ps1 -Configuration Debug -NoLaunch` produced `Saved/Trailer/trailer_launch_preset.json` for trailer/showcase capture setup.
- `Tools/CollectCrashReports.ps1 -DryRun` produced `Saved/CrashUploads/crash_upload_manifest.json`, and `Tools/UploadCrashReports.ps1 -DryRun` validated the transport dry run.
- `Tools/ReviewVerificationBaselines.ps1 -ListGoldenProfiles` validated required baseline keys, printed available golden profiles, and recognized the committed default plus known adapter profile set.
- `Tools/ApproveVerificationBaseline.ps1 -DryRun` produced `Saved/Verification/baseline_approval.json` with hashes for baselines, performance profiles, golden profiles, and golden captures.
- `Tools/PackageDisparity.ps1 -Configuration Release -IncludeSymbols -CreateArchive` produced `dist/DISPARITY-Release`, `package_manifest.json`, symbols when PDBs are available, and a zip artifact.
- `Tools/IndexDisparitySymbols.ps1` produced `Symbols/symbols_manifest.json` for packaged PDBs.
- `Tools/CreateDisparityInstaller.ps1 -CreateArchive` produced `dist/Installer/DISPARITY-SetupManifest.json` and an installer payload zip.
- `Tools/SmokeTestDisparity.ps1 -ExecutablePath .\dist\DISPARITY-Release\DisparityGame.exe -ExerciseWindow` launched the packaged build, found/resized/exercised the window, and closed it cleanly.
- `Tools/VerifyDisparity.ps1` produced PASS packaged runtime reports, captures, golden comparisons, showcase/trailer/high-res/VFX/beat/audio-analysis/Shot-Director/async-IO/animation-skinning/render-graph-callback coverage, and local performance history rows for all six suites against `dist/DISPARITY-Release\DisparityGame.exe`.
- `Tools/SummarizePerformanceHistory.ps1 -BaselinePath Assets/Verification/PerformanceBaselines.dperf` completed successfully as part of `Tools/VerifyDisparity.ps1`.
- The v20 pass was verified with `Tools/VerifyDisparity.ps1` before handoff; run `git status --short` and review the signed commit output before pushing.

After feature work, run `powershell -ExecutionPolicy Bypass -File .\Tools\VerifyDisparity.ps1` and `git status --short` before declaring the repo healthy.

## Likely Next Followups

Good next steps for making the engine more modern and AAA-like:

- Bind renderer pass resources from graph allocation handles instead of renderer member variables now that graph callbacks execute.
- Replace the current object-ID readback diagnostic ring with fully non-blocking staging resources, hover cache visualization, and latency histograms.
- Replace the remaining 2x PPM high-resolution proof with graph-owned offscreen render targets, MSAA resolve options, tiled supersampling, and async capture workers.
- Expand v3 Shot Director metadata into camera splines, editable easing curves, renderer/audio cue timeline tracks, shot thumbnails, and non-modal preview scrubbing.
- Promote the VFX stats surface into a dedicated particle/ribbon renderer with soft particles, depth fade, sorting controls, GPU simulation, motion vectors, and temporal VFX reprojection.
- Add more committed per-adapter golden profiles from real verification hardware and turn baseline approval manifests into signed baseline update approvals.
- Replace `.dglbpack` placeholders with optimized cooked mesh/material/animation packages, runtime dependency invalidation, retargeting, and real GPU skinning palette uploads.
- Promote prefab variant/parent/nested metadata into nested prefab instancing, multi-object override diffing, dependency-aware recursive apply/revert, and undo grouping.
- Replace the WinMM prototype implementation behind the current audio surface with XAudio2 voices, streamed music, spatial emitters, attenuation curves, production meters, and real content-driven VFX pulses.
- Replace installer manifests with a real bootstrapper, publish symbols to a symbol server, add authenticated crash upload retries, run packaged runtime smoke tests on interactive CI runners, and expand trailer launch presets into OBS scene/profile automation.
