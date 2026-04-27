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
- Editor/runtime v27 builds on v26 with named editor preference profiles under `Saved/Editor/Profiles`, reset/profile save/load controls in the `Viewport` panel, schema v3 high-resolution capture manifests with preset/async-compression/EXR readiness metadata, typed capture preset diagnostics, VFX emitter/sort/GPU-buffer stats, cooked package dependency-preview and reload-rollback readiness, runtime report schema metrics for all of those surfaces, baseline-required v27 counters, and performance-history summaries for profiles, capture presets, VFX emitters, and cooked dependency previews.
- Editor/runtime v26 builds on v25 with real persisted editor preferences at `Saved/Editor/editor_preferences.json`, autosaved viewport HUD row/pin state, viewport-toolbar visibility, editor-camera enablement, transform precision/pivot/orientation, and command-history filters, plus a compact click-through viewport toolbar over the renderer-owned ImGui viewport texture for camera mode, post-debug cycling, capture queueing, object-ID/depth row toggles, and HUD visibility. Runtime verification now round-trips preferences through an isolated probe file, exercises toolbar actions, emits v26 report metrics, requires v26 baseline counters, and includes those counters in performance-history summaries.
- Editor/runtime v25 builds on v24 with a forty-point production followup manifest in `Assets/Verification/V25ProductionBatch.dfollowups`, typed runtime readiness profiles for editor preferences, viewport toolbar state, advanced gizmos, asset pipeline promotion, rendering roadmap items, capture/VFX, sequencer/audio, and production automation, a Profiler `Production Readiness v25` table, runtime report metrics for every `v25_point_*`, baseline-required `min_v25_production_points=40`, `Tools/ReviewProductionBatch.ps1`, release-readiness manifest coverage for the production batch, and performance-history summaries that include the v25 point count.
- Editor/runtime v24 builds on v23 with ten production followup batches: configurable/pinnable viewport HUD rows plus object-ID/depth debug thumbnails, transform precision controls for gizmo nudging, filterable command history, assetized runtime report schema assertions in `Assets/Verification/RuntimeReportSchema.dschema`, v6 `.dshot` clip-lane/nested-sequence/hold/role metadata, VFX renderer profile validation, cooked package GPU-resource readiness metadata, dependency-invalidation verification, audio meter calibration metadata, release-readiness manifests, v24 baseline counters, and performance-history schema/release counters.
- Editor/runtime v23 builds on v22 with an in-viewport diagnostic HUD over the renderer-owned editor texture, last GPU-picked object/depth/stale-age display, readback cache/latency overlay rows, high-resolution capture tile/resolve status, schema v2 high-resolution capture manifests, a row-buffered tent-like 2x capture resolve, v23 runtime report schema assertions, baseline-required viewport overlay/high-resolution resolve coverage, and median-aware performance trend gating.
- Editor/runtime v22 builds on v21 with GPU-pick hover-cache and latency histogram diagnostics, v5 `.dshot` easing/renderer-track/audio-track/thumbnail metadata, generated Shot Director thumbnails, non-modal preview scrubbing coverage, graph-owned high-resolution capture color/resolve resources plus capture manifests, runtime particle/ribbon VFX resources with depth fade and temporal-history counters, cooked `DSGLBPK2` package runtime-resource validation, nested prefab verification, production-style audio mixer/spatial/content-pulse counters, signed baseline update approval manifests, cooked package review tooling, symbol publishing, an installer bootstrapper command, OBS scene/profile metadata generation, and CI artifact paths for the new manifests.
- Editor/runtime v21 builds on v20 with graph-handle resource binding for renderer passes, non-blocking async object-ID/depth readback slots, v4 `.dshot` camera spline/timeline/thumbnail metadata, Shot Director spline controls, rift VFX GPU-simulation/motion-vector/temporal-reprojection counters, a dynamically loaded XAudio2 generated-tone path with WinMM fallback, async 2x capture worker coverage, structured `DSGLBPK2` `.dglbpack` glTF package manifests, Git-signature-aware baseline approval metadata, interactive CI plans, package artifact workflow inputs, bootstrapper/symbol-server plans, and crash upload retry/backoff.
- Editor/runtime v20 builds on v19 with graph-owned render-pass callbacks and callback/barrier/resource-handle diagnostics, direct ImGui presentation of the dedicated editor viewport texture, object-ID readback ring diagnostics, v3 `.dshot` easing/renderer-pulse/audio-cue/bookmark metadata, rift VFX system stats, material texture-slot persistence/export, prefab variant/parent/nested dependency metadata, async text IO through the job system, transform blending plus skinning palette metadata, audio analysis values, deterministic `.dglbpack` cook placeholders, committed adapter golden profiles, and a baseline approval manifest script.
- Editor/runtime v19 builds on v18.3 with GPU-backed scene/player/gizmo object-ID and depth readback for viewport picking, double-sided material rendering with back-face normal flipping for imported single-surface showcase meshes, a queued capture path that can write WIC PNG alongside PPM, an in-editor Shot Director panel for `.dshot` keys, prefab override comparison with apply/revert against `Beacon.dprefab`, asset dependency graph display, richer cook dependency metadata, and a trailer launch preset script for repeatable public capture.
- Editor/runtime v18.3 includes docking, multi-viewport panels, undo/redo with command labels, selection outlines, viewport click-picking, Ctrl multi-select, stable-ID/OBB editor picking, dedicated editor viewport/object-ID/object-depth GPU targets, gizmo hover highlighting, copy/paste/duplicate/delete scene-object workflows, an independent editor camera with right-drag WASD/QE fly controls, simple transform gizmo buttons plus camera-scaled 3D translate/rotate/scale handles, torus rotate rings, translucent XY/XZ/YZ translate-plane handles, snapping and world/local space, prefab apply/save workflows, schema v4 scene saves with double-sided material flags, an animated procedural DISPARITY rift set piece, F7 cinematic showcase mode, F8 authored trailer/photo mode, F9 2x PPM capture workflow, emissive material channels, rift particles/ribbons/lightning/fog/lens-dirt/beat-pulse VFX, depth-of-field/title-safe/film-grain presentation controls, visibly stronger post effects with seven debug views, bus-based audio controls, bus sends, simple meters, mixer snapshots, opt-in cinematic cue tones, smoothed generated tone envelopes, persistent WinMM tone output warm-up, spatial listener orientation, XAudio2 availability detection, an asset database with import settings plus deterministic cooked metadata and `.dassetbin` package manifests, a compiled render graph schedule with pass CPU/GPU timings, culled passes, transition diagnostics, alias candidates, resource lifetimes, transient allocation slots, alias reuse, renderer pass-dispatch validation, job system, version reporting, crash logs plus local crash upload manifests/dry-run transport, deterministic runtime self-verification, six-suite assetized replay/baseline/golden verification, showcase/trailer/high-res/VFX/beat coverage counters, deterministic rift timing for runtime captures, frame capture validation, adapter-aware golden profile selection, golden diff thumbnails, committed performance baselines, Dear ImGui duplicate literal ID verification, package manifests/symbols/zip artifacts, symbol indexing, installer payload manifests, stronger window smoke tests, and unified static/runtime verification scripts.

