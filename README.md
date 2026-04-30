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

- `WASD`: move the player placeholder. A connected XInput gamepad left stick also contributes to movement.
- Mouse: orbit the third-person camera.
- `Tab`: release or recapture the mouse.
- `Esc`: quit.
- `F1`: toggle the Dear ImGui editor.
- `F2`: play a short generated audio test tone through XAudio2 when available or the warmed WinMM fallback, and show status in the editor menu bar.
- `F3`: cycle the selected scene object.
- `F5`: reload the serialized scene and scene script; visible changes appear after editing `Assets/Scenes/Prototype.dscene` or `Assets/Scripts/Prototype.dscript`.
- `F6`: save the runtime scene snapshot to `Saved/PrototypeRuntime.dscene` and show status in the editor menu bar.
- `F7`: toggle cinematic showcase mode, hide the editor, boost post-processing, and orbit the animated DISPARITY rift for capture-friendly footage.
- `F8`: toggle trailer/photo mode with authored camera shots from `Assets/Cinematics/Showcase.dshot`, depth of field, lens dirt, and title-safe guide overlays.
- `F9`: capture the current presented frame and write a source PPM, source PNG, async 2x PPM photo, and schema v3 high-resolution capture manifest under `Saved/Captures`.
- `F10`: reset the public playable demo, returning rift shards, phase anchors, resonance gates, phase relays, collision obstacles, enemies, sentinels, menu/failure state, checkpoint/retry state, extraction state, animation state, and HUD objectives to their start state.
- Press `Space`, or gamepad `A`, to dash/vault through traversal barriers. Press `P`, or gamepad `Start`, to pause/resume the public demo flow.
- Hold `Shift` while using `WASD`, or hold gamepad `LB`, in the playable demo to sprint. Collect all six rift shards, align the three phase anchors, tune the two resonance gates, stabilize the three phase relays during overcharge windows, dash through traversal blockers, evade enemy pressure, then return to the extraction beacon to complete the loop.
- `Ctrl+Z` / `Ctrl+Y`: undo and redo editor-side scene/player/renderer edits.
- `Ctrl+C` / `Ctrl+V` / `Ctrl+D` / `Delete`: copy, paste, duplicate, or delete the selected scene object.
- When the mouse is released with `Tab`, left-click the viewport to pick objects. Hold `Ctrl` while clicking or selecting in the hierarchy to multi-select. The editor tries GPU object-ID readback first and falls back to CPU ray tests.
- In editor-camera mode, right-drag to look around. While right-dragging, use `WASD` to fly, `Q`/`E` to descend/ascend, and `Shift` for faster movement.
- Left-drag the colored X/Y/Z handles, rotate rings, or translucent translate-plane handles at the selection pivot depending on the `Gizmo mode`; hold `Shift` while dragging to snap.

Editor UI:

- `F1`: show/hide Dear ImGui editor panels.
- Dock panels into the main viewport, or drag panels outside the main window with ImGui multi-viewport enabled.
- `Hierarchy`: select the player or scene entities, then copy/paste/duplicate/delete scene objects.
- `Viewport`: enable the independent editor camera, frame the player/selection, choose gizmo translate/rotate/scale and world/local space, inspect the live GPU-pick/capture diagnostics HUD, save/load/reset/import/export/diff named preference profiles, apply gameplay/editor/trailer workspace presets, and use right-drag plus WASD/QE to move without driving gameplay input.
- `Inspector`: edit transforms/materials and use simple transform gizmo buttons; selected objects also draw draggable, camera-scaled 3D axis/ring/plane gizmo handles in the viewport.
- `Assets`: reload scene/script, toggle hot reload, inspect the asset database and dependency graph, cook dirty metadata caches, export glTF materials, inspect glTF metadata, and save/apply prefabs.
- `Shot Director`: edit, add, save, reload, capture, thumbnail, and preview-scrub v6 `.dshot` trailer keys without leaving the running editor.
- `Demo Director`: inspect the public vertical slice stage, objective distance, shard/anchor/gate/relay progress, checkpoint/retry/pressure/footstep/combo/collision/traversal/enemy/gamepad/audio/animation telemetry, recent gameplay events, and v30/v31/v32/v33 readiness while the demo is running.
- `Renderer`: toggle VSync, tone mapping, shadows, CSM coverage, clustered lights, bloom, SSAO, anti-aliasing, temporal AA, color grading, depth of field, lens dirt, cinematic overlays, and post debug views.
- `Audio Mixer`: adjust master/bus volumes, mute buses, play generated UI/SFX/spatial test tones, optionally enable cinematic cue tones, inspect bus sends/meters/production counters, and store/recall a mixer snapshot.
- `Engine Services`: inspect registered services, telemetry counters/events, config variables, runtime commands, editor panel/workspace diagnostics, public-demo event routes, and the live v49 production catalog table/reload/preview/execution/Action Director mutation surface from one live readiness panel.

## Engine v0 Features

- Native Win32 window and message loop.
- Fixed/update timing support.
- Keyboard and raw mouse input.
- DirectX 11 renderer with depth, shaders, procedural meshes, materials, and directional lighting.
- Third-person DISPARITY walking scene using procedural geometry only.

