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
- Editor/runtime v4 includes docking, multi-viewport panels, undo/redo, selection outlines, copy/paste/duplicate/delete scene-object workflows, simple transform gizmo buttons, prefab apply/save workflows, visibly stronger post effects with debug views, bus-based audio controls, spatial tone preview, an asset database, render graph scaffold, job system, version reporting, crash logs, and CI/package verification scripts.

## Important Paths

- `DisparityEngine/Source/Disparity/Disparity.h`: public umbrella include.
- `DisparityEngine/Source/Disparity/Core/`: application loop, input, time, logging, crash handler, version surface, layer hook.
- `DisparityEngine/Source/Disparity/Rendering/Renderer.*`: DirectX 11 renderer, GPU resources, shader setup, draw calls, HDR post-processing.
- `DisparityEngine/Source/Disparity/Rendering/RenderGraph.*`: small render graph scaffold for explicit future passes/resources.
- `DisparityEngine/Source/Disparity/Assets/`: asset database, material assets, simple JSON, glTF loading, asset hot reload, and tiny prefab IO.
- `DisparityEngine/Source/Disparity/ECS/`: small entity/component registry.
- `DisparityEngine/Source/Disparity/Diagnostics/`: frame profiler.
- `DisparityEngine/Source/Disparity/Editor/`: Dear ImGui wrapper plus legacy title overlay helper.
- `DisparityEngine/Source/Disparity/Animation/`: keyframe and bob animation helpers.
- `DisparityEngine/Source/Disparity/Audio/`: basic Windows audio hooks.
- `DisparityEngine/Source/Disparity/Scripting/`: tiny scene script runner.
- `DisparityEngine/Source/Disparity/Runtime/`: job system scaffold.
- `DisparityEngine/Source/Disparity/Scene/`: camera, material, mesh, procedural primitive factory, scene object types.
- `DisparityGame/Source/DisparityGame.cpp`: first third-person walking prototype.
- `Assets/Shaders/Basic.hlsl`: runtime-compiled scene shader.
- `Assets/Shaders/PostProcess.hlsl`: runtime-compiled HDR tone-mapping pass.
- `Assets/Scenes/Prototype.dscene`: serialized prototype scene.
- `Assets/Scripts/Prototype.dscript`: tiny prototype scene script.
- `Assets/Prefabs/Beacon.dprefab`: tiny prefab used by the script.
- `Assets/Meshes/SampleTriangle.gltf`: embedded-buffer sample glTF mesh for loader validation.
- `Tools/PackageDisparity.ps1`: Release/Debug packaging helper.
- `Tools/SmokeTestDisparity.ps1`: launch-and-close runtime smoke helper.
- `.github/workflows/windows-build.yml`: GitHub Actions Debug/Release build, shader validation, and package validation.
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

## Coding Conventions

- Keep the project native VS `.sln`/`.vcxproj` unless the user explicitly asks for CMake.
- Keep C++20, x64 Debug/Release, warning level 4.
- Prefer small, readable engine interfaces over heavy abstractions at this stage.
- Keep DirectX 11 implementation details inside `DisparityEngine`.
- Use DirectXMath for math until a deliberate math-library decision is made.
- Dear ImGui docking branch is intentionally vendored; do not add more external dependencies without user approval.
- Use `apply_patch` for manual edits.

## Verified Baseline

On 2026-04-26:

- `Debug|x64` built successfully with 0 warnings and 0 errors.
- `Release|x64` built successfully with 0 warnings and 0 errors.
- `Assets/Shaders/Basic.hlsl` and `Assets/Shaders/PostProcess.hlsl` compiled successfully for `VSMain` and `PSMain` with `fxc`.
- `Tools/SmokeTestDisparity.ps1 -Configuration Debug` launched `DisparityGame.exe`, kept it alive for 3 seconds, and closed it cleanly.
- `Tools/PackageDisparity.ps1 -Configuration Release` produced `dist/DISPARITY-Release`.
- `Tools/SmokeTestDisparity.ps1 -ExecutablePath .\dist\DISPARITY-Release\DisparityGame.exe` launched the packaged build for 3 seconds and closed it cleanly.

After feature work, re-run Debug/Release builds, shader compiles for both HLSL files, runtime smoke, package script, packaged runtime smoke, and `git status --short` before declaring the repo healthy.

## Likely Next Followups

Good next steps for making the engine more modern and AAA-like:

- Integrate the render graph scaffold into the live DX11 renderer with explicit resources, pass lifetime tracking, and GPU profiling.
- Add a dedicated editor viewport with object picking, independent editor camera controls, selection outlines driven by object IDs, and real 3D gizmo handles.
- Upgrade serialization to stable deterministic IDs, schema versions, undo grouping, prefab variants, dependency-aware apply/revert, and save-game separation.
- Replace prototype glTF runtime loading with cooked `.glb` import, animation blending, retargeting, and GPU skinning palette upload.
- Replace WinMM with XAudio2 or another production backend for voices, sends, snapshots, streamed music, spatial emitters, listener orientation, attenuation curves, and meters.
- Add symbols handling, installer-style packaging, crash upload plumbing, and packaged smoke tests in an environment with an interactive desktop.
