# DISPARITY Roadmap

The current engine now has functional v30 versions of many requested followups, including a small public vertical slice for capture and demos. The next milestones should keep rotating through gameplay, graph, editor viewport, shot-track, VFX, asset, audio, capture, and verification work so no production lane falls behind.

## v30 Completed Vertical Slice Batch

- Added `Assets/Verification/V30VerticalSlice.dfollowups`, a thirty-six-point vertical-slice manifest spanning gameplay, visuals, HUD, editor, backend telemetry, rendering/capture, verification, and production hygiene.
- The public demo now chains shard collection into three phase anchors, then gates extraction through the charged rift. It also records checkpoints, exposes retry behavior, and reports objective distance and stage transitions.
- New public-facing visuals include phase-anchor glyph rings, bridge beams from aligned anchors to the rift, a checkpoint marker, low-stability warning rings, upgraded route beams, and HUD state for stage, anchors, retries, checkpoints, and objective distance.
- The editor now has a `Demo Director` panel for public-demo telemetry, reset/checkpoint/retry controls, and recent gameplay events, while the Profiler exposes a v30 readiness table.
- Runtime verification completes the shard-anchor-extraction route deterministically, forces a checkpoint retry path, validates the Demo Director/readiness surfaces, and reports every `v30_point_*` metric.
- Runtime baselines, runtime report schema checks, release-readiness review, manifest review, and performance-history summaries now require v30 vertical-slice counters.

## v29 Completed Public Playable Demo Batch

- Added `Assets/Verification/V29PublicDemo.dfollowups`, a thirty-point public-demo followup manifest spanning gameplay, visuals, HUD, audio feedback, capture, verification, and production hygiene.
- The prototype now has a replayable shard-collection objective: collect six glowing rift shards, manage sentinel pressure, return to the extraction beacon, and trigger a visible completion pulse.
- `F10` resets the public demo state while the game is running, while `Shift+WASD` adds a sprint layer for the first playable loop.
- The scene now draws route markers, shard beacons, sentinel hazards, extraction gate feedback, rift charge/surge modulation, and HUD objective/status text that remain visible when the editor is hidden.
- Runtime verification completes the public demo route deterministically, validates shard pickups, completion, HUD frames, beacon draws, stability/focus values, and all thirty `v29_point_*` metrics.
- Runtime baselines, runtime report schema checks, release-readiness review, manifest review, and performance-history summaries now require the v29 public-demo counters.

## v28 Completed Diversified Production Batch

- Added `Assets/Verification/V28DiversifiedBatch.dfollowups`, a thirty-six-point production followup manifest with six points each for editor workflow, asset pipeline promotion, rendering, runtime sequencer, audio production, and production publishing.
- Editor workflow now includes profile export/import/default-diff controls, workspace preset metadata, gameplay/editor/trailer workspace buttons, and capture-progress diagnostics in the `Viewport` panel.
- Cooked package runtime metadata now reports texture binding counts, streaming readiness, streaming priority levels, skinning palette uploads, retargeting maps, live invalidation tickets, rollback journal entries, and estimated upload bytes.
- Rendering diagnostics now promote the graph/capture/VFX roadmap into explicit advanced-readiness counters for bind/barrier execution, alias validation, GPU culling, Forward+ bins, CSM splits, motion-vector targets, tiled capture, temporal resolve, and VFX renderer handoff.
- Runtime sequencer and audio diagnostics now cover curve/clip/thumbnail/nested/keyboard-preview/undo readiness plus streamed music, spatial emitters, attenuation assets, calibrated metering, XAudio2 voice readiness, and content-driven pulse inputs.
- Production publishing diagnostics now cover schema metric breadth, baseline diff-package readiness, installer bootstrapper output, symbol publish plans, OBS WebSocket command metadata, interactive CI runtime gates, artifact uploads, crash retries, and signed approval intent.
- Runtime reports, runtime baselines, the schema manifest, baseline review, release-readiness review, production-batch review, and performance-history summaries all require the new v28 counters and all thirty-six point metrics.

## v27 Completed Diversified Production Batch