## Engine v49 Action Director Live Mutations Implemented

- Added `Assets/Verification/V49ActionMutation.dfollowups`, a twenty-four-point batch that turns Action Director plans into live engine/editor/game mutation state.
- `ProductionAssetRuntimeCatalog` now builds `ProductionRuntimeMutationPlan` rows with mutation targets, runtime/editor/gameplay flags, budget-bound tags, deterministic costs, and summary diagnostics.
- `Engine Services > Production Catalogs v49 Live Mutations` now shows mutation totals, live mutation state, active workspace/command/trace rows, a prioritized mutation table, and an `Apply Mutations` control.
- The playable scene now renders v49 mutation pillars and spawned encounter wave markers around the DISPARITY rift, layered on top of the existing Action Director ribbon burst so the catalog visibly changes the showpiece area.
- Runtime reports now emit `v49_action_mutation_points`, mutation plan counts, budget/workspace/command/trace/gameplay mutation counters, world pillar/wave counters, verification assets, docs readiness, and every `v49_point_*` metric.

## Engine v48 Action Director Implemented

- Added `Assets/Verification/V48ActionDirector.dfollowups`, a twenty-four-point batch that turns executable catalog previews into a stronger Action Director surface.
- `ProductionAssetRuntimeCatalog` now builds prioritized runtime action plans with deterministic scores, high-impact/editor-visible/playable flags, and summary diagnostics usable by engine, editor, and game code.
- `Engine Services > Production Catalogs v48 Action Director` now shows action-plan totals, a prioritized action queue, `Director Burst` control, and execution history/queue/ribbon/ghost counters.
- The playable scene now renders a larger Action Director burst around the DISPARITY rift: animated route ribbon segments spiral from the selected action and encounter ghost markers orbit the rift for a more showable public-demo moment.
- Runtime reports now emit `v48_action_director_points`, action-plan counts, director request/history/queue counters, visible ribbon/ghost counters, verification assets, docs readiness, and every `v48_point_*` metric.

## Engine v47 Catalog Execution Mode Implemented

- Added `Assets/Verification/V47CatalogExecutionMode.dfollowups`, a twenty-four-point batch that turns selectable catalog previews into executable editor/game actions.
- `GameProductionRuntimeCatalog.*` now owns catalog execution state, execute/stop helpers, execution detail counters, domain executable counts, and rift-world marker accounting without growing `DisparityGame.cpp`.
- `Engine Services > Production Catalogs v47` now has `Execute Preview` and `Stop` controls, plus a live execution status row that reports the selected binding action, request count, pulse count, and world marker count.
- The playable scene now renders visible execution markers around the DISPARITY rift. The default objective-route action draws a pulsing route beam so you can see the catalog row affecting the game world immediately; Engine and Editor rows show different overlay marker patterns when selected and executed.
- Runtime reports now emit `v47_catalog_execution_points`, execute/stop/pulse counters, Engine/Editor/Game executable binding counts, execution overlay and route-beam counters, verification assets, docs readiness, and every `v47_point_*` metric.

## Engine v46 Catalog Action Preview Implemented

- Added `Assets/Verification/V46CatalogActionPreview.dfollowups`, a twenty-four-point batch that makes the live production catalog selectable and visible as an editor/game action preview.
- `GameProductionRuntimeCatalog.*` now owns `ProductionCatalogPreviewState`, selection clamping, preview priming, per-domain preview counts, and preview stats so the root game file stays at its enforced line budget.
- `Engine Services > Production Catalogs v46` now has selectable catalog rows plus `Preview First`, `Next`, and `Clear` actions, and it shows the selected binding detail line with domain, action, entry, field count, and active/cleared state.
- The public demo now highlights the selected catalog binding with a stronger focused beacon marker around the DISPARITY rift, defaulting to the objective-route binding so the preview is immediately visible.
- Runtime reports now emit `v46_catalog_action_preview_points`, preview selection/cycle/clear/detail/focused-beacon counters, Engine/Editor/Game preview binding counts, command coverage, verification assets, docs readiness, and every `v46_point_*` metric.

## Engine v45 Live Production Catalog Implemented

- Added `Assets/Verification/V45LiveProductionCatalog.dfollowups`, a twenty-four-point batch that turns the validated v44 runtime catalog into visible engine/editor/game bindings.
- Extended `ProductionAssetRuntimeCatalog` with `ProductionRuntimeBinding` rows, diagnostics, ready/active/domain counts, and field lookup so engine systems can query catalog content without re-parsing text.
- Added `DisparityGame/Source/DisparityGame/GameProductionRuntimeCatalog.*` to bridge catalog data into the editor, runtime verification, and public demo while preserving the root game-file line budget.
- `Engine Services` now shows a `Production Catalogs v45` table with source, domain, action, directive, field counts, runtime-ready state, and a reload button.
- The playable scene now renders catalog-driven orbiting domain beacons around the DISPARITY rift, making the live production content visible in the world instead of only in reports.
- Runtime reports now emit `v45_live_catalog_points`, runtime/ready binding counts, engine/editor/game binding splits, panel rows, visible beacons, objective/encounter binding counts, negative-fixture checks, and all `v45_point_*` metrics.

