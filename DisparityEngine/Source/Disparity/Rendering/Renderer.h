#pragma once

#include "Disparity/Math/Transform.h"
#include "Disparity/Rendering/RenderGraph.h"
#include "Disparity/Scene/Camera.h"
#include "Disparity/Scene/Material.h"
#include "Disparity/Scene/Mesh.h"

#include <DirectXMath.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <d3d11.h>
#include <deque>
#include <filesystem>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

namespace Disparity
{
    struct DirectionalLight
    {
        DirectX::XMFLOAT3 Direction = { -0.45f, -1.0f, 0.35f };
        DirectX::XMFLOAT3 Color = { 1.0f, 0.96f, 0.88f };
        float Intensity = 0.85f;
        float AmbientIntensity = 0.22f;
    };

    struct PointLight
    {
        DirectX::XMFLOAT3 Position = { 0.0f, 2.0f, 0.0f };
        float Radius = 6.0f;
        DirectX::XMFLOAT3 Color = { 1.0f, 1.0f, 1.0f };
        float Intensity = 1.0f;
    };

    struct RendererSettings
    {
        bool VSync = true;
        bool ToneMapping = true;
        bool Shadows = true;
        bool CascadedShadows = true;
        bool ClusteredLighting = true;
        bool Bloom = true;
        bool SSAO = true;
        bool AntiAliasing = true;
        bool TemporalAA = true;
        bool DepthOfField = false;
        bool LensDirt = false;
        bool CinematicOverlay = false;
        float Exposure = 1.0f;
        float ShadowStrength = 0.48f;
        float BloomStrength = 0.42f;
        float BloomThreshold = 0.72f;
        float SsaoStrength = 0.55f;
        float AntiAliasingStrength = 0.85f;
        float TemporalBlend = 0.12f;
        float ColorSaturation = 1.04f;
        float ColorContrast = 1.03f;
        float DepthOfFieldFocus = 0.985f;
        float DepthOfFieldRange = 0.026f;
        float DepthOfFieldStrength = 0.0f;
        float LensDirtStrength = 0.0f;
        float VignetteStrength = 0.08f;
        float LetterboxAmount = 0.0f;
        float TitleSafeOpacity = 0.0f;
        float FilmGrainStrength = 0.0f;
        float PresentationPulse = 0.0f;
        uint32_t PostDebugView = 0;
        uint32_t ShadowMapSize = 2048;
    };

    struct FrameCaptureResult
    {
        bool Success = false;
        std::filesystem::path Path;
        std::string Error;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint64_t RgbChecksum = 0;
        uint64_t NonBlackPixels = 0;
        uint64_t BrightPixels = 0;
        double AverageLuma = 0.0;
    };

    struct RendererFrameGraphDiagnostics
    {
        bool DispatchOrderValid = false;
        uint32_t ExecutedPasses = 0;
        uint32_t TransientAllocations = 0;
        uint32_t AliasedResources = 0;
        uint32_t TransitionBarriers = 0;
        uint32_t ResourceHandles = 0;
        uint32_t GraphResourceBindings = 0;
        uint32_t GraphHandleBindHits = 0;
        uint32_t GraphHandleBindMisses = 0;
        uint32_t GraphCallbacksBound = 0;
        uint32_t GraphCallbacksExecuted = 0;
        uint32_t PendingCaptureRequests = 0;
        uint32_t ObjectIdReadbackRingSize = 0;
        uint32_t ObjectIdReadbackPending = 0;
        uint32_t ObjectIdReadbackRequests = 0;
        uint32_t ObjectIdReadbackCompletions = 0;
        uint32_t ObjectIdReadbackLatencyFrames = 0;
        uint32_t ObjectIdReadbackBusySkips = 0;
    };

    struct EditorViewportResourcesInfo
    {
        bool ViewportTargetReady = false;
        bool ObjectIdTargetReady = false;
        bool ObjectDepthTargetReady = false;
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    struct EditorObjectIdReadback
    {
        bool Valid = false;
        uint32_t ObjectId = 0;
        float Depth = 1.0f;
        uint32_t X = 0;
        uint32_t Y = 0;
        std::string Error;
    };

    class Renderer
    {
    public:
        Renderer() = default;
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        bool Initialize(HWND windowHandle, uint32_t width, uint32_t height);
        void Shutdown();
        void Resize(uint32_t width, uint32_t height);

        MeshHandle CreateMesh(const MeshData& meshData);
        TextureHandle CreateTextureFromFile(const std::filesystem::path& path);

        void SetCamera(const Camera& camera);
        void SetLighting(const DirectionalLight& light);
        void SetPointLights(const PointLight* lights, uint32_t count);
        void SetSettings(const RendererSettings& settings);

        void BeginFrame(const float clearColor[4]);
        void BeginShadowPass(const DirectionalLight& light, const DirectX::XMFLOAT3& focus, float radius);
        void EndShadowPass();
        void DrawMesh(MeshHandle mesh, const Transform& transform, const Material& material);
        void DrawMeshWithId(MeshHandle mesh, const Transform& transform, const Material& material, uint32_t editorObjectId);
        void EndFrame();
        void RequestFrameCapture(std::filesystem::path outputPath);

