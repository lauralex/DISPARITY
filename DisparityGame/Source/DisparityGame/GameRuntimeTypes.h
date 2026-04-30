#pragma once

#include "DisparityGame/GameFollowupCatalog.h"

#include <Disparity/Disparity.h>

#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace DisparityGame
{
    inline constexpr float Pi = 3.1415926535f;
    inline constexpr size_t InvalidIndex = std::numeric_limits<size_t>::max();
    inline constexpr uint32_t EditorPickPlayerId = 1u;
    inline constexpr uint32_t EditorPickSceneObjectBase = 1000u;
    inline constexpr uint32_t EditorPickGizmoAxisBase = 0x10000000u;
    inline constexpr uint32_t EditorPickGizmoPlaneBase = 0x20000000u;

    // Shared data shapes for the prototype layer. Keeping them outside DisparityGame.cpp
    // makes the layer read more like orchestration and less like a private database.

struct RuntimeVerificationConfig
{
    bool Enabled = false;
    bool CaptureFrame = true;
    bool InputPlayback = true;
    bool EnforceBudgets = true;
    bool UseBaseline = true;
    uint32_t TargetFrames = 90;
    double CpuFrameBudgetMs = 120.0;
    double GpuFrameBudgetMs = 50.0;
    double PassBudgetMs = 60.0;
    std::filesystem::path ReportPath = "Saved/Verification/runtime_verify.txt";
    std::filesystem::path CapturePath = "Saved/Verification/runtime_capture.ppm";
    std::filesystem::path ReplayPath = "Assets/Verification/Prototype.dreplay";
    std::filesystem::path BaselinePath = "Assets/Verification/RuntimeBaseline.dverify";
};


struct EditState
{
    Disparity::Scene SceneData;
    DirectX::XMFLOAT3 PlayerPosition = {};
    float PlayerYaw = 0.0f;
    Disparity::Material PlayerBodyMaterial;
    Disparity::Material PlayerHeadMaterial;
    Disparity::RendererSettings RendererSettings;
};

struct HistoryEntry
{
    std::string Label;
    EditState State;
};

struct RuntimeBudgetStats
{
    uint32_t CpuSamples = 0;
    uint32_t GpuSamples = 0;
    double CpuFrameMaxMs = 0.0;
    double CpuFrameAverageMs = 0.0;
    double GpuFrameMaxMs = 0.0;
    double GpuFrameAverageMs = 0.0;
    double PassCpuMaxMs = 0.0;
    double PassGpuMaxMs = 0.0;
    std::string PassCpuMaxName;
    std::string PassGpuMaxName;
};

struct RuntimePlaybackStats
{
    bool Started = false;
    bool Finished = false;
    uint32_t Steps = 0;
    DirectX::XMFLOAT3 StartPosition = {};
    DirectX::XMFLOAT3 LastPosition = {};
    DirectX::XMFLOAT3 EndPosition = {};
    float NetDistance = 0.0f;
    float Distance = 0.0f;
};

struct RuntimeReplayStep
{
    uint32_t StartFrame = 0;
    uint32_t EndFrame = 0;
    DirectX::XMFLOAT3 MoveInput = {};
    float CameraYawDelta = 0.0f;
    float CameraPitchDelta = 0.0f;
};

struct TrailerShotKey
{
    float Time = 0.0f;
    DirectX::XMFLOAT3 Position = {};
    DirectX::XMFLOAT3 Target = {};
    float Focus = 0.985f;
    float DofStrength = 0.45f;
    float LensDirt = 0.58f;
    float Letterbox = 0.075f;
    float EaseIn = 0.35f;
    float EaseOut = 0.35f;
    float RendererPulse = 0.0f;
    float AudioCue = 0.0f;
    std::string Bookmark;
    std::string SplineMode = "catmull";
    std::string TimelineLane = "camera";
    DirectX::XMFLOAT3 ThumbnailTint = { 0.42f, 0.82f, 1.0f };
    std::string EasingCurve = "smoothstep";
    std::string RendererTrack = "post_stack";
    std::string AudioTrack = "cue";
    std::string ThumbnailPath;
    std::string ClipLane = "camera_main";
    std::string NestedSequence = "main";
    float HoldSeconds = 0.0f;
    std::string ShotRole = "establishing";
};

struct GpuPickVisualizationState
{
    bool HasCache = false;
    bool LastResolved = false;
    uint32_t LastObjectId = 0;
    uint32_t LastX = 0;
    uint32_t LastY = 0;
    uint32_t LastLatencyFrames = 0;
    uint32_t LastResolvedFrame = 0;
    uint32_t CacheHits = 0;
    uint32_t CacheMisses = 0;
    uint32_t BusySkips = 0;
    uint32_t PendingSlots = 0;
    float LastDepth = 1.0f;
    std::string LastObjectName = "None";
    std::array<uint32_t, 8> LatencyBuckets = {};
};

struct RiftVfxParticle
{
    DirectX::XMFLOAT3 Position = {};
    float Size = 0.1f;
    float Depth = 0.0f;
    float DepthFade = 1.0f;
    float Roll = 0.0f;
    bool Hot = false;
};

struct RiftVfxRibbonSegment
{
    DirectX::XMFLOAT3 Position = {};
    DirectX::XMFLOAT3 Rotation = {};
    DirectX::XMFLOAT3 Scale = {};
    float Depth = 0.0f;
};

struct CookedPackageRuntimeResource
{
    bool Loaded = false;
    bool GpuReady = false;
    bool ReloadRollbackReady = false;
    bool StreamingReady = false;
    bool RetargetingReady = false;
    uint32_t Meshes = 0;
    uint32_t Primitives = 0;
    uint32_t Materials = 0;
    uint32_t Nodes = 0;
    uint32_t Animations = 0;
    uint32_t Skins = 0;
    uint32_t Dependencies = 0;
    uint32_t DependencyInvalidationPreviewCount = 0;
    uint32_t GpuMeshResources = 0;
    uint32_t GpuMaterialResources = 0;
    uint32_t AnimationClips = 0;
    uint32_t TextureBindings = 0;
    uint32_t SkinningPaletteUploads = 0;
    uint32_t RetargetingMaps = 0;
    uint32_t StreamingPriorityLevels = 0;
    uint32_t LiveInvalidationTickets = 0;
    uint32_t RollbackJournalEntries = 0;
    uint64_t EstimatedUploadBytes = 0;
    std::filesystem::path Path;
};

struct RiftVfxSystemStats
{
    uint32_t FogCards = 0;
    uint32_t Particles = 0;
    uint32_t Ribbons = 0;
    uint32_t LightningArcs = 0;
    uint32_t SoftParticleCandidates = 0;
    uint32_t SortedBatches = 0;
    uint32_t GpuSimulationBatches = 0;
    uint32_t MotionVectorCandidates = 0;
    uint32_t TemporalReprojectionSamples = 0;
    uint32_t DepthFadeParticles = 0;
    uint32_t GpuParticleDispatches = 0;
    uint32_t TemporalHistorySamples = 0;
    uint32_t EmitterCount = 0;
    uint32_t SortBuckets = 0;
    uint32_t GpuBufferBytes = 0;
};

struct PpmImage
{
    uint32_t Width = 0;
    uint32_t Height = 0;
    std::vector<unsigned char> Pixels;
};

struct HighResolutionCaptureMetrics
{
    std::string PresetName = "Trailer2x";
    uint32_t Scale = 2;
    uint32_t Tiles = 4;
    uint32_t MsaaSamples = 4;
    uint32_t ResolveSamples = 4;
    std::string ResolveFilter = "tent";
    bool AsyncCompressionReady = true;
    bool ExrOutputPlanned = true;
};

struct HighResolutionCapturePresetProfile
{
    std::string Name = "Trailer2x";
    uint32_t Scale = 2;
    uint32_t Tiles = 4;
    uint32_t MsaaSamples = 4;
    uint32_t ResolveSamples = 4;
    std::string ResolveFilter = "tent";
    bool AsyncCompressionReady = true;
    bool ExrOutputPlanned = true;
};

struct ViewportOverlaySettings
{
    bool Enabled = true;
    bool ShowGpuPick = true;
    bool ShowReadback = true;
    bool ShowCapture = true;
    bool ShowDebugThumbnails = true;
    bool Pinned = true;
};

struct TransformPrecisionState
{
    float Step = 0.05f;
    int PivotMode = 0;
    int OrientationMode = 0;
};

struct RiftVfxRendererProfile
{
    bool SoftParticles = true;
    bool DepthFade = true;
    bool Sorted = true;
    bool GpuSimulation = true;
    bool MotionVectors = true;
    bool TemporalReprojection = true;
    uint32_t MaxParticles = 512;
    uint32_t MaxRibbons = 96;
};

struct RiftVfxEmitterProfile
{
    bool SoftParticleDepthFade = true;
    bool PerEmitterSorting = true;
    bool GpuSimulationBuffers = true;
    bool TemporalReprojection = true;
    uint32_t EmitterCount = 5;
    uint32_t SortBuckets = 6;
    uint32_t GpuBufferBytesPerParticle = 64;
    uint32_t GpuBufferBytesPerRibbon = 96;
};

struct AudioMeterCalibrationProfile
{
    float ReferencePeakDb = -12.0f;
    float ReferenceRmsDb = -18.0f;
    float AttackMs = 8.0f;
    float ReleaseMs = 120.0f;
};

struct ProductionFollowupPoint
{
    const char* Key = "";
    const char* Domain = "";
    const char* Description = "";
};

struct EditorPreferenceProfile
{
    bool ViewportHudRows = true;
    bool TransformPrecision = true;
    bool CommandHistoryFilter = true;
    bool DockLayout = true;
    bool UserPreferencePath = true;
    std::filesystem::path PreferencePath = "Saved/Editor/editor_preferences.json";
    std::filesystem::path ProfileDirectory = "Saved/Editor/Profiles";
};

struct ViewportToolbarProfile
{
    bool CameraMode = true;
    bool RenderDebugMode = true;
    bool CaptureState = true;
    bool ObjectIdOverlay = true;
    bool DepthOverlay = true;
};

struct GizmoAdvancedProfile
{
    bool DepthAwareHover = true;
    bool ConstraintPreview = true;
    bool NumericEntry = true;
    bool PivotOrientationEditing = true;
    bool HandleMetadata = true;
};

struct AssetPipelineReadinessProfile
{
    bool GpuMeshUpload = true;
    bool TextureSlotBinding = true;
    bool AnimationResources = true;
    bool DependencyPreview = true;
    bool ReloadRollback = true;
};

struct RenderingRoadmapProfile
{
    bool ExplicitBarriers = true;
    bool AliasLifetimeValidation = true;
    bool GpuCullingPlan = true;
    bool ForwardPlusPlan = true;
    bool CascadedShadowPlan = true;
};

struct CaptureVfxReadinessProfile
{
    bool TiledOffscreenPlan = true;
    bool ResolveFilterCatalog = true;
    bool ExrOutputPlan = true;
    bool AsyncCompressionPlan = true;
    bool VfxDebugVisualizers = true;
};

struct SequencerAudioReadinessProfile
{
    bool CurveEditorPlan = true;
    bool ClipBlendingPlan = true;
    bool BookmarkTracks = true;
    bool StreamedMusicPlan = true;
    bool ContentPulsePlan = true;
};

struct ProductionAutomationReadinessProfile
{
    bool GoldenProfileExpansion = true;
    bool SchemaVersioning = true;
    bool InteractiveCiGate = true;
    bool InstallerArtifact = true;
    bool ObsWebSocketAutomation = true;
};

struct EditorWorkflowDiagnostics
{
    bool ProfileImportExport = false;
    bool ProfileDiffing = false;
    bool DockLayoutPersistence = false;
    bool ToolbarCustomization = false;
    bool CaptureProgress = false;
    bool CommandMacroReview = false;
    uint32_t WorkspacePresets = 0;
    uint32_t ProfileDiffFields = 0;
    uint32_t CommandMacroSteps = 0;
    std::string ActiveWorkspacePreset = "Editor";
    std::filesystem::path ExportPath;
};

struct AssetPipelinePromotionDiagnostics
{
    bool OptimizedGpuPackageLoaded = false;
    bool LiveInvalidationReady = false;
    bool ReloadRollbackJournal = false;
    bool TextureBindingsReady = false;
    bool RetargetingReady = false;
    bool GpuSkinningReady = false;
    uint32_t GpuMeshes = 0;
    uint32_t TextureBindings = 0;
    uint32_t AnimationClips = 0;
    uint32_t InvalidationTickets = 0;
    uint32_t RollbackEntries = 0;
    uint32_t StreamingPriorityLevels = 0;
};

struct RenderingAdvancedDiagnostics
{
    bool ExplicitBindBarriers = false;
    bool AliasLifetimeValidation = false;
    bool GpuCullingBuckets = false;
    bool ForwardPlusLightBins = false;
    bool CascadedShadowCascades = false;
    bool MotionVectorTargets = false;
    bool TemporalResolveQuality = false;
    bool SsrSsgiExperiment = false;
    uint32_t BarrierCount = 0;
    uint32_t AliasValidations = 0;
    uint32_t CullingBuckets = 0;
    uint32_t LightBins = 0;
    uint32_t ShadowCascades = 0;
    uint32_t MotionVectorTargetsCount = 0;
};

struct RuntimeSequencerDiagnostics
{
    bool AssetStreamingRequests = false;
    bool CancellationTokens = false;
    bool PriorityQueues = false;
    bool FileWatchers = false;
    bool ScriptReloadBoundary = false;
    bool ScriptStatePreservation = false;
    bool SequencerClipBlending = false;
    bool KeyboardPreview = false;
    uint32_t StreamingRequests = 0;
    uint32_t CancellationTokenCount = 0;
    uint32_t PriorityLevels = 0;
    uint32_t FileWatchCount = 0;
    uint32_t ScriptStateSlots = 0;
    uint32_t ClipBlendPairs = 0;
    uint32_t KeyboardBindings = 0;
};

struct AudioProductionFeatureDiagnostics
{
    bool XAudio2MixerVoices = false;
    bool StreamedMusicAssets = false;
    bool SpatialEmitterComponents = false;
    bool AttenuationCurveAssets = false;
    bool CalibratedMeters = false;
    bool ContentAmplitudePulses = false;
    uint32_t MixerVoices = 0;
    uint32_t StreamedMusicAssetsCount = 0;
    uint32_t SpatialEmitters = 0;
    uint32_t AttenuationCurves = 0;
    uint32_t CalibratedMetersCount = 0;
    uint32_t ContentPulseInputs = 0;
};

struct ProductionPublishingDiagnostics
{
    bool PerGpuGoldenProfiles = false;
    bool BaselineDiffPackage = false;
    bool SchemaCompatibility = false;
    bool InteractiveRunner = false;
    bool SignedInstallerArtifact = false;
    bool SymbolServerEndpoint = false;
    bool ObsWebSocketCommands = false;
    uint32_t GoldenProfiles = 0;
    uint32_t SchemaMetrics = 0;
    uint32_t ObsCommands = 0;
    std::filesystem::path DiffPackagePath;
};

struct PublicDemoShard
{
    DirectX::XMFLOAT3 Position = {};
    DirectX::XMFLOAT3 Color = { 0.3f, 0.9f, 1.0f };
    float Phase = 0.0f;
    bool Collected = false;
};

struct PublicDemoSentinel
{
    float Radius = 4.0f;
    float Speed = 1.0f;
    float Phase = 0.0f;
    float Height = 0.8f;
};

enum class PublicDemoStage
{
    CollectShards,
    ActivateAnchors,
    TuneResonance,
    StabilizeRelays,
    ExtractionReady,
    Completed
};

enum class PublicDemoMenuState
{
    Playing,
    Paused,
    Completed
};

enum class PublicDemoAnimationState
{
    Idle,
    Walk,
    Sprint,
    Dash,
    Stabilize,
    Failure,
    Complete
};

struct PublicDemoAnchor
{
    DirectX::XMFLOAT3 Position = {};
    DirectX::XMFLOAT3 Color = { 0.95f, 0.42f, 1.0f };
    float Phase = 0.0f;
    bool Activated = false;
};

struct PublicDemoResonanceGate
{
    DirectX::XMFLOAT3 Position = {};
    DirectX::XMFLOAT3 Color = { 0.38f, 1.0f, 0.86f };
    float Radius = 1.35f;
    float Phase = 0.0f;
    bool Tuned = false;
};

struct PublicDemoPhaseRelay
{
    DirectX::XMFLOAT3 Position = {};
    DirectX::XMFLOAT3 Color = { 1.0f, 0.75f, 0.28f };
    float Radius = 1.45f;
    float Phase = 0.0f;
    float OrbitRadius = 0.65f;
    bool Stabilized = false;
};

struct PublicDemoCollisionObstacle
{
    DirectX::XMFLOAT3 Position = {};
    DirectX::XMFLOAT3 HalfExtents = { 0.5f, 0.5f, 0.5f };
    DirectX::XMFLOAT3 Color = { 0.45f, 0.72f, 1.0f };
    bool Traversable = false;
    bool Used = false;
};

enum class PublicDemoEnemyArchetype
{
    Hunter,
    Warden,
    Disruptor
};

struct PublicDemoEnemy
{
    DirectX::XMFLOAT3 Position = {};
    DirectX::XMFLOAT3 Home = {};
    DirectX::XMFLOAT3 PatrolOffset = { 0.86f, 0.0f, 0.64f };
    PublicDemoEnemyArchetype Archetype = PublicDemoEnemyArchetype::Hunter;
    float AggroRadius = 4.0f;
    float ContactRadius = 0.82f;
    float Speed = 2.2f;
    float Phase = 0.0f;
    float StunTimer = 0.0f;
    float TelegraphTimer = 0.0f;
    float HitReactionTimer = 0.0f;
    float LineOfSightScore = 0.0f;
    bool Alerted = false;
    bool Evaded = false;
    bool HasLineOfSight = false;
    bool Telegraphing = false;
};

struct PublicDemoCueDefinition
{
    std::string Name;
    std::string Bus = "SFX";
    float FrequencyHz = 440.0f;
    float DurationSeconds = 0.06f;
    float Volume = 0.10f;
};

struct PublicDemoGamepadState
{
    bool Available = false;
    bool Connected = false;
    bool StartPressed = false;
    bool SouthPressed = false;
    bool LeftShoulderDown = false;
    DirectX::XMFLOAT2 Move = {};
    WORD Buttons = 0;
};

struct PublicDemoCheckpoint
{
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 6.0f };
    float Yaw = Pi;
    PublicDemoStage Stage = PublicDemoStage::CollectShards;
    uint32_t ShardsCollected = 0;
    uint32_t AnchorsActivated = 0;
    uint32_t RelaysStabilized = 0;
    bool Valid = false;
};

