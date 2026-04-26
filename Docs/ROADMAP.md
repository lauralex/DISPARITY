# DISPARITY Roadmap

The current engine now has functional v17 versions of many requested followups. The next milestones should turn those prototypes into durable production systems and build on the new showcase-friendly presentation layer.

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

- Populate the dedicated editor object-ID/depth targets with scene-object IDs, player IDs, and gizmo handle IDs, then move viewport picking from CPU OBB tests to GPU readback.
- Make editor viewport compositing use the dedicated viewport texture as the visible ImGui image instead of the swap-chain back buffer.
- Expand the current mesh/ring/plane gizmo handles with depth-aware hover occlusion, constraint previews, numerical transform entry, pivot/orientation controls, and true object-ID handle picking.
- Upgrade the current selection outline plus copy/paste/duplicate/delete/multi-select support with undo grouping, command filters, and a filterable command history panel.
- Add prefab override visualization, nested prefabs, prefab variants, and dependency-aware apply/revert.

## Asset Pipeline

- Replace `.dassetbin` source bundles with optimized cooked `.glb` mesh/material/animation payloads and dependency graph invalidation.
- Expand glTF-to-material export with texture slots for base color, normal, metallic-roughness, emissive, and occlusion.
- Add animation clips, skeleton assets, animation blending, retargeting, and GPU skinning palette upload.
- Add hot-reload dependency graphs so reloading one source asset updates all dependent runtime resources, not only the current prototype scene/script/material set.

## Rendering

- Move renderer execution fully onto graph-owned pass callbacks and bind transient resources from allocation handles instead of renderer member variables.
- Turn render-graph transition diagnostics into explicit DX11 bind/unbind barriers and add resource alias lifetime validation around the new physical allocation slots.
- Add GPU frustum/occlusion culling and real clustered or Forward+ light binning.
- Replace the single shadow-map coverage mode with true cascaded shadow maps.
- Add normal/depth pre-pass options, SSR/SSGI experiments, motion vectors, and a more correct temporal AA resolve beyond the current FXAA-style resolve plus history blend.
- Add a real VFX layer for the rift: billboard particles, ribbon trails, lightning arcs, volumetric fog cards, lens dirt, and material emissive controls instead of HDR albedo-only glow.
- Add trailer/photo-mode rendering controls: camera splines, depth of field, cinematic bars, high-resolution captures, and deterministic shot playback.
- Investigate a DX12 or Vulkan backend once the DX11 renderer has a stable render graph contract.

## Runtime

- Use the new job system for async file IO, asset streaming, and frame pacing diagnostics.
- Add serialization versioning, save-game separation, and deterministic scene IDs.
- Add physics, collision queries, controller movement, animation-driven character logic, and gameplay event routing.
- Add scripting reload boundaries, script state preservation, and a safer script asset format.
- Add a shot director asset format so camera moves, renderer settings, audio cues, and scene beats can be authored for repeatable trailers and vertical-slice demos.

## Audio

- Replace WinMM playback with XAudio2 behind the v16 snapshot/meter/listener/backend-selection surface.
- Add real mixer voices, sends, snapshots, streamed music layers, spatial emitters, attenuation curves, and debugging meters.

## Production

- Add committed per-GPU/driver golden tolerance profiles for known adapters and tighter local overrides.
- Turn the local baseline review and performance history summary into automated commit-to-commit regression gates with explicit baseline update approvals.
- Extend CI with packaged runtime smoke tests by default when an interactive desktop runner is available.
- Replace the installer payload manifest with a real installer bootstrapper, add symbol-server publishing, and add authenticated crash upload with retry/backoff.
