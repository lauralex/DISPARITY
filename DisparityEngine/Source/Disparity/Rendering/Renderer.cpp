#include "Disparity/Rendering/Renderer.h"

#include "Disparity/Core/Log.h"
#include "Disparity/Editor/EditorGui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <d3dcompiler.h>
#include <filesystem>
#include <string>
#include <vector>
#include <wincodec.h>

namespace Disparity
{
    namespace
    {
        struct FrameConstants
        {
            DirectX::XMFLOAT4X4 ViewProjection = {};
            DirectX::XMFLOAT3 CameraPosition = {};
            float ElapsedTime = 0.0f;
            DirectX::XMFLOAT3 LightDirection = {};
            float AmbientIntensity = 0.0f;
            DirectX::XMFLOAT3 LightColor = {};
            float LightIntensity = 0.0f;
            DirectX::XMFLOAT4X4 LightViewProjection = {};
            float ShadowStrength = 0.0f;
            float ShadowMapTexelSize = 0.0f;
            float PointLightCount = 0.0f;
            float ClusteredLighting = 0.0f;
            PointLight PointLights[8] = {};
        };

        struct ObjectConstants
        {
            DirectX::XMFLOAT4X4 World = {};
            DirectX::XMFLOAT3 Albedo = {};
            float Roughness = 0.65f;
            float Metallic = 0.0f;
            float Alpha = 1.0f;
            float UseTexture = 0.0f;
            float Padding = 0.0f;
        };

        struct PostConstants
        {
            float Exposure = 1.0f;
            float ToneMapEnabled = 1.0f;
            float BloomStrength = 0.0f;
            float SsaoStrength = 0.0f;
            float TaaBlend = 0.0f;
            float HistoryValid = 0.0f;
            DirectX::XMFLOAT2 InvResolution = {};
            float BloomThreshold = 0.72f;
            float AntiAliasingStrength = 0.0f;
            float ColorSaturation = 1.0f;
            float ColorContrast = 1.0f;
            float PostDebugView = 0.0f;
            float AntiAliasingEnabled = 0.0f;
            DirectX::XMFLOAT2 Padding = {};
        };

        std::string HrToString(HRESULT hr)
        {
            char buffer[32] = {};
            std::snprintf(buffer, sizeof(buffer), "0x%08X", static_cast<unsigned int>(hr));
            return std::string(buffer);
        }

        std::filesystem::path GetExecutableDirectory()
        {
            std::array<wchar_t, MAX_PATH> buffer = {};
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0 || length >= buffer.size())
            {
                return std::filesystem::current_path();
            }

            return std::filesystem::path(buffer.data()).parent_path();
        }

        std::filesystem::path FindAssetPath(const std::filesystem::path& relativePath)
        {
            if (std::filesystem::exists(relativePath))
            {
                return relativePath;
            }

            const std::array<std::filesystem::path, 2> roots = {
                std::filesystem::current_path(),
                GetExecutableDirectory()
            };

            for (const std::filesystem::path& root : roots)
            {
                std::filesystem::path cursor = root;

                for (int depth = 0; depth < 8; ++depth)
                {
                    const std::filesystem::path candidate = cursor / relativePath;
                    if (std::filesystem::exists(candidate))
                    {
                        return candidate;
                    }

                    if (!cursor.has_parent_path() || cursor.parent_path() == cursor)
                    {
                        break;
                    }

                    cursor = cursor.parent_path();
                }
            }

            return relativePath;
        }