struct PublicDemoDiagnostics
{
    bool ObjectiveLoopReady = false;
    bool AllShardsCollected = false;
    bool ExtractionCompleted = false;
    bool HudRendered = false;
    bool BeaconsRendered = false;
    bool SentinelPressure = false;
    bool SprintEnergy = false;
    bool ChainedObjectives = false;
    bool AnchorPuzzleComplete = false;
    bool CheckpointReady = false;
    bool RetryReady = false;
    bool DirectorPanelReady = false;
    bool RouteTelemetry = false;
    bool EventQueueReady = false;
    bool GamepadInputSurface = false;
    bool ResonanceGateComplete = false;
    bool CollisionTraversalReady = false;
    bool FailureScreenReady = false;
    bool GameplayEventsRouted = false;
    bool FootstepCueReady = false;
    bool PressureCueReady = false;
    bool MixSafeCueRouting = false;
    bool ContentPulseLinked = false;
    bool PhaseRelayComplete = false;
    bool RelayOverchargeReady = false;
    bool ComplexRouteReady = false;
    bool TraversalLoopReady = false;
    bool ComboObjectiveReady = false;
    bool DirectorPhaseRelayReady = false;
    bool EnemyBehaviorReady = false;
    bool GamepadMenuReady = false;
    bool FailurePresentationReady = false;
    bool ContentAudioReady = false;
    bool AnimationHookReady = false;
    bool EnemyArchetypeReady = false;
    bool EnemyLineOfSightReady = false;
    bool EnemyTelegraphReady = false;
    bool EnemyHitReactionReady = false;
    bool ControllerPolishReady = false;
    bool AnimationBlendTreeReady = false;
    bool AccessibilityReady = false;
    bool RenderingAAAReadiness = false;
    bool ProductionAAAReadiness = false;
    uint32_t ShardsTotal = 0;
    uint32_t ShardsCollected = 0;
    uint32_t AnchorsTotal = 0;
    uint32_t AnchorsActivated = 0;
    uint32_t ResonanceGatesTotal = 0;
    uint32_t ResonanceGatesTuned = 0;
    uint32_t PhaseRelaysTotal = 0;
    uint32_t PhaseRelaysStabilized = 0;
    uint32_t RelayOverchargeWindows = 0;
    uint32_t RelayBridgeDraws = 0;
    uint32_t TraversalMarkers = 0;
    uint32_t ComboChainSteps = 0;
    uint32_t BeaconDraws = 0;
    uint32_t HudFrames = 0;
    uint32_t SentinelTicks = 0;
    uint32_t ObjectiveStageTransitions = 0;
    uint32_t RetryCount = 0;
    uint32_t CheckpointCount = 0;
    uint32_t DirectorFrames = 0;
    uint32_t EventCount = 0;
    uint32_t PressureHits = 0;
    uint32_t FootstepEvents = 0;
    uint32_t GameplayEventRoutes = 0;
    uint32_t CollisionSolves = 0;
    uint32_t TraversalActions = 0;
    uint32_t EnemyChaseTicks = 0;
    uint32_t EnemyEvades = 0;
    uint32_t EnemyContacts = 0;
    uint32_t GamepadFrames = 0;
    uint32_t MenuTransitions = 0;
    uint32_t FailurePresentationFrames = 0;
    uint32_t ContentAudioCues = 0;
    uint32_t AnimationStateChanges = 0;
    uint32_t EnemyArchetypes = 0;
    uint32_t EnemyLineOfSightChecks = 0;
    uint32_t EnemyTelegraphs = 0;
    uint32_t EnemyHitReactions = 0;
    uint32_t ControllerGroundedFrames = 0;
    uint32_t ControllerSlopeSamples = 0;
    uint32_t ControllerMaterialSamples = 0;
    uint32_t ControllerMovingPlatformFrames = 0;
    uint32_t ControllerCameraCollisionFrames = 0;
    uint32_t AnimationClipEvents = 0;
    uint32_t AnimationBlendSamples = 0;
    uint32_t AccessibilityToggles = 0;
    uint32_t RenderingReadinessItems = 0;
    uint32_t ProductionReadinessItems = 0;
    float CompletionTimeSeconds = 0.0f;
    float Stability = 0.0f;
    float Focus = 0.0f;
    float ObjectiveDistance = 0.0f;
};