        [[nodiscard]] uint32_t GetWidth() const;
        [[nodiscard]] uint32_t GetHeight() const;
        [[nodiscard]] ID3D11Device* GetDevice() const;
        [[nodiscard]] ID3D11DeviceContext* GetContext() const;
        [[nodiscard]] const RendererSettings& GetSettings() const;
        [[nodiscard]] const RenderGraph& GetRenderGraph() const;
        [[nodiscard]] uint32_t GetFrameDrawCalls() const;
        [[nodiscard]] uint32_t GetSceneDrawCalls() const;
        [[nodiscard]] uint32_t GetShadowDrawCalls() const;
        [[nodiscard]] RendererFrameGraphDiagnostics GetFrameGraphDiagnostics() const;
        [[nodiscard]] EditorViewportResourcesInfo GetEditorViewportResources() const;
        [[nodiscard]] ID3D11ShaderResourceView* GetEditorViewportShaderResourceView() const;
        void QueueEditorObjectIdReadback(uint32_t x, uint32_t y) const;
        [[nodiscard]] bool TryResolveEditorObjectIdReadback(EditorObjectIdReadback& outReadback) const;
        [[nodiscard]] EditorObjectIdReadback ReadEditorObjectId(uint32_t x, uint32_t y) const;
        [[nodiscard]] double GetGpuFrameMilliseconds() const;
        [[nodiscard]] bool IsGpuTimingAvailable() const;
        [[nodiscard]] bool HasLastFrameCapture() const;
        [[nodiscard]] const FrameCaptureResult& GetLastFrameCapture() const;

    private:
        struct GpuMesh
        {
            Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
            Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
            uint32_t IndexCount = 0;
        };

        struct GpuTexture
        {
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderResourceView;
        };

        struct GpuPassProfile
        {
            Microsoft::WRL::ComPtr<ID3D11Query> StartQuery;
            Microsoft::WRL::ComPtr<ID3D11Query> EndQuery;
            bool QueryIssued = false;
        };

        struct GraphResourceBinding
        {
            uint32_t ResourceId = 0;
            ID3D11Texture2D* Texture = nullptr;
            ID3D11RenderTargetView* RenderTargetView = nullptr;
            ID3D11DepthStencilView* DepthStencilView = nullptr;
            ID3D11ShaderResourceView* ShaderResourceView = nullptr;
        };

        struct ObjectIdReadbackSlot
        {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> IdStaging;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStaging;
            uint32_t X = 0;
            uint32_t Y = 0;
            uint64_t RequestFrame = 0;
            bool Pending = false;
        };

        bool CreateDeviceAndSwapChain();
        bool CreateBackBufferResources();
        bool CreateShaders();
        bool CreateStates();
        bool CreateConstantBuffers();
        bool CreateShadowResources();
        void CreateGpuProfilingResources();
        void ReleaseBackBufferResources();
        void ReleaseShadowResources();
        void ApplyScenePipeline();
        void UploadFrameConstants(const DirectX::XMMATRIX& viewProjection);
        void RenderPostProcess();
        void BuildFrameRenderGraph();
        void BeginGraphPass(uint32_t passId);
        void EndGraphPass();
        void BeginGpuFrameProfile();
        void EndGpuFrameProfile();
        void ResolveGpuFrameProfile();
        void EnsureGpuPassProfiles(size_t passCount);
        void BeginGpuPassProfile(uint32_t passId);
        void EndGpuPassProfile(uint32_t passId);
        void ResolveGpuPassProfiles();
        void BindGraphResource(
            uint32_t resourceId,
            ID3D11Texture2D* texture,
            ID3D11RenderTargetView* renderTargetView,
            ID3D11DepthStencilView* depthStencilView,
            ID3D11ShaderResourceView* shaderResourceView);
        [[nodiscard]] const GraphResourceBinding* FindGraphResourceBinding(uint32_t resourceId) const;
        [[nodiscard]] ID3D11Texture2D* GetGraphTexture(uint32_t resourceId) const;
        [[nodiscard]] ID3D11RenderTargetView* GetGraphRenderTargetView(uint32_t resourceId) const;
        [[nodiscard]] ID3D11DepthStencilView* GetGraphDepthStencilView(uint32_t resourceId) const;
        [[nodiscard]] ID3D11ShaderResourceView* GetGraphShaderResourceView(uint32_t resourceId) const;
        [[nodiscard]] bool EnsureObjectIdReadbackSlot(ObjectIdReadbackSlot& slot) const;
        void ResetObjectIdReadbackSlots();
        [[nodiscard]] FrameCaptureResult CaptureBackBuffer(const std::filesystem::path& outputPath) const;

