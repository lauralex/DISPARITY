# DISPARITY Roadmap

The current engine now has functional v4 versions of many requested followups. The next milestones should turn those prototypes into durable production systems.

## Editor

- Build a dedicated scene viewport with object picking and camera controls independent of gameplay input.
- Replace the button-based transform gizmo with real 3D translate/rotate/scale handles.
- Upgrade the current selection outline plus copy/paste/duplicate/delete support with multi-select, undo grouping, and command history labels.
- Add prefab override visualization, nested prefabs, prefab variants, and dependency-aware apply/revert.

## Asset Pipeline

- Expand the new `AssetDatabase` into real `.glb` cooking, import settings, cached cooked assets, and dependency graph invalidation.
- Convert glTF materials into engine material assets with texture slots for base color, normal, metallic-roughness, emissive, and occlusion.
- Add animation clips, skeleton assets, animation blending, retargeting, and GPU skinning palette upload.
- Add hot-reload dependency graphs so reloading one source asset updates all dependent runtime resources, not only the current prototype scene/script/material set.

## Rendering

- Integrate the new render graph scaffold into `Renderer` with explicit passes and resource lifetime tracking.
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

- Replace WinMM playback with XAudio2 or another production backend.
- Add real mixer voices, sends, snapshots, streamed music layers, spatial emitters, listener orientation, attenuation curves, and debugging meters.

## Production

- Extend the new GitHub Actions build/shader/package workflow with packaged smoke tests that launch from `dist` in an interactive desktop environment.
- Add installer-style packaging, symbols handling, versioned release artifacts, and crash upload plumbing beyond local `Saved/CrashLogs` reports.