struct PublicDemoStateSnapshot
{
    std::array<PublicDemoShard, 6> Shards = {};
    std::array<PublicDemoAnchor, 3> Anchors = {};
    std::array<PublicDemoResonanceGate, 2> ResonanceGates = {};
    std::array<PublicDemoPhaseRelay, 3> PhaseRelays = {};
    std::array<PublicDemoCollisionObstacle, 5> Obstacles = {};
    std::array<PublicDemoEnemy, 3> Enemies = {};
    PublicDemoCheckpoint Checkpoint = {};
    std::deque<std::string> Events = {};
    DirectX::XMFLOAT3 PlayerPosition = {};
    float PlayerYaw = 0.0f;
    float Stability = 0.0f;
    float Focus = 0.0f;
    float Elapsed = 0.0f;
    float Surge = 0.0f;
    uint32_t Collected = 0;
    uint32_t AnchorsActivated = 0;
    uint32_t StageTransitions = 0;
    uint32_t RetryCount = 0;
    uint32_t CheckpointCount = 0;
    uint32_t PressureHitCount = 0;
    uint32_t FootstepEventCount = 0;
    uint32_t GameplayEventRouteCount = 0;
    uint32_t RelayOverchargeWindowCount = 0;
    uint32_t RelayBridgeDrawCount = 0;
    uint32_t TraversalMarkerCount = 0;
    uint32_t ComboChainStepCount = 0;
    uint32_t CollisionSolveCount = 0;
    uint32_t TraversalVaultCount = 0;
    uint32_t TraversalDashCount = 0;
    uint32_t EnemyChaseTickCount = 0;
    uint32_t EnemyEvadeCount = 0;
    uint32_t EnemyContactCount = 0;
    uint32_t EnemyLineOfSightCheckCount = 0;
    uint32_t EnemyTelegraphCount = 0;
    uint32_t EnemyHitReactionCount = 0;
    uint32_t ControllerGroundedFrameCount = 0;
    uint32_t ControllerSlopeSampleCount = 0;
    uint32_t ControllerMaterialSampleCount = 0;
    uint32_t ControllerMovingPlatformFrameCount = 0;
    uint32_t ControllerCameraCollisionFrameCount = 0;
    uint32_t AnimationClipEventCount = 0;
    uint32_t AnimationBlendSampleCount = 0;
    uint32_t AccessibilityToggleCount = 0;
    uint32_t RenderingReadinessCount = 0;
    uint32_t ProductionReadinessCount = 0;
    uint32_t GamepadFrameCount = 0;
    uint32_t MenuTransitionCount = 0;
    uint32_t FailurePresentationFrameCount = 0;
    uint32_t ContentAudioCueCount = 0;
    uint32_t AnimationStateChangeCount = 0;
    bool ExtractionReady = false;
    bool Completed = false;
    PublicDemoMenuState MenuState = PublicDemoMenuState::Playing;
    PublicDemoAnimationState AnimationState = PublicDemoAnimationState::Idle;
    PublicDemoStage Stage = PublicDemoStage::CollectShards;
};

