# DISPARITY Roadmap

The current engine now has functional v9 versions of many requested followups. The next milestones should turn those prototypes into durable production systems.

## Editor

- Build a dedicated editor viewport render target that owns camera state, scene picking, and tool overlays.
- Add object-ID selection and object-ID gizmo handle picking to replace the current ray-test approximations.
- Expand the current mesh/ring/plane gizmo handles with depth-aware hover states, constraint previews, numerical transform entry, and pivot/orientation controls.
- Upgrade the current selection outline plus copy/paste/duplicate/delete/multi-select support with undo grouping, command filters, and visible command history.
- Add prefab override visualization, nested prefabs, prefab variants, and dependency-aware apply/revert.

## Asset Pipeline

- Expand the new `AssetDatabase` metadata cook into real `.glb` cooking and dependency graph invalidation.
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

- Replace WinMM playback with XAudio2 or another production backend.
- Add real mixer voices, sends, snapshots, streamed music layers, spatial emitters, listener orientation, attenuation curves, and debugging meters.

## Production

- Extend the new GitHub Actions build/shader/package workflow with packaged smoke tests that launch from `dist` in an interactive desktop environment.
- Add installer-style packaging, symbols handling, versioned release artifacts, and crash upload plumbing beyond local `Saved/CrashLogs` reports.