- Editor preferences now support named profile save/load/reset from the `Viewport` panel. Profiles are sanitized into `Saved/Editor/Profiles/<name>.json`, while the main preference file remembers the active profile name.
- High-resolution capture now has a typed `Trailer2x` preset profile. Runtime capture manifests moved to schema v3 with preset name, async-compression readiness, and planned EXR output fields in addition to the existing scale/tile/MSAA/tent-resolve metadata.
- Rift VFX diagnostics now include emitter count, sort bucket count, and estimated GPU buffer bytes, preparing the existing particle/ribbon proof for a dedicated emitter renderer.
- Cooked `DSGLBPK2` runtime resources now expose dependency-invalidation preview counts and reload-rollback readiness, making package reload risks visible in runtime reports instead of only in cook metadata.
- Runtime verification now validates named preference profiles, capture preset metadata, VFX emitter profile diagnostics, and cooked dependency preview/rollback state. Runtime reports, baselines, schema assertions, baseline review, and performance-history summaries all require the new v27 counters.

## v26 Completed Long-Horizon Editor Foundations

- Editor preferences now persist to `Saved/Editor/editor_preferences.json`, covering viewport HUD row visibility, HUD pinning, viewport-toolbar visibility, editor-camera enablement, transform precision, pivot/orientation mode, and the command-history filter.
- Preference changes autosave shortly after UI edits, while runtime verification uses an isolated `Saved/Verification/editor_preferences_probe.json` round trip so user settings cannot make verification flaky.
- The `Viewport` panel now draws a compact interactive toolbar directly over the renderer-owned ImGui viewport texture. It can toggle editor/game camera, cycle post debug views, queue captures, and toggle object-ID, depth/readback, and HUD overlay state without leaving the viewport.
- Runtime verification now validates preference save/load behavior and viewport toolbar actions, writes dedicated report metrics, and requires those counters in every suite baseline.
- `RuntimeReportSchema.dschema`, `RuntimeVerifyDisparity.ps1`, `ReviewVerificationBaselines.ps1`, and `SummarizePerformanceHistory.ps1` now track the v26 preference/toolbar counters.

## v25 Completed Forty-Point Production Batch

- Added `Assets/Verification/V25ProductionBatch.dfollowups`, a forty-point production followup manifest spanning editor preferences, viewport toolbar surfaces, gizmo depth/constraint/numeric metadata, asset GPU-resource readiness, render-graph roadmap diagnostics, tiled capture/VFX readiness, sequencer/audio plans, and production automation.
- The runtime now has typed v25 readiness profiles for those eight domains and validates every manifest point during deterministic runtime verification.
- The Profiler panel includes a `Production Readiness v25` table with point number, domain, readiness state, and description so the batch can be inspected live in the editor.
- Runtime reports now write `v25_production_points` plus one stable `v25_point_*` metric for each point, allowing wrapper scripts and baselines to fail on incomplete coverage.
- Runtime baselines now require all forty v25 points through `min_v25_production_points=40`.
- `Tools/ReviewProductionBatch.ps1` validates manifest count, key naming, uniqueness, domains, and descriptions, and `Tools/VerifyDisparity.ps1` runs it as part of the full gate.
- `RuntimeVerifyDisparity.ps1`, `ReviewVerificationBaselines.ps1`, `ReviewReleaseReadiness.ps1`, and `SummarizePerformanceHistory.ps1` now understand the v25 point counter.
- `Assets/Verification/RuntimeReportSchema.dschema` now requires every v25 point metric, so future schema or runtime-report drift fails in the same place as existing runtime diagnostics.

## v24 Completed Production Batch