## Engine v44 Runtime Catalog Activation Implemented

- Added `Assets/Verification/V44RuntimeCatalogActivation.dfollowups`, a twenty-four-point batch that consumes the v43-validated production assets into an engine-owned runtime catalog.
- Added `ProductionAssetRuntimeCatalog` to parse directive entries and key/value metadata from engine, editor, and game production assets after validation succeeds.
- Runtime reports now emit `v44_engine_catalog_assets`, `v44_editor_catalog_assets`, `v44_game_catalog_assets`, `v44_runtime_catalog_assets`, `v44_runtime_catalog_entries`, `v44_runtime_catalog_fields`, `v44_runtime_catalog_activations`, `v44_runtime_catalog_domains`, `v44_runtime_catalog_actions`, `v44_runtime_catalog_missing_fields`, `v44_catalog_points`, and all `v44_point_*` metrics.
- The v44 catalog turns the production manifests into a reusable data surface for future EventBus, scheduler, streaming, editor workspace, command palette, encounter, route, save-slot, and combat-sandbox integration.
- Schema checks, baselines, release readiness, production-batch review, performance-history summaries, README, docs, roadmap, and AGENTS context now require `v44_catalog_points=24`.

## Engine v43 Live Production Validation Implemented

- Added `Assets/Verification/V43LiveProductionValidation.dfollowups`, a twenty-four-point batch that turns the v42 production-surface assets into structurally validated live content gates.
- Added engine-owned `ProductionAssetValidator` support for line/directive counts, required tokens, activation metadata, missing-field counts, and stable content hashes.
- Promoted the v42 engine/editor/game manifests with activation tags such as `capture=true`, `enforce=true`, `editable=true`, `spawn=true`, `trigger=true`, and `wave=true`.
- Runtime reports now emit `v43_engine_live_assets`, `v43_editor_editable_assets`, `v43_game_playable_assets`, `v43_validated_assets`, `v43_activation_bindings`, `v43_missing_fields`, `v43_asset_hash_low`, per-asset hash/version lines, `v43_validation_points`, and all `v43_point_*` metrics.
- Schema checks, baselines, release readiness, production-batch review, performance-history summaries, README, docs, roadmap, and AGENTS context now require `v43_validation_points=24`.
- Hardened production PowerShell hashing in cook/package/symbol/crash/baseline tools with .NET SHA-256 fallbacks for shells where `Get-FileHash` is unavailable.

## Engine v42 Production Surface Implemented

- Added `Assets/Verification/V42ProductionSurface.dfollowups`, a twenty-four-point production-surface batch with six Engine, six Editor, six Game, and six Verification/Docs points.
- Added content-backed engine manifests for event trace channels, scheduler phase budgets, scheduler task graph edges, scene-query layers, streaming budget classes, and render budget classes.
- Added editor manifests for workspace layouts, command palette metadata, viewport bookmarks, inspector presets, dock-layout migration, and Shot Director track validation.
- Added public-demo manifests for encounter planning, controller feel tuning, objective routing, accessibility options, save-slot planning, and combat sandbox beats.
- Runtime reports, schema checks, baselines, release readiness, performance-history summaries, README, docs, roadmap, and AGENTS context now require `v42_content_points=24`.

## Engine v41 Breadth Batch Implemented

- Added `Assets/Verification/V41BreadthBatch.dfollowups`, a twenty-point BFS-style batch that rotates through Engine, Editor, Game, and Verification/Docs instead of concentrating on one subsystem.
- Engine coverage now cross-checks EventBus queued flushes, FrameScheduler phase breadth, SceneQueryWorld raycast coverage, AssetStreamingPlan scheduling, and RenderGraphBudget checks in one runtime report surface.
- Editor coverage now links viewport/object/gizmo picking, high-resolution capture paths, persisted preferences plus toolbar actions, transform/command history, and Shot Director sequencing into v41 readiness metrics.
- Game coverage now ties the public-demo objective route, traversal/collision, enemy archetypes, input/failure loops, and audio/animation content into one balanced demo-readiness gate.
- Verification/docs coverage requires the v41 schema, baseline keys, release-readiness manifest, performance-history summary columns, and docs/AGENTS references to stay synchronized with the runtime report.

## Engine v40 Diversified Batch Implemented

- Added `Assets/Verification/V40DiversifiedBatch.dfollowups`, a fifteen-point Engine/Editor/Game batch that continues the roadmap without touching the line-budgeted root game file.
- Extended `RuntimeCommandRegistry` diagnostics with command-history success/failure splits, bound command counts, unique binding counts, and command documentation coverage.
- Extended `EditorPanelRegistry` workspace diagnostics with migration-ready layouts, preferred focus targets, gamepad-ready workspaces, toolbar profile coverage, and command-routed workspace counts.
- Extended `GameEventRouteCatalog` with checkpoint-link, save-relevant, chapter-replay, accessibility-critical, and HUD-visible route metadata for the public demo.
- Runtime reports, schema checks, baselines, release-readiness review, production-batch review, and performance-history summaries now require all v40 counters and `v40_point_*` metrics.