inline constexpr size_t V25ProductionPointCount = 40;
inline constexpr size_t V28DiversifiedPointCount = 36;
inline constexpr size_t V29PublicDemoPointCount = 30;
inline constexpr size_t V30VerticalSlicePointCount = 36;
inline constexpr size_t V31DiversifiedPointCount = 30;
inline constexpr size_t V32RoadmapPointCount = 60;
inline constexpr size_t V33PlayableDemoPointCount = 50;
inline constexpr size_t V34AAAFoundationPointCount = 60;
inline constexpr size_t V35EngineArchitecturePointCount = 50;
inline constexpr size_t V38DiversifiedPointCount = V38DiversifiedBatchPointCount;
inline constexpr size_t V39RoadmapPointCount = V39RoadmapBatchPointCount;
inline constexpr size_t V40DiversifiedPointCount = V40DiversifiedBatchPointCount;
inline constexpr size_t V41BreadthPointCount = V41BreadthBatchPointCount;
inline constexpr size_t V42ProductionSurfacePointCount = V42ProductionSurfaceBatchPointCount;
inline constexpr size_t V43LiveValidationPointCount = V43LiveValidationBatchPointCount;
inline constexpr size_t V44RuntimeCatalogPointCount = V44RuntimeCatalogBatchPointCount;
inline constexpr size_t V45LiveCatalogPointCount = V45LiveCatalogBatchPointCount;
inline constexpr size_t V46CatalogActionPreviewPointCount = V46CatalogActionPreviewBatchPointCount;
inline constexpr size_t V47CatalogExecutionPointCount = V47CatalogExecutionBatchPointCount;
inline constexpr size_t V48ActionDirectorPointCount = V48ActionDirectorBatchPointCount;
inline constexpr size_t V49ActionMutationPointCount = V49ActionMutationBatchPointCount;
inline constexpr size_t PublicDemoShardCount = 6;
inline constexpr size_t PublicDemoAnchorCount = 3;
inline constexpr size_t PublicDemoResonanceGateCount = 2;
inline constexpr size_t PublicDemoPhaseRelayCount = 3;
inline constexpr size_t PublicDemoObstacleCount = 5;
inline constexpr size_t PublicDemoEnemyCount = 3;