## Important Paths

- `DisparityEngine/Source/Disparity/Disparity.h`: public umbrella include.
- `DisparityEngine/Source/Disparity/Core/`: application loop, input, time, logging, crash handler, version surface, layer hook.
- `DisparityEngine/Source/Disparity/Rendering/Renderer.*`: DirectX 11 renderer, GPU resources, shader setup, draw calls, emissive material constants, HDR post-processing, depth-of-field/lens-dirt/cinematic overlay controls, graph-resource bindings, dedicated editor viewport/object-ID/object-depth targets, async object-ID/depth readback ring diagnostics, graph-owned high-resolution capture diagnostics, GPU timing, graph dispatch/callback/resource-binding validation, queued PPM/PNG frame capture, and WIC PNG export.
- `DisparityEngine/Source/Disparity/Rendering/RenderGraph.*`: compiled render graph scaffold with pass dependency scheduling, culled pass tracking, transition diagnostics, alias candidates, resource lifetimes, transient allocation slots, alias reuse, and timing diagnostics.
- `DisparityEngine/Source/Disparity/Assets/`: asset database, material assets, simple JSON, glTF loading, asset hot reload, and tiny prefab IO.
- `DisparityEngine/Source/Disparity/ECS/`: small entity/component registry.
- `DisparityEngine/Source/Disparity/Diagnostics/`: frame profiler.
- `DisparityEngine/Source/Disparity/Editor/`: Dear ImGui wrapper plus legacy title overlay helper.
- `DisparityEngine/Source/Disparity/Animation/`: keyframe, bob, transform blending, and skinning palette helpers.
- `DisparityEngine/Source/Disparity/Audio/`: Windows audio hooks with bus/meter/snapshot/analysis surfaces, production-style voice/stream/spatial/attenuation counters, dynamic XAudio2 generated-tone playback when available, and WinMM fallback output.
- `DisparityEngine/Source/Disparity/Scripting/`: tiny scene script runner.
- `DisparityEngine/Source/Disparity/Runtime/`: job system scaffold.
- `DisparityEngine/Source/Disparity/Scene/`: camera, material, mesh, procedural primitive factory including cube/plane/terrain/torus helpers, scene object types.
- `DisparityGame/Source/DisparityGame.cpp`: first third-person walking prototype, editor viewport diagnostics HUD, persisted editor preferences, viewport toolbar overlay, runtime verification scenario coverage, showcase/trailer mode orchestration, and high-resolution capture resolve workflow.
- `Assets/Shaders/Basic.hlsl`: runtime-compiled scene shader.
- `Assets/Shaders/PostProcess.hlsl`: runtime-compiled HDR tone-mapping pass.
- `Assets/Cinematics/Showcase.dshot`: authored v6 trailer/photo camera keys with spline, timeline-lane, editable easing, renderer/audio tracks, thumbnail metadata, clip lanes, nested sequence names, hold times, and shot-role metadata for repeatable public capture.
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
- `Assets/Verification/RuntimeReportSchema.dschema`: required runtime-report metric schema used by the verification wrapper.
- `Assets/Verification/V25ProductionBatch.dfollowups`: forty stable v25 production followup points that runtime verification and tooling must keep in sync.
- `Assets/Verification/Goldens/*.ppm`: suite-specific golden thumbnails used by runtime verification.
- `Assets/Verification/GoldenProfiles/Default.dgoldenprofile`: default golden comparison tolerance profile.
- `Assets/Verification/PerformanceBaselines.dperf`: committed suite-level performance thresholds for trend comparison.
- `Saved/Editor/editor_preferences.json`: generated local editor preference file; do not commit unless intentionally turning it into a checked-in default template.
- `Saved/Editor/Profiles/*.json`: generated named editor preference profiles; do not commit unless intentionally creating a checked-in default profile set.
- `Assets/ImportSettings/Assets/Meshes/SampleTriangle.gltf.dimport`: sample import settings file for the asset database.
- `Assets/Scripts/Prototype.dscript`: tiny prototype scene script.
- `Assets/Prefabs/Beacon.dprefab`: tiny prefab used by the script.
- `Assets/Meshes/SampleTriangle.gltf`: embedded-buffer sample glTF mesh for loader validation.
- `Tools/PackageDisparity.ps1`: Release/Debug packaging helper.
- `Tools/CookDisparityAssets.ps1`: deterministic cooked asset metadata, `.dassetbin` source package, and structured `DSGLBPK2` `.dglbpack` optimized glTF package manifest helper.
- `Tools/ReviewCookedPackages.ps1`: validates optimized `.dglbpack` packages and writes `Saved/CookedAssets/package_review.json`.
- `Tools/LaunchTrailerCapture.ps1`: writes a trailer capture launch preset and optionally starts Debug/Release or packaged DISPARITY for public recording sessions.
- `Tools/GenerateObsSceneProfile.ps1`: writes OBS scene/profile metadata under `Saved/Trailer/OBS`.
- `Tools/CollectCrashReports.ps1`: local crash upload manifest/bundle helper.
- `Tools/UploadCrashReports.ps1`: crash upload dry-run/transport helper with retry/backoff.
- `Tools/SmokeTestDisparity.ps1`: launch, window-detect, resize/hotkey, and close runtime smoke helper.
- `Tools/RuntimeVerifyDisparity.ps1`: launches `DisparityGame.exe --verify-runtime`, waits for the runtime PASS/FAIL report, verifies capture output, asserts the report metrics listed in `Assets/Verification/RuntimeReportSchema.dschema`, records performance history including v27 profile/capture/VFX/cooked-dependency counters, and fails on non-zero exit.
- `Tools/VerifyDisparity.ps1`: full local verification gate for static and runtime checks.
- `Tools/TestImGuiIds.ps1`: static Dear ImGui literal label check that fails on duplicate widget IDs or empty labels in editor windows.
- `Tools/CompareCaptureDisparity.ps1`: downsamples PPM captures and compares or updates golden thumbnails, including diff thumbnail output.
- `Tools/ReviewVerificationBaselines.ps1`: checks baseline assets for required capture/render-graph/editor-target/golden/v24-v27 coverage fields and runs performance summaries.
- `Tools/ReviewProductionBatch.ps1`: validates the v25 forty-point production followup manifest and writes `Saved/Verification/v25_production_batch_review.json`.
- `Tools/ApproveVerificationBaseline.ps1`: writes a local approval manifest with hashes for verification baselines, performance baselines, golden profiles, golden thumbnails, and Git signature metadata.
- `Tools/ApproveVerificationUpdate.ps1`: writes signed baseline/golden update intent metadata for verification changes.
- `Tools/GenerateInteractiveCiPlan.ps1`: writes `Saved/CI/interactive_ci_plan.json` for interactive GPU runners, packaged smoke, artifacts, runtime suites, and trailer/OBS capture expectations.
- `Tools/SummarizePerformanceHistory.ps1`: prints recent CPU/GPU and scenario trend summaries grouped by suite/executable, includes viewport/high-resolve/schema/release/preference/toolbar/profile/capture-preset/VFX-emitter/cooked-dependency counters, and compares previous-run plus recent-median deltas against committed performance baselines.
- `Tools/IndexDisparitySymbols.ps1`: writes symbol manifests and symbol-server publish plans for packaged PDB payloads.
- `Tools/PublishDisparitySymbols.ps1`: copies packaged PDBs into `dist/SymbolServer` and writes `symbol_publish_manifest.json`.
- `Tools/CreateDisparityInstaller.ps1`: writes installer payload manifests, bootstrapper plans, and optional installer payload zips.
- `Tools/CreateDisparityBootstrapper.ps1`: writes `dist/Installer/DISPARITY-InstallerBootstrapper.cmd`.
- `Tools/ReviewReleaseReadiness.ps1`: validates package, installer/bootstrapper, symbols, OBS profile, and runtime-report schema manifests, then writes `Saved/Release/release_readiness_manifest.json`.
- `.github/workflows/windows-build.yml`: GitHub Actions static verification through `Tools/VerifyDisparity.ps1 -SkipRuntime`, plus opt-in `workflow_dispatch` package, packaged-smoke, artifact-upload, and runtime gates for interactive runners.
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