## Engine v39 Roadmap Batch Implemented

- Added `Assets/Verification/V39RoadmapBatch.dfollowups`, a fifteen-point roadmap batch with five Engine, five Editor, and five Game points.
- Extended `RuntimeCommandRegistry` with execution history, binding-conflict diagnostics, category summaries, required-category validation, and trace-ready runtime-report counters.
- Expanded `EditorPanelRegistry` workspace descriptors with saved dock-layout paths, team-default flags, workspace command bindings, controller navigation hints, and toolbar customization slot counts.
- Promoted `GameEventRouteCatalog` routes with replay flags, breadcrumb labels, selection-link metadata, trace channels, checkpoint/failure routes, and runtime-report coverage.
- Split v39 readiness evaluation/report writing into `DisparityGame/Source/DisparityGame/GameRoadmapBatch.*`, then wired schema, baselines, performance history, source-split checks, release readiness, and production-batch review through v39.

## Engine v38 Diversified Batch Implemented

- Added engine-owned `RuntimeCommandRegistry` support for command categories, bindings, enable/disable state, duplicate tracking, execution diagnostics, and command search.
- Promoted `EditorPanelRegistry` into a live panel/workspace surface with searchable panels, workspace application, navigation diagnostics, a `Panels`/`Workspaces` menu, and the `Engine Services` panel.
- Added `DisparityGame/Source/DisparityGame/GameEventRouteCatalog.*` to document objective, encounter, traversal, audio, and failure event routes backed by telemetry counters and event channels.
- Added `Assets/Verification/V38DiversifiedBatch.dfollowups`, a Profiler `Diversified Engine/Editor/Game Readiness v38` table, runtime report metrics for all `v38_point_*` entries, schema and baseline gates, release-readiness review, performance-history summaries, and production-batch review coverage.

## Engine v37 Source Split Correction Implemented

- `DisparityGame.cpp` has been cut from 15,096 lines to about 13,300 lines by extracting shared runtime types, helper functions, and legacy production followup catalogs.
- New focused files live under `DisparityGame/Source/DisparityGame`: `GameRuntimeTypes.*`, `GameRuntimeHelpers.*`, and `GameProductionCatalog.*` join the existing followup catalog and module registry.
- `Tools/VerifyGameSourceSplit.ps1` now fails the build if the root game file grows past the configured limit, if split files are missing from the Visual Studio project, or if extracted catalog/type blocks creep back into `DisparityGame.cpp`.
- `Tools/VerifyDisparity.ps1` runs the game source split check before compiling, so this readability work is protected by the normal verification gate.

## Engine v36 Mixed Batch Implemented

- Added engine-owned `ServiceRegistry`, `TelemetryStream`, `ConfigVarRegistry`, and `EditorPanelRegistry` modules for subsystem readiness, structured counters/events, typed runtime tuning, and deterministic editor panel ordering/visibility.
- Started the game source split: new v36 followup data lives in `DisparityGame/Source/DisparityGame/GameFollowupCatalog.*`, and the readable module/source inventory lives in `DisparityGame/Source/DisparityGame/GameModuleRegistry.*` instead of adding another large table to `DisparityGame.cpp`.
- The Profiler now includes a `Mixed Engine/Game/Editor Readiness v36` table with service, telemetry, config, editor-panel, and source-split diagnostics.
- Runtime verification reports `v36_mixed_batch_points`, service registry counters, telemetry counters, config variable counters, editor panel registry counters, game module split counters, and every `v36_point_*` metric.
- Baselines, runtime schema checks, release readiness, production-batch review, performance history, docs, and AGENTS context all require the v36 mixed batch.

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
- glTF runtime path now creates all mesh primitive handles, binds parsed base-color textures, honors `doubleSided` materials, instantiates mesh nodes into the scene, stores skin inverse bind matrices, reads JOINTS_0/WEIGHTS_0, and parses animation sampler keyframes for playback.
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

## Engine v18 Followups Implemented

- `Assets/Cinematics/Showcase.dshot` now drives an authored trailer/photo camera path with shot key interpolation, per-shot focus/lens/letterbox values, and deterministic playback through `F8`.
- `F9` queues a one-click 2x frame capture workflow that writes `Saved/Captures/DISPARITY_photo_source.ppm` and `Saved/Captures/DISPARITY_photo_2x.ppm`.
- Materials now carry emissive color/intensity data through `.dmat`, renderer constants, and the scene shader, so the rift can glow without relying only on bright albedo.
- The rift now has a real presentational VFX layer: billboard particles, hot particles, ribbon trails, lightning beams, fog cards, lens dirt, film grain, stronger depth-of-field, title-safe guides, and beat-synced audio-reactive pulses.
- Runtime verification now exercises trailer/photo mode, high-resolution capture, all seven post debug views, rift VFX draw coverage, and beat-pulse coverage across all six suites, with refreshed baselines and golden thumbnails.

## Engine v18.1 Audio Fix