- The viewport diagnostic HUD now has explicit enable/pin controls, per-row visibility toggles, and object-ID/depth debug thumbnail swatches over the renderer-owned editor texture.
- Inspector transform tooling now includes a `Transform Precision` section with editable nudge step plus pivot/orientation diagnostics, and the 3D gizmo uses that precision step for translate/rotate/scale changes.
- The Profiler command history is filterable so undo/redo and editor-action audits stay readable in longer sessions.
- Runtime report schema checks moved into `Assets/Verification/RuntimeReportSchema.dschema`, which lets new required metrics be reviewed as content instead of duplicating hardcoded script lists.
- `Assets/Cinematics/Showcase.dshot` moved to v6 with clip lanes, nested sequence names, hold durations, and shot role metadata. The Shot Director exposes those fields and runtime verification validates them.
- Runtime VFX verification now has an explicit renderer-profile surface for soft particles, depth fade, sorting, GPU simulation, motion vectors, and temporal reprojection capacity.
- Cooked package verification now promotes optimized package metadata into a runtime GPU-resource readiness surface with mesh/material/animation counts and estimated upload bytes.
- Asset dependency invalidation is now covered by runtime verification using the asset database dependency graph.
- Audio verification now includes calibration metadata for reference peak/RMS levels plus meter attack/release settings.
- Release readiness review now validates package, installer/bootstrapper, symbol, OBS, and runtime-schema artifacts through `Tools/ReviewReleaseReadiness.ps1`.
- Runtime baselines, baseline review, runtime reports, and performance history now require all ten v24 batch counters.

## v23 Completed Production Batch

- The editor viewport now draws an in-viewport diagnostics HUD over the dedicated renderer texture. It reports camera mode, post-debug mode, bloom/TAA state, last GPU-picked object name/id/depth, stale readback age, readback pending/latency/cache counters, and high-resolution capture tile/resolve state.
- High-resolution `F9` capture manifests moved to schema v2 with scale, tile count, MSAA samples, tent resolve filter, resolve sample count, tile jitter, and async worker metadata. The 2x proof now uses a row-buffered tent-like resolve instead of nearest-neighbor expansion.
- Runtime reports now expose `viewport_overlay_tests`, `high_res_resolve_tests`, `gpu_pick_stale_frames`, `gpu_pick_last_object`, `high_res_resolve_filter`, and `high_res_resolve_samples`.
- `Tools/RuntimeVerifyDisparity.ps1` now asserts the v23 runtime report schema directly, so missing viewport/capture diagnostics fail targeted and full verification instead of silently producing thin performance history rows.
- Runtime baselines and baseline review now require viewport overlay and high-resolution resolve coverage.
- Performance history output now records the v23 counters and compares frame-time regressions against both the previous run and the recent median, keeping one-off OS scheduling spikes visible without making them automatic gate failures.

## v22 Completed Production Batch

- GPU editor picking now records hover-cache samples and latency histogram buckets. The Profiler render-graph tree exposes the cache and bucket state so async object-ID readback behavior can be inspected during editor work and verification.
- Trailer `.dshot` files moved to v5 with editable easing curves, renderer-track labels, audio-track labels, generated thumbnail paths, and non-modal preview scrubbing. The Shot Director can generate per-shot PPM thumbnails under `Saved/ShotThumbnails`, and runtime verification requires timeline/thumbnail/scrub coverage.
- High-resolution capture now allocates graph-owned offscreen color/resolve resources, reports target/tile/MSAA/pass diagnostics, and writes a capture manifest next to the async 2x PPM output.
- Rift VFX simulation now keeps particle and ribbon runtime resources, applies depth fade, sorts render batches, and reports depth-fade particle plus temporal-history counters alongside the existing GPU-simulation/motion-vector/reprojection stats.
- Optimized `DSGLBPK2` packages are now reviewed by `Tools/ReviewCookedPackages.ps1`, loaded by runtime verification as cooked package resources, and checked for dependency invalidation metadata.
- Prefab verification now includes nested prefab instancing coverage alongside the existing variant/parent metadata checks.
- The audio surface now reports production-style mixer voices, streamed music layers, spatial emitters, attenuation curves, meter updates, and content-driven pulse counts. Spatial tone verification exercises the new counters without relying on external background audio.
- Baseline workflows now include `Tools/ApproveVerificationUpdate.ps1`, which writes explicit signed update intent metadata for baseline/golden changes.
- Release tooling now includes symbol publishing to `dist/SymbolServer`, an installer bootstrapper command, OBS scene/profile metadata generation, and CI artifact upload paths for the new manifests.
- `Tools/VerifyDisparity.ps1`, GitHub Actions, runtime baselines, and performance CSV output were expanded to cover the v22 editor, Shot Director, capture, cooked package, nested prefab, audio, symbol, installer, and OBS surfaces.

## v21 Completed Production Batch

