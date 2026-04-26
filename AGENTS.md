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
- Geometry includes procedural primitives, procedural terrain, and a glTF 2.0 scene loader path for static mesh primitives, material texture binding, node instancing, skin metadata, joint/weight attributes, and animation sampler data.
- Editor/runtime v14 includes docking, multi-viewport panels, undo/redo with command labels, selection outlines, viewport click-picking, Ctrl multi-select, stable-ID/OBB editor picking, gizmo hover highlighting, copy/paste/duplicate/delete scene-object workflows, an independent editor camera with right-drag WASD/QE fly controls, simple transform gizmo buttons plus camera-scaled 3D translate/rotate/scale handles, torus rotate rings, translucent XY/XZ/YZ translate-plane handles, snapping and world/local space, prefab apply/save workflows, visibly stronger post effects with debug views, bus-based audio controls, spatial tone preview, an asset database with import settings and cooked metadata files, a compiled render graph schedule with pass CPU/GPU timings, culled passes, transition diagnostics, alias candidates, resource lifetimes, job system, version reporting, crash logs, deterministic runtime self-verification, multi-suite assetized replay/baseline verification, frame capture validation, golden thumbnail comparison, editor precision verification, performance trend summaries, stronger window smoke tests, and unified static/runtime verification scripts.

## Important Paths

- `DisparityEngine/Source/Disparity/Disparity.h`: public umbrella include.
- `DisparityEngine/Source/Disparity/Core/`: application loop, input, time, logging, crash handler, version surface, layer hook.
- `DisparityEngine/Source/Disparity/Rendering/Renderer.*`: DirectX 11 renderer, GPU resources, shader setup, draw calls, HDR post-processing, GPU timing, and PPM frame capture/readback.
- `DisparityEngine/Source/Disparity/Rendering/RenderGraph.*`: compiled render graph scaffold with pass dependency scheduling, culled pass tracking, transition diagnostics, alias candidates, resource lifetimes, and timing diagnostics.
- `DisparityEngine/Source/Disparity/Assets/`: asset database, material assets, simple JSON, glTF loading, asset hot reload, and tiny prefab IO.
- `DisparityEngine/Source/Disparity/ECS/`: small entity/component registry.
- `DisparityEngine/Source/Disparity/Diagnostics/`: frame profiler.
- `DisparityEngine/Source/Disparity/Editor/`: Dear ImGui wrapper plus legacy title overlay helper.
- `DisparityEngine/Source/Disparity/Animation/`: keyframe and bob animation helpers.
- `DisparityEngine/Source/Disparity/Audio/`: basic Windows audio hooks.
- `DisparityEngine/Source/Disparity/Scripting/`: tiny scene script runner.
- `DisparityEngine/Source/Disparity/Runtime/`: job system scaffold.
- `DisparityEngine/Source/Disparity/Scene/`: camera, material, mesh, procedural primitive factory including cube/plane/terrain/torus helpers, scene object types.
- `DisparityGame/Source/DisparityGame.cpp`: first third-person walking prototype.
- `Assets/Shaders/Basic.hlsl`: runtime-compiled scene shader.
- `Assets/Shaders/PostProcess.hlsl`: runtime-compiled HDR tone-mapping pass.
- `Assets/Scenes/Prototype.dscene`: serialized prototype scene.
- `Assets/Verification/RuntimeSuites.dverify`: runtime verification suite manifest.
- `Assets/Verification/Prototype.dreplay`: deterministic runtime verification replay asset.
- `Assets/Verification/CameraSweep.dreplay`: camera-heavy deterministic runtime verification replay asset.
- `Assets/Verification/EditorPrecision.dreplay`: editor stable-ID picking and gizmo picking runtime verification replay asset.
- `Assets/Verification/RuntimeBaseline.dverify`: runtime capture/performance baseline asset.
- `Assets/Verification/CameraSweepBaseline.dverify`: camera-heavy runtime capture/performance baseline asset.
- `Assets/Verification/EditorPrecisionBaseline.dverify`: editor precision runtime capture/performance baseline asset.
- `Assets/Verification/Goldens/*.ppm`: suite-specific golden thumbnails used by runtime verification.
- `Assets/ImportSettings/Assets/Meshes/SampleTriangle.gltf.dimport`: sample import settings file for the asset database.
- `Assets/Scripts/Prototype.dscript`: tiny prototype scene script.
- `Assets/Prefabs/Beacon.dprefab`: tiny prefab used by the script.
- `Assets/Meshes/SampleTriangle.gltf`: embedded-buffer sample glTF mesh for loader validation.
- `Tools/PackageDisparity.ps1`: Release/Debug packaging helper.
- `Tools/SmokeTestDisparity.ps1`: launch, window-detect, resize/hotkey, and close runtime smoke helper.
- `Tools/RuntimeVerifyDisparity.ps1`: launches `DisparityGame.exe --verify-runtime`, waits for the runtime PASS/FAIL report, verifies capture output, and fails on non-zero exit.
- `Tools/VerifyDisparity.ps1`: full local verification gate for static and runtime checks.
- `Tools/CompareCaptureDisparity.ps1`: downsamples PPM captures and compares or updates golden thumbnails.
- `Tools/SummarizePerformanceHistory.ps1`: prints recent CPU/GPU and editor/gizmo pick trend summaries grouped by suite/executable.
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