- Showcase and trailer modes no longer trigger generated WinMM cue tones by default, preventing glitchy repeated prototype audio during public capture.
- The Audio Mixer has an opt-in `Cinematic cue tones` toggle for testing those cues, and generated test tones now use a short attack/release envelope to reduce clicks.

## Engine v18.2 Audio Warm-Up Fix

- Generated WinMM tones now reuse a persistent output device and include a tiny silent pre-roll, so cue playback no longer depends on another app keeping the Windows audio endpoint awake.

## Engine v18.3 ImGui ID Verification Fix

- Renderer and Inspector controls now use unique Dear ImGui labels, fixing the duplicate `Lens dirt` ID warning popup.
- `Tools/TestImGuiIds.ps1` is now part of `Tools/VerifyDisparity.ps1`, so duplicate literal ImGui labels and empty control labels fail verification before runtime.

## Engine v19 Production Followups Implemented

- Scene objects, player parts, and viewport gizmo handles now write stable IDs and depth into dedicated GPU object-ID targets. Editor hover, click picking, and gizmo drag startup try GPU readback first and keep CPU ray tests as a fallback.
- `F9` now queues captures instead of replacing the pending request, and the renderer can export PNG through WIC. The public photo flow writes `DISPARITY_photo_source.ppm`, `DISPARITY_photo_source.png`, and `DISPARITY_photo_2x.ppm`.
- Materials and scene files now carry a `double_sided` flag. DX11 uses a no-cull rasterizer state with back-face normal flipping so single-surface imported showcase meshes remain visible while rotating, and runtime verification reports `imported_gltf_double_sided=true`.
- The new `Shot Director` panel edits `Assets/Cinematics/Showcase.dshot`, adds/captures keys from the current camera, scrubs trailer time, and saves/reloads without leaving the running build.
- The Inspector shows Beacon prefab override counts and can apply or revert the selected object against `Assets/Prefabs/Beacon.dprefab` while preserving world position and stable ID.
- The asset database exposes dependency graph totals in the Assets panel. `Tools/CookDisparityAssets.ps1` now records declared glTF buffer/image, material texture, script prefab, and import-setting dependencies plus cook payload metadata.
- `Tools/LaunchTrailerCapture.ps1` writes `Saved/Trailer/trailer_launch_preset.json` and can launch Debug, Release, or packaged DISPARITY for repeatable recording sessions.

## Engine v20 Production Followups Implemented

- Render passes now run graph-owned callbacks and report callback, barrier, resource-handle, capture-queue, and object-ID readback-ring diagnostics.
- The editor `Viewport` panel displays the renderer-owned editor viewport texture directly through Dear ImGui.
- `Assets/Cinematics/Showcase.dshot` is now v3 with easing, renderer pulse, audio cue, and bookmark metadata. Trailer playback and runtime verification use those fields.
- The rift VFX layer exposes particle/ribbon/fog/lightning/soft-particle/sorted-batch stats in runtime verification.
- Material assets and glTF material export preserve base-color, normal, metallic-roughness, emissive, and occlusion texture slots.
- Prefabs store variant, parent, and nested prefab metadata, and the asset database/cook pipeline track those dependencies.
- The job system has an async text-read helper used by runtime verification.
- Animation now includes transform blending and a skinning palette upload-generation surface.
- Audio now exposes peak/RMS/beat-envelope analysis values for the future XAudio2 mixer.
- Asset cooking can emit deterministic `.dglbpack` optimized-package placeholders for glTF/glB sources.
- Verification now requires v20 coverage counters and writes a baseline approval manifest with hashes for baselines and goldens.

## Engine v21 Production Followups Implemented

- Render passes bind resources through graph handles and report binding hits/misses, callback execution, barriers, allocations, alias reuse, and dispatch validity.
- GPU editor picking uses non-blocking object-ID/depth readback slots and reports pending, busy, completion, and latency diagnostics.
- `Assets/Cinematics/Showcase.dshot` is now v4 with Catmull-Rom spline mode, timeline-lane metadata, and thumbnail tint metadata.
- Rift VFX reports GPU-simulation, motion-vector, and temporal-reprojection counters.
- Generated tones prefer a dynamically loaded XAudio2 source-voice path when available, with WinMM as the fallback.
- `F9` queues the 2x capture through an async worker, and asset cooking writes structured `DSGLBPK2` package manifests for glTF sources.
- Baseline approvals include Git signature metadata, and production tooling emits interactive CI, package artifact, symbol-server, bootstrapper, and crash-upload retry plans.

## Engine v22 Production Followups Implemented

- GPU editor picking now records hover-cache samples and latency histogram buckets, with Profiler visualization and runtime baseline coverage.
- `Assets/Cinematics/Showcase.dshot` is now v5 with editable easing curves, renderer/audio timeline tracks, generated thumbnail paths, and non-modal preview scrubbing.
- `F9` now records graph-owned high-resolution capture target/tile/MSAA/pass diagnostics and writes `DISPARITY_photo_2x.dcapture.json` next to the async 2x output.
- Rift VFX keeps particle/ribbon runtime resources, applies depth fade, sorts batches, and reports temporal-history samples.
- Runtime verification loads optimized `DSGLBPK2` cooked package metadata, validates dependency invalidation, and checks nested prefab instancing coverage.
- Audio verification now covers mixer voice counts, streamed music layers, spatial emitters, attenuation curves, meter updates, and content pulse counts.
- Production tooling adds cooked package review, signed baseline update approval intent, symbol publishing to `dist/SymbolServer`, an installer bootstrapper command, OBS scene/profile metadata, and CI artifact upload paths for the new manifests.