        bool CompileShader(
            const std::filesystem::path& shaderPath,
            const char* entryPoint,
            const char* target,
            Microsoft::WRL::ComPtr<ID3DBlob>& shaderBlob)
        {
            UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#if defined(_DEBUG)
            flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

            Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
            const HRESULT hr = D3DCompileFromFile(
                shaderPath.c_str(),
                nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                entryPoint,
                target,
                flags,
                0,
                shaderBlob.GetAddressOf(),
                errorBlob.GetAddressOf());

            if (FAILED(hr))
            {
                if (errorBlob)
                {
                    Log(LogLevel::Error, std::string(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize()));
                }
                else
                {
                    Log(LogLevel::Error, "Shader compilation failed with HRESULT " + HrToString(hr));
                }

                return false;
            }

            return true;
        }
    }

    Renderer::~Renderer()
    {
        Shutdown();
    }

    bool Renderer::Initialize(HWND windowHandle, uint32_t width, uint32_t height)
    {
        m_windowHandle = windowHandle;
        m_width = std::max(1u, width);
        m_height = std::max(1u, height);

        const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(comResult))
        {
            m_comInitialized = true;
        }
        else if (comResult != RPC_E_CHANGED_MODE)
        {
            Log(LogLevel::Warning, "COM initialization failed; texture loading may not work. HRESULT " + HrToString(comResult));
        }

        if (!CreateDeviceAndSwapChain())
        {
            return false;
        }

        DirectX::XMStoreFloat4x4(&m_lightViewProjection, DirectX::XMMatrixIdentity());

        if (!CreateBackBufferResources() || !CreateShadowResources() || !CreateShaders() || !CreateConstantBuffers() || !CreateStates())
        {
            return false;
        }

        m_initialized = true;
        return true;
    }

    void Renderer::Shutdown()
    {
        if (m_context)
        {
            m_context->ClearState();
        }

        m_textures.clear();
        m_meshes.clear();
        m_linearSamplerState.Reset();
        m_alphaBlendState.Reset();
        m_opaqueBlendState.Reset();
        m_depthStencilState.Reset();
        m_rasterizerState.Reset();
        m_objectConstantBuffer.Reset();
        m_frameConstantBuffer.Reset();
        m_inputLayout.Reset();
        m_postConstantBuffer.Reset();
        m_postPixelShader.Reset();
        m_postVertexShader.Reset();
        m_pixelShader.Reset();
        m_vertexShader.Reset();
        ReleaseBackBufferResources();
        ReleaseShadowResources();
        m_swapChain.Reset();
        m_context.Reset();
        m_device.Reset();
        m_initialized = false;
        m_frameBegun = false;

        if (m_comInitialized)
        {
            CoUninitialize();
            m_comInitialized = false;
        }
    }

    void Renderer::Resize(uint32_t width, uint32_t height)
    {
        if (!m_initialized || width == 0 || height == 0)
        {
            return;
        }

        m_width = width;
        m_height = height;

        if (m_context)
        {
            m_context->OMSetRenderTargets(0, nullptr, nullptr);
        }

        ReleaseBackBufferResources();

        const HRESULT hr = m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Swap chain resize failed with HRESULT " + HrToString(hr));
            return;
        }

        CreateBackBufferResources();
    }

    MeshHandle Renderer::CreateMesh(const MeshData& meshData)
    {
        if (!m_device || meshData.Vertices.empty() || meshData.Indices.empty())
        {
            return 0;
        }

        GpuMesh mesh;
        mesh.IndexCount = static_cast<uint32_t>(meshData.Indices.size());

        D3D11_BUFFER_DESC vertexBufferDesc = {};
        vertexBufferDesc.ByteWidth = static_cast<UINT>(meshData.Vertices.size() * sizeof(Vertex));
        vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexData = {};
        vertexData.pSysMem = meshData.Vertices.data();

        HRESULT hr = m_device->CreateBuffer(&vertexBufferDesc, &vertexData, mesh.VertexBuffer.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Vertex buffer creation failed with HRESULT " + HrToString(hr));
            return 0;
        }

        D3D11_BUFFER_DESC indexBufferDesc = {};
        indexBufferDesc.ByteWidth = static_cast<UINT>(meshData.Indices.size() * sizeof(uint32_t));
        indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexData = {};
        indexData.pSysMem = meshData.Indices.data();

        hr = m_device->CreateBuffer(&indexBufferDesc, &indexData, mesh.IndexBuffer.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Index buffer creation failed with HRESULT " + HrToString(hr));
            return 0;
        }

        const MeshHandle handle = m_nextMeshHandle++;
        m_meshes.emplace(handle, std::move(mesh));
        return handle;
    }

    TextureHandle Renderer::CreateTextureFromFile(const std::filesystem::path& path)
    {
        if (!m_device)
        {
            return 0;
        }

        const std::filesystem::path resolvedPath = FindAssetPath(path);
        if (!std::filesystem::exists(resolvedPath))
        {
            Log(LogLevel::Warning, "Texture file not found: " + resolvedPath.string());
            return 0;
        }

        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(factory.GetAddressOf()));
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "WIC factory creation failed with HRESULT " + HrToString(hr));
            return 0;
        }

        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        hr = factory->CreateDecoderFromFilename(
            resolvedPath.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            decoder.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Texture decode failed for " + resolvedPath.string() + " HRESULT " + HrToString(hr));
            return 0;
        }

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, frame.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Texture frame retrieval failed with HRESULT " + HrToString(hr));
            return 0;
        }

        UINT width = 0;
        UINT height = 0;
        frame->GetSize(&width, &height);
        if (width == 0 || height == 0)
        {
            return 0;
        }

        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(converter.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "WIC format converter creation failed with HRESULT " + HrToString(hr));
            return 0;
        }

        hr = converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Texture format conversion failed with HRESULT " + HrToString(hr));
            return 0;
        }

        std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
        hr = converter->CopyPixels(nullptr, width * 4u, static_cast<UINT>(pixels.size()), pixels.data());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Texture pixel copy failed with HRESULT " + HrToString(hr));
            return 0;
        }

        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initialData = {};
        initialData.pSysMem = pixels.data();
        initialData.SysMemPitch = width * 4u;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        hr = m_device->CreateTexture2D(&textureDesc, &initialData, texture.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "D3D texture creation failed with HRESULT " + HrToString(hr));
            return 0;
        }

        GpuTexture gpuTexture;
        hr = m_device->CreateShaderResourceView(texture.Get(), nullptr, gpuTexture.ShaderResourceView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Texture shader resource view creation failed with HRESULT " + HrToString(hr));
            return 0;
        }

        const TextureHandle handle = m_nextTextureHandle++;
        m_textures.emplace(handle, std::move(gpuTexture));
        return handle;
    }

    void Renderer::SetCamera(const Camera& camera)
    {
        m_camera = camera;
    }

    void Renderer::SetLighting(const DirectionalLight& light)
    {
        m_light = light;
    }

    void Renderer::SetPointLights(const PointLight* lights, uint32_t count)
    {
        m_pointLightCount = std::min<uint32_t>(count, static_cast<uint32_t>(m_pointLights.size()));
        for (uint32_t index = 0; index < m_pointLightCount; ++index)
        {
            m_pointLights[index] = lights[index];
            m_pointLights[index].Radius = std::max(0.1f, m_pointLights[index].Radius);
            m_pointLights[index].Intensity = std::max(0.0f, m_pointLights[index].Intensity);
        }
    }

    void Renderer::SetSettings(const RendererSettings& settings)
    {
        m_settings = settings;
        m_settings.Exposure = std::clamp(m_settings.Exposure, 0.1f, 4.0f);
        m_settings.ShadowStrength = std::clamp(m_settings.ShadowStrength, 0.0f, 0.95f);
        m_settings.BloomStrength = std::clamp(m_settings.BloomStrength, 0.0f, 1.25f);
        m_settings.BloomThreshold = std::clamp(m_settings.BloomThreshold, 0.05f, 2.0f);
        m_settings.SsaoStrength = std::clamp(m_settings.SsaoStrength, 0.0f, 1.0f);
        m_settings.AntiAliasingStrength = std::clamp(m_settings.AntiAliasingStrength, 0.0f, 1.0f);
        m_settings.TemporalBlend = std::clamp(m_settings.TemporalBlend, 0.0f, 0.35f);
        m_settings.ColorSaturation = std::clamp(m_settings.ColorSaturation, 0.0f, 2.0f);
        m_settings.ColorContrast = std::clamp(m_settings.ColorContrast, 0.0f, 2.0f);
        m_settings.PostDebugView = std::clamp(m_settings.PostDebugView, 0u, 4u);
        m_settings.ShadowMapSize = std::clamp(m_settings.ShadowMapSize, 512u, 4096u);
    }

    void Renderer::BeginFrame(const float clearColor[4])
    {
        if (!m_initialized || !m_hdrSceneRenderTargetView || !m_depthStencilView)
        {
            return;
        }

        m_frameBegun = true;
        m_elapsedTime += 1.0f / 60.0f;

        m_shadowPassActive = false;
        m_context->OMSetRenderTargets(1, m_hdrSceneRenderTargetView.GetAddressOf(), m_depthStencilView.Get());
        m_context->ClearRenderTargetView(m_hdrSceneRenderTargetView.Get(), clearColor);
        m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

        ApplyScenePipeline();
    }

    void Renderer::BeginShadowPass(const DirectionalLight& light, const DirectX::XMFLOAT3& focus, float radius)
    {
        if (!m_frameBegun || !m_settings.Shadows || !m_shadowMapDepthView)
        {
            return;
        }

        m_light = light;
        radius = std::max(radius, 4.0f);

        DirectX::XMFLOAT3 lightDirection = light.Direction;
        DirectX::XMVECTOR lightVector = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&lightDirection));
        DirectX::XMVECTOR focusVector = DirectX::XMLoadFloat3(&focus);
        DirectX::XMVECTOR lightPosition = DirectX::XMVectorSubtract(focusVector, DirectX::XMVectorScale(lightVector, radius));
        DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        const DirectX::XMVECTOR almostUp = DirectX::XMVector3Dot(lightVector, up);
        if (std::abs(DirectX::XMVectorGetX(almostUp)) > 0.92f)
        {
            up = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        }

        const DirectX::XMMATRIX lightView = DirectX::XMMatrixLookAtLH(lightPosition, focusVector, up);
        const DirectX::XMMATRIX lightProjection = DirectX::XMMatrixOrthographicLH(radius * 2.0f, radius * 2.0f, 0.1f, radius * 3.0f);
        DirectX::XMStoreFloat4x4(&m_lightViewProjection, lightView * lightProjection);

        ID3D11ShaderResourceView* nullViews[3] = { nullptr, nullptr, nullptr };
        m_context->PSSetShaderResources(0, 3, nullViews);

        m_shadowPassActive = true;
        m_context->OMSetRenderTargets(0, nullptr, m_shadowMapDepthView.Get());
        m_context->ClearDepthStencilView(m_shadowMapDepthView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(m_settings.ShadowMapSize);
        viewport.Height = static_cast<float>(m_settings.ShadowMapSize);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &viewport);
        m_context->RSSetState(m_rasterizerState.Get());
        m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
        m_context->OMSetBlendState(m_opaqueBlendState.Get(), nullptr, 0xffffffff);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->IASetInputLayout(m_inputLayout.Get());
        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(nullptr, nullptr, 0);

        UploadFrameConstants(lightView * lightProjection);
    }

    void Renderer::EndShadowPass()
    {
        if (!m_shadowPassActive)
        {
            return;
        }

        m_shadowPassActive = false;
        ApplyScenePipeline();
    }

    void Renderer::DrawMesh(MeshHandle mesh, const Transform& transform, const Material& material)
    {
        if (!m_frameBegun)
        {
            return;
        }

        const auto found = m_meshes.find(mesh);
        if (found == m_meshes.end())
        {
            return;
        }

        ObjectConstants objectConstants = {};
        DirectX::XMStoreFloat4x4(&objectConstants.World, transform.ToMatrix());
        objectConstants.Albedo = material.Albedo;
        objectConstants.Roughness = std::clamp(material.Roughness, 0.02f, 1.0f);
        objectConstants.Metallic = std::clamp(material.Metallic, 0.0f, 1.0f);
        objectConstants.Alpha = std::clamp(material.Alpha, 0.0f, 1.0f);

        ID3D11ShaderResourceView* textureView = nullptr;
        if (material.BaseColorTexture != 0)
        {
            const auto texture = m_textures.find(material.BaseColorTexture);
            if (texture != m_textures.end())
            {
                textureView = texture->second.ShaderResourceView.Get();
                objectConstants.UseTexture = 1.0f;
            }
        }

        m_context->UpdateSubresource(m_objectConstantBuffer.Get(), 0, nullptr, &objectConstants, 0, 0);

        ID3D11Buffer* objectBuffer = m_objectConstantBuffer.Get();
        m_context->VSSetConstantBuffers(1, 1, &objectBuffer);
        m_context->PSSetConstantBuffers(1, 1, &objectBuffer);

        if (m_shadowPassActive)
        {
            const UINT stride = sizeof(Vertex);
            const UINT offset = 0;
            ID3D11Buffer* vertexBuffer = found->second.VertexBuffer.Get();
            m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            m_context->IASetIndexBuffer(found->second.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            m_context->DrawIndexed(found->second.IndexCount, 0, 0);
            return;
        }

        m_context->PSSetShaderResources(0, 1, &textureView);

        if (objectConstants.Alpha < 0.999f)
        {
            const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            m_context->OMSetBlendState(m_alphaBlendState.Get(), blendFactor, 0xffffffff);
        }
        else
        {
            m_context->OMSetBlendState(m_opaqueBlendState.Get(), nullptr, 0xffffffff);
        }

        const UINT stride = sizeof(Vertex);
        const UINT offset = 0;
        ID3D11Buffer* vertexBuffer = found->second.VertexBuffer.Get();
        m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        m_context->IASetIndexBuffer(found->second.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        m_context->DrawIndexed(found->second.IndexCount, 0, 0);

        ID3D11ShaderResourceView* nullTexture = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullTexture);
    }

    void Renderer::EndFrame()
    {
        if (!m_frameBegun || !m_swapChain)
        {
            return;
        }

        RenderPostProcess();
        EditorGui::Render();
        m_swapChain->Present(m_settings.VSync ? 1u : 0u, 0);
        m_frameBegun = false;
    }

    uint32_t Renderer::GetWidth() const
    {
        return m_width;
    }

    uint32_t Renderer::GetHeight() const
    {
        return m_height;
    }

    ID3D11Device* Renderer::GetDevice() const
    {
        return m_device.Get();
    }

    ID3D11DeviceContext* Renderer::GetContext() const
    {
        return m_context.Get();
    }

    const RendererSettings& Renderer::GetSettings() const
    {
        return m_settings;
    }

    bool Renderer::CreateDeviceAndSwapChain()
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferDesc.Width = m_width;
        swapChainDesc.BufferDesc.Height = m_height;
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 1;
        swapChainDesc.OutputWindow = m_windowHandle;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_0
        };

        UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL createdFeatureLevel = D3D_FEATURE_LEVEL_11_0;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createFlags,
            featureLevels,
            static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            m_swapChain.GetAddressOf(),
            m_device.GetAddressOf(),
            &createdFeatureLevel,
            m_context.GetAddressOf());

