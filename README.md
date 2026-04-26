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
- When the mouse is released with `Tab`, left-click the viewport to pick objects. Hold `Ctrl` while clicking or selecting in the hierarchy to multi-select.
- In editor-camera mode, right-drag to look around. While right-dragging, use `WASD` to fly, `Q`/`E` to descend/ascend, and `Shift` for faster movement.
- Left-drag the colored X/Y/Z handles, rotate rings, or translucent translate-plane handles at the selection pivot depending on the `Gizmo mode`; hold `Shift` while dragging to snap.

Editor UI:

- `F1`: show/hide Dear ImGui editor panels.
- Dock panels into the main viewport, or drag panels outside the main window with ImGui multi-viewport enabled.
- `Hierarchy`: select the player or scene entities, then copy/paste/duplicate/delete scene objects.
- `Viewport`: enable the independent editor camera, frame the player/selection, choose gizmo translate/rotate/scale and world/local space, and use right-drag plus WASD/QE to move without driving gameplay input.
- `Inspector`: edit transforms/materials and use simple transform gizmo buttons; selected objects also draw draggable, camera-scaled 3D axis/ring/plane gizmo handles in the viewport.
- `Assets`: reload scene/script, toggle hot reload, inspect the asset database, cook dirty metadata caches, export glTF materials, inspect glTF metadata, and save/apply prefabs.
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

## Engine v5 Followups Implemented

- Renderer publishes a live render-graph snapshot and draw-call counters to the profiler panel.
- The main viewport supports click-to-select object picking and Ctrl multi-selection.
- The editor has an independent orbit/pan camera that can frame the player or selected object without moving the player.
- Undo/redo now carries command labels, and the profiler shows recent command history.
- Scene files are versioned as v2 with stable object IDs while still loading older v1 scene lines.
- `AssetDatabase` recognizes import settings, cooks dirty assets into metadata cache files through the job system, and exposes a glTF material export workflow.

## Engine v6 Followups Implemented

- `RenderGraph` now compiles pass dependencies into an execution schedule instead of only listing submitted passes.
- Render graph diagnostics include pass CPU timings, resource lifetime ranges, validation errors, and scheduled pass order in the profiler.
- The DX11 renderer records a GPU frame timestamp query and reports the most recent GPU frame time after the query warms up.
- Selected objects and the player draw colored 3D transform handles at the active selection pivot.

## Engine v7 Followups Implemented

- `RenderGraph` now tracks disabled/culled passes, read/write transition diagnostics, and alias candidates for non-overlapping internal resources.
- The profiler shows CPU and GPU time per scheduled graph pass, plus culled passes, resource transitions, lifetimes, alias opportunities, and validation.
- The shadow-map graph pass is now culled when shadows are disabled, so render-graph diagnostics reflect renderer settings instead of only submitted work.
- The visible X/Y/Z transform handles are interactive: left-drag a handle to move the player, selected object, or multi-selection, with `Shift` snapping.

## Engine v8 Followups Implemented

- Editor-camera navigation now supports right-drag fly controls: `WASD` for planar movement, `Q`/`E` for vertical movement, and `Shift` for faster movement.
- The visible 3D gizmo now has selectable translate, rotate, and scale modes.
- Gizmo axes can operate in world or local space; local space follows the selected player yaw or the first selected object rotation.
- Gizmo drags support undo labels for translate/rotate/scale operations and still work across multi-selection.

## Engine v9 Followups Implemented

- Procedural torus generation now backs visible rotate rings instead of cube-only rotate markers.
- 3D gizmo handles scale with camera distance, making them easier to hit from both close and wide editor views.
- Translate mode adds translucent XY/XZ/YZ planar drag handles for moving selected scene objects on two axes at once.
- Gizmo picking, drag status, snapping, and undo labels now understand both axis handles and plane handles.

## Engine v10 Followups Implemented

- The application can now return explicit non-zero exit codes from runtime self-verification failures.
- `DisparityGame.exe --verify-runtime --verify-frames=N` runs deterministic runtime checks, exercises scene reload, runtime scene saving, renderer setting toggles, selection changes, draw-call counters, and render-graph validation, then writes `Saved/Verification/runtime_verify.txt`.
- `Tools/SmokeTestDisparity.ps1` now waits for the actual DISPARITY window, can resize it, and can send basic editor hotkeys before closing.
- `Tools/RuntimeVerifyDisparity.ps1` and `Tools/VerifyDisparity.ps1` provide repeatable local static/runtime verification, including warning-free Debug/Release builds, optional MSVC static analysis, shader compilation, window smoke, runtime self-test, packaging, and packaged runtime self-test.
- GitHub Actions now uses the unified verification script for static CI coverage.

## Engine v11 Followups Implemented

- Runtime verification now drives deterministic player/camera input playback and records playback distance/step counts in `Saved/Verification/runtime_verify.txt`.
- The DX11 renderer can capture the post-editor back buffer to a portable PPM file and reports capture dimensions, luminance, nonblack pixels, bright pixels, and an RGB checksum.
- Runtime verification validates `Saved/Verification/runtime_capture.ppm` so the gate catches blank or invalid frames instead of only checking draw-call counters.
- Runtime verification tracks CPU frame time, GPU frame time when timestamp queries are available, and render-graph pass CPU/GPU maxima against configurable budgets.
- GitHub Actions includes an opt-in `workflow_dispatch` runtime verification step for machines with an interactive desktop.

More detail lives in `Docs/ENGINE_FEATURES.md` and `Docs/ROADMAP.md`.

## Future Followups

- Turn the scheduled render graph into the authoritative renderer execution path with real DX11 resource lifetime ownership, alias allocation decisions, async compute candidates, and GPU-driven culling.
- Add a dedicated editor viewport render target, object-ID selection buffers, depth-aware gizmo handle picking, and stronger transform constraints.
- Add prefab variants, nested prefabs, dependency-aware apply/revert, and save-game separation.
- Add production `.glb` cooking, animation retargeting/blending, and GPU skinning palettes.
- Replace the WinMM prototype audio backend with XAudio2 voices, sends, snapshots, streamed layers, spatial emitters, and meters.
- Add golden-image comparison thresholds, deterministic input script files, and historical performance trend tracking on top of the v11 verifier.
