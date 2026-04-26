# DISPARITY Roadmap

The current engine now has functional versions of the requested followups. The next milestones should turn those prototypes into durable production systems.

## Editor

- Build a dedicated scene viewport with object picking and camera controls independent of gameplay input.
- Replace the button-based transform gizmo with real 3D translate/rotate/scale handles.
- Add selection outlines, copy/paste, duplicate, delete, multi-select, undo grouping, and command history labels.
- Add prefab override visualization, nested prefabs, prefab variants, and dependency-aware apply/revert.

## Asset Pipeline

- Add `.glb` support, external buffer/image dependency tracking, import settings, and cached cooked assets.
- Convert glTF materials into engine material assets with texture slots for base color, normal, metallic-roughness, emissive, and occlusion.
- Add animation clips, skeleton assets, animation blending, retargeting, and GPU skinning palette upload.
- Add hot-reload dependency graphs so reloading one source asset updates all dependent runtime resources.

## Rendering

- Move from direct renderer calls to a render graph with explicit passes and resource lifetime tracking.
- Add GPU frustum/occlusion culling and real clustered or Forward+ light binning.
- Replace the single shadow-map coverage mode with true cascaded shadow maps.
- Add normal/depth pre-pass options, SSR/SSGI experiments, color grading, motion vectors, and a more correct temporal AA resolve.
- Investigate a DX12 or Vulkan backend once the DX11 renderer has a stable render graph contract.

## Runtime

- Add a job system, async file IO, asset streaming, and frame pacing diagnostics.
- Add serialization versioning, save-game separation, and deterministic scene IDs.
- Add physics, collision queries, controller movement, animation-driven character logic, and gameplay event routing.
- Add scripting reload boundaries, script state preservation, and a safer script asset format.

## Audio

- Replace WinMM playback with XAudio2 or another production backend.
- Add real mixer voices, sends, snapshots, streamed music layers, spatial emitters, listener orientation, attenuation curves, and debugging meters.

## Production

- Add automated Debug/Release CI builds.
- Add shader compilation validation in CI.
- Add packaged smoke tests that launch from `dist`.
- Add installer-style packaging, version stamping, crash logs, and symbols handling.
