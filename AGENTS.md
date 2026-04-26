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
- Editor/runtime v9 includes docking, multi-viewport panels, undo/redo with command labels, selection outlines, viewport click-picking, Ctrl multi-select, copy/paste/duplicate/delete scene-object workflows, an independent editor camera with right-drag WASD/QE fly controls, simple transform gizmo buttons plus camera-scaled 3D translate/rotate/scale handles, torus rotate rings, translucent XY/XZ/YZ translate-plane handles, snapping and world/local space, prefab apply/save workflows, visibly stronger post effects with debug views, bus-based audio controls, spatial tone preview, an asset database with import settings and cooked metadata files, a compiled render graph schedule with pass CPU/GPU timings, culled passes, transition diagnostics, alias candidates, resource lifetimes, job system, version reporting, crash logs, and CI/package verification scripts.

## Important Paths

- `DisparityEngine/Source/Disparity/Disparity.h`: public umbrella include.
- `DisparityEngine/Source/Disparity/Core/`: application loop, input, time, logging, crash handler, version surface, layer hook.
- `DisparityEngine/Source/Disparity/Rendering/Renderer.*`: DirectX 11 renderer, GPU resources, shader setup, draw calls, HDR post-processing.
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
- `Assets/ImportSettings/Assets/Meshes/SampleTriangle.gltf.dimport`: sample import settings file for the asset database.
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

On 2026-04-26 after the v9 screen-stable gizmo followup pass:

- `Debug|x64` built successfully with 0 warnings and 0 errors.
- `Release|x64` built successfully with 0 warnings and 0 errors.
- `Assets/Shaders/Basic.hlsl` and `Assets/Shaders/PostProcess.hlsl` compiled successfully for `VSMain` and `PSMain` with `fxc`.
- `Tools/SmokeTestDisparity.ps1 -Configuration Debug` launched `DisparityGame.exe`, kept it alive for 3 seconds, and closed it cleanly.
- `Tools/PackageDisparity.ps1 -Configuration Release` produced `dist/DISPARITY-Release`.
- `Tools/SmokeTestDisparity.ps1 -ExecutablePath .\dist\DISPARITY-Release\DisparityGame.exe` launched the packaged build for 3 seconds and closed it cleanly.
- The v9 pass was committed only after the above checks and `git status --short` review.

After feature work, re-run Debug/Release builds, shader compiles for both HLSL files, runtime smoke, package script, packaged runtime smoke, and `git status --short` before declaring the repo healthy.

## Likely Next Followups

Good next steps for making the engine more modern and AAA-like:

- Move renderer execution onto the compiled render graph with real DX11 resource ownership, alias allocation decisions, async compute candidates, and GPU-driven culling.
- Add a dedicated editor viewport render target, object-ID selection buffer, object-ID gizmo handle picking, depth-aware hover states, and stronger transform constraints.
- Upgrade serialization to stable deterministic IDs, schema versions, undo grouping, prefab variants, dependency-aware apply/revert, and save-game separation.
- Replace prototype glTF runtime loading and metadata cooking with true cooked `.glb` import, animation blending, retargeting, and GPU skinning palette upload.
- Replace WinMM with XAudio2 or another production backend for voices, sends, snapshots, streamed music, spatial emitters, listener orientation, attenuation curves, and meters.
- Add symbols handling, installer-style packaging, crash upload plumbing, and packaged smoke tests in an environment with an interactive desktop.