It writes `Saved/Verification/runtime_verify.txt`, captures `Saved/Verification/runtime_capture.ppm`, loads a `.dreplay` asset, compares against a `.dverify` baseline, exercises deterministic player/camera input playback, scene reload, runtime scene save, renderer setting toggles, selection cycling, draw-call counters, render-graph validation, image stats, and CPU/GPU/pass performance budgets, then exits with code `0` for PASS or `20` for FAIL. `Tools/RuntimeVerifyDisparity.ps1` also compares the capture against the baseline's golden thumbnail unless `-DisableGoldenComparison` is used.

## Current Controls

- `WASD`: move the player placeholder.
- Mouse: orbit the third-person camera while captured.
- `Tab`: release or recapture the mouse.
- `Esc`: quit.
- `F1`: toggle Dear ImGui editor panels.
- `F2`: play a short generated WinMM audio test tone and show status in the overlay.
- `F3`: cycle selected scene object in the overlay.
- `F5`: reload scene and script; this is only visually obvious after the asset files are edited while the game is running.
- `F6`: save runtime scene snapshot to `Saved/PrototypeRuntime.dscene`.
- `Ctrl+Z` / `Ctrl+Y`: undo and redo editor-side scene/player/renderer edits.
- `Ctrl+C` / `Ctrl+V` / `Ctrl+D` / `Delete`: copy, paste, duplicate, or delete the selected scene object.
- When mouse capture is released with `Tab`, left-click the main viewport to pick objects. Hold `Ctrl` while picking or selecting in Hierarchy for multi-selection.
- The `Viewport` panel can enable the independent editor camera; right-drag plus `WASD`/`Q`/`E`, or arrow/Page keys, moves that camera without moving the player.
- Selected objects and the player draw colored, camera-scaled 3D transform handles at the active selection pivot. Choose translate/rotate/scale and world/local space in `Viewport`, then left-drag an X/Y/Z axis marker, rotate ring, or scene-object translate plane; hold `Shift` while dragging to snap.

## Coding Conventions

- Keep the project native VS `.sln`/`.vcxproj` unless the user explicitly asks for CMake.
- Keep C++20, x64 Debug/Release, warning level 4.
- Prefer small, readable engine interfaces over heavy abstractions at this stage.
- Keep DirectX 11 implementation details inside `DisparityEngine`.
- Use DirectXMath for math until a deliberate math-library decision is made.
- Dear ImGui docking branch is intentionally vendored; do not add more external dependencies without user approval.
- Use `apply_patch` for manual edits.

## Verified Baseline

On 2026-04-27 after the v14 verification followup pass:

- `Debug|x64` built successfully with 0 warnings and 0 errors.
- `Release|x64` built successfully with 0 warnings and 0 errors.
- MSVC static analysis completed successfully.
- `Assets/Shaders/Basic.hlsl` and `Assets/Shaders/PostProcess.hlsl` compiled successfully for `VSMain` and `PSMain` with `fxc`.
- `Tools/SmokeTestDisparity.ps1 -Configuration Debug -ExerciseWindow` launched `DisparityGame.exe`, found the window, resized it, sent basic editor hotkeys, kept it alive for 3 seconds, and closed it cleanly.
- `Tools/RuntimeVerifyDisparity.ps1 -Configuration Debug -Frames 90` produced PASS runtime reports for the `Prototype`, `CameraSweep`, and `EditorPrecision` suites, wrote captures/thumbnails, compared them against `Assets/Verification/Goldens/*.ppm`, validated stable-ID editor picks and gizmo picks, loaded the replay/baseline assets, and appended `Saved/Verification/performance_history.csv`.
- `Tools/PackageDisparity.ps1 -Configuration Release` produced `dist/DISPARITY-Release`.
- `Tools/SmokeTestDisparity.ps1 -ExecutablePath .\dist\DISPARITY-Release\DisparityGame.exe -ExerciseWindow` launched the packaged build, found/resized/exercised the window, and closed it cleanly.
- `Tools/RuntimeVerifyDisparity.ps1 -ExecutablePath .\dist\DISPARITY-Release\DisparityGame.exe -Frames 90` produced PASS packaged runtime reports, captures, golden comparisons, and local performance history rows for all three suites.
- `Tools/SummarizePerformanceHistory.ps1` completed successfully as part of `Tools/VerifyDisparity.ps1`.
- The v14 pass was committed only after `Tools/VerifyDisparity.ps1` and `git status --short` review.

After feature work, run `powershell -ExecutionPolicy Bypass -File .\Tools\VerifyDisparity.ps1` and `git status --short` before declaring the repo healthy.

## Likely Next Followups

Good next steps for making the engine more modern and AAA-like:

- Move renderer execution onto the compiled render graph with real DX11 resource ownership, alias allocation decisions, async compute candidates, and GPU-driven culling.
- Add a dedicated editor viewport render target, GPU object-ID selection buffer, GPU object-ID gizmo handle picking, depth-aware hover states, and stronger transform constraints.
- Expand the new multi-suite replay/baseline/golden harness into editor camera, gizmo dragging, asset reload, and post-debug scenario coverage.
- Add per-GPU/driver golden tolerance profiles and commit-to-commit performance regression baselines.
- Upgrade serialization to stable deterministic IDs, schema versions, undo grouping, prefab variants, dependency-aware apply/revert, and save-game separation.
- Replace prototype glTF runtime loading and metadata cooking with true cooked `.glb` import, animation blending, retargeting, and GPU skinning palette upload.
- Replace WinMM with XAudio2 or another production backend for voices, sends, snapshots, streamed music, spatial emitters, listener orientation, attenuation curves, and meters.
- Add symbols handling, installer-style packaging, crash upload plumbing, and packaged smoke tests in an environment with an interactive desktop.