## Engine v23 Production Followups Implemented

- The `Viewport` panel now overlays a compact diagnostics HUD on the renderer-owned editor texture, including camera/render mode, bloom/TAA state, last GPU-picked object/depth/stale age, readback latency/cache state, and high-resolution capture tile/resolve state.
- `F9` high-resolution capture manifests are schema v3 and record the capture preset, scale, tile count, MSAA samples, tile jitter, async worker/compression readiness, planned EXR output, and the current tent-like 2x resolve.
- The 2x capture proof now uses a row-buffered tent-like resolve instead of nearest-neighbor expansion.
- Runtime reports and performance history include viewport overlay tests, high-resolution resolve tests, GPU-pick stale frame age, last GPU-picked object, resolve filter, and resolve sample count.
- `RuntimeVerifyDisparity.ps1` asserts the v23 report schema directly, and performance summaries compare frame-time regressions against both the previous run and recent median to reduce noise from one-off OS scheduling spikes.

## Engine v24 Production Followups Implemented

- Ten followup batches landed together: viewport HUD controls/thumbnails, transform precision controls, filterable command history, runtime schema manifests, v6 Shot Director sequencing metadata, VFX renderer profile validation, cooked package GPU-resource promotion metadata, dependency-invalidation verification, audio meter calibration metadata, and release-readiness manifests.
- The viewport diagnostics HUD can be enabled, pinned, trimmed row-by-row, and supplemented with object-ID/depth debug thumbnails.
- `Inspector > Transform Precision` exposes nudge step, pivot mode, and orientation mode; the 3D gizmo now uses the editable precision step for translate/rotate/scale nudges.
- The Profiler command-history panel is filterable, which keeps long editor sessions easier to audit.
- `Assets/Cinematics/Showcase.dshot` is now v6 with clip lanes, nested sequence names, hold time, and shot role metadata for the next sequencer pass.
- Runtime reports, runtime baselines, performance history, and `Assets/Verification/RuntimeReportSchema.dschema` now require the ten v24 counters so future verification metrics can be reviewed through an asset instead of hardcoded script edits.
- `Tools/ReviewReleaseReadiness.ps1` checks packaged manifests, installer/bootstrapper output, symbol manifests, OBS profile metadata, and the runtime report schema before release artifacts are considered complete.

## Engine v25-v27 Production Followups Implemented

- `Assets/Verification/V25ProductionBatch.dfollowups` tracks forty stable production points across editor, gizmo, asset, rendering, capture, VFX, sequencer/audio, and production automation work; runtime verification exports one metric for every point.
- Editor preferences persist to `Saved/Editor/editor_preferences.json`, round-trip through named sanitized profiles under `Saved/Editor/Profiles`, and are validated by isolated verification probes.
- The viewport texture now has an overlaid toolbar for camera mode, post-debug cycling, capture queueing, object-ID/depth rows, and HUD visibility.
- `F9` capture manifests are schema v3 with typed `Trailer2x` preset, async-compression readiness, planned EXR metadata, and graph-owned high-resolution capture diagnostics.
- VFX emitter profiles, cooked dependency-preview/rollback readiness, and profile/capture/VFX/cooked-dependency counters are required by baselines, schema checks, and performance history.

## Engine v28 Diversified Production Batch Implemented

- A new `Assets/Verification/V28DiversifiedBatch.dfollowups` manifest covers thirty-six stable points, six each for editor workflow, asset pipeline promotion, rendering, runtime sequencer, audio production, and production publishing.
- The `Viewport` panel now includes profile export/import/default-diff controls, remembers workspace preset metadata, and offers gameplay/editor/trailer workspace preset buttons with capture progress reporting.
- Cooked package runtime metadata now exposes streaming readiness, texture-binding counts, skinning palette uploads, retargeting maps, streaming priority levels, live invalidation tickets, rollback journal entries, and upload-byte estimates.
- Rendering diagnostics now cover graph-owned advanced pass readiness such as bind/barrier promotion, culling/binning, CSM, motion-vector targets, tiled capture, temporal resolve, and VFX renderer promotion counters.
- Runtime and audio diagnostics now cover sequencer editing/readiness, keyboard preview bindings, undoable shot edits, real-content pulse inputs, streamed-music layer readiness, spatial emitter authoring, attenuation assets, and calibrated mixer metering.
- Production tooling now reviews both v25 and v28 followup manifests, requires v28 category counters in every runtime baseline, records v28 performance-history columns, checks release-readiness schema coverage, and reports the new category counters in summaries.

## Engine v29 Public Demo Batch Implemented