- Renderer passes now bind color/depth/SRV targets through graph resource handles and report binding counts, hit/miss totals, callback execution, barriers, allocations, and dispatch validity.
- GPU editor picking now uses a non-blocking object-ID/depth staging ring. The editor queues a readback, resolves completed slots with `DO_NOT_WAIT`, and reports pending, busy-skip, completion, and latency diagnostics.
- Trailer `.dshot` files moved to v4 with Catmull-Rom spline mode, timeline lane metadata, and thumbnail tint metadata. The Shot Director exposes spline mode and thumbnail color controls, and runtime verification validates spline/timeline coverage.
- Rift VFX diagnostics now include GPU-simulation batch, motion-vector candidate, and temporal-reprojection sample counters in reports and baseline checks.
- Generated tone playback now prefers a dynamically loaded XAudio2 source-voice path when available, with WinMM retained as a fallback. Runtime reports expose XAudio2 initialization and voice counters.
- `F9` high-resolution photo output now queues the 2x write through an async worker and reports tiled capture coverage instead of doing the expensive write directly in the frame loop.
- Asset cooking now writes structured `DSGLBPK2` `.dglbpack` payload manifests for glTF assets, including mesh/primitive/material/node/animation counts, accessor metadata, buffer hashes, and import-setting dependencies.
- Baseline approval manifests now include Git signature status, signer/key metadata, optional signed-HEAD enforcement, and manifest hashing. Verification checks that signature metadata is present.
- Production tooling now emits an interactive CI plan, GitHub Actions has opt-in package/smoke/artifact steps, installer generation writes a bootstrapper plan, symbol indexing records a symbol-server publish plan, and crash upload supports retry/backoff.

## v20 Completed Production Batch

- Renderer passes now execute graph-owned callbacks, report callback execution/bound counts, transition barrier counts, resource handle counts, pending capture requests, and object-ID readback ring diagnostics in runtime reports and performance history.
- The editor `Viewport` panel presents the dedicated renderer-owned viewport texture through Dear ImGui instead of only inspecting the swap-chain image.
- Object-ID/depth picking now exposes a lightweight readback-ring diagnostic surface with request/completion/latency counters, keeping the current picker stable while preparing the non-blocking path.
- `.dshot` shot keys moved to v3 with easing, renderer pulse, audio cue, and bookmark metadata. Trailer playback uses the easing metadata and runtime verification validates shot-director coverage.
- The rift VFX layer now reports dedicated particle/ribbon/fog/lightning/soft-particle/sorted-batch stats, and runtime verification validates the VFX system surface in addition to raw draw counts.
- Material assets and glTF material export now preserve base-color, normal, metallic-roughness, emissive, and occlusion texture slots.
- Prefabs now store variant, parent, and nested prefab dependency metadata; the asset database and cook pipeline include those dependencies.
- The job system gained an async text file read helper, and runtime verification exercises it against the trailer shot track.
- The animation module now exposes transform blending and a small skinning palette upload-generation surface, with runtime verification coverage.
- The audio layer exposes content analysis values (peak, RMS, beat envelope, active voices), and the mixer/runtime report display them for the future XAudio2 backend.
- Asset cooking can emit deterministic optimized `.dglbpack` placeholders for glTF/glB sources alongside `.dassetbin`, dependency metadata, hashes, and cook payload labels.
- Verification baselines now require the new async IO, material slot, prefab variant, shot director, audio analysis, VFX system, animation/skinning, render-graph callback, and barrier counters. Adapter golden profiles and a baseline approval manifest script were added to the full verification gate.

## v19 Completed Production Batch

- The scene shader now writes HDR color, editor object IDs, and editor object depth. Scene objects, the player, gizmo axes, rotate rings, and translate-plane handles receive deterministic GPU IDs, and editor picking tries GPU readback before CPU ray/OBB fallback.
- Frame capture requests are queued. The renderer can write WIC PNG captures in addition to the existing PPM path, and `F9` now emits a source PPM, source PNG, and 2x PPM photo.
- The running editor now has a `Shot Director` panel for `.dshot` files: reload/save, add keys, capture the current camera, edit per-shot transform/focus/lens/letterbox values, and scrub trailer time.
- The Inspector now shows Beacon prefab override counts and supports apply/revert workflows that preserve world position and stable IDs.
- The asset database exposes a dependency graph, and the cook script records glTF URI, material texture, script prefab, and import-setting dependencies plus cook payload metadata.
- `Tools/LaunchTrailerCapture.ps1` creates repeatable trailer launch presets and can launch Debug, Release, or packaged builds for public recording.

