# DISPARITY

DISPARITY is starting as a small native C++20/DirectX 11 engine and game prototype for Visual Studio 2022.

The repo is initialized as a Git repository. Dear ImGui is vendored in `ThirdParty/imgui` for the runtime editor UI.

## Build

1. Open `DISPARITY.sln` in Visual Studio 2022.
2. Select `Debug|x64` or `Release|x64`.
3. Build the solution.

Command line build:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" .\DISPARITY.sln /m /p:Configuration=Debug /p:Platform=x64
& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" .\DISPARITY.sln /m /p:Configuration=Release /p:Platform=x64
```

## Run

The game executable is written to:

```text
bin\x64\Debug\DisparityGame.exe
bin\x64\Release\DisparityGame.exe
```

Controls:

- `WASD`: move the player placeholder.
- Mouse: orbit the third-person camera.
- `Tab`: release or recapture the mouse.
- `Esc`: quit.
- `F1`: toggle the Dear ImGui editor.
- `F2`: play a short generated audio test tone and show status in the editor menu bar.
- `F3`: cycle the selected scene object.
- `F5`: reload the serialized scene and scene script; visible changes appear after editing `Assets/Scenes/Prototype.dscene` or `Assets/Scripts/Prototype.dscript`.
- `F6`: save the runtime scene snapshot to `Saved/PrototypeRuntime.dscene` and show status in the editor menu bar.
- `Ctrl+Z` / `Ctrl+Y`: undo and redo editor-side scene/player/renderer edits.
- `Ctrl+C` / `Ctrl+V` / `Ctrl+D` / `Delete`: copy, paste, duplicate, or delete the selected scene object.

Editor UI:

- `F1`: show/hide Dear ImGui editor panels.
- Dock panels into the main viewport, or drag panels outside the main window with ImGui multi-viewport enabled.
- `Hierarchy`: select the player or scene entities, then copy/paste/duplicate/delete scene objects.
- `Inspector`: edit transforms/materials and use simple transform gizmo buttons.
- `Assets`: reload scene/script, toggle hot reload, inspect the asset database, inspect glTF metadata, and save/apply prefabs.
- `Renderer`: toggle VSync, tone mapping, shadows, CSM coverage, clustered lights, bloom, SSAO, anti-aliasing, temporal AA, color grading, and post debug views.
- `Audio Mixer`: adjust master/bus volumes, mute buses, and play generated UI/SFX/spatial test tones.

## Engine v0 Features

- Native Win32 window and message loop.
- Fixed/update timing support.
- Keyboard and raw mouse input.
- DirectX 11 renderer with depth, shaders, procedural meshes, materials, and directional lighting.
- Third-person DISPARITY walking scene using procedural geometry only.

## Engine v1 Followups Implemented

- Lightweight editor/profiler overlay through the window title.
- Material asset files in `Assets/Materials`.
- Minimal glTF 2.0 static mesh import path with a sample embedded-buffer mesh.
- Texture loading hook through WIC and renderer texture handles.
- Scene serialization in `Assets/Scenes/Prototype.dscene`.
- Tiny scene scripting in `Assets/Scripts/Prototype.dscript`.
- Small ECS registry for named entities, transforms, and mesh renderers.
- Procedural terrain grid generation.
- Basic transform/bob animation helpers.
- Simple audio hook using Windows audio notification/wave playback.
- HDR scene render target with tone-mapping post-process shader.
- Alpha-blended contact shadows.
- Packaging script at `Tools/PackageDisparity.ps1`.

## Engine v2 Followups Implemented

- Dear ImGui editor panels using the Win32/DX11 backends.
- Renderer settings exposed in-editor: VSync, tone mapping, exposure, shadow maps, and shadow strength.
- Real shadow-map pass using a depth texture sampled by the main shader.
- glTF scene loader path that loads mesh primitives and parses material, texture, node, skin, and animation metadata.
- Asset hot reload for the prototype scene, script, prefab, and material files.
- Tiny prefab format in `Assets/Prefabs`.
- Prototype runtime rendering now uses ECS entities generated from the scene/prefab data.
- Audio mixer controls with generated tones and master volume.

## Engine v3 Followups Implemented

- Dear ImGui docking branch is vendored, with docking and multi-viewport editor windows enabled.
- Editor undo/redo covers scene object transforms/materials, player edits, and renderer setting changes.
- Inspector includes simple transform gizmo controls for move/scale/yaw edits without external gizmo dependencies.
- Prefab workflows can apply the selected scene object back to `Assets/Prefabs/Beacon.dprefab` or save runtime prefabs under `Saved`.
- glTF runtime path now creates all mesh primitive handles, binds parsed base-color textures, instantiates mesh nodes into the scene, stores skin inverse bind matrices, reads JOINTS_0/WEIGHTS_0, and parses animation sampler keyframes for playback.
- Renderer includes multiple point lights, a forward+/clustered-style light toggle, broader CSM-style shadow coverage, depth-SRV SSAO, bloom, and temporal history blending.
- Audio has named buses, bus volume/mute controls, generated tones routed through buses, streamed/looped wave hooks, and simple stereo spatial tone preview.

## Engine v4 Followups Implemented

- Post processing is now intentionally visible: scene lighting stays HDR before post, bloom has threshold/strength controls, SSAO is stronger around depth contacts, anti-aliasing has an FXAA-style edge resolve, and the renderer panel includes Bloom/SSAO/AA/Depth debug views.
- The editor draws a selected-object outline and supports copy, paste, duplicate, and delete from the Hierarchy panel, Edit menu, and keyboard shortcuts.
- `AssetDatabase` scans `Assets`, classifies source files, tracks glTF/script/material dependencies, reports dirty cooked outputs, and feeds hot reload.
- `RenderGraph` and `JobSystem` are added as small engine interfaces for future explicit render passes and async asset/runtime work.
- Production hygiene now includes GitHub Actions Debug/Release builds, shader validation, packaging validation, `Tools/SmokeTestDisparity.ps1`, version reporting, and crash report files under `Saved/CrashLogs`.

More detail lives in `Docs/ENGINE_FEATURES.md` and `Docs/ROADMAP.md`.

## Future Followups

- Integrate the render graph into the live DirectX 11 renderer and add GPU-driven culling.
- Add a true editor viewport with object picking, independent editor camera controls, and real 3D transform handles.
- Replace the tiny scene/prefab formats with stable IDs, versioned serialization, variants, and dependency-aware apply/revert.
- Add production `.glb` cooking, animation retargeting/blending, and GPU skinning palettes.
- Replace the WinMM prototype audio backend with XAudio2 voices, sends, snapshots, streamed layers, spatial emitters, and meters.
