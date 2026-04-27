#include "Disparity/Rendering/Renderer.h"

#include "Disparity/Core/Log.h"
#include "Disparity/Editor/EditorGui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <d3dcompiler.h>
#include <deque>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
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
            DirectX::XMFLOAT4 AlbedoRoughness = {};
            DirectX::XMFLOAT4 Surface = {};
            DirectX::XMFLOAT4 Emissive = {};
            DirectX::XMUINT4 ObjectInfo = {};
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
            float DepthOfFieldStrength = 0.0f;
            float DepthOfFieldFocus = 0.985f;
            float DepthOfFieldRange = 0.026f;
            float LensDirtStrength = 0.0f;
            float VignetteStrength = 0.08f;
            float LetterboxAmount = 0.0f;
            float TitleSafeOpacity = 0.0f;
            float FilmGrainStrength = 0.0f;
            float PresentationPulse = 0.0f;
            float PostPadding = 0.0f;
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

        bool WritePngFromRgbaRows(
            const std::filesystem::path& outputPath,
            uint32_t width,
            uint32_t height,
            const unsigned char* pixels,
            uint32_t rowPitch,
            std::string& error)
        {
            Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
            HRESULT hr = CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(factory.GetAddressOf()));
            if (FAILED(hr) || !factory)
            {
                error = "WIC factory creation failed with HRESULT " + HrToString(hr);
                return false;
            }

            Microsoft::WRL::ComPtr<IWICStream> stream;
            hr = factory->CreateStream(stream.GetAddressOf());
            if (FAILED(hr) || !stream)
            {
                error = "WIC stream creation failed with HRESULT " + HrToString(hr);
                return false;
            }

            hr = stream->InitializeFromFilename(outputPath.c_str(), GENERIC_WRITE);
            if (FAILED(hr))
            {
                error = "WIC stream file open failed with HRESULT " + HrToString(hr);
                return false;
            }

            Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
            hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf());
            if (FAILED(hr) || !encoder)
            {
                error = "WIC PNG encoder creation failed with HRESULT " + HrToString(hr);
                return false;
            }

            hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
            if (FAILED(hr))
            {
                error = "WIC PNG encoder initialization failed with HRESULT " + HrToString(hr);
                return false;
            }

            Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
            hr = encoder->CreateNewFrame(frame.GetAddressOf(), nullptr);
            if (FAILED(hr) || !frame)
            {
                error = "WIC PNG frame creation failed with HRESULT " + HrToString(hr);
                return false;
            }

            hr = frame->Initialize(nullptr);
            if (FAILED(hr))
            {
                error = "WIC PNG frame initialization failed with HRESULT " + HrToString(hr);
                return false;
            }

            hr = frame->SetSize(width, height);
            if (FAILED(hr))
            {
                error = "WIC PNG size setup failed with HRESULT " + HrToString(hr);
                return false;
            }

            WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppRGBA;
            hr = frame->SetPixelFormat(&pixelFormat);
            if (FAILED(hr) || !IsEqualGUID(pixelFormat, GUID_WICPixelFormat32bppRGBA))
            {
                error = "WIC PNG pixel format setup failed with HRESULT " + HrToString(hr);
                return false;
            }

            hr = frame->WritePixels(height, rowPitch, rowPitch * height, const_cast<unsigned char*>(pixels));
            if (FAILED(hr))
            {
                error = "WIC PNG pixel write failed with HRESULT " + HrToString(hr);
                return false;
            }

            hr = frame->Commit();
            if (FAILED(hr))
            {
                error = "WIC PNG frame commit failed with HRESULT " + HrToString(hr);
                return false;
            }

            hr = encoder->Commit();
            if (FAILED(hr))
            {
                error = "WIC PNG encoder commit failed with HRESULT " + HrToString(hr);
                return false;
            }

            return true;
        }

        std::string LowerExtension(const std::filesystem::path& path)
        {
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
            return extension;
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

        CreateGpuProfilingResources();

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
        m_doubleSidedRasterizerState.Reset();
        m_rasterizerState.Reset();
        m_objectConstantBuffer.Reset();
        m_frameConstantBuffer.Reset();
        m_inputLayout.Reset();
        m_postConstantBuffer.Reset();
        m_postPixelShader.Reset();
        m_postVertexShader.Reset();
        m_pixelShader.Reset();
        m_vertexShader.Reset();
        m_gpuFrameEndQuery.Reset();
        m_gpuFrameStartQuery.Reset();
        m_gpuFrameDisjointQuery.Reset();
        m_gpuPassProfiles.clear();
        m_gpuTimestampFrequency = 0;
        m_gpuTimingSupported = false;
        m_gpuTimingValid = false;
        m_gpuFrameQueryIssued = false;
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
        m_settings.DepthOfFieldFocus = std::clamp(m_settings.DepthOfFieldFocus, 0.0f, 1.0f);
        m_settings.DepthOfFieldRange = std::clamp(m_settings.DepthOfFieldRange, 0.001f, 0.25f);
        m_settings.DepthOfFieldStrength = std::clamp(m_settings.DepthOfFieldStrength, 0.0f, 1.0f);
        m_settings.LensDirtStrength = std::clamp(m_settings.LensDirtStrength, 0.0f, 1.0f);
        m_settings.VignetteStrength = std::clamp(m_settings.VignetteStrength, 0.0f, 1.0f);
        m_settings.LetterboxAmount = std::clamp(m_settings.LetterboxAmount, 0.0f, 0.25f);
        m_settings.TitleSafeOpacity = std::clamp(m_settings.TitleSafeOpacity, 0.0f, 1.0f);
        m_settings.FilmGrainStrength = std::clamp(m_settings.FilmGrainStrength, 0.0f, 0.25f);
        m_settings.PresentationPulse = std::clamp(m_settings.PresentationPulse, 0.0f, 1.0f);
        m_settings.PostDebugView = std::clamp(m_settings.PostDebugView, 0u, 6u);
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
        m_frameDrawCalls = 0;
        m_sceneDrawCalls = 0;
        m_shadowDrawCalls = 0;
        m_graphExecutedPasses.clear();
        m_graphDispatchOrderValid = true;

        m_shadowPassActive = false;
        BuildFrameRenderGraph();
        BeginGpuFrameProfile();
        BeginGraphPass(m_graphClearPass);
        m_context->OMSetRenderTargets(1, m_hdrSceneRenderTargetView.GetAddressOf(), m_depthStencilView.Get());
        m_context->ClearRenderTargetView(m_hdrSceneRenderTargetView.Get(), clearColor);
        m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        if (m_editorObjectIdRenderTargetView && m_editorObjectDepthRenderTargetView)
        {
            const float noObjectId[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            const float farDepth[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
            m_context->ClearRenderTargetView(m_editorObjectIdRenderTargetView.Get(), noObjectId);
            m_context->ClearRenderTargetView(m_editorObjectDepthRenderTargetView.Get(), farDepth);
        }
        EndGraphPass();

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
        BeginGraphPass(m_graphShadowPass);
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
        EndGraphPass();
        ApplyScenePipeline();
    }

    void Renderer::DrawMesh(MeshHandle mesh, const Transform& transform, const Material& material)
    {
        DrawMeshWithId(mesh, transform, material, 0);
    }

    void Renderer::DrawMeshWithId(MeshHandle mesh, const Transform& transform, const Material& material, uint32_t editorObjectId)
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
        objectConstants.AlbedoRoughness = {
            material.Albedo.x,
            material.Albedo.y,
            material.Albedo.z,
            std::clamp(material.Roughness, 0.02f, 1.0f)
        };
        objectConstants.Surface = {
            std::clamp(material.Metallic, 0.0f, 1.0f),
            std::clamp(material.Alpha, 0.0f, 1.0f),
            0.0f,
            std::max(0.0f, material.EmissiveIntensity)
        };
        objectConstants.Emissive = {
            std::max(0.0f, material.Emissive.x),
            std::max(0.0f, material.Emissive.y),
            std::max(0.0f, material.Emissive.z),
            0.0f
        };
        objectConstants.ObjectInfo = { editorObjectId, 0u, 0u, 0u };

        ID3D11ShaderResourceView* textureView = nullptr;
        if (material.BaseColorTexture != 0)
        {
            const auto texture = m_textures.find(material.BaseColorTexture);
            if (texture != m_textures.end())
            {
                textureView = texture->second.ShaderResourceView.Get();
                objectConstants.Surface.z = 1.0f;
            }
        }

        m_context->UpdateSubresource(m_objectConstantBuffer.Get(), 0, nullptr, &objectConstants, 0, 0);

        ID3D11Buffer* objectBuffer = m_objectConstantBuffer.Get();
        m_context->VSSetConstantBuffers(1, 1, &objectBuffer);
        m_context->PSSetConstantBuffers(1, 1, &objectBuffer);
        ID3D11RasterizerState* rasterizerState = material.DoubleSided ? m_doubleSidedRasterizerState.Get() : m_rasterizerState.Get();
        m_context->RSSetState(rasterizerState);

        if (m_shadowPassActive)
        {
            ++m_frameDrawCalls;
            ++m_shadowDrawCalls;
            const UINT stride = sizeof(Vertex);
            const UINT offset = 0;
            ID3D11Buffer* vertexBuffer = found->second.VertexBuffer.Get();
            m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            m_context->IASetIndexBuffer(found->second.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            m_context->DrawIndexed(found->second.IndexCount, 0, 0);
            return;
        }

        if (m_activeGraphPass != m_graphScenePass)
        {
            BeginGraphPass(m_graphScenePass);
        }

        m_context->PSSetShaderResources(0, 1, &textureView);
        if (editorObjectId != 0 && m_editorObjectIdRenderTargetView && m_editorObjectDepthRenderTargetView)
        {
            ID3D11RenderTargetView* targets[3] = {
                m_hdrSceneRenderTargetView.Get(),
                m_editorObjectIdRenderTargetView.Get(),
                m_editorObjectDepthRenderTargetView.Get()
            };
            m_context->OMSetRenderTargets(3, targets, m_depthStencilView.Get());
        }
        else
        {
            m_context->OMSetRenderTargets(1, m_hdrSceneRenderTargetView.GetAddressOf(), m_depthStencilView.Get());
        }

        if (objectConstants.Surface.y < 0.999f)
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
        ++m_frameDrawCalls;
        ++m_sceneDrawCalls;

        ID3D11ShaderResourceView* nullTexture = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullTexture);
    }

    void Renderer::EndFrame()
    {
        if (!m_frameBegun || !m_swapChain)
        {
            return;
        }

        EndGraphPass();
        BeginGraphPass(m_graphEditorViewportPass);
        if (m_editorViewportRenderTargetView)
        {
            const float transparentBlack[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            m_context->ClearRenderTargetView(m_editorViewportRenderTargetView.Get(), transparentBlack);
        }
        EndGraphPass();
        BeginGraphPass(m_graphPostPass);
        RenderPostProcess();
        EndGraphPass();
        BeginGraphPass(m_graphEditorPass);
        EditorGui::Render();
        EndGraphPass();
        if (!m_pendingFrameCapturePaths.empty())
        {
            const std::filesystem::path capturePath = std::move(m_pendingFrameCapturePaths.front());
            m_pendingFrameCapturePaths.pop_front();
            m_lastFrameCapture = CaptureBackBuffer(capturePath);
            m_hasLastFrameCapture = true;
        }
        EndGpuFrameProfile();
        m_swapChain->Present(m_settings.VSync ? 1u : 0u, 0);
        m_frameBegun = false;
    }

    void Renderer::RequestFrameCapture(std::filesystem::path outputPath)
    {
        m_pendingFrameCapturePaths.push_back(std::move(outputPath));
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

    const RenderGraph& Renderer::GetRenderGraph() const
    {
        return m_renderGraph;
    }

    uint32_t Renderer::GetFrameDrawCalls() const
    {
        return m_frameDrawCalls;
    }

    uint32_t Renderer::GetSceneDrawCalls() const
    {
        return m_sceneDrawCalls;
    }

    uint32_t Renderer::GetShadowDrawCalls() const
    {
        return m_shadowDrawCalls;
    }

    RendererFrameGraphDiagnostics Renderer::GetFrameGraphDiagnostics() const
    {
        return RendererFrameGraphDiagnostics{
            m_graphDispatchOrderValid,
            static_cast<uint32_t>(m_graphExecutedPasses.size()),
            m_graphTransientAllocations,
            m_graphAliasedResources
        };
    }

    EditorViewportResourcesInfo Renderer::GetEditorViewportResources() const
    {
        return EditorViewportResourcesInfo{
            m_editorViewportTexture.Get() != nullptr && m_editorViewportRenderTargetView.Get() != nullptr,
            m_editorObjectIdTexture.Get() != nullptr && m_editorObjectIdRenderTargetView.Get() != nullptr,
            m_editorObjectDepthTexture.Get() != nullptr && m_editorObjectDepthRenderTargetView.Get() != nullptr,
            m_width,
            m_height
        };
    }

    EditorObjectIdReadback Renderer::ReadEditorObjectId(uint32_t x, uint32_t y) const
    {
        EditorObjectIdReadback result;
        result.X = x;
        result.Y = y;

        if (!m_device || !m_context || !m_editorObjectIdTexture || !m_editorObjectDepthTexture)
        {
            result.Error = "Editor pick targets are not ready.";
            return result;
        }
        if (x >= m_width || y >= m_height)
        {
            result.Error = "Editor pick readback is outside the viewport.";
            return result;
        }

        D3D11_TEXTURE2D_DESC idDesc = {};
        m_editorObjectIdTexture->GetDesc(&idDesc);
        idDesc.Width = 1;
        idDesc.Height = 1;
        idDesc.BindFlags = 0;
        idDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        idDesc.MiscFlags = 0;
        idDesc.Usage = D3D11_USAGE_STAGING;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> idStaging;
        HRESULT hr = m_device->CreateTexture2D(&idDesc, nullptr, idStaging.GetAddressOf());
        if (FAILED(hr) || !idStaging)
        {
            result.Error = "Object-ID staging texture creation failed with HRESULT " + HrToString(hr);
            return result;
        }

        D3D11_TEXTURE2D_DESC depthDesc = {};
        m_editorObjectDepthTexture->GetDesc(&depthDesc);
        depthDesc.Width = 1;
        depthDesc.Height = 1;
        depthDesc.BindFlags = 0;
        depthDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        depthDesc.MiscFlags = 0;
        depthDesc.Usage = D3D11_USAGE_STAGING;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStaging;
        hr = m_device->CreateTexture2D(&depthDesc, nullptr, depthStaging.GetAddressOf());
        if (FAILED(hr) || !depthStaging)
        {
            result.Error = "Object-depth staging texture creation failed with HRESULT " + HrToString(hr);
            return result;
        }

        D3D11_BOX sourceBox = {};
        sourceBox.left = x;
        sourceBox.right = x + 1u;
        sourceBox.top = y;
        sourceBox.bottom = y + 1u;
        sourceBox.front = 0;
        sourceBox.back = 1;
        m_context->CopySubresourceRegion(idStaging.Get(), 0, 0, 0, 0, m_editorObjectIdTexture.Get(), 0, &sourceBox);
        m_context->CopySubresourceRegion(depthStaging.Get(), 0, 0, 0, 0, m_editorObjectDepthTexture.Get(), 0, &sourceBox);

        D3D11_MAPPED_SUBRESOURCE idMapped = {};
        hr = m_context->Map(idStaging.Get(), 0, D3D11_MAP_READ, 0, &idMapped);
        if (FAILED(hr))
        {
            result.Error = "Object-ID staging map failed with HRESULT " + HrToString(hr);
            return result;
        }
        result.ObjectId = *static_cast<const uint32_t*>(idMapped.pData);
        m_context->Unmap(idStaging.Get(), 0);

        D3D11_MAPPED_SUBRESOURCE depthMapped = {};
        hr = m_context->Map(depthStaging.Get(), 0, D3D11_MAP_READ, 0, &depthMapped);
        if (FAILED(hr))
        {
            result.Error = "Object-depth staging map failed with HRESULT " + HrToString(hr);
            return result;
        }
        result.Depth = *static_cast<const float*>(depthMapped.pData);
        m_context->Unmap(depthStaging.Get(), 0);

        result.Valid = result.ObjectId != 0;
        return result;
    }

    double Renderer::GetGpuFrameMilliseconds() const
    {
        return m_gpuFrameMilliseconds;
    }

    bool Renderer::IsGpuTimingAvailable() const
    {
        return m_gpuTimingSupported && m_gpuTimingValid;
    }

    bool Renderer::HasLastFrameCapture() const
    {
        return m_hasLastFrameCapture;
    }

    const FrameCaptureResult& Renderer::GetLastFrameCapture() const
    {
        return m_lastFrameCapture;
    }

    FrameCaptureResult Renderer::CaptureBackBuffer(const std::filesystem::path& outputPath) const
    {
        FrameCaptureResult result;
        result.Path = outputPath;

        if (!m_device || !m_context || !m_swapChain)
        {
            result.Error = "Renderer is not initialized.";
            return result;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
        if (FAILED(hr) || !backBuffer)
        {
            result.Error = "Back buffer retrieval failed with HRESULT " + HrToString(hr);
            return result;
        }

        D3D11_TEXTURE2D_DESC desc = {};
        backBuffer->GetDesc(&desc);
        if (desc.Width == 0 || desc.Height == 0 || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM)
        {
            result.Error = "Back buffer has an unsupported capture format.";
            return result;
        }

        D3D11_TEXTURE2D_DESC stagingDesc = desc;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MiscFlags = 0;
        stagingDesc.Usage = D3D11_USAGE_STAGING;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
        hr = m_device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.GetAddressOf());
        if (FAILED(hr) || !stagingTexture)
        {
            result.Error = "Capture staging texture creation failed with HRESULT " + HrToString(hr);
            return result;
        }

        m_context->CopyResource(stagingTexture.Get(), backBuffer.Get());

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        hr = m_context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr))
        {
            result.Error = "Capture staging texture map failed with HRESULT " + HrToString(hr);
            return result;
        }

        if (outputPath.has_parent_path())
        {
            std::filesystem::create_directories(outputPath.parent_path());
        }

        result.Width = desc.Width;
        result.Height = desc.Height;
        result.RgbChecksum = 1469598103934665603ull;

        std::vector<unsigned char> rgbaPixels;
        const bool writePng = LowerExtension(outputPath) == ".png";
        if (writePng)
        {
            rgbaPixels.resize(static_cast<size_t>(result.Width) * static_cast<size_t>(result.Height) * 4u);
        }
        std::ofstream image;
        if (!writePng)
        {
            image.open(outputPath, std::ios::binary | std::ios::trunc);
            if (!image)
            {
                m_context->Unmap(stagingTexture.Get(), 0);
                result.Error = "Capture image file could not be opened.";
                return result;
            }
            image << "P6\n" << result.Width << " " << result.Height << "\n255\n";
        }

        for (uint32_t y = 0; y < desc.Height; ++y)
        {
            const auto* row = static_cast<const unsigned char*>(mapped.pData) + static_cast<size_t>(mapped.RowPitch) * y;
            for (uint32_t x = 0; x < desc.Width; ++x)
            {
                const unsigned char* pixel = row + static_cast<size_t>(x) * 4u;
                const unsigned char rgb[3] = { pixel[0], pixel[1], pixel[2] };
                if (writePng)
                {
                    const size_t outputIndex = (static_cast<size_t>(y) * result.Width + x) * 4u;
                    rgbaPixels[outputIndex + 0u] = pixel[0];
                    rgbaPixels[outputIndex + 1u] = pixel[1];
                    rgbaPixels[outputIndex + 2u] = pixel[2];
                    rgbaPixels[outputIndex + 3u] = pixel[3];
                }
                else
                {
                    image.write(reinterpret_cast<const char*>(rgb), sizeof(rgb));
                }

                const uint32_t brightness = static_cast<uint32_t>(rgb[0]) + static_cast<uint32_t>(rgb[1]) + static_cast<uint32_t>(rgb[2]);
                result.NonBlackPixels += brightness > 8u ? 1u : 0u;
                result.BrightPixels += brightness > 540u ? 1u : 0u;
                result.AverageLuma +=
                    static_cast<double>(rgb[0]) * 0.2126 +
                    static_cast<double>(rgb[1]) * 0.7152 +
                    static_cast<double>(rgb[2]) * 0.0722;

                for (const unsigned char channel : rgb)
                {
                    result.RgbChecksum ^= static_cast<uint64_t>(channel);
                    result.RgbChecksum *= 1099511628211ull;
                }
            }
        }

        m_context->Unmap(stagingTexture.Get(), 0);
        const uint64_t pixelCount = static_cast<uint64_t>(result.Width) * static_cast<uint64_t>(result.Height);
        if (pixelCount > 0)
        {
            result.AverageLuma /= static_cast<double>(pixelCount);
        }
        if (writePng)
        {
            result.Success = WritePngFromRgbaRows(
                outputPath,
                result.Width,
                result.Height,
                rgbaPixels.data(),
                result.Width * 4u,
                result.Error);
        }
        else
        {
            result.Success = image.good();
        }
        if (!result.Success)
        {
            if (result.Error.empty())
            {
                result.Error = "Capture image write failed.";
            }
        }
        return result;
    }

    void Renderer::BuildFrameRenderGraph()
    {
        m_renderGraph.Reset();
        m_graphBackBuffer = m_renderGraph.AddResource("Back Buffer", RenderGraphResourceKind::External);
        m_graphHdrScene = m_renderGraph.AddResource("HDR Scene Color", RenderGraphResourceKind::Texture);
        m_graphDepth = m_renderGraph.AddResource("Scene Depth", RenderGraphResourceKind::Texture);
        m_graphHistory = m_renderGraph.AddResource("Temporal History", RenderGraphResourceKind::Texture);
        m_graphShadowMap = m_renderGraph.AddResource("Directional Shadow Map", RenderGraphResourceKind::Texture);
        m_graphEditorViewport = m_renderGraph.AddResource("Editor Viewport Color", RenderGraphResourceKind::Texture);
        m_graphEditorObjectIds = m_renderGraph.AddResource("Editor Object IDs", RenderGraphResourceKind::Texture);
        m_graphEditorObjectDepth = m_renderGraph.AddResource("Editor Object Depth", RenderGraphResourceKind::Texture);
        m_graphClearPass = m_renderGraph.AddPass("Clear HDR+Depth+Pick Targets", {}, { m_graphHdrScene, m_graphDepth, m_graphEditorObjectIds, m_graphEditorObjectDepth });
        m_graphShadowPass = m_renderGraph.AddPass("Shadow Map", {}, { m_graphShadowMap });
        m_renderGraph.SetPassEnabled(m_graphShadowPass, m_settings.Shadows, "Shadows disabled");
        std::vector<uint32_t> sceneReads = m_settings.Shadows ? std::vector<uint32_t>{ m_graphShadowMap } : std::vector<uint32_t>{};
        m_graphScenePass = m_renderGraph.AddPass("Scene Color + Object IDs", std::move(sceneReads), { m_graphHdrScene, m_graphDepth, m_graphEditorObjectIds, m_graphEditorObjectDepth });
        m_graphEditorViewportPass = m_renderGraph.AddPass(
            "Editor Viewport Targets",
            { m_graphHdrScene, m_graphDepth },
            { m_graphEditorViewport });
        m_graphPostPass = m_renderGraph.AddPass("Post Process", { m_graphHdrScene, m_graphDepth, m_graphHistory }, { m_graphBackBuffer, m_graphHistory });
        m_graphEditorPass = m_renderGraph.AddPass("Editor UI", { m_graphBackBuffer }, { m_graphBackBuffer });
        m_activeGraphPass = std::numeric_limits<uint32_t>::max();
        (void)m_renderGraph.Compile();
        m_graphTransientAllocations = 0;
        m_graphAliasedResources = 0;
        for (const RenderGraphResourceAllocation& allocation : m_renderGraph.GetResourceAllocations())
        {
            if (!allocation.External)
            {
                ++m_graphTransientAllocations;
            }
            if (allocation.Aliased)
            {
                ++m_graphAliasedResources;
            }
        }
        EnsureGpuPassProfiles(m_renderGraph.GetPasses().size());
    }

    void Renderer::BeginGraphPass(uint32_t passId)
    {
        if (passId == std::numeric_limits<uint32_t>::max())
        {
            return;
        }

        if (m_activeGraphPass == passId)
        {
            return;
        }

        EndGraphPass();
        const auto& passes = m_renderGraph.GetPasses();
        if (passId >= passes.size())
        {
            m_graphDispatchOrderValid = false;
        }
        else
        {
            const uint32_t expectedOrder = passes[passId].ExecutionOrder;
            if (expectedOrder != std::numeric_limits<uint32_t>::max() &&
                expectedOrder != static_cast<uint32_t>(m_graphExecutedPasses.size()))
            {
                m_graphDispatchOrderValid = false;
            }
        }
        m_graphExecutedPasses.push_back(passId);
        m_activeGraphPass = passId;
        m_graphPassStart = std::chrono::steady_clock::now();
        BeginGpuPassProfile(passId);
    }

    void Renderer::EndGraphPass()
    {
        if (m_activeGraphPass == std::numeric_limits<uint32_t>::max())
        {
            return;
        }

        const auto end = std::chrono::steady_clock::now();
        EndGpuPassProfile(m_activeGraphPass);
        m_renderGraph.RecordPassCpuTime(
            m_activeGraphPass,
            std::chrono::duration<double, std::milli>(end - m_graphPassStart).count());
        m_activeGraphPass = std::numeric_limits<uint32_t>::max();
    }

    void Renderer::CreateGpuProfilingResources()
    {
        m_gpuTimingSupported = false;
        m_gpuTimingValid = false;
        m_gpuFrameQueryIssued = false;

        if (!m_device)
        {
            return;
        }

        D3D11_QUERY_DESC disjointDesc = {};
        disjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        D3D11_QUERY_DESC timestampDesc = {};
        timestampDesc.Query = D3D11_QUERY_TIMESTAMP;

        const HRESULT disjointResult = m_device->CreateQuery(&disjointDesc, m_gpuFrameDisjointQuery.GetAddressOf());
        const HRESULT startResult = m_device->CreateQuery(&timestampDesc, m_gpuFrameStartQuery.GetAddressOf());
        const HRESULT endResult = m_device->CreateQuery(&timestampDesc, m_gpuFrameEndQuery.GetAddressOf());
        m_gpuTimingSupported = SUCCEEDED(disjointResult) && SUCCEEDED(startResult) && SUCCEEDED(endResult);
    }

    void Renderer::BeginGpuFrameProfile()
    {
        ResolveGpuFrameProfile();
        if (!m_gpuFrameQueryIssued)
        {
            ResolveGpuPassProfiles();
        }
        if (!m_gpuTimingSupported || !m_context || m_gpuFrameQueryIssued)
        {
            return;
        }

        m_context->Begin(m_gpuFrameDisjointQuery.Get());
        m_context->End(m_gpuFrameStartQuery.Get());
    }

    void Renderer::EndGpuFrameProfile()
    {
        if (!m_gpuTimingSupported || !m_context || m_gpuFrameQueryIssued)
        {
            return;
        }

        m_context->End(m_gpuFrameEndQuery.Get());
        m_context->End(m_gpuFrameDisjointQuery.Get());
        m_gpuFrameQueryIssued = true;
    }

    void Renderer::ResolveGpuFrameProfile()
    {
        if (!m_gpuTimingSupported || !m_context || !m_gpuFrameQueryIssued)
        {
            return;
        }

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = {};
        UINT64 startTicks = 0;
        UINT64 endTicks = 0;
        const UINT flags = D3D11_ASYNC_GETDATA_DONOTFLUSH;
        const HRESULT disjointResult = m_context->GetData(m_gpuFrameDisjointQuery.Get(), &disjoint, sizeof(disjoint), flags);
        const HRESULT startResult = m_context->GetData(m_gpuFrameStartQuery.Get(), &startTicks, sizeof(startTicks), flags);
        const HRESULT endResult = m_context->GetData(m_gpuFrameEndQuery.Get(), &endTicks, sizeof(endTicks), flags);

        if (disjointResult != S_OK || startResult != S_OK || endResult != S_OK)
        {
            return;
        }

        m_gpuFrameQueryIssued = false;
        if (disjoint.Disjoint || disjoint.Frequency == 0)
        {
            for (GpuPassProfile& passProfile : m_gpuPassProfiles)
            {
                passProfile.QueryIssued = false;
            }
            return;
        }

        m_gpuTimestampFrequency = disjoint.Frequency;
        if (endTicks >= startTicks)
        {
            m_gpuFrameMilliseconds = (static_cast<double>(endTicks - startTicks) / static_cast<double>(disjoint.Frequency)) * 1000.0;
            m_gpuTimingValid = true;
        }
        ResolveGpuPassProfiles();
    }

    void Renderer::EnsureGpuPassProfiles(size_t passCount)
    {
        if (!m_gpuTimingSupported || !m_device)
        {
            return;
        }

        if (m_gpuPassProfiles.size() >= passCount)
        {
            return;
        }

        D3D11_QUERY_DESC timestampDesc = {};
        timestampDesc.Query = D3D11_QUERY_TIMESTAMP;
        while (m_gpuPassProfiles.size() < passCount)
        {
            GpuPassProfile profile;
            const HRESULT startResult = m_device->CreateQuery(&timestampDesc, profile.StartQuery.GetAddressOf());
            const HRESULT endResult = m_device->CreateQuery(&timestampDesc, profile.EndQuery.GetAddressOf());
            if (FAILED(startResult) || FAILED(endResult))
            {
                m_gpuTimingSupported = false;
                m_gpuPassProfiles.clear();
                return;
            }

            m_gpuPassProfiles.push_back(std::move(profile));
        }
    }

    void Renderer::BeginGpuPassProfile(uint32_t passId)
    {
        if (!m_gpuTimingSupported || !m_context || passId >= m_gpuPassProfiles.size())
        {
            return;
        }

        GpuPassProfile& profile = m_gpuPassProfiles[passId];
        if (profile.QueryIssued)
        {
            return;
        }

        m_context->End(profile.StartQuery.Get());
    }

    void Renderer::EndGpuPassProfile(uint32_t passId)
    {
        if (!m_gpuTimingSupported || !m_context || passId >= m_gpuPassProfiles.size())
        {
            return;
        }

        GpuPassProfile& profile = m_gpuPassProfiles[passId];
        if (profile.QueryIssued)
        {
            return;
        }

        m_context->End(profile.EndQuery.Get());
        profile.QueryIssued = true;
    }

    void Renderer::ResolveGpuPassProfiles()
    {
        if (!m_gpuTimingSupported || !m_context || m_gpuTimestampFrequency == 0)
        {
            return;
        }

        const UINT flags = D3D11_ASYNC_GETDATA_DONOTFLUSH;
        for (uint32_t passId = 0; passId < m_gpuPassProfiles.size(); ++passId)
        {
            GpuPassProfile& profile = m_gpuPassProfiles[passId];
            if (!profile.QueryIssued)
            {
                continue;
            }

            UINT64 startTicks = 0;
            UINT64 endTicks = 0;
            const HRESULT startResult = m_context->GetData(profile.StartQuery.Get(), &startTicks, sizeof(startTicks), flags);
            const HRESULT endResult = m_context->GetData(profile.EndQuery.Get(), &endTicks, sizeof(endTicks), flags);
            if (startResult != S_OK || endResult != S_OK)
            {
                continue;
            }

            profile.QueryIssued = false;
            if (endTicks >= startTicks)
            {
                const double milliseconds = (static_cast<double>(endTicks - startTicks) / static_cast<double>(m_gpuTimestampFrequency)) * 1000.0;
                m_renderGraph.RecordPassGpuTime(passId, milliseconds);
            }
        }
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

        hr = m_device->CreateTexture2D(&hdrDesc, nullptr, m_editorViewportTexture.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Editor viewport texture creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateRenderTargetView(m_editorViewportTexture.Get(), nullptr, m_editorViewportRenderTargetView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Editor viewport render target creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_editorViewportTexture.Get(), nullptr, m_editorViewportShaderResourceView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Editor viewport shader resource view creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_TEXTURE2D_DESC objectIdDesc = {};
        objectIdDesc.Width = m_width;
        objectIdDesc.Height = m_height;
        objectIdDesc.MipLevels = 1;
        objectIdDesc.ArraySize = 1;
        objectIdDesc.Format = DXGI_FORMAT_R32_UINT;
        objectIdDesc.SampleDesc.Count = 1;
        objectIdDesc.Usage = D3D11_USAGE_DEFAULT;
        objectIdDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        hr = m_device->CreateTexture2D(&objectIdDesc, nullptr, m_editorObjectIdTexture.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Editor object-ID texture creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateRenderTargetView(m_editorObjectIdTexture.Get(), nullptr, m_editorObjectIdRenderTargetView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Editor object-ID render target creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_editorObjectIdTexture.Get(), nullptr, m_editorObjectIdShaderResourceView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Editor object-ID shader resource view creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_TEXTURE2D_DESC objectDepthDesc = objectIdDesc;
        objectDepthDesc.Format = DXGI_FORMAT_R32_FLOAT;

        hr = m_device->CreateTexture2D(&objectDepthDesc, nullptr, m_editorObjectDepthTexture.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Editor object-depth texture creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateRenderTargetView(m_editorObjectDepthTexture.Get(), nullptr, m_editorObjectDepthRenderTargetView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Editor object-depth render target creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_editorObjectDepthTexture.Get(), nullptr, m_editorObjectDepthShaderResourceView.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Editor object-depth shader resource view creation failed with HRESULT " + HrToString(hr));
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

        D3D11_RASTERIZER_DESC doubleSidedRasterizerDesc = rasterizerDesc;
        doubleSidedRasterizerDesc.CullMode = D3D11_CULL_NONE;

        hr = m_device->CreateRasterizerState(&doubleSidedRasterizerDesc, m_doubleSidedRasterizerState.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Double-sided rasterizer state creation failed with HRESULT " + HrToString(hr));
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
        opaqueBlendDesc.IndependentBlendEnable = TRUE;
        opaqueBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        opaqueBlendDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        opaqueBlendDesc.RenderTarget[2].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = m_device->CreateBlendState(&opaqueBlendDesc, m_opaqueBlendState.GetAddressOf());
        if (FAILED(hr))
        {
            Log(LogLevel::Error, "Opaque blend state creation failed with HRESULT " + HrToString(hr));
            return false;
        }

        D3D11_BLEND_DESC alphaBlendDesc = {};
        alphaBlendDesc.IndependentBlendEnable = TRUE;
        alphaBlendDesc.RenderTarget[0].BlendEnable = TRUE;
        alphaBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        alphaBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        alphaBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        alphaBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        alphaBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        alphaBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        alphaBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        alphaBlendDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        alphaBlendDesc.RenderTarget[2].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

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
        m_editorObjectDepthShaderResourceView.Reset();
        m_editorObjectDepthRenderTargetView.Reset();
        m_editorObjectDepthTexture.Reset();
        m_editorObjectIdShaderResourceView.Reset();
        m_editorObjectIdRenderTargetView.Reset();
        m_editorObjectIdTexture.Reset();
        m_editorViewportShaderResourceView.Reset();
        m_editorViewportRenderTargetView.Reset();
        m_editorViewportTexture.Reset();
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
        postConstants.DepthOfFieldStrength = m_settings.DepthOfField ? m_settings.DepthOfFieldStrength : 0.0f;
        postConstants.DepthOfFieldFocus = m_settings.DepthOfFieldFocus;
        postConstants.DepthOfFieldRange = m_settings.DepthOfFieldRange;
        postConstants.LensDirtStrength = m_settings.LensDirt ? m_settings.LensDirtStrength : 0.0f;
        postConstants.VignetteStrength = m_settings.VignetteStrength;
        postConstants.LetterboxAmount = m_settings.CinematicOverlay ? m_settings.LetterboxAmount : 0.0f;
        postConstants.TitleSafeOpacity = m_settings.CinematicOverlay ? m_settings.TitleSafeOpacity : 0.0f;
        postConstants.FilmGrainStrength = m_settings.FilmGrainStrength;
        postConstants.PresentationPulse = m_settings.PresentationPulse;
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