## v18 Completed Public Showcase Batch

- `Assets/Cinematics/Showcase.dshot` provides deterministic authored trailer/photo camera keys with per-shot target, focus, lens dirt, and letterbox values.
- `F8` toggles trailer/photo playback, while `F9` writes a source PPM plus 2x upscaled public-demo capture under `Saved/Captures`.
- Materials now persist emissive color/intensity through `.dmat`, renderer constants, and HLSL, allowing showpiece geometry to glow without albedo-only hacks.
- The DISPARITY rift now layers billboard particles, hot particles, ribbon trails, lightning beams, fog cards, lens dirt, film grain, depth-of-field, title-safe overlays, and beat-synced presentation pulses.
- Runtime verification records `trailer_frames`, `high_res_capture_tests`, `rift_vfx_draws`, and `audio_beat_pulses`, requires them in all six baselines, refreshes all suite goldens, and keeps high-resolution capture work out of normal frame-budget samples.

## v17 Completed Showcase Batch

- The prototype scene now includes a procedural DISPARITY rift made from existing engine meshes, HDR materials, animated rings, orbiting shards, dark spires, and stronger colored point-light choreography.
- `F7` toggles a cinematic showcase mode that hides the editor, switches to an orbiting rift camera, boosts bloom/SSAO/AA/color grading, and restores previous renderer settings afterward.
- Runtime verification exercises showcase mode, records `showcase_frames`, and uses deterministic rift timing during verification captures so the new VFX remains golden-testable.
- Runtime baselines, performance history, baseline review, and suite goldens now know about showcase coverage and the refreshed rift scene thumbnails.

## v16 Completed Production Batch

- `RenderGraph` assigns transient texture/buffer resources to physical allocation slots and reports alias reuse, not just alias opportunities.
- The DX11 renderer checks every submitted graph pass against the compiled schedule and records dispatch validity in runtime reports.
- Dedicated editor viewport, object-ID, and object-depth GPU targets are allocated, cleared, exposed through renderer diagnostics, and required by runtime baselines.
- Runtime reports and performance history now include render-graph allocation, alias, dispatch-valid, editor target readiness, and XAudio2 availability fields.
- Runtime verification detects the primary display adapter and can select adapter-specific golden tolerance profiles when they exist.
- Asset cooking can write deterministic `.dassetbin` binary source bundles with package hashes alongside metadata records.
- Production tooling now includes baseline review, symbol indexing, installer payload manifest/archive creation, and crash upload dry-run transport.
- The audio layer detects XAudio2 runtime availability and exposes a preference switch while keeping the v16 WinMM playback path stable.

## v15 Completed Production Batch

- Runtime verification expanded from three suites to six: `Prototype`, `CameraSweep`, `EditorPrecision`, `PostDebug`, `AssetReload`, and `GizmoDrag`.
- Runtime reports now include scenario counters for scene reload/save, post-debug view cycling, gizmo transform constraints, and audio snapshot validation.
- Golden comparisons now use profile files in `Assets/Verification/GoldenProfiles` and write diff thumbnails for investigation.
- Performance trend summaries now compare against committed thresholds in `Assets/Verification/PerformanceBaselines.dperf`.
- Packaging now produces a hashed manifest, optional symbol payload, and optional zip artifact.
- Local crash upload plumbing now writes a manifest and optional bundle from `Saved/CrashLogs`.
- Asset cooking now has a deterministic metadata manifest script for source hashes, import settings, and cook records.
- Scene saves emit schema v3 metadata with deterministic-ID and save-game separation flags.
- The WinMM prototype audio layer gained backend naming, listener orientation, bus sends, simple meters, and mixer snapshots.

## Editor

