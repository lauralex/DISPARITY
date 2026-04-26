#pragma once

#include "Disparity/Math/Transform.h"
#include "Disparity/Rendering/RenderGraph.h"
#include "Disparity/Scene/Camera.h"
#include "Disparity/Scene/Material.h"
#include "Disparity/Scene/Mesh.h"

#include <DirectXMath.h>
#include <array>
#include <cstdint>
#include <d3d11.h>
#include <filesystem>
#include <unordered_map>
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
        float Exposure = 1.0f;
        float ShadowStrength = 0.48f;
        float BloomStrength = 0.42f;
        float BloomThreshold = 0.72f;
        float SsaoStrength = 0.55f;
        float AntiAliasingStrength = 0.85f;
        float TemporalBlend = 0.12f;
        float ColorSaturation = 1.04f;
        float ColorContrast = 1.03f;
        uint32_t PostDebugView = 0;
        uint32_t ShadowMapSize = 2048;
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
        void EndFrame();

        [[nodiscard]] uint32_t GetWidth() const;
        [[nodiscard]] uint32_t GetHeight() const;
        [[nodiscard]] ID3D11Device* GetDevice() const;
        [[nodiscard]] ID3D11DeviceContext* GetContext() const;
        [[nodiscard]] const RendererSettings& GetSettings() const;
        [[nodiscard]] const RenderGraph& GetRenderGraph() const;
        [[nodiscard]] uint32_t GetFrameDrawCalls() const;
        [[nodiscard]] uint32_t GetSceneDrawCalls() const;
        [[nodiscard]] uint32_t GetShadowDrawCalls() const;

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

        bool CreateDeviceAndSwapChain();
        bool CreateBackBufferResources();
        bool CreateShaders();
        bool CreateStates();
        bool CreateConstantBuffers();
        bool CreateShadowResources();
        void ReleaseBackBufferResources();
        void ReleaseShadowResources();
        void ApplyScenePipeline();
        void UploadFrameConstants(const DirectX::XMMATRIX& viewProjection);
        void RenderPostProcess();
        void BuildFrameRenderGraph();

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

        Camera m_camera;
        DirectionalLight m_light;
        RendererSettings m_settings;
        RenderGraph m_renderGraph;
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
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_opaqueBlendState;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_alphaBlendState;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_linearSamplerState;

        std::unordered_map<MeshHandle, GpuMesh> m_meshes;
        std::unordered_map<TextureHandle, GpuTexture> m_textures;
        std::array<PointLight, 8> m_pointLights = {};
        uint32_t m_pointLightCount = 0;
        MeshHandle m_nextMeshHandle = 1;
        TextureHandle m_nextTextureHandle = 1;
        bool m_comInitialized = false;
        bool m_historyValid = false;
    };
}
