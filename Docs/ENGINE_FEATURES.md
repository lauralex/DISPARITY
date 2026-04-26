# DISPARITY Engine Feature Guide

This document is the practical test map for the current Visual Studio 2022 C++20/DirectX 11 engine.

## Editor

- Press `F1` to show or hide the Dear ImGui editor.
- Drag panels into the main dockspace, or pull them outside the main window to test multi-viewport support.
- Use `Hierarchy` to select the player or a scene object.
- Selected objects render with a bright outline in the main viewport.
- Press `Tab` to release the mouse, then left-click the main viewport to pick objects. Hold `Ctrl` while picking or selecting in `Hierarchy` to multi-select.
- Use `Copy`, `Paste`, `Duplicate`, and `Delete` in the `Hierarchy` panel or `Edit` menu. Keyboard shortcuts are `Ctrl+C`, `Ctrl+V`, `Ctrl+D`, and `Delete`.
- The `Viewport` panel enables an editor camera. Use `Frame Selection`, `Frame Player`, right-drag look, arrow/Page keys, or right-drag plus `WASD`/`Q`/`E` to inspect the scene without moving the player.
- Use `Inspector` to edit transforms/materials. The `Transform Gizmo` section provides small move, scale, and yaw buttons without adding another third-party dependency, and the viewport draws camera-scaled 3D gizmo handles at the current selection pivot. Choose translate/rotate/scale and world/local space in `Viewport`, then left-drag an axis marker, rotate ring, or translucent translate-plane handle; hold `Shift` while dragging for snapping.
- Use `Ctrl+Z` and `Ctrl+Y`, or the `DISPARITY` menu, to test undo/redo. The profiler shows recent command labels.

## Assets And Prefabs

- Edit `Assets/Scenes/Prototype.dscene`, `Assets/Scripts/Prototype.dscript`, `Assets/Prefabs/Beacon.dprefab`, or the player material files while the game is running.
- Keep `Hot reload assets` enabled in the `Assets` panel and wait about half a second, or press `F5` to force scene/script reload.
- Select a scene object, then use `Apply Selection To Beacon Prefab` to write that object shape/material back to `Assets/Prefabs/Beacon.dprefab`.
- Select a scene object, then use `Save Selection As Runtime Prefab` to write a prefab snapshot under `Saved`.
- Use `Rescan Database` to rebuild the in-editor asset database. It classifies assets, shows dirty cooked state, and tracks glTF buffer/image references, material texture references, and script prefab references.
- Use `Cook Dirty Assets` to write metadata cache files under `Saved/CookedAssets` through the engine job system.
- Use `Export glTF Materials` to convert loaded glTF material metadata into `.dmat` material assets under `Assets/Materials/Imported`.
- Import settings use `.dimport` files. `Assets/ImportSettings/Assets/Meshes/SampleTriangle.gltf.dimport` is the sample mesh import settings file.

## glTF

- `Assets/Meshes/SampleTriangle.gltf` is loaded at startup.
- The loader creates renderer meshes for every primitive, tracks primitive material indices, binds base-color textures when present, instantiates mesh nodes into the scene, reads skin joint/weight attributes, stores inverse bind matrices, and parses animation sampler keyframes.
- The `Assets` panel shows mesh, primitive, material, node, skin, and animation counts.
- `glTF animation playback` previews parsed animation channels when an imported asset includes them.

## Rendering

- The renderer remains DirectX 11 and shader-model 5.
- The `Renderer` panel exposes VSync, tone mapping, shadow maps, broader CSM-style shadow coverage, clustered light contribution, bloom, SSAO, anti-aliasing, temporal AA, exposure, shadow strength, bloom threshold/strength, SSAO strength, AA strength, temporal blend, saturation, contrast, and post debug views.
- The scene uses a directional light, shadow map, and four point lights.
- Post-processing uses the HDR scene texture, temporal history texture, and depth SRV.
- The `Profiler` panel shows frame timings, job worker count, renderer draw-call counters, GPU frame timing once DX11 timestamp queries warm up, and compiled render-graph schedule/resource lifetime diagnostics. The render graph view also reports per-pass GPU timings, culled passes, read/write transition diagnostics, and alias candidates.
- If bloom/SSAO/AA differences are hard to spot in the final image, use `Renderer > Post debug`: `Bloom` isolates bright bleed, `SSAO mask` shows contact darkening, `AA edges` shows the edge detector, and `Depth` visualizes the depth buffer.
- Bloom became more visible because the scene shader no longer clamps lighting before the HDR post pass.

## Audio

- The `Audio Mixer` panel exposes Master/SFX/UI/Music/Ambience buses.
- Use `Low Tone`, `High Tone`, and `Spatial` to test generated bus-routed tones.
- `F2` plays the UI notification tone.
- `AudioSystem::StreamMusic` and `PlayWaveFileAsync` provide WinMM-backed async/looped wave playback hooks for future content.

## Packaging

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\PackageDisparity.ps1 -Configuration Release
```

The packaged build is written to `dist/DISPARITY-Release`.

## Verification

Use the full local gate before calling the repository healthy:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\VerifyDisparity.ps1
```

The verification script runs `git diff --check`, warning-free Debug and Release builds, MSVC static analysis, all shader entry-point compiles, Debug window smoke, Debug runtime self-verification, Release packaging, packaged window smoke, and packaged runtime self-verification.

Targeted checks are still useful while iterating:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\SmokeTestDisparity.ps1 -Configuration Debug -ExerciseWindow
powershell -ExecutionPolicy Bypass -File .\Tools\RuntimeVerifyDisparity.ps1 -Configuration Debug -Frames 90
.\bin\x64\Debug\DisparityGame.exe --verify-runtime --verify-frames=90
```

Runtime verification writes `Saved/Verification/runtime_verify.txt` and exits non-zero if a runtime invariant fails. Crashes write a small report to `Saved/CrashLogs`. GitHub Actions runs the static side of `Tools/VerifyDisparity.ps1`; interactive runtime smoke remains a local gate.