It writes `Saved/Verification/runtime_verify.txt`, captures `Saved/Verification/runtime_capture.ppm`, loads a `.dreplay` asset, compares against a `.dverify` baseline, exercises deterministic player/camera input playback, scene reload, runtime scene save, renderer post-debug cycling, cinematic showcase mode, authored trailer/photo mode, graph-owned async 2x high-resolution capture with schema v3 preset/tent resolve metadata, selection cycling, stable-ID object/gizmo picks, gizmo transform constraints, viewport HUD controls/debug thumbnails, transform precision controls, filterable command history, editor preference save/load/profile round trips, viewport toolbar action coverage, audio mixer snapshots, audio meter calibration, async text IO, material texture-slot persistence, prefab variant and nested-prefab metadata, cooked package loading/GPU-resource promotion/dependency-preview/rollback readiness, asset dependency invalidation, shot-director easing/bookmarks/splines/timeline tracks/thumbnails/preview scrubbing/v6 sequencing metadata, XAudio2 backend state, production audio counters, audio analysis, animation blending/skinning palette helpers, VFX renderer profile and emitter counters, rift VFX draw/beat-pulse/system/GPU-simulation/depth-fade/GPU-buffer counters, draw-call counters, render-graph validation, render-graph allocation/alias/dispatch/callback/barrier/resource-binding/high-resolution-capture diagnostics, async object-ID readback ring/hover-cache/latency-histogram/viewport-overlay diagnostics, dedicated editor viewport/object-ID/object-depth target readiness, release readiness coverage, all forty v25 production followup points, image stats, and CPU/GPU/pass performance budgets, then exits with code `0` for PASS or `20` for FAIL. `Tools/RuntimeVerifyDisparity.ps1` also compares the capture against the baseline's golden thumbnail through an adapter-specific profile when present or the default golden profile unless `-DisableGoldenComparison` is used, and fails fast if required report metrics from `Assets/Verification/RuntimeReportSchema.dschema` are missing.