struct RuntimeBaseline
{
    uint32_t ExpectedCaptureWidth = 1280;
    uint32_t ExpectedCaptureHeight = 720;
    uint32_t MinEditorPickTests = 1;
    uint32_t MinGizmoPickTests = 1;
    uint32_t MinGizmoDragTests = 0;
    uint32_t MinSceneReloads = 0;
    uint32_t MinSceneSaves = 0;
    uint32_t MinPostDebugViews = 0;
    uint32_t MinShowcaseFrames = 0;
    uint32_t MinTrailerFrames = 0;
    uint32_t MinHighResCaptures = 0;
    uint32_t MinRiftVfxDraws = 0;
    uint32_t MinAudioBeatPulses = 0;
    uint32_t MinAudioSnapshotTests = 0;
    uint32_t MinRenderGraphAllocations = 4;
    uint32_t MinRenderGraphAliasedResources = 1;
    uint32_t MinRenderGraphCallbacks = 5;
    uint32_t MinRenderGraphBarriers = 1;
    uint32_t MinRenderGraphResourceBindings = 8;
    uint32_t MinAsyncIoTests = 1;
    uint32_t MinMaterialTextureSlotTests = 1;
    uint32_t MinPrefabVariantTests = 1;
    uint32_t MinShotDirectorTests = 1;
    uint32_t MinShotSplineTests = 1;
    uint32_t MinAudioAnalysisTests = 1;
    uint32_t MinXAudio2BackendTests = 1;
    uint32_t MinVfxSystemTests = 1;
    uint32_t MinGpuVfxSimulationTests = 1;
    uint32_t MinAnimationSkinningTests = 1;
    uint32_t MinGpuPickHoverCacheTests = 1;
    uint32_t MinGpuPickLatencyHistogramTests = 1;
    uint32_t MinShotTimelineTrackTests = 1;
    uint32_t MinShotThumbnailTests = 1;
    uint32_t MinShotPreviewScrubTests = 1;
    uint32_t MinGraphHighResCaptureTests = 1;
    uint32_t MinCookedPackageTests = 1;
    uint32_t MinAssetInvalidationTests = 1;
    uint32_t MinNestedPrefabTests = 1;
    uint32_t MinAudioProductionTests = 1;
    uint32_t MinViewportOverlayTests = 1;
    uint32_t MinHighResResolveTests = 1;
    uint32_t MinViewportHudControlTests = 1;
    uint32_t MinTransformPrecisionTests = 1;
    uint32_t MinCommandHistoryFilterTests = 1;
    uint32_t MinRuntimeSchemaManifestTests = 1;
    uint32_t MinShotSequencerTests = 1;
    uint32_t MinVfxRendererProfileTests = 1;
    uint32_t MinCookedGpuResourceTests = 1;
    uint32_t MinDependencyInvalidationTests = 1;
    uint32_t MinAudioMeterCalibrationTests = 1;
    uint32_t MinReleaseReadinessTests = 1;
    uint32_t MinV25ProductionPoints = static_cast<uint32_t>(V25ProductionPointCount);
    uint32_t MinEditorPreferencePersistenceTests = 1;
    uint32_t MinViewportToolbarTests = 1;
    uint32_t MinEditorPreferenceProfileTests = 1;
    uint32_t MinCapturePresetTests = 1;
    uint32_t MinVfxEmitterProfileTests = 1;
    uint32_t MinCookedDependencyPreviewTests = 1;
    uint32_t MinEditorWorkflowTests = 1;
    uint32_t MinAssetPipelinePromotionTests = 1;
    uint32_t MinRenderingAdvancedTests = 1;
    uint32_t MinRuntimeSequencerFeatureTests = 1;
    uint32_t MinAudioProductionFeatureTests = 1;
    uint32_t MinProductionPublishingTests = 1;
    uint32_t MinV28DiversifiedPoints = static_cast<uint32_t>(V28DiversifiedPointCount);
    uint32_t MinPublicDemoTests = 1;
    uint32_t MinPublicDemoShardPickups = static_cast<uint32_t>(PublicDemoShardCount);
    uint32_t MinPublicDemoHudFrames = 1;
    uint32_t MinPublicDemoBeaconDraws = 1;
    uint32_t MinV29PublicDemoPoints = static_cast<uint32_t>(V29PublicDemoPointCount);
    uint32_t MinPublicDemoObjectiveStages = 3;
    uint32_t MinPublicDemoAnchorActivations = static_cast<uint32_t>(PublicDemoAnchorCount);
    uint32_t MinPublicDemoRetries = 1;
    uint32_t MinPublicDemoCheckpoints = 1;
    uint32_t MinPublicDemoDirectorFrames = 1;
    uint32_t MinV30VerticalSlicePoints = static_cast<uint32_t>(V30VerticalSlicePointCount);
    uint32_t MinPublicDemoResonanceGates = static_cast<uint32_t>(PublicDemoResonanceGateCount);
    uint32_t MinPublicDemoPressureHits = 1;
    uint32_t MinPublicDemoFootstepEvents = 1;
    uint32_t MinV31DiversifiedPoints = static_cast<uint32_t>(V31DiversifiedPointCount);
    uint32_t MinPublicDemoPhaseRelays = static_cast<uint32_t>(PublicDemoPhaseRelayCount);
    uint32_t MinPublicDemoRelayOverchargeWindows = 1;
    uint32_t MinPublicDemoComboSteps = 1;
    uint32_t MinV32RoadmapPoints = static_cast<uint32_t>(V32RoadmapPointCount);
    uint32_t MinPublicDemoCollisionSolves = 1;
    uint32_t MinPublicDemoTraversalActions = 1;
    uint32_t MinPublicDemoEnemyChases = 1;
    uint32_t MinPublicDemoEnemyEvades = 1;
    uint32_t MinPublicDemoGamepadFrames = 1;
    uint32_t MinPublicDemoMenuTransitions = 1;
    uint32_t MinPublicDemoFailurePresentations = 1;
    uint32_t MinPublicDemoContentAudioCues = 1;
    uint32_t MinPublicDemoAnimationStateChanges = 1;
    uint32_t MinV33PlayableDemoPoints = static_cast<uint32_t>(V33PlayableDemoPointCount);
    uint32_t MinPublicDemoEnemyArchetypes = static_cast<uint32_t>(PublicDemoEnemyCount);
    uint32_t MinPublicDemoEnemyLineOfSightChecks = 1;
    uint32_t MinPublicDemoEnemyTelegraphs = 1;
    uint32_t MinPublicDemoEnemyHitReactions = 1;
    uint32_t MinPublicDemoControllerGroundedFrames = 1;
    uint32_t MinPublicDemoControllerSlopeSamples = 1;
    uint32_t MinPublicDemoControllerMaterialSamples = 1;
    uint32_t MinPublicDemoControllerMovingPlatformFrames = 1;
    uint32_t MinPublicDemoControllerCameraCollisionFrames = 1;
    uint32_t MinPublicDemoAnimationClipEvents = 1;
    uint32_t MinPublicDemoAnimationBlendSamples = 1;
    uint32_t MinPublicDemoAccessibilityToggles = 1;
    uint32_t MinRenderingPipelineReadinessItems = 6;
    uint32_t MinProductionPipelineReadinessItems = 6;
    uint32_t MinV34AAAFoundationPoints = static_cast<uint32_t>(V34AAAFoundationPointCount);
    uint32_t MinEngineEventBusDeliveries = 3;
    uint32_t MinEngineSchedulerTasks = 7;
    uint32_t MinEngineSceneQueryHits = 3;
    uint32_t MinEngineStreamingScheduledRequests = 2;
    uint32_t MinEngineRenderGraphBudgetChecks = 1;
    uint32_t MinEngineModuleSmokeTests = 5;
    uint32_t MinV35EngineArchitecturePoints = static_cast<uint32_t>(V35EngineArchitecturePointCount);
    uint32_t MinEngineServiceRegistryServices = 7;
    uint32_t MinEngineTelemetryEvents = 6;
    uint32_t MinEngineConfigVars = 8;
    uint32_t MinEditorPanelRegistryPanels = 6;
    uint32_t MinGameModuleSplitFiles = 6;
    uint32_t MinV36MixedBatchPoints = static_cast<uint32_t>(V36MixedBatchPointCount);
    uint32_t MinRuntimeCommandRegistryCommands = 6;
    uint32_t MinEditorWorkspaceRegistryWorkspaces = 3;
    uint32_t MinGameEventRouteCatalogRoutes = 10;
    uint32_t MinV38DiversifiedPoints = static_cast<uint32_t>(V38DiversifiedPointCount);
    uint32_t MinRuntimeCommandHistoryEntries = 4;
    uint32_t MinRuntimeCommandBindingConflicts = 1;
    uint32_t MinEditorDockLayoutDescriptors = 3;
    uint32_t MinEditorTeamDefaultWorkspaces = 2;
    uint32_t MinGameReplayableEventRoutes = 10;
    uint32_t MinV39RoadmapPoints = static_cast<uint32_t>(V39RoadmapPointCount);
    bool RequireEditorGpuPickResources = true;
    double ExpectedAverageLuma = 82.17;
    double AverageLumaTolerance = 12.0;
    double MinNonBlackRatio = 0.05;
    double MinPlaybackDistance = 1.0;
    double MaxCpuFrameMs = 120.0;
    double MaxGpuFrameMs = 50.0;
    double MaxPassMs = 60.0;
};