        HWND m_windowHandle = nullptr;
        uint32_t m_width = 1;
        uint32_t m_height = 1;
        bool m_initialized = false;
        bool m_frameBegun = false;
        float m_elapsedTime = 0.0f;
        bool m_shadowPassActive = false;
        uint32_t m_frameDrawCalls = 0;
        uint32_t m_sceneDrawCalls = 0;
        uint32_t m_shadowDrawCalls = 0;
        uint32_t m_graphBackBuffer = 0;
        uint32_t m_graphHdrScene = 0;
        uint32_t m_graphDepth = 0;
        uint32_t m_graphHistory = 0;
        uint32_t m_graphShadowMap = 0;
        uint32_t m_graphEditorViewport = 0;
        uint32_t m_graphEditorObjectIds = 0;
        uint32_t m_graphEditorObjectDepth = 0;
        uint32_t m_graphClearPass = std::numeric_limits<uint32_t>::max();
        uint32_t m_graphShadowPass = std::numeric_limits<uint32_t>::max();
        uint32_t m_graphScenePass = std::numeric_limits<uint32_t>::max();
        uint32_t m_graphEditorViewportPass = std::numeric_limits<uint32_t>::max();
        uint32_t m_graphPostPass = std::numeric_limits<uint32_t>::max();
        uint32_t m_graphEditorPass = std::numeric_limits<uint32_t>::max();
        uint32_t m_activeGraphPass = std::numeric_limits<uint32_t>::max();
        std::vector<uint32_t> m_graphExecutedPasses;
        bool m_graphDispatchOrderValid = false;
        uint32_t m_graphTransientAllocations = 0;
        uint32_t m_graphAliasedResources = 0;
        uint32_t m_graphTransitionBarriers = 0;
        uint32_t m_graphResourceHandles = 0;
        uint32_t m_graphResourceBindingCount = 0;
        uint32_t m_graphCallbacksBound = 0;
        uint32_t m_graphCallbacksExecuted = 0;
        mutable uint32_t m_graphHandleBindHits = 0;
        mutable uint32_t m_graphHandleBindMisses = 0;
        mutable uint32_t m_objectIdReadbackRingCursor = 0;
        mutable uint32_t m_objectIdReadbackRequests = 0;
        mutable uint32_t m_objectIdReadbackCompletions = 0;
        mutable uint32_t m_objectIdReadbackLatencyFrames = 0;
        mutable uint32_t m_objectIdReadbackBusySkips = 0;
        uint64_t m_frameIndex = 0;
        std::chrono::steady_clock::time_point m_graphPassStart;
        double m_gpuFrameMilliseconds = 0.0;
        uint64_t m_gpuTimestampFrequency = 0;
        bool m_gpuTimingSupported = false;
        bool m_gpuTimingValid = false;
        bool m_gpuFrameQueryIssued = false;

        Camera m_camera;
        DirectionalLight m_light;
        RendererSettings m_settings;
        RenderGraph m_renderGraph;
        std::vector<GraphResourceBinding> m_graphBindings;
        mutable std::array<ObjectIdReadbackSlot, 3> m_objectIdReadbackSlots;
        FrameCaptureResult m_lastFrameCapture;
        std::deque<std::filesystem::path> m_pendingFrameCapturePaths;
        DirectX::XMFLOAT4X4 m_lightViewProjection = {};

        Microsoft::WRL::ComPtr<ID3D11Device> m_device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
        Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_backBufferRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_hdrSceneTexture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_hdrSceneRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_hdrSceneShaderResourceView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthStencilBuffer;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_depthStencilShaderResourceView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_historyTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_historyShaderResourceView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_editorViewportTexture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_editorViewportRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_editorViewportShaderResourceView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_editorObjectIdTexture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_editorObjectIdRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_editorObjectIdShaderResourceView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_editorObjectDepthTexture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_editorObjectDepthRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_editorObjectDepthShaderResourceView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_shadowMapTexture;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_shadowMapDepthView;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shadowMapShaderResourceView;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_postVertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_postPixelShader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_frameConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_objectConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_postConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_doubleSidedRasterizerState;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_opaqueBlendState;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_alphaBlendState;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_linearSamplerState;
        Microsoft::WRL::ComPtr<ID3D11Query> m_gpuFrameDisjointQuery;
        Microsoft::WRL::ComPtr<ID3D11Query> m_gpuFrameStartQuery;
        Microsoft::WRL::ComPtr<ID3D11Query> m_gpuFrameEndQuery;
        std::vector<GpuPassProfile> m_gpuPassProfiles;

        std::unordered_map<MeshHandle, GpuMesh> m_meshes;
        std::unordered_map<TextureHandle, GpuTexture> m_textures;
        std::array<PointLight, 8> m_pointLights = {};
        uint32_t m_pointLightCount = 0;
        MeshHandle m_nextMeshHandle = 1;
        TextureHandle m_nextTextureHandle = 1;
        bool m_comInitialized = false;
        bool m_historyValid = false;
        bool m_hasLastFrameCapture = false;
    };
}