- The prototype now has a small public playable loop layered into the showpiece scene: collect six visible rift shards, manage stability/focus pressure, and return to the extraction beacon for a completion beat.
- New public-demo visuals include shard beacons, path markers, sentinel patrol pressure, extraction gate feedback, rift charge intensification, HUD objectives, and stronger completion feedback that reads well in capture.
- `F10` resets the loop while the game is running, and runtime verification exercises a deterministic shard-to-anchor-to-resonance-gate completion route.
- `Assets/Verification/V29PublicDemo.dfollowups` tracks thirty public-demo readiness points across gameplay, visuals, HUD, audio feedback, capture, verification, and production hygiene.
- Runtime reports, baselines, release-readiness review, performance history, and the schema manifest now require public-demo counters including shard pickups, HUD frames, beacon draws, completion, and all thirty `v29_point_*` metrics.

## Engine v30 Vertical Slice Batch Implemented

- The public demo is now a richer mini vertical slice: collect six shards, align three phase anchors, survive stability pressure, retry from checkpoints, then extract through the charged rift.
- New visible show-off elements include phase-anchor glyphs, bridge beams from aligned anchors to the rift, a checkpoint marker, low-stability warning rings, upgraded objective routing, and HUD readouts for stage, anchors, checkpoint, retry count, and objective distance.
- The editor has a `Demo Director` panel plus a v30 readiness table in the Profiler, exposing gameplay event telemetry, checkpoint controls, current stage, and backend verification state.
- `Assets/Verification/V30VerticalSlice.dfollowups` tracks thirty-six points across gameplay, visuals, HUD, editor, backend telemetry, rendering/capture, verification, and production.
- Runtime reports, baselines, release-readiness review, performance history, and schema assertions now require v30 counters including objective-stage transitions, anchor activations, retries, checkpoints, director frames, and all `v30_point_*` metrics.

## Engine v31 Diversified Roadmap Batch Implemented

- `Assets/Verification/V31DiversifiedBatch.dfollowups` tracks thirty roadmap points, five each for Editor, Asset Pipeline, Rendering, Runtime, Audio, and Production.
- The public demo now adds two visible resonance gates after phase anchors, with tuned gate glyphs and bridge beams feeding the rift before extraction opens.
- Runtime reports and baselines now cover resonance gates, pressure hits, footstep cadence events, event routing, failure/retry presentation, and all `v31_point_*` metrics.
- The Profiler has a v31 diversified readiness table, and release-readiness/performance-history tooling now includes the v31 manifest and counters.

## Engine v32 Sixty-Point Roadmap Batch Implemented

- `Assets/Verification/V32RoadmapBatch.dfollowups` tracks sixty roadmap points, ten each for Editor, Asset Pipeline, Rendering, Runtime, Audio, and Production.
- The public demo now extends the shard/anchor/gate route with three phase relays. Each relay has visible rings, vertical glyphs, orbit shards, bridge beams, overcharge windows, and combo-route telemetry before extraction opens.
- Runtime reports and baselines now require phase-relay stabilization, relay overcharge windows, combo-chain steps, traversal markers, relay bridge draws, and all `v32_point_*` metrics.
- The Profiler has a v32 roadmap readiness table, while runtime schema checks, production manifest review, release readiness, baseline review, and performance-history summaries all include the new v32 gates.

## Engine v33 Playable Demo Batch Implemented

- `Assets/Verification/V33PlayableDemoBatch.dfollowups` tracks fifty playable-demo points across CollisionTraversal, EnemyAI, GamepadMenu, FailurePresentation, and AudioAnimation.
- The public demo now has collision-backed arena/blocker resolution, sliding push-out, dash/vault traversal barriers, simple enemy patrol/chase/contact/evade behavior, and a pause/completion/failure presentation flow.
- Gameplay input now accepts keyboard/mouse plus dynamic XInput gamepad polling without adding a new external dependency. `Space`/gamepad `A` dashes, `Shift`/`LB` sprints, and `P`/`Start` pauses.
- Public-demo feedback now reads cue definitions from `Assets/Audio/PublicDemoCues.daudio` and player state names from `Assets/Animation/PublicDemoPlayer.danim`, with player material tinting for idle, walk, sprint, dash, stabilize, failure, and complete states.
- Runtime reports, schema checks, baselines, release readiness, performance history, and production manifest review now require collision/traversal, enemy, gamepad/menu, failure, content audio, animation, and all `v33_point_*` metrics.

## Engine v34 AAA Foundation Batch Implemented

- `Assets/Verification/V34AAAFoundationBatch.dfollowups` tracks sixty near-AAA foundation points across EncounterAI, Controller, Animation, EditorUX, Rendering, and Production.
- The public demo now has three enemy archetypes: Hunter, Warden, and Disruptor. They expose role-specific colors, patrol offsets, line-of-sight scoring, telegraph windows, hit-reaction telemetry, and Demo Director readouts.
- Controller polish now records grounded, slope, material, moving-platform, camera-collision, dash-recovery, and accessibility/title-flow readiness counters for future character-controller work.
- Animation has a new `Assets/Animation/PublicDemoBlendTree.danimgraph` manifest with clips, transitions, animation events, and root-motion preview entries. Runtime verification exercises blend samples and event routing.
- Runtime reports, schema checks, baselines, release readiness, performance history, and production manifest review now require v34 enemy/controller/animation/accessibility/rendering/production readiness plus all `v34_point_*` metrics.