## Current Controls

- `WASD`: move the player placeholder.
- Mouse: orbit the third-person camera while captured.
- `Tab`: release or recapture the mouse.
- `Esc`: quit.
- `F1`: toggle Dear ImGui editor panels.
- `F2`: play a short generated audio test tone through XAudio2 when initialized, or WinMM fallback, and show status in the overlay.
- `F3`: cycle selected scene object in the overlay.
- `F5`: reload scene and script; this is only visually obvious after the asset files are edited while the game is running.
- `F6`: save runtime scene snapshot to `Saved/PrototypeRuntime.dscene`.
- `F7`: toggle cinematic showcase mode, hide the editor, boost the post stack, and orbit the animated DISPARITY rift for capture-friendly footage.
- `F8`: toggle authored trailer/photo mode from `Assets/Cinematics/Showcase.dshot`.
- `F9`: queue a source PPM, source PNG, async 2x PPM photo capture, and schema v3 high-resolution capture manifest under `Saved/Captures`.
- `Audio Mixer > Cinematic cue tones`: optional generated cue tones for showcase/trailer; default is off to keep capture audio clean. Generated tones prefer XAudio2 and fall back to the warmed WinMM path.
- `Ctrl+Z` / `Ctrl+Y`: undo and redo editor-side scene/player/renderer edits.
- `Ctrl+C` / `Ctrl+V` / `Ctrl+D` / `Delete`: copy, paste, duplicate, or delete the selected scene object.
- When mouse capture is released with `Tab`, left-click the main viewport to pick objects. Hold `Ctrl` while picking or selecting in Hierarchy for multi-selection. The editor now tries GPU object-ID readback first and falls back to CPU ray/OBB picking.
- The `Viewport` panel can enable the independent editor camera; right-drag plus `WASD`/`Q`/`E`, or arrow/Page keys, moves that camera without moving the player.
- The `Viewport` panel persists HUD/toolbar/camera/transform/filter/profile preferences, has a `Preference Profiles` save/load/reset section, and overlays toolbar buttons on the viewport texture: `Cam`, `Dbg`, `Cap`, `ID`, `Depth`, and `HUD`.
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