#if defined(_DEBUG)
        if (FAILED(hr))
        {
            createFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                createFlags,
                featureLevels,
                static_cast<UINT>(std::size(featureLevels)),
                D3D11_SDK_VERSION,
                &swapChainDesc,
                m_swapChain.GetAddressOf(),
                m_device.GetAddressOf(),
                &createdFeatureLevel,
                m_context.GetAddressOf());
        }
#endif

        if (FAILED(hr))
        {
            Log(LogLevel::Error, "D3D11 device creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        return true;
    }

    bool Renderer::CreateBackBufferResources()
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Swap chain back buffer retrieval failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_backBufferRenderTargetView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Render target view creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_TEXTURE2D_DESC hdrDesc = {};
        hdrDesc.Width = m_width;
        hdrDesc.Height = m_height;
        hdrDesc.MipLevels = 1;
        hdrDesc.ArraySize = 1;
        hdrDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        hdrDesc.SampleDesc.Count = 1;
        hdrDesc.Usage = D3D11_USAGE_DEFAULT;
        hdrDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        hr = m_device->CreateTexture2D(&hdrDesc, nullptr, m_hdrSceneTexture.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "HDR scene texture creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateRenderTargetView(m_hdrSceneTexture.Get(), nullptr, m_hdrSceneRenderTargetView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "HDR render target view creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_hdrSceneTexture.Get(), nullptr, m_hdrSceneShaderResourceView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "HDR shader resource view creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = m_width;
        depthDesc.Height = m_height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        hr = m_device->CreateTexture2D(&depthDesc, nullptr, m_depthStencilBuffer.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Depth buffer creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
        depthViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

        hr = m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &depthViewDesc, m_depthStencilView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Depth stencil view creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
        depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        depthSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        depthSrvDesc.Texture2D.MipLevels = 1;

        hr = m_device->CreateShaderResourceView(m_depthStencilBuffer.Get(), &depthSrvDesc, m_depthStencilShaderResourceView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Depth shader resource view creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateTexture2D(&hdrDesc, nullptr, m_historyTexture.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Temporal history texture creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_historyTexture.Get(), nullptr, m_historyShaderResourceView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Temporal history shader resource view creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        m_historyValid = false;
        return true;
    }

    bool Renderer::CreateShadowResources()
    {
        D3D11_TEXTURE2D_DESC shadowDesc = {};
        shadowDesc.Width = m_settings.ShadowMapSize;
        shadowDesc.Height = m_settings.ShadowMapSize;
        shadowDesc.MipLevels = 1;
        shadowDesc.ArraySize = 1;
        shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        shadowDesc.SampleDesc.Count = 1;
        shadowDesc.Usage = D3D11_USAGE_DEFAULT;
        shadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = m_device->CreateTexture2D(&shadowDesc, nullptr, m_shadowMapTexture.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Shadow map texture creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

        hr = m_device->CreateDepthStencilView(m_shadowMapTexture.Get(), &dsvDesc, m_shadowMapDepthView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Shadow map depth view creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = m_device->CreateShaderResourceView(m_shadowMapTexture.Get(), &srvDesc, m_shadowMapShaderResourceView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Shadow map shader resource view creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        return true;
    }

    bool Renderer::CreateShaders()
    {
        const std::filesystem::path shaderPath = FindAssetPath(L"Assets/Shaders/Basic.hlsl");
        const std::filesystem::path postShaderPath = FindAssetPath(L"Assets/Shaders/PostProcess.hlsl");

        Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> postVertexShaderBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> postPixelShaderBlob;

        if (!CompileShader(shaderPath, "VSMain", "vs_5_0", vertexShaderBlob) ||
            !CompileShader(shaderPath, "PSMain", "ps_5_0", pixelShaderBlob) ||
            !CompileShader(postShaderPath, "VSMain", "vs_5_0", postVertexShaderBlob) ||
            !CompileShader(postShaderPath, "PSMain", "ps_5_0", postPixelShaderBlob))
        {
            return false;
        }

        HRESULT hr = m_device->CreateVertexShader(
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            nullptr,
            m_vertexShader.GetAddressOf());

        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Vertex shader creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreatePixelShader(
            pixelShaderBlob->GetBufferPointer(),
            pixelShaderBlob->GetBufferSize(),
            nullptr,
            m_pixelShader.GetAddressOf());

        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Pixel shader creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateVertexShader(
            postVertexShaderBlob->GetBufferPointer(),
            postVertexShaderBlob->GetBufferSize(),
            nullptr,
            m_postVertexShader.GetAddressOf());

        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Post-process vertex shader creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreatePixelShader(
            postPixelShaderBlob->GetBufferPointer(),
            postPixelShaderBlob->GetBufferSize(),
            nullptr,
            m_postPixelShader.GetAddressOf());

        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Post-process pixel shader creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        const D3D11_INPUT_ELEMENT_DESC inputElements[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(DirectX::XMFLOAT3), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(DirectX::XMFLOAT3) * 2u, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = m_device->CreateInputLayout(
            inputElements,
            static_cast<UINT>(std::size(inputElements)),
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            m_inputLayout.GetAddressOf());

        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Input layout creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        return true;
    }

    bool Renderer::CreateStates()
    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_BACK;
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthClipEnable = TRUE;

        HRESULT hr = m_device->CreateRasterizerState(&rasterizerDesc, m_rasterizerState.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Rasterizer state creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_DEPTH_STENCIL_DESC depthDesc = {};
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthDesc.DepthFunc = D3D11_COMPARISON_LESS;

        hr = m_device->CreateDepthStencilState(&depthDesc, m_depthStencilState.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Depth stencil state creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_BLEND_DESC opaqueBlendDesc = {};
        opaqueBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = m_device->CreateBlendState(&opaqueBlendDesc, m_opaqueBlendState.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Opaque blend state creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_BLEND_DESC alphaBlendDesc = {};
        alphaBlendDesc.RenderTarget[0].BlendEnable = TRUE;
        alphaBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        alphaBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        alphaBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        alphaBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        alphaBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        alphaBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        alphaBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = m_device->CreateBlendState(&alphaBlendDesc, m_alphaBlendState.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Alpha blend state creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = m_device->CreateSamplerState(&samplerDesc, m_linearSamplerState.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Linear sampler creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        return true;
    }

    bool Renderer::CreateConstantBuffers()
    {
        D3D11_BUFFER_DESC frameBufferDesc = {};
        frameBufferDesc.ByteWidth = sizeof(FrameConstants);
        frameBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        frameBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        HRESULT hr = m_device->CreateBuffer(&frameBufferDesc, nullptr, m_frameConstantBuffer.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Frame constant buffer creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_BUFFER_DESC objectBufferDesc = {};
        objectBufferDesc.ByteWidth = sizeof(ObjectConstants);
        objectBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        objectBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        hr = m_device->CreateBuffer(&objectBufferDesc, nullptr, m_objectConstantBuffer.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Object constant buffer creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_BUFFER_DESC postBufferDesc = {};
        postBufferDesc.ByteWidth = sizeof(PostConstants);
        postBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        postBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        hr = m_device->CreateBuffer(&postBufferDesc, nullptr, m_postConstantBuffer.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Post-process constant buffer creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        return true;
    }

    void Renderer::ReleaseBackBufferResources()
    {
        m_historyShaderResourceView.Reset();
        m_historyTexture.Reset();
        m_depthStencilShaderResourceView.Reset();
        m_hdrSceneShaderResourceView.Reset();
        m_hdrSceneRenderTargetView.Reset();
        m_hdrSceneTexture.Reset();
        m_depthStencilView.Reset();
        m_depthStencilBuffer.Reset();
        m_backBufferRenderTargetView.Reset();
        m_historyValid = false;
    }

    void Renderer::ReleaseShadowResources()
    {
        m_shadowMapShaderResourceView.Reset();
        m_shadowMapDepthView.Reset();
        m_shadowMapTexture.Reset();
    }

    void Renderer::ApplyScenePipeline()
    {
        m_context->OMSetRenderTargets(1, m_hdrSceneRenderTargetView.GetAddressOf(), m_depthStencilView.Get());

        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(m_width);
        viewport.Height = static_cast<float>(m_height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &viewport);
        m_context->RSSetState(m_rasterizerState.Get());
        m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
        m_context->OMSetBlendState(m_opaqueBlendState.Get(), nullptr, 0xffffffff);

        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->IASetInputLayout(m_inputLayout.Get());
        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

        ID3D11SamplerState* sampler = m_linearSamplerState.Get();
        m_context->PSSetSamplers(0, 1, &sampler);

        ID3D11ShaderResourceView* shadowMap = (m_settings.Shadows && m_shadowMapShaderResourceView) ? m_shadowMapShaderResourceView.Get() : nullptr;
        m_context->PSSetShaderResources(1, 1, &shadowMap);

        UploadFrameConstants(m_camera.GetViewProjectionMatrix());
    }

    void Renderer::UploadFrameConstants(const DirectX::XMMATRIX& viewProjection)
    {
        FrameConstants frameConstants = {};
        DirectX::XMStoreFloat4x4(&frameConstants.ViewProjection, viewProjection);
        frameConstants.CameraPosition = m_camera.GetPosition();
        frameConstants.ElapsedTime = m_elapsedTime;
        frameConstants.LightDirection = m_light.Direction;
        frameConstants.AmbientIntensity = m_light.AmbientIntensity;
        frameConstants.LightColor = m_light.Color;
        frameConstants.LightIntensity = m_light.Intensity;
        frameConstants.LightViewProjection = m_lightViewProjection;
        frameConstants.ShadowStrength = m_settings.Shadows ? m_settings.ShadowStrength : 0.0f;
        frameConstants.ShadowMapTexelSize = 1.0f / static_cast<float>(std::max(1u, m_settings.ShadowMapSize));
        frameConstants.PointLightCount = m_settings.ClusteredLighting ? static_cast<float>(m_pointLightCount) : 0.0f;
        frameConstants.ClusteredLighting = m_settings.ClusteredLighting ? 1.0f : 0.0f;
        for (uint32_t index = 0; index < m_pointLightCount && index < m_pointLights.size(); ++index)
        {
            frameConstants.PointLights[index] = m_pointLights[index];
        }

        m_context->UpdateSubresource(m_frameConstantBuffer.Get(), 0, nullptr, &frameConstants, 0, 0);

        ID3D11Buffer* frameBuffer = m_frameConstantBuffer.Get();
        m_context->VSSetConstantBuffers(0, 1, &frameBuffer);
        m_context->PSSetConstantBuffers(0, 1, &frameBuffer);
    }

    void Renderer::RenderPostProcess()
    {
        if (!m_backBufferRenderTargetView || !m_hdrSceneShaderResourceView)
        {
            return;
        }

        m_context->OMSetRenderTargets(1, m_backBufferRenderTargetView.GetAddressOf(), nullptr);
        m_context->OMSetDepthStencilState(nullptr, 0);
        m_context->OMSetBlendState(m_opaqueBlendState.Get(), nullptr, 0xffffffff);
        m_context->RSSetState(m_rasterizerState.Get());

        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(m_width);
        viewport.Height = static_cast<float>(m_height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &viewport);

        m_context->IASetInputLayout(nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ID3D11Buffer* nullVertexBuffer = nullptr;
        const UINT stride = 0;
        const UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, &nullVertexBuffer, &stride, &offset);
        m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);

        m_context->VSSetShader(m_postVertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_postPixelShader.Get(), nullptr, 0);

        PostConstants postConstants;
        postConstants.Exposure = m_settings.Exposure;
        postConstants.ToneMapEnabled = m_settings.ToneMapping ? 1.0f : 0.0f;
        postConstants.BloomStrength = m_settings.Bloom ? m_settings.BloomStrength : 0.0f;
        postConstants.SsaoStrength = m_settings.SSAO ? m_settings.SsaoStrength : 0.0f;
        postConstants.TaaBlend = (m_settings.TemporalAA && m_historyValid) ? m_settings.TemporalBlend : 0.0f;
        postConstants.HistoryValid = m_historyValid ? 1.0f : 0.0f;
        postConstants.InvResolution = {
            1.0f / static_cast<float>(std::max(1u, m_width)),
            1.0f / static_cast<float>(std::max(1u, m_height))
        };
        postConstants.BloomThreshold = m_settings.BloomThreshold;
        postConstants.AntiAliasingStrength = m_settings.AntiAliasingStrength;
        postConstants.ColorSaturation = m_settings.ColorSaturation;
        postConstants.ColorContrast = m_settings.ColorContrast;
        postConstants.PostDebugView = static_cast<float>(m_settings.PostDebugView);
        postConstants.AntiAliasingEnabled = m_settings.AntiAliasing ? 1.0f : 0.0f;
        m_context->UpdateSubresource(m_postConstantBuffer.Get(), 0, nullptr, &postConstants, 0, 0);

        ID3D11Buffer* postBuffer = m_postConstantBuffer.Get();
        m_context->PSSetConstantBuffers(0, 1, &postBuffer);

        ID3D11ShaderResourceView* shaderResources[3] = {
            m_hdrSceneShaderResourceView.Get(),
            m_historyShaderResourceView.Get(),
            m_depthStencilShaderResourceView.Get()
        };
        ID3D11SamplerState* sampler = m_linearSamplerState.Get();
        m_context->PSSetShaderResources(0, 3, shaderResources);
        m_context->PSSetSamplers(0, 1, &sampler);
        m_context->Draw(3, 0);

        ID3D11ShaderResourceView* nullTextures[3] = { nullptr, nullptr, nullptr };
        m_context->PSSetShaderResources(0, 3, nullTextures);

        if (m_historyTexture && m_hdrSceneTexture)
        {
            m_context->CopyResource(m_historyTexture.Get(), m_hdrSceneTexture.Get());
            m_historyValid = true;
        }
    }
}
