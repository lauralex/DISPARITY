# DISPARITY Roadmap

The current engine now has functional v19 versions of many requested followups. The next milestones should turn the new GPU picking, shot editing, capture, and dependency scaffolds into durable production systems while continuing toward a modern content pipeline and renderer architecture.

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

- Move GPU object-ID/depth picking to an async readback ring with hover latency diagnostics and last-frame/object cache visualization.
- Make editor viewport compositing use the dedicated viewport texture as the visible ImGui image instead of the swap-chain back buffer.
- Expand the current mesh/ring/plane gizmo handles with depth-aware hover occlusion, constraint previews, numerical transform entry, pivot/orientation controls, and richer object-ID handle metadata.
- Upgrade the current selection outline plus copy/paste/duplicate/delete/multi-select support with undo grouping, command filters, and a filterable command history panel.
- Add nested prefabs, prefab variants, multi-object override diffing, recursive dependency-aware apply/revert, and undo grouping.

## Asset Pipeline

- Replace `.dassetbin` source bundles with optimized cooked `.glb` mesh/material/animation payloads and runtime dependency graph invalidation.
- Expand glTF-to-material export with texture slots for base color, normal, metallic-roughness, emissive, and occlusion.
- Add animation clips, skeleton assets, animation blending, retargeting, and GPU skinning palette upload.
- Use the dependency graph for hot-reload invalidation so reloading one source asset updates all dependent runtime resources, not only the current prototype scene/script/material set.

## Rendering

- Move renderer execution fully onto graph-owned pass callbacks and bind transient resources from allocation handles instead of renderer member variables.
- Turn render-graph transition diagnostics into explicit DX11 bind/unbind barriers and add resource alias lifetime validation around the new physical allocation slots.
- Add GPU frustum/occlusion culling and real clustered or Forward+ light binning.
- Replace the single shadow-map coverage mode with true cascaded shadow maps.
- Add normal/depth pre-pass options, SSR/SSGI experiments, motion vectors, and a more correct temporal AA resolve beyond the current FXAA-style resolve plus history blend.
- Replace the remaining CPU 2x PPM photo path with graph-owned offscreen high-resolution render targets, multi-sample resolves, tiled supersampling, and async capture workers.
- Upgrade the rift VFX from mesh/billboard draw calls to a dedicated particle/ribbon renderer with soft particles, depth fade, sorting controls, and GPU simulation options.
- Add motion vectors, temporal VFX reprojection, better TAA resolve, and exposure curves tuned for trailer captures.
- Investigate a DX12 or Vulkan backend once the DX11 renderer has a stable render graph contract.

## Runtime

- Use the new job system for async file IO, asset streaming, and frame pacing diagnostics.
- Add serialization versioning, save-game separation, and deterministic scene IDs.
- Add physics, collision queries, controller movement, animation-driven character logic, and gameplay event routing.
- Add scripting reload boundaries, script state preservation, and a safer script asset format.
- Expand the shot-director editor with camera splines, easing curves, renderer setting tracks, audio cue tracks, shot thumbnails, bookmarks, and non-modal preview scrubbing.

## Audio

- Replace WinMM playback with XAudio2 behind the v16 snapshot/meter/listener/backend-selection surface.
- Add real mixer voices, sends, snapshots, streamed music layers, spatial emitters, attenuation curves, debugging meters, and amplitude analysis that can drive VFX pulses from actual content.

## Production

- Add committed per-GPU/driver golden tolerance profiles for known adapters and tighter local overrides.
- Turn the local baseline review and performance history summary into automated commit-to-commit regression gates with explicit baseline update approvals.
- Extend CI with packaged runtime smoke tests by default when an interactive desktop runner is available.
- Replace the installer payload manifest with a real installer bootstrapper, add symbol-server publishing, and add authenticated crash upload with retry/backoff.
- Expand trailer automation into OBS profile/scene generation, watermark toggles, capture metadata approval, and packaged vertical-slice launch presets.
