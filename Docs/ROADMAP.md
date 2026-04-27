# DISPARITY Roadmap

The current engine now has functional v21 versions of many requested followups. The next milestones should deepen the new graph, editor viewport, shot-track, VFX, asset, audio, capture, and verification scaffolds into fully durable production systems.

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

- Add hover latency histograms, last-frame/object cache visualization, and editor overlays for the new async GPU picking ring.
- Add viewport toolbar overlays for camera mode, render mode, object-ID/depth debug images, and capture status on top of the dedicated ImGui viewport texture.
- Expand the current mesh/ring/plane gizmo handles with depth-aware hover occlusion, constraint previews, numerical transform entry, pivot/orientation controls, and richer object-ID handle metadata.
- Upgrade the current selection outline plus copy/paste/duplicate/delete/multi-select support with undo grouping, command filters, and a filterable command history panel.
- Promote the new prefab variant/parent/nested metadata into nested-prefab instancing, multi-object override diffing, recursive dependency-aware apply/revert, and undo grouping.

## Asset Pipeline

- Load the new structured `.dglbpack` mesh/material/animation manifests directly through runtime resources.
- Promote exported texture slots into real texture binding for normal, metallic-roughness, emissive, and occlusion maps instead of metadata-only persistence.
- Expand the new blend/skinning API into animation clips, skeleton assets, retargeting, state machines, and GPU skinning constant/structured-buffer uploads.
- Use the dependency graph for hot-reload invalidation so reloading one source asset updates all dependent runtime resources, not only the current prototype scene/script/material set.

## Rendering

- Turn render-graph transition diagnostics into explicit DX11 bind/unbind barriers and add resource alias lifetime validation around the physical allocation slots.
- Add GPU frustum/occlusion culling and real clustered or Forward+ light binning.
- Replace the single shadow-map coverage mode with true cascaded shadow maps.
- Add normal/depth pre-pass options, SSR/SSGI experiments, motion vectors, and a more correct temporal AA resolve beyond the current FXAA-style resolve plus history blend.
- Replace the current async 2x photo worker with graph-owned offscreen high-resolution render targets, multi-sample resolves, and true tiled supersampling.
- Upgrade the VFX stats/simulation surface into a dedicated particle/ribbon renderer with soft particles, depth fade, sorting controls, real GPU simulation, motion vectors, and temporal reprojection.
- Add motion vectors, temporal VFX reprojection, better TAA resolve, and exposure curves tuned for trailer captures.
- Investigate a DX12 or Vulkan backend once the DX11 renderer has a stable render graph contract.

## Runtime

- Extend the job-system async IO helper into asset streaming, cancellation, priorities, file watching, and frame pacing diagnostics.
- Add serialization versioning, save-game separation, and deterministic scene IDs.
- Add physics, collision queries, controller movement, animation-driven character logic, and gameplay event routing.
- Add scripting reload boundaries, script state preservation, and a safer script asset format.
- Expand v4 shot metadata into editable easing curves, multi-track renderer/audio timelines, generated thumbnails, bookmarks, and non-modal preview scrubbing.

## Audio

- Expand the new XAudio2 tone path into real mixer voices, sends, snapshots, streamed music layers, spatial emitters, attenuation curves, production meters, and content-driven amplitude analysis that drives VFX pulses from actual audio.

## Production

- Add more committed per-GPU/driver golden tolerance profiles from real verification machines and tighter local overrides.
- Promote signed baseline approvals from metadata into a review command that updates goldens/performance thresholds with explicit approver intent.
- Run packaged runtime smoke tests by default on a dedicated interactive desktop runner.
- Replace the bootstrapper plan with an actual installer executable and publish symbols through a real symbol server.
- Expand trailer automation into OBS profile/scene generation, watermark toggles, capture metadata approval, and packaged vertical-slice launch presets.