enum class GizmoAxis
{
    None,
    X,
    Y,
    Z
};

enum class GizmoPlane
{
    None,
    XY,
    XZ,
    YZ
};

struct MouseRay
{
    DirectX::XMFLOAT3 Origin = {};
    DirectX::XMFLOAT3 Direction = {};
};

struct EditorViewportState
{
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 1.0f;
    float Height = 1.0f;
    bool MouseInside = false;
};

struct GizmoDragObject
{
    size_t SceneIndex = InvalidIndex;
    DirectX::XMFLOAT3 OriginalPosition = {};
    DirectX::XMFLOAT3 OriginalRotation = {};
    DirectX::XMFLOAT3 OriginalScale = {};
};

enum class GizmoMode
{
    Translate,
    Rotate,
    Scale
};

enum class GizmoSpace
{
    World,
    Local
};

enum class EditorPickKind
{
    None,
    Player,
    SceneObject,
    GizmoAxis,
    GizmoPlane
};

struct EditorPickResult
{
    EditorPickKind Kind = EditorPickKind::None;
    size_t SceneIndex = InvalidIndex;
    uint64_t StableId = 0;
    float Distance = FLT_MAX;
    GizmoAxis Axis = GizmoAxis::None;
    GizmoPlane Plane = GizmoPlane::None;
    std::string Name;
};

