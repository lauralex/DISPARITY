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
- `F7`: toggle cinematic showcase mode, hide the editor, boost post-processing, and orbit the animated DISPARITY rift for capture-friendly footage.
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
- `Audio Mixer`: adjust master/bus volumes, mute buses, play generated UI/SFX/spatial test tones, inspect bus sends/meters, and store/recall a mixer snapshot.

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

- Runtime verification now drives deterministic player/camera input playback and records playback path distance/step counts in `Saved/Verification/runtime_verify.txt`.
- The DX11 renderer can capture the post-editor back buffer to a portable PPM file and reports capture dimensions, luminance, nonblack pixels, bright pixels, and an RGB checksum.
- Runtime verification validates `Saved/Verification/runtime_capture.ppm` so the gate catches blank or invalid frames instead of only checking draw-call counters.
- Runtime verification tracks CPU frame time, GPU frame time when timestamp queries are available, and render-graph pass CPU/GPU maxima against configurable budgets.
- GitHub Actions includes an opt-in `workflow_dispatch` runtime verification step for machines with an interactive desktop.

## Engine v12 Followups Implemented

- Deterministic runtime playback now lives in `Assets/Verification/Prototype.dreplay` instead of hardcoded C++ frame ranges.
- Runtime image/performance expectations now live in `Assets/Verification/RuntimeBaseline.dverify`.
- Runtime verification compares capture dimensions, average luminance tolerance, nonblack ratio, replay path distance, and CPU/GPU/pass budgets against the baseline asset.
- `RuntimeVerifyDisparity.ps1` appends `Saved/Verification/performance_history.csv` so local verification runs produce a trendable performance/capture history.
- `VerifyDisparity.ps1` exposes replay and baseline path overrides for future scenario-specific verification suites.

## Engine v13 Followups Implemented

- `Assets/Verification/RuntimeSuites.dverify` now drives multiple runtime suites through the unified verifier.
- `CameraSweep` adds a second deterministic replay/baseline path focused on camera movement and render stability.
- Runtime verification compares captured frames against suite-specific 64x36 golden PPM thumbnails under `Assets/Verification/Goldens`.
- `Tools/CompareCaptureDisparity.ps1` can update goldens or fail on mean-delta/bad-pixel-ratio drift.
- `Tools/SummarizePerformanceHistory.ps1` reports recent CPU/GPU deltas by suite and executable.
- `VerifyDisparity.ps1` now runs all suites for Debug and packaged builds, then prints the local performance trend summary.

## Engine v14 Followups Implemented

- Editor picking now uses viewport-owned screen mapping instead of raw full-window assumptions.
- Scene selection uses OBB ray tests and stable pick IDs, so the editor reports deterministic object tokens instead of only names.
- Gizmo axes and planes now expose hover state and brighten when hovered or dragged.
- Runtime verification validates editor precision with stable-ID object picks and gizmo handle picks.
- `EditorPrecision` adds a third replay/baseline/golden suite focused on editor interaction invariants.
- Performance history rows now include editor pick and gizmo pick counts.

## Engine v15 Followups Implemented

- Runtime verification now records scenario coverage counters for scene reloads, runtime scene saves, post-debug view cycling, gizmo transform constraints, and audio snapshot validation.
- The suite manifest now covers `Prototype`, `CameraSweep`, `EditorPrecision`, `PostDebug`, `AssetReload`, and `GizmoDrag`.
- Golden comparisons support tolerance profiles under `Assets/Verification/GoldenProfiles` and write diff thumbnails for faster visual debugging.
- Performance summaries can compare recent runs against committed suite baselines in `Assets/Verification/PerformanceBaselines.dperf`.
- `Tools/CookDisparityAssets.ps1` writes deterministic cooked metadata manifests under `Saved/CookedAssets`.
- `Tools/PackageDisparity.ps1` now writes a package manifest with file hashes, can include PDB symbols, and can create a zip artifact.
- `Tools/CollectCrashReports.ps1` prepares crash upload manifests and optional local bundles.
- Scene saving now emits schema v3 metadata with deterministic ID and save-game separation flags.
- The current WinMM prototype audio layer gained a production-facing backend name, listener orientation, bus sends, peak/RMS meters, and mixer snapshots.

## Engine v16 Followups Implemented

- `RenderGraph` now assigns transient resources to physical allocation slots, marks alias reuse, and exposes allocation diagnostics alongside existing schedules/lifetimes/barriers.
- The DX11 renderer records submitted pass order against the compiled graph and runtime verification fails if the renderer skips or reorders compiled passes.
- The renderer owns dedicated editor GPU targets for viewport color, object IDs, and object depth, laying the groundwork for GPU picking and editor-only compositing.
- Runtime verification baselines now require render-graph allocation/alias counts and ready editor viewport/object-ID/depth resources.
- Runtime reports and performance history now include graph allocation, aliasing, dispatch-valid, editor target, and XAudio2 availability metrics.
- `RuntimeVerifyDisparity.ps1` detects the primary display adapter and can use adapter-specific golden tolerance profiles when present.
- `CookDisparityAssets.ps1 -BinaryPackages` now writes deterministic `.dassetbin` cooked packages next to metadata records.
- Production tooling now includes verification baseline review, symbol indexing, installer payload manifest generation, and crash upload dry-run transport.
- The audio layer detects XAudio2 runtime availability and exposes a preference switch while keeping the stable WinMM render path for v16.

## Engine v17 Followups Implemented

- The prototype scene now has an always-visible procedural DISPARITY rift built from engine-owned torus/cube meshes, with animated rings, orbiting shards, dark spires, HDR materials, and eight point lights.
- `F7` toggles a cinematic showcase camera that hides the editor, boosts bloom/SSAO/AA/color grading, orbits the rift, and plays a short spatial cue so the build has an immediate recording mode.
- Runtime verification now exercises showcase mode, records `showcase_frames`, restores renderer settings before capture, and uses deterministic rift timing during verification for repeatable thumbnails.
- All runtime golden thumbnails and luminance baselines were refreshed for the new showpiece scene.

More detail lives in `Docs/ENGINE_FEATURES.md` and `Docs/ROADMAP.md`.

## Future Followups

- Execute all renderer passes through graph-owned callbacks and bind resources from graph allocation handles instead of renderer member variables.
- Populate the editor object-ID/depth targets with real scene-object and gizmo-handle IDs, then switch viewport picking to GPU readback.
- Replace `.dassetbin` source bundles with true cooked mesh/material/animation payloads, dependency invalidation, and runtime streaming.
- Add prefab variants, nested prefabs, override diffing, and dependency-aware apply/revert with undo grouping.
- Replace the WinMM playback path with XAudio2 voices, sends, streamed music, spatial emitters, attenuation curves, and live meters.
- Add real installer bootstrapper output, symbol-server indexing, crash upload authentication/retry, and packaged runtime smoke on interactive CI runners.
- Add a trailer/photo mode with timeline camera splines, depth-of-field, title-safe overlays, and one-click high-resolution frame capture.
- Give the rift authored VFX controls: particle ribbons, volumetric fog cards, lightning arcs, lens dirt, and beat-synced audio-reactive pulses.