## Engine v35 Architecture Batch Implemented

- `Assets/Verification/V35EngineArchitectureBatch.dfollowups` tracks fifty engine-first points across Core, Runtime, Scene, Assets, Rendering, and Production.
- `DisparityEngine` now owns small production-facing service modules for typed event dispatch, frame-phase scheduling, scene overlap/raycast queries, asset streaming budgets, and render-graph budget analysis.
- Runtime verification smokes every new module and reports `engine_event_bus_*`, `engine_scheduler_*`, `engine_scene_query_*`, `engine_streaming_*`, `engine_render_graph_budget_*`, `engine_module_smoke_tests`, and all `v35_point_*` metrics.
- The Profiler now includes an `Engine Architecture Readiness v35` table, making engine service health inspectable separately from the public-demo gameplay layer.

More detail lives in `Docs/ENGINE_FEATURES.md` and `Docs/ROADMAP.md`.

## Future Followups

- Feed the v43-validated manifests into the live EventBus, FrameScheduler, SceneQueryWorld, asset streaming plan, and render-graph budget systems instead of only validating them in runtime reports.
- Build editor authoring panels for the v43 manifests so workspace layouts, command palette entries, viewport bookmarks, inspector presets, dock migrations, and shot-track validation can be edited and revalidated in-session.
- Use the validated gameplay manifests to drive encounter spawning, controller preset loading, objective triggers, accessibility toggles, save-slot checkpoints, and combat sandbox waves in the public demo.
- Extend the validator with field types, numeric ranges, version migrations, and per-line diagnostics that can be surfaced in CI and the editor.
- Extract the repeated PowerShell SHA-256 fallback into a shared verification helper once the tool suite gets a common module loader.
- Promote the v35 event bus into a replayable engine event stream with scoped channels, payload schemas, editor event inspection, trace capture, and deterministic save/load playback.
- Turn the v35 frame scheduler into the main engine tick orchestrator with dependency-aware jobs, phase budgets, task graph visualization, and per-system throttling.
- Expand the v35 scene query world into a broad-phase acceleration structure used by picking, character collision, trigger volumes, audio occlusion, and AI perception.
- Replace the v35 asset streaming plan with real async IO, priority lanes, cancellation tokens, dependency-aware GPU uploads, rollback journals, and editor-visible streaming pressure.
- Enforce v35 render-graph budgets in the renderer with budget classes, per-pass memory estimates, alias lifetime validation, and CI trend gates.
- Turn the v34 enemy archetype proof into a data-driven encounter system with behavior trees/state machines, navigation volumes, squad roles, perception memory, difficulty budgets, and designer-authored encounter prefabs.
- Promote the controller telemetry into a real swept capsule character controller with ledge checks, step-up/step-down, slope limits, material friction, moving-platform parenting, camera obstruction solving, and replay-tested feel presets.
- Replace the v34 blend-tree manifest with authored clip assets, editor transition previews, animation event tracks, root-motion extraction, additive poses, state-machine debugging, and GPU skinning playback.
- Expand the title/accessibility/save-slot readiness into actual title/settings/save/chapter-select UI with input remapping, subtitle/color/contrast options, persistence, and controller-first navigation.
- Promote v28 graph-owned rendering diagnostics into actual DX11 pass execution: explicit bind/unbind barriers, alias lifetime validation, GPU culling, Forward+ lighting, cascaded shadows, motion-vector rendering, and stronger temporal AA.
- Move the high-resolution capture proof from resolved source-frame sampling to true tiled offscreen supersampling with per-tile camera jitter, selectable resolve filters, real EXR output, and real async compression workers.
- Turn editor profile import/export/diff and workspace presets into a versioned per-project preference system with dock-layout files, schema migration, conflict-safe merge, and checked-in team defaults.
- Replace cooked package readiness metadata with real optimized GPU mesh/material/animation uploads, live dependency invalidation, runtime streaming, retargeting, and GPU skinning palette buffers.
- Promote nested prefab metadata into multi-object override diffing, recursive dependency-aware apply/revert, undo grouping, and dependency-aware scene reloads.
- Replace audio production/readiness counters with real XAudio2 mixer voices, streamed music assets, spatial emitter components, attenuation-curve assets, calibrated meters, and content-driven VFX pulses from decoded buffers.
- Expand v6 Shot Director plus v28 runtime sequencer diagnostics into a full sequencer with curve editing, clip lanes, thumbnails, nested sequences, clip blending, keyboard preview, and undoable edits.
- Promote rift particle/ribbon profiles into a dedicated GPU VFX renderer with soft particles, emitter sorting controls, motion vectors, temporal reprojection, and debug visualizers.
- Replace release readiness manifests, bootstrapper plans, symbol-server plans, and OBS command diagnostics with signed publishing/install artifacts, interactive CI runtime gates, and real OBS WebSocket automation.