On 2026-04-27 after the v27 diversified production batch:

- `Debug|x64` built successfully with 0 warnings and 0 errors.
- `Release|x64` built successfully with 0 warnings and 0 errors.
- MSVC static analysis completed successfully.
- `Tools/TestImGuiIds.ps1` completed successfully and is wired into `Tools/VerifyDisparity.ps1`.
- `Assets/Shaders/Basic.hlsl` and `Assets/Shaders/PostProcess.hlsl` compiled successfully for `VSMain` and `PSMain` with `fxc`.
- `Tools/SmokeTestDisparity.ps1 -Configuration Debug -ExerciseWindow` launched `DisparityGame.exe`, found the window, resized it, sent basic editor hotkeys, kept it alive for 3 seconds, and closed it cleanly.
- `Tools/RuntimeVerifyDisparity.ps1 -Configuration Debug -Frames 90 -DisableGoldenComparison` completed successfully and reported `version=0.27.0`, `editor_preference_profile_tests=1`, `capture_preset_tests=1`, `vfx_emitter_profile_tests=1`, `cooked_dependency_preview_tests=1`, `rift_vfx_gpu_buffer_bytes=3648`, and `cooked_package_reload_rollback_ready=true`.
- `Tools/VerifyDisparity.ps1` completed successfully for v27; it now includes assetized runtime report schema assertions, ten v24 production-batch counters, forty v25 production followup point counters, v26 editor-preference persistence and viewport-toolbar counters, v27 named profile/capture-preset/VFX-emitter/cooked-dependency counters, viewport-HUD/debug-thumbnail baseline coverage, transform precision checks, command-history filtering checks, v6 Shot Director metadata checks, VFX profile checks, cooked GPU-resource readiness checks, dependency invalidation/preview/rollback checks, audio calibration checks, production followup manifest review, release-readiness review, cooked package review, Git-signature-aware baseline approval metadata checks, signed baseline update approval intent, trailer launch/OBS plan generation, symbol publishing, installer bootstrapper generation, and median-aware performance trend review.
- `Tools/CookDisparityAssets.ps1 -Configuration Debug -BinaryPackages -Clean` produced `Saved/CookedAssets/manifest.dcook` plus `.dassetbin` package records and a structured `DSGLBPK2` package for `Assets/Meshes/SampleTriangle.gltf` with mesh/primitive/material/node/animation/buffer metadata.
- `Tools/ReviewCookedPackages.ps1` produced `Saved/CookedAssets/package_review.json` and validated optimized package runtime-loadability metadata.
- `Tools/LaunchTrailerCapture.ps1 -Configuration Debug -NoLaunch` produced `Saved/Trailer/trailer_launch_preset.json` for trailer/showcase capture setup.
- `Tools/GenerateObsSceneProfile.ps1` produced `Saved/Trailer/OBS/DISPARITY-Trailer-Scene.json` for public recording setup.
- `Tools/CollectCrashReports.ps1 -DryRun` produced `Saved/CrashUploads/crash_upload_manifest.json`, and `Tools/UploadCrashReports.ps1 -DryRun` validated the transport dry run.
- `Tools/ReviewVerificationBaselines.ps1 -ListGoldenProfiles` validated required baseline keys, including all v24 production-batch counters, `min_v25_production_points=40`, `min_editor_preference_persistence_tests=1`, `min_viewport_toolbar_tests=1`, and the v27 `min_editor_preference_profile_tests`, `min_capture_preset_tests`, `min_vfx_emitter_profile_tests`, and `min_cooked_dependency_preview_tests` gates, printed available golden profiles, and recognized the committed default plus known adapter profile set.
- `Tools/ReviewProductionBatch.ps1` validated `Assets/Verification/V25ProductionBatch.dfollowups` as exactly forty uniquely named v25 points and wrote `Saved/Verification/v25_production_batch_review.json`.
- `Tools/ApproveVerificationBaseline.ps1 -DryRun` produces `Saved/Verification/baseline_approval.json` with hashes plus Git signature status, signer/key metadata, and optional `-RequireSignedHead` enforcement.
- `Tools/ApproveVerificationUpdate.ps1` produces `Saved/Verification/baseline_update_approval.json` with explicit update intent, Git signature status, and verification file hashes.
- `Tools/PackageDisparity.ps1 -Configuration Release -IncludeSymbols -CreateArchive` produced `dist/DISPARITY-Release`, `package_manifest.json`, symbols when PDBs are available, and a zip artifact.
- `Tools/IndexDisparitySymbols.ps1` produces `Symbols/symbols_manifest.json` with symbol-server publish plan metadata.
- `Tools/PublishDisparitySymbols.ps1` produces `dist/SymbolServer/symbol_publish_manifest.json`.
- `Tools/CreateDisparityInstaller.ps1 -CreateArchive` produces `dist/Installer/DISPARITY-SetupManifest.json`, `DISPARITY-BootstrapperPlan.json`, and an installer payload zip.
- `Tools/CreateDisparityBootstrapper.ps1` produces `dist/Installer/DISPARITY-InstallerBootstrapper.cmd`.
- `Tools/ReviewReleaseReadiness.ps1` produces `Saved/Release/release_readiness_manifest.json` and validates package, installer/bootstrapper, symbol, OBS, runtime schema, and v25 production batch artifacts.
- `Tools/SmokeTestDisparity.ps1 -ExecutablePath .\dist\DISPARITY-Release\DisparityGame.exe -ExerciseWindow` launched the packaged build, found/resized/exercised the window, and closed it cleanly.
- `Tools/VerifyDisparity.ps1` produced PASS packaged runtime reports, captures, golden comparisons, showcase/trailer/high-res/VFX/beat/audio-analysis/Shot-Director/async-IO/animation-skinning/render-graph-callback/GPU-pick-histogram/cooked-package/nested-prefab/audio-production/viewport-overlay/high-resolve/viewport-HUD/transform-precision/command-history/schema/shot-sequencer/VFX-profile/VFX-emitter/cooked-GPU/dependency-invalidation/cooked-dependency-preview/audio-calibration/release-readiness/v25=40/prefs=1/toolbar=1/profiles=1/capture-presets=1 coverage, and local performance history rows for all six suites against `dist\DISPARITY-Release\DisparityGame.exe`.
- `Tools/SummarizePerformanceHistory.ps1 -BaselinePath Assets/Verification/PerformanceBaselines.dperf` completed successfully as part of `Tools/VerifyDisparity.ps1`, including previous-run and recent-median CPU/GPU regression reporting.
- The v27 pass was verified with `Tools/VerifyDisparity.ps1`; run `git status --short` and review the signed commit output before pushing.