- Promote v28 profile import/export/diff and workspace presets into a versioned per-project preference system with dock-layout files, conflict-safe migration, team defaults, and toolbar customization.
- Upgrade the viewport toolbar from text buttons to an icon/hotkey-hint surface with command search, per-workspace layout presets, and reviewable toolbar customization.
- Expand the current mesh/ring/plane gizmo handles with depth-aware hover occlusion, constraint previews, numerical transform entry, richer pivot/orientation editing, and richer object-ID handle metadata.
- Upgrade the current selection outline plus copy/paste/duplicate/delete/multi-select support with undo grouping, command categories, history export, and reviewable command macros.
- Promote the new prefab variant/parent/nested metadata into nested-prefab instancing, multi-object override diffing, recursive dependency-aware apply/revert, and undo grouping.

## Asset Pipeline

- Replace the v28 package-promotion diagnostics with real optimized GPU mesh/material/animation resource uploads from structured `.dglbpack` packages.
- Promote exported texture-slot counts into real normal, metallic-roughness, emissive, and occlusion texture binding in the renderer.
- Expand the skinning/retargeting diagnostics into skeleton assets, animation clips, state machines, retargeting profiles, and GPU skinning constant/structured-buffer uploads.
- Use the dependency graph for live hot-reload invalidation so reloading one source asset updates all dependent GPU/runtime resources, with rollback journals applied instead of only reported.

## Rendering

- Turn v28 render-graph advanced diagnostics into real DX11 bind/unbind barriers and resource alias lifetime validation around physical allocation slots.
- Replace GPU culling and Forward+ diagnostic counters with actual compute/CPU fallback culling and clustered light-bin buffers.
- Replace shadow coverage diagnostics with true cascaded shadow maps and per-cascade stabilization controls.
- Add normal/depth pre-pass options, SSR/SSGI experiments, real motion-vector rendering, and a more correct temporal AA resolve beyond the current FXAA-style resolve plus history blend.
- Move the high-resolution proof from source-frame resampling to true tiled offscreen rendering with per-tile camera jitter, selectable resolve filters, real EXR output, and async compression workers.
- Upgrade the VFX emitter/profile diagnostics into a dedicated renderer with soft particles, GPU simulation buffers, per-emitter sorting controls, motion vectors, temporal reprojection, and debug visualizers.
- Add motion vectors, temporal VFX reprojection, better TAA resolve, and exposure curves tuned for trailer captures.
- Investigate a DX12 or Vulkan backend once the DX11 renderer has a stable render graph contract.

## Runtime

- Turn the v30 vertical slice into a more game-like demo with collision-backed traversal, a failure/retry screen, controller/gamepad gameplay, a title/menu-to-gameplay flow, and more expressive objective gates.
- Add animation-state placeholders, footstep/pickup/anchor/completion events, pressure-hit feedback, objective-gate affordances, and clearer success/failure telemetry for the playable demo.
- Extend the job-system async IO helper into asset streaming, cancellation, priorities, file watching, and frame pacing diagnostics.
- Add serialization versioning, save-game separation, and deterministic scene IDs.
- Add physics, collision queries, controller movement, animation-driven character logic, and gameplay event routing.
- Add scripting reload boundaries, script state preservation, and a safer script asset format.
- Promote v28 sequencer diagnostics into a real sequencer with curve editing, clip lanes, shot thumbnails, bookmarks, nested sequences, clip blending, keyboard preview, undoable edits, and non-modal scrubbing.

## Audio

- Replace the v28 audio production diagnostics with real XAudio2 mixer voices, streamed music assets, spatial emitter components, attenuation-curve assets, calibrated meters, and content-driven amplitude analysis that drives VFX pulses from decoded audio buffers.

## Production

- Add more committed per-GPU/driver golden tolerance profiles from real verification machines and tighter local overrides.
- Promote signed baseline update approvals and the v28 diff-package diagnostics into a review command that updates goldens/performance thresholds with explicit approver intent and stores an auditable diff package.
- Promote the runtime report schema manifest into a versioned schema validation system with compatibility checks and explicit review diffs.
- Run packaged runtime smoke tests by default on a dedicated interactive desktop runner.
- Replace the release-readiness/bootstrapper command with an actual installer executable and publish symbols through a real symbol server endpoint.
- Expand OBS scene/profile generation plus v28 OBS command diagnostics into real OBS WebSocket automation, watermark toggles, capture metadata approval, and packaged vertical-slice launch presets.