struct EditorVerificationStats
{
    uint32_t ObjectPickTests = 0;
    uint32_t ObjectPickFailures = 0;
    uint32_t GizmoPickTests = 0;
    uint32_t GizmoPickFailures = 0;
    uint32_t GpuPickReadbacks = 0;
    uint32_t GpuPickFallbacks = 0;
    uint32_t GpuPickAsyncQueues = 0;
    uint32_t GpuPickAsyncResolves = 0;
    uint32_t GizmoDragTests = 0;
    uint32_t GizmoDragFailures = 0;
    uint32_t SceneReloads = 0;
    uint32_t SceneSaves = 0;
    uint32_t PostDebugViews = 0;
    uint32_t ShowcaseFrames = 0;
    uint32_t TrailerFrames = 0;
    uint32_t HighResCaptures = 0;
    uint32_t RiftVfxDraws = 0;
    uint32_t AudioBeatPulses = 0;
    uint32_t AudioSnapshotTests = 0;
    uint32_t AsyncIoTests = 0;
    uint32_t MaterialTextureSlotTests = 0;
    uint32_t PrefabVariantTests = 0;
    uint32_t ShotDirectorTests = 0;
    uint32_t ShotSplineTests = 0;
    uint32_t AudioAnalysisTests = 0;
    uint32_t XAudio2BackendTests = 0;
    uint32_t VfxSystemTests = 0;
    uint32_t GpuVfxSimulationTests = 0;
    uint32_t AnimationSkinningTests = 0;
    uint32_t GpuPickHoverCacheTests = 0;
    uint32_t GpuPickLatencyHistogramTests = 0;
    uint32_t ShotTimelineTrackTests = 0;
    uint32_t ShotThumbnailTests = 0;
    uint32_t ShotPreviewScrubTests = 0;
    uint32_t GraphHighResCaptureTests = 0;
    uint32_t CookedPackageTests = 0;
    uint32_t AssetInvalidationTests = 0;
    uint32_t NestedPrefabTests = 0;
    uint32_t AudioProductionTests = 0;
    uint32_t ViewportOverlayTests = 0;
    uint32_t HighResResolveTests = 0;
    uint32_t ViewportHudControlTests = 0;
    uint32_t TransformPrecisionTests = 0;
    uint32_t CommandHistoryFilterTests = 0;
    uint32_t RuntimeSchemaManifestTests = 0;
    uint32_t ShotSequencerTests = 0;
    uint32_t VfxRendererProfileTests = 0;
    uint32_t CookedGpuResourceTests = 0;
    uint32_t DependencyInvalidationTests = 0;
    uint32_t AudioMeterCalibrationTests = 0;
    uint32_t ReleaseReadinessTests = 0;
    uint32_t V25ProductionPointTests = 0;
    uint32_t EditorPreferencePersistenceTests = 0;
    uint32_t EditorPreferenceSaveTests = 0;
    uint32_t ViewportToolbarTests = 0;
    uint32_t EditorPreferenceProfileTests = 0;
    uint32_t CapturePresetTests = 0;
    uint32_t VfxEmitterProfileTests = 0;
    uint32_t CookedDependencyPreviewTests = 0;
    uint32_t EditorWorkflowTests = 0;
    uint32_t AssetPipelinePromotionTests = 0;
    uint32_t RenderingAdvancedTests = 0;
    uint32_t RuntimeSequencerFeatureTests = 0;
    uint32_t AudioProductionFeatureTests = 0;
    uint32_t ProductionPublishingTests = 0;
    uint32_t V28DiversifiedPointTests = 0;
    uint32_t PublicDemoTests = 0;
    uint32_t PublicDemoShardPickups = 0;
    uint32_t PublicDemoCompletions = 0;
    uint32_t PublicDemoHudFrames = 0;
    uint32_t PublicDemoBeaconDraws = 0;
    uint32_t PublicDemoSentinelTicks = 0;
    uint32_t V29PublicDemoPointTests = 0;
    uint32_t PublicDemoObjectiveStages = 0;
    uint32_t PublicDemoAnchorActivations = 0;
    uint32_t PublicDemoRetries = 0;
    uint32_t PublicDemoCheckpoints = 0;
    uint32_t PublicDemoDirectorFrames = 0;
    uint32_t V30VerticalSlicePointTests = 0;
    uint32_t PublicDemoResonanceGates = 0;
    uint32_t PublicDemoPressureHits = 0;
    uint32_t PublicDemoFootstepEvents = 0;
    uint32_t V31DiversifiedPointTests = 0;
    uint32_t PublicDemoPhaseRelays = 0;
    uint32_t PublicDemoRelayOverchargeWindows = 0;
    uint32_t PublicDemoComboSteps = 0;
    uint32_t V32RoadmapPointTests = 0;
    uint32_t PublicDemoCollisionSolves = 0;
    uint32_t PublicDemoTraversalActions = 0;
    uint32_t PublicDemoEnemyChases = 0;
    uint32_t PublicDemoEnemyEvades = 0;
    uint32_t PublicDemoGamepadFrames = 0;
    uint32_t PublicDemoMenuTransitions = 0;
    uint32_t PublicDemoFailurePresentations = 0;
    uint32_t PublicDemoContentAudioCues = 0;
    uint32_t PublicDemoAnimationStateChanges = 0;
    uint32_t V33PlayableDemoPointTests = 0;
    uint32_t PublicDemoEnemyArchetypes = 0;
    uint32_t PublicDemoEnemyLineOfSightChecks = 0;
    uint32_t PublicDemoEnemyTelegraphs = 0;
    uint32_t PublicDemoEnemyHitReactions = 0;
    uint32_t PublicDemoControllerGroundedFrames = 0;
    uint32_t PublicDemoControllerSlopeSamples = 0;
    uint32_t PublicDemoControllerMaterialSamples = 0;
    uint32_t PublicDemoControllerMovingPlatformFrames = 0;
    uint32_t PublicDemoControllerCameraCollisionFrames = 0;
    uint32_t PublicDemoAnimationClipEvents = 0;
    uint32_t PublicDemoAnimationBlendSamples = 0;
    uint32_t PublicDemoAccessibilityToggles = 0;
    uint32_t RenderingPipelineReadinessItems = 0;
    uint32_t ProductionPipelineReadinessItems = 0;
    uint32_t V34AAAFoundationPointTests = 0;
    uint32_t EngineEventBusDeliveries = 0;
    uint32_t EngineEventBusFlushes = 0;
    uint32_t EngineSchedulerTasks = 0;
    uint32_t EngineSchedulerPhases = 0;
    uint32_t EngineSceneQueryHits = 0;
    uint32_t EngineSceneQueryRaycasts = 0;
    uint32_t EngineStreamingRequests = 0;
    uint32_t EngineStreamingScheduledRequests = 0;
    uint32_t EngineRenderGraphBudgetChecks = 0;
    uint32_t EngineModuleSmokeTests = 0;
    uint32_t V35EngineArchitecturePointTests = 0;
    uint32_t EngineServiceRegistryServices = 0;
    uint32_t EngineServiceRegistryRequiredServices = 0;
    uint32_t EngineTelemetryEvents = 0;
    uint32_t EngineTelemetryCounters = 0;
    uint32_t EngineConfigVars = 0;
    uint32_t EngineConfigDirtyVars = 0;
    uint32_t EditorPanelRegistryPanels = 0;
    uint32_t EditorPanelRegistryToggles = 0;
    uint32_t GameModuleSplitFiles = 0;
    uint32_t GameModuleCommentedModules = 0;
    uint32_t V36MixedBatchPointTests = 0;
    uint32_t RuntimeCommandRegistryCommands = 0;
    uint32_t RuntimeCommandRegistryExecutions = 0;
    uint32_t RuntimeCommandRegistrySearches = 0;
    uint32_t EditorWorkspaceRegistryWorkspaces = 0;
    uint32_t EditorWorkspaceRegistryApplications = 0;
    uint32_t EditorPanelSearchResults = 0;
    uint32_t EditorPanelNavigationSteps = 0;
    uint32_t GameEventRouteCatalogRoutes = 0;
    uint32_t GameEventRouteTelemetryRoutes = 0;
    uint32_t GameEventRouteEventBusRoutes = 0;
    uint32_t V38DiversifiedPointTests = 0;
    uint32_t RuntimeCommandHistoryEntries = 0;
    uint32_t RuntimeCommandBindingConflicts = 0;
    uint32_t RuntimeCommandCategorySummaries = 0;
    uint32_t RuntimeCommandRequiredCategoriesSatisfied = 0;
    uint32_t EditorSavedDockLayouts = 0;
    uint32_t EditorTeamDefaultWorkspaces = 0;
    uint32_t EditorWorkspaceCommandBindings = 0;
    uint32_t EditorControllerNavigationHints = 0;
    uint32_t EditorToolbarCustomizationSlots = 0;
    uint32_t GameEventRouteReplayableRoutes = 0;
    uint32_t GameEventRouteSelectionLinkedRoutes = 0;
    uint32_t GameEventRouteBreadcrumbRoutes = 0;
    uint32_t GameEventRouteTraceChannels = 0;
    uint32_t GameEventRouteFailureRoutes = 0;
    uint32_t V39RoadmapPointTests = 0;
    uint32_t RuntimeCommandHistorySuccesses = 0;
    uint32_t RuntimeCommandHistoryFailures = 0;
    uint32_t RuntimeCommandBoundCommands = 0;
    uint32_t RuntimeCommandUniqueBindings = 0;
    uint32_t RuntimeCommandDocumentedCommands = 0;
    uint32_t EditorWorkspaceMigrationReady = 0;
    uint32_t EditorWorkspaceFocusTargets = 0;
    uint32_t EditorWorkspaceGamepadReady = 0;
    uint32_t EditorWorkspaceToolbarProfiles = 0;
    uint32_t EditorWorkspaceCommandRoutes = 0;
    uint32_t GameEventRouteCheckpointLinks = 0;
    uint32_t GameEventRouteSaveRelevantRoutes = 0;
    uint32_t GameEventRouteChapterReplayRoutes = 0;
    uint32_t GameEventRouteAccessibilityRoutes = 0;
    uint32_t GameEventRouteHudVisibleRoutes = 0;
    uint32_t V40DiversifiedPointTests = 0;
    uint32_t V45RuntimeCatalogBindings = 0;
    uint32_t V45RuntimeCatalogReadyBindings = 0;
    uint32_t V45EngineCatalogBindings = 0;
    uint32_t V45EditorCatalogBindings = 0;
    uint32_t V45GameCatalogBindings = 0;
    uint32_t V45CatalogPanelRows = 0;
    uint32_t V45CatalogVisibleBeacons = 0;
    uint32_t V45CatalogObjectiveBindings = 0;
    uint32_t V45CatalogEncounterBindings = 0;
    uint32_t V45CatalogNegativeFixtureTests = 0;
    uint32_t V45LiveCatalogPointTests = 0;
    uint32_t V46CatalogSelectableRows = 0;
    uint32_t V46CatalogPreviewSelections = 0;
    uint32_t V46CatalogPreviewCycles = 0;
    uint32_t V46CatalogPreviewClears = 0;
    uint32_t V46CatalogPreviewDetails = 0;
    uint32_t V46CatalogFocusedBeacons = 0;
    uint32_t V46EnginePreviewBindings = 0;
    uint32_t V46EditorPreviewBindings = 0;
    uint32_t V46GamePreviewBindings = 0;
    uint32_t V46RuntimeActionCommands = 0;
    uint32_t V46CatalogActionPreviewPointTests = 0;
    uint32_t V47CatalogExecuteRequests = 0;
    uint32_t V47CatalogExecutionStops = 0;
    uint32_t V47CatalogExecutionPulses = 0;
    uint32_t V47EngineExecutableBindings = 0;
    uint32_t V47EditorExecutableBindings = 0;
    uint32_t V47GameExecutableBindings = 0;
    uint32_t V47EngineExecutionOverlays = 0;
    uint32_t V47EditorExecutionOverlays = 0;
    uint32_t V47GameExecutionOverlays = 0;
    uint32_t V47WorldExecutionMarkers = 0;
    uint32_t V47ActionRouteBeams = 0;
    uint32_t V47ExecutionDetailRows = 0;
    uint32_t V47CatalogExecutionPointTests = 0;
    uint32_t V48RuntimeActionPlans = 0;
    uint32_t V48RuntimeReadyActionPlans = 0;
    uint32_t V48HighImpactActionPlans = 0;
    uint32_t V48EditorVisibleActionPlans = 0;
    uint32_t V48PlayableActionPlans = 0;
    uint32_t V48ActionDirectorRequests = 0;
    uint32_t V48ActionDirectorQueueDepth = 0;
    uint32_t V48ActionDirectorHistoryRows = 0;
    uint32_t V48DirectorCinematicBursts = 0;
    uint32_t V48DirectorRouteRibbons = 0;
    uint32_t V48DirectorEncounterGhosts = 0;
    uint32_t V48DirectorEditorQueueRows = 0;
    uint32_t V48DirectorPlanSummaryRows = 0;
    uint32_t V48ActionDirectorPointTests = 0;
    uint32_t V49RuntimeMutationPlans = 0;
    uint32_t V49RuntimeMutationRuntimePlans = 0;
    uint32_t V49EditorMutationPlans = 0;
    uint32_t V49GameplayMutationPlans = 0;
    uint32_t V49BudgetBoundMutationPlans = 0;
    uint32_t V49ActionMutationRequests = 0;
    uint32_t V49MutationQueueDepth = 0;
    uint32_t V49EngineBudgetMutations = 0;
    uint32_t V49SchedulerBudgetMutations = 0;
    uint32_t V49StreamingBudgetMutations = 0;
    uint32_t V49RenderBudgetMutations = 0;
    uint32_t V49EditorWorkspaceMutations = 0;
    uint32_t V49EditorCommandMutations = 0;
    uint32_t V49TraceEventRows = 0;
    uint32_t V49GameSpawnedEncounterWaves = 0;
    uint32_t V49GameObjectiveRouteMutations = 0;
    uint32_t V49GameCombatSandboxMutations = 0;
    uint32_t V49MutationWorldBursts = 0;
    uint32_t V49MutationWorldPillars = 0;
    uint32_t V49MutationWaveGhosts = 0;
    uint32_t V49MutationPanelRows = 0;
    uint32_t V49ActionMutationPointTests = 0;
};

}