After feature work, run `powershell -ExecutionPolicy Bypass -File .\Tools\VerifyDisparity.ps1` and `git status --short` before declaring the repo healthy.

## Likely Next Followups

Good next steps for making the engine more modern and AAA-like:

- Extend named editor preference profiles with import/export, profile diffing, dock-layout persistence, conflict-safe schema migration, and per-project defaults.
- Upgrade the viewport toolbar with icon buttons, hotkey hints, capture progress, toolbar customization, and saved workspace presets for gameplay, editor, and trailer capture.
- Replace the remaining high-resolution proof with true tiled offscreen rendering, per-tile camera jitter, selectable resolves, real EXR output, and async compression workers backed by the v27 capture preset surface.
- Expand v6 Shot Director and v25 sequencer readiness into a real sequencer with curve editing, clip lanes, shot thumbnails, nested sequences, clip blending, undoable edits, and keyboard-driven preview.
- Promote the VFX runtime/readiness profiles into a dedicated renderer with soft particles, GPU buffers, emitter sorting controls, motion vectors, temporal VFX reprojection, and debug visualizers.
- Load `DSGLBPK2` packages as optimized GPU mesh/material/animation resources, turn dependency previews into live hot-reload invalidation, rollback on failed reload, add retargeting, and upload real GPU skinning palettes.
- Promote prefab variant/parent/nested metadata into nested prefab instancing, multi-object override diffing, dependency-aware recursive apply/revert, and undo grouping.
- Replace the audio production/calibration counters with real XAudio2 mixer voices, streamed music assets, spatial emitter components, attenuation-curve assets, calibrated meters, and content-driven VFX pulses.
- Replace release-readiness/bootstrapper/symbol-server/OBS plans with actual publishing/install artifacts, run packaged runtime smoke tests on interactive CI runners, and expand OBS metadata into OBS WebSocket scene/profile automation.
