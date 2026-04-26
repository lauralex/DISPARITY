# DISPARITY Roadmap

The current engine now has functional v15 versions of many requested followups. The next milestones should turn those prototypes into durable production systems.

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

- Build a dedicated editor viewport render target that owns camera state, scene picking, and tool overlays.
- Replace the CPU stable-ID/OBB picking groundwork with a GPU object-ID and depth picking buffer for scene objects and gizmo handles.
- Expand the current mesh/ring/plane gizmo handles with depth-aware hover occlusion, constraint previews, numerical transform entry, pivot/orientation controls, and true object-ID handle picking.
- Upgrade the current selection outline plus copy/paste/duplicate/delete/multi-select support with undo grouping, command filters, and a filterable command history panel.
- Add prefab override visualization, nested prefabs, prefab variants, and dependency-aware apply/revert.

## Asset Pipeline

- Expand the new deterministic metadata cook into real `.glb` cooking, binary mesh/material packages, and dependency graph invalidation.
- Expand glTF-to-material export with texture slots for base color, normal, metallic-roughness, emissive, and occlusion.
- Add animation clips, skeleton assets, animation blending, retargeting, and GPU skinning palette upload.
- Add hot-reload dependency graphs so reloading one source asset updates all dependent runtime resources, not only the current prototype scene/script/material set.

## Rendering

- Move renderer execution onto the compiled render graph, then convert the current transition/alias/cull diagnostics into real DX11 resource ownership and pass dispatch decisions.
- Add GPU frustum/occlusion culling and real clustered or Forward+ light binning.
- Replace the single shadow-map coverage mode with true cascaded shadow maps.
- Add normal/depth pre-pass options, SSR/SSGI experiments, motion vectors, and a more correct temporal AA resolve beyond the current FXAA-style resolve plus history blend.
- Investigate a DX12 or Vulkan backend once the DX11 renderer has a stable render graph contract.

## Runtime

- Use the new job system for async file IO, asset streaming, and frame pacing diagnostics.
- Add serialization versioning, save-game separation, and deterministic scene IDs.
- Add physics, collision queries, controller movement, animation-driven character logic, and gameplay event routing.
- Add scripting reload boundaries, script state preservation, and a safer script asset format.

## Audio

- Replace WinMM playback with XAudio2 or another production backend behind the v15 snapshot/meter/listener surface.
- Add real mixer voices, sends, snapshots, streamed music layers, spatial emitters, attenuation curves, and debugging meters.

## Production

- Add per-GPU/driver golden tolerance profiles beyond the default profile, including adapter detection and tighter local overrides.
- Turn the local performance history summary into automated commit-to-commit regression gates with reviewed baseline update workflow.
- Extend CI with packaged runtime smoke tests by default when an interactive desktop runner is available.
- Add installer-style packaging, versioned release artifacts, symbol indexing, and real crash upload transport beyond local bundle creation.
