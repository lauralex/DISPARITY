#include <Disparity/Disparity.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cctype>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <windows.h>

namespace
{
    constexpr float Pi = 3.1415926535f;
    constexpr size_t InvalidIndex = std::numeric_limits<size_t>::max();
    constexpr uint32_t EditorPickPlayerId = 1u;
    constexpr uint32_t EditorPickSceneObjectBase = 1000u;
    constexpr uint32_t EditorPickGizmoAxisBase = 0x10000000u;
    constexpr uint32_t EditorPickGizmoPlaneBase = 0x20000000u;

    DirectX::XMFLOAT3 Add(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    DirectX::XMFLOAT3 Subtract(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    DirectX::XMFLOAT3 Scale(const DirectX::XMFLOAT3& value, float scalar)
    {
        return { value.x * scalar, value.y * scalar, value.z * scalar };
    }

    DirectX::XMFLOAT3 Lerp(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    float SmoothStep01(float value)
    {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    float Dot(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    DirectX::XMFLOAT3 Cross(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    float Length(const DirectX::XMFLOAT3& value)
    {
        return std::sqrt(Dot(value, value));
    }

    DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& value)
    {
        const float length = Length(value);
        if (length <= 0.0001f)
        {
            return {};
        }

        return { value.x / length, value.y / length, value.z / length };
    }

    DirectX::XMFLOAT3 TransformDirection(const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT3& rotation)
    {
        const DirectX::XMVECTOR vector = DirectX::XMLoadFloat3(&direction);
        const DirectX::XMMATRIX matrix = DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);
        DirectX::XMFLOAT3 result = {};
        DirectX::XMStoreFloat3(&result, DirectX::XMVector3TransformNormal(vector, matrix));
        return Normalize(result);
    }

    std::string SafeFileStem(std::string value)
    {
        if (value.empty())
        {
            value = "Material";
        }

        for (char& character : value)
        {
            const unsigned char c = static_cast<unsigned char>(character);
            if (std::isalnum(c) == 0 && character != '_' && character != '-')
            {
                character = '_';
            }
        }

        return value;
    }

    DirectX::XMFLOAT3 NormalizeFlat(const DirectX::XMFLOAT3& value)
    {
        const float length = std::sqrt(value.x * value.x + value.z * value.z);
        if (length <= 0.0001f)
        {
            return {};
        }

        return { value.x / length, 0.0f, value.z / length };
    }

    DirectX::XMFLOAT3 QuaternionToEuler(const DirectX::XMFLOAT4& q)
    {
        const float sinrCosp = 2.0f * (q.w * q.x + q.y * q.z);
        const float cosrCosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
        const float roll = std::atan2(sinrCosp, cosrCosp);

        const float sinp = 2.0f * (q.w * q.y - q.z * q.x);
        const float pitch = std::abs(sinp) >= 1.0f ? std::copysign(Pi * 0.5f, sinp) : std::asin(sinp);

        const float sinyCosp = 2.0f * (q.w * q.z + q.x * q.y);
        const float cosyCosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
        const float yaw = std::atan2(sinyCosp, cosyCosp);
        return { pitch, yaw, roll };
    }

    Disparity::Transform CombineTransforms(const Disparity::Transform& parent, const Disparity::Transform& child)
    {
        Disparity::Transform result = child;
        result.Position = {
            parent.Position.x + child.Position.x * parent.Scale.x,
            parent.Position.y + child.Position.y * parent.Scale.y,
            parent.Position.z + child.Position.z * parent.Scale.z
        };
        result.Rotation = Add(parent.Rotation, child.Rotation);
        result.Scale = {
            parent.Scale.x * child.Scale.x,
            parent.Scale.y * child.Scale.y,
            parent.Scale.z * child.Scale.z
        };
        return result;
    }

    Disparity::Material LoadMaterial(
        Disparity::Renderer& renderer,
        const std::filesystem::path& path,
        const Disparity::Material& fallback)
    {
        Disparity::MaterialAsset asset;
        if (!Disparity::MaterialAssetIO::Load(path, asset))
        {
            return fallback;
        }

        if (!asset.BaseColorTexturePath.empty())
        {
            asset.MaterialData.BaseColorTexture = renderer.CreateTextureFromFile(asset.BaseColorTexturePath);
        }

        return asset.MaterialData;
    }

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

    std::string Trim(std::string value)
    {
        const auto isNotSpace = [](unsigned char character) {
            return std::isspace(character) == 0;
        };

        value.erase(value.begin(), std::find_if(value.begin(), value.end(), isNotSpace));
        value.erase(std::find_if(value.rbegin(), value.rend(), isNotSpace).base(), value.end());
        return value;
    }

    bool HasArgument(const std::wstring& arguments, const std::wstring& name)
    {
        return arguments.find(name) != std::wstring::npos;
    }

    uint32_t ParseUnsignedArgument(const std::wstring& arguments, const std::wstring& name, uint32_t fallback)
    {
        const size_t begin = arguments.find(name);
        if (begin == std::wstring::npos)
        {
            return fallback;
        }

        const size_t valueBegin = begin + name.size();
        const size_t valueEnd = arguments.find(L' ', valueBegin);
        const std::wstring value = arguments.substr(valueBegin, valueEnd == std::wstring::npos ? std::wstring::npos : valueEnd - valueBegin);
        try
        {
            return static_cast<uint32_t>(std::stoul(value));
        }
        catch (...)
        {
            return fallback;
        }
    }

    std::filesystem::path ParsePathArgument(const std::wstring& arguments, const std::wstring& name, const std::filesystem::path& fallback)
    {
        const size_t begin = arguments.find(name);
        if (begin == std::wstring::npos)
        {
            return fallback;
        }

        const size_t valueBegin = begin + name.size();
        const size_t valueEnd = arguments.find(L' ', valueBegin);
        const std::wstring value = arguments.substr(valueBegin, valueEnd == std::wstring::npos ? std::wstring::npos : valueEnd - valueBegin);
        return value.empty() ? fallback : std::filesystem::path(value);
    }

    double ParseDoubleArgument(const std::wstring& arguments, const std::wstring& name, double fallback)
    {
        const size_t begin = arguments.find(name);
        if (begin == std::wstring::npos)
        {
            return fallback;
        }

        const size_t valueBegin = begin + name.size();
        const size_t valueEnd = arguments.find(L' ', valueBegin);
        const std::wstring value = arguments.substr(valueBegin, valueEnd == std::wstring::npos ? std::wstring::npos : valueEnd - valueBegin);
        try
        {
            return std::stod(value);
        }
        catch (...)
        {
            return fallback;
        }
    }

    class DisparityGameLayer final : public Disparity::Layer
    {
    public:
        explicit DisparityGameLayer(RuntimeVerificationConfig runtimeVerification = {})
            : m_runtimeVerification(std::move(runtimeVerification))
        {
        }

        bool OnAttach(Disparity::Application& application) override
        {
            m_application = &application;
            m_renderer = &application.GetRenderer();

            m_cubeMesh = m_renderer->CreateMesh(Disparity::PrimitiveFactory::CreateCube());
            m_planeMesh = m_renderer->CreateMesh(Disparity::PrimitiveFactory::CreatePlane(48.0f));
            m_terrainMesh = m_renderer->CreateMesh(Disparity::PrimitiveFactory::CreateTerrainGrid(64, 56.0f, 0.32f));
            m_gizmoPlaneHandleMesh = m_renderer->CreateMesh(Disparity::PrimitiveFactory::CreatePlane(1.0f));
            m_gizmoRingMesh = m_renderer->CreateMesh(Disparity::PrimitiveFactory::CreateTorus(1.0f, 0.026f, 64, 8));
            m_vfxQuadMesh = m_renderer->CreateMesh(Disparity::PrimitiveFactory::CreatePlane(1.0f));

            if (Disparity::GltfLoader::LoadScene("Assets/Meshes/SampleTriangle.gltf", m_gltfSceneAsset))
            {
                LoadGltfRuntimeAssets();
            }

            if (m_cubeMesh == 0 || m_planeMesh == 0 || m_terrainMesh == 0 || m_gizmoPlaneHandleMesh == 0 || m_gizmoRingMesh == 0 || m_vfxQuadMesh == 0)
            {
                return false;
            }

            m_meshes.emplace("cube", m_cubeMesh);
            m_meshes.emplace("plane", m_planeMesh);
            m_meshes.emplace("terrain", m_terrainMesh);
            RegisterGltfMeshes();

            InitializeMaterials();

            const float aspect = static_cast<float>(application.GetWidth()) / static_cast<float>(application.GetHeight());
            m_camera.SetPerspective(DirectX::XMConvertToRadians(67.0f), aspect, 0.1f, 400.0f);
            m_editorCamera.SetPerspective(DirectX::XMConvertToRadians(67.0f), aspect, 0.1f, 400.0f);
            m_showcaseCamera.SetPerspective(DirectX::XMConvertToRadians(58.0f), aspect, 0.1f, 520.0f);
            m_trailerCamera.SetPerspective(DirectX::XMConvertToRadians(52.0f), aspect, 0.1f, 650.0f);
            LoadTrailerShotAsset();

            if (!m_scene.Load("Assets/Scenes/Prototype.dscene", m_meshes))
            {
                BuildFallbackScene();
            }
            (void)Disparity::ScriptRunner::RunSceneScript("Assets/Scripts/Prototype.dscript", m_scene, m_meshes);
            AppendImportedGltfSceneObjects();

            BuildRuntimeRegistry();
            WatchAssets();

            UpdateCamera();
            UpdateEditorCamera(0.0f, true, true);
            UpdateTrailerCamera(0.0f);
            if (m_runtimeVerification.Enabled)
            {
                m_editorVisible = false;
                m_hotReloadEnabled = false;
                LoadRuntimeVerificationAssets();
                AddRuntimeVerificationNote("Runtime verification mode enabled.");
            }
            return true;
        }

        void OnUpdate(const Disparity::TimeStep& timeStep) override
        {
            const float dt = timeStep.DeltaSeconds();
            const ImGuiIO& io = ImGui::GetIO();
            const bool editorCapturesMouse = m_editorVisible && io.WantCaptureMouse;
            const bool editorCapturesKeyboard = m_editorVisible && io.WantCaptureKeyboard;
            RefreshEditorViewportState();

            if (Disparity::Input::WasKeyPressed(VK_F1))
            {
                m_editorVisible = !m_editorVisible;
            }
            if (Disparity::Input::WasKeyPressed(VK_F2))
            {
                Disparity::AudioSystem::PlayNotification();
                SetStatus("Played generated audio test tone");
            }
            if (Disparity::Input::WasKeyPressed(VK_F3))
            {
                CycleSelection();
            }
            if (Disparity::Input::WasKeyPressed(VK_F5))
            {
                ReloadSceneAndScript();
            }
            if (Disparity::Input::WasKeyPressed(VK_F6))
            {
                SaveRuntimeScene();
            }
            if (Disparity::Input::WasKeyPressed(VK_F7))
            {
                SetShowcaseMode(!m_showcaseMode);
            }
            if (Disparity::Input::WasKeyPressed(VK_F8))
            {
                SetTrailerMode(!m_trailerMode);
            }
            if (Disparity::Input::WasKeyPressed(VK_F9))
            {
                RequestHighResolutionCapture();
            }
            if (Disparity::Input::IsKeyDown(VK_CONTROL) && Disparity::Input::WasKeyPressed('Z'))
            {
                UndoEdit();
            }
            if (Disparity::Input::IsKeyDown(VK_CONTROL) && Disparity::Input::WasKeyPressed('Y'))
            {
                RedoEdit();
            }
            const bool editorShortcutAllowed = m_editorVisible && !io.WantTextInput;
            if (editorShortcutAllowed && Disparity::Input::IsKeyDown(VK_CONTROL) && Disparity::Input::WasKeyPressed('C'))
            {
                CopySelectedObject();
            }
            if (editorShortcutAllowed && Disparity::Input::IsKeyDown(VK_CONTROL) && Disparity::Input::WasKeyPressed('V'))
            {
                PasteSceneObject();
            }
            if (editorShortcutAllowed && Disparity::Input::IsKeyDown(VK_CONTROL) && Disparity::Input::WasKeyPressed('D'))
            {
                DuplicateSelectedObject();
            }
            if (editorShortcutAllowed && Disparity::Input::WasKeyPressed(VK_DELETE))
            {
                DeleteSelectedObject();
            }
            if (m_gizmoDragging)
            {
                if (Disparity::Input::IsMouseButtonDown(0))
                {
                    UpdateGizmoDrag();
                }
                else
                {
                    EndGizmoDrag();
                }
            }

            if (m_editorVisible && !m_gizmoDragging && !Disparity::Input::IsMouseCaptured() && !editorCapturesMouse && Disparity::Input::WasMouseButtonPressed(0))
            {
                if (!TryBeginGizmoDrag())
                {
                    PickSceneObjectAtMouse();
                }
            }

            const DirectX::XMFLOAT2 mouseDelta = Disparity::Input::GetMouseDelta();
            if (Disparity::Input::IsMouseCaptured() && !editorCapturesMouse && !m_editorCameraEnabled && !m_showcaseMode && !m_trailerMode)
            {
                m_cameraYaw += mouseDelta.x * 0.0025f;
                m_cameraPitch = std::clamp(m_cameraPitch - mouseDelta.y * 0.0022f, -0.15f, 0.95f);
            }

            PollHotReload(dt);
            UpdatePlayer((editorCapturesKeyboard || m_editorCameraEnabled || m_showcaseMode || m_trailerMode) ? 0.0f : dt);
            AnimateScene(dt);
            UpdateCamera();
            UpdateShowcaseCamera(dt);
            UpdateTrailerCamera(dt);
            UpdateRiftBeat(dt);
            CompleteHighResolutionCaptureIfReady();
            UpdateEditorCamera(dt, editorCapturesMouse, editorCapturesKeyboard);
            UpdateEditorHover(editorCapturesMouse);
            const DirectX::XMFLOAT3 listenerPosition = GetRenderCamera().GetPosition();
            Disparity::AudioSystem::SetListenerOrientation(
                listenerPosition,
                Normalize(Subtract(Add(m_playerPosition, { 0.0f, 1.2f, 0.0f }), listenerPosition)),
                { 0.0f, 1.0f, 0.0f });
            UpdateStatusTimer(dt);
            UpdateRuntimeVerification(dt);
        }

        void OnRender(Disparity::Renderer& renderer) override
        {
            renderer.SetCamera(GetRenderCamera());

            Disparity::DirectionalLight light;
            light.Direction = { -0.35f, -1.0f, 0.25f };
            light.Color = { 1.0f, 0.92f, 0.78f };
            light.Intensity = (m_showcaseMode || m_trailerMode) ? 1.36f : 1.18f;
            light.AmbientIntensity = (m_showcaseMode || m_trailerMode) ? 0.25f : 0.20f;
            renderer.SetLighting(light);
            const float visualTime = ShowcaseVisualTime();
            const float riftPulse = std::max(0.5f + 0.5f * std::sin(visualTime * 2.6f), m_riftBeatPulse);
            const float showcaseBoost = (m_showcaseMode || m_trailerMode) ? (1.8f + m_riftBeatPulse * 0.45f) : 1.0f;
            const DirectX::XMFLOAT3 riftLightA = Add(m_riftPosition, { std::sin(visualTime * 0.9f) * 1.8f, 0.45f, std::cos(visualTime * 0.9f) * 1.2f });
            const DirectX::XMFLOAT3 riftLightB = Add(m_riftPosition, { std::sin(visualTime * 1.17f + Pi) * 2.0f, -0.15f, std::cos(visualTime * 1.17f + Pi) * 1.7f });
            const std::array<Disparity::PointLight, 8> pointLights = {
                Disparity::PointLight{ { -4.0f, 3.2f, 3.0f }, 8.0f, { 0.28f, 0.52f, 1.0f }, 0.82f },
                Disparity::PointLight{ { 4.5f, 2.5f, -2.0f }, 7.0f, { 1.0f, 0.46f, 0.24f }, 0.62f },
                Disparity::PointLight{ { 0.0f, 2.0f, 7.0f }, 5.0f, { 0.9f, 0.82f, 0.38f }, 0.55f },
                Disparity::PointLight{ Add(m_playerPosition, { 0.0f, 1.75f, 0.0f }), 4.5f, { 0.35f, 0.72f, 1.0f }, 0.45f },
                Disparity::PointLight{ m_riftPosition, 13.5f, { 0.20f, 0.78f, 1.0f }, (1.6f + riftPulse * 0.8f) * showcaseBoost },
                Disparity::PointLight{ riftLightA, 9.5f, { 1.0f, 0.18f, 0.82f }, (1.1f + riftPulse * 0.7f) * showcaseBoost },
                Disparity::PointLight{ riftLightB, 8.0f, { 0.56f, 0.28f, 1.0f }, (0.85f + riftPulse * 0.55f) * showcaseBoost },
                Disparity::PointLight{ Add(m_riftPosition, { 0.0f, -1.55f, 0.0f }), 10.0f, { 0.18f, 0.9f, 0.72f }, 0.75f * showcaseBoost }
            };
            renderer.SetPointLights(pointLights.data(), static_cast<uint32_t>(pointLights.size()));

            const float shadowRadius = renderer.GetSettings().CascadedShadows ? 34.0f : 20.0f;
            renderer.BeginShadowPass(light, m_playerPosition, shadowRadius);
            DrawWorld(renderer, false);
            DrawPlayer(renderer);
            renderer.EndShadowPass();

            renderer.SetCamera(GetRenderCamera());
            renderer.SetLighting(light);
            DrawWorld(renderer, true);
            DrawPlayer(renderer);
            DrawSelectionOutline(renderer);
            DrawSelectionGizmoHandles(renderer);
        }

        void OnGui() override
        {
            if (!m_editorVisible)
            {
                return;
            }

            DrawDockspace();
            DrawMainMenu();
            DrawViewportPanel();
            DrawHierarchyPanel();
            DrawInspectorPanel();
            DrawAssetsPanel();
            DrawRendererPanel();
            DrawShotDirectorPanel();
            DrawAudioPanel();
            DrawProfilerPanel();
        }

        void DrawWorld(Disparity::Renderer& renderer, bool includePresentationVfx)
        {
            const auto& objects = m_scene.GetObjects();
            for (size_t index = 0; index < objects.size(); ++index)
            {
                const Disparity::NamedSceneObject& object = objects[index];
                renderer.DrawMeshWithId(
                    object.Object.Mesh,
                    object.Object.TransformData,
                    object.Object.MaterialData,
                    SceneObjectPickId(index));
            }

            DrawShowcaseRift(renderer);
            if (includePresentationVfx)
            {
                DrawRiftVfx(renderer);
            }
        }

        void OnResize(unsigned int width, unsigned int height) override
        {
            if (height == 0)
            {
                return;
            }

            const float aspect = static_cast<float>(width) / static_cast<float>(height);
            m_camera.SetPerspective(DirectX::XMConvertToRadians(67.0f), aspect, 0.1f, 400.0f);
            m_editorCamera.SetPerspective(DirectX::XMConvertToRadians(67.0f), aspect, 0.1f, 400.0f);
            m_showcaseCamera.SetPerspective(DirectX::XMConvertToRadians(58.0f), aspect, 0.1f, 520.0f);
            m_trailerCamera.SetPerspective(DirectX::XMConvertToRadians(52.0f), aspect, 0.1f, 650.0f);
        }

    private:
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
        };

        struct RiftVfxSystemStats
        {
            uint32_t FogCards = 0;
            uint32_t Particles = 0;
            uint32_t Ribbons = 0;
            uint32_t LightningArcs = 0;
            uint32_t SoftParticleCandidates = 0;
            uint32_t SortedBatches = 0;
        };

        struct PpmImage
        {
            uint32_t Width = 0;
            uint32_t Height = 0;
            std::vector<unsigned char> Pixels;
        };

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
            uint32_t MinAsyncIoTests = 1;
            uint32_t MinMaterialTextureSlotTests = 1;
            uint32_t MinPrefabVariantTests = 1;
            uint32_t MinShotDirectorTests = 1;
            uint32_t MinAudioAnalysisTests = 1;
            uint32_t MinVfxSystemTests = 1;
            uint32_t MinAnimationSkinningTests = 1;
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
            uint32_t AudioAnalysisTests = 0;
            uint32_t VfxSystemTests = 0;
            uint32_t AnimationSkinningTests = 0;
        };

        void InitializeMaterials()
        {
            m_playerBodyMaterial.Albedo = { 0.18f, 0.42f, 0.85f };
            m_playerBodyMaterial.Roughness = 0.38f;
            m_playerBodyMaterial.Metallic = 0.08f;
            m_playerBodyMaterial = LoadMaterial(*m_renderer, "Assets/Materials/PlayerBody.dmat", m_playerBodyMaterial);

            m_playerHeadMaterial.Albedo = { 0.85f, 0.88f, 0.95f };
            m_playerHeadMaterial.Roughness = 0.30f;
            m_playerHeadMaterial.Metallic = 0.02f;
            m_playerHeadMaterial = LoadMaterial(*m_renderer, "Assets/Materials/PlayerHead.dmat", m_playerHeadMaterial);

            m_shadowMaterial.Albedo = { 0.0f, 0.0f, 0.0f };
            m_shadowMaterial.Roughness = 1.0f;
            m_shadowMaterial.Alpha = 0.28f;

            m_gizmoXMaterial.Albedo = { 1.0f, 0.15f, 0.12f };
            m_gizmoXMaterial.Roughness = 0.25f;
            m_gizmoYMaterial.Albedo = { 0.18f, 0.92f, 0.28f };
            m_gizmoYMaterial.Roughness = 0.25f;
            m_gizmoZMaterial.Albedo = { 0.16f, 0.42f, 1.0f };
            m_gizmoZMaterial.Roughness = 0.25f;
            m_gizmoCenterMaterial.Albedo = { 1.0f, 0.92f, 0.25f };
            m_gizmoCenterMaterial.Roughness = 0.2f;
            m_gizmoPlaneXYMaterial.Albedo = { 1.0f, 0.82f, 0.16f };
            m_gizmoPlaneXYMaterial.Roughness = 0.35f;
            m_gizmoPlaneXYMaterial.Alpha = 0.36f;
            m_gizmoPlaneXZMaterial.Albedo = { 0.92f, 0.2f, 0.82f };
            m_gizmoPlaneXZMaterial.Roughness = 0.35f;
            m_gizmoPlaneXZMaterial.Alpha = 0.36f;
            m_gizmoPlaneYZMaterial.Albedo = { 0.12f, 0.82f, 1.0f };
            m_gizmoPlaneYZMaterial.Roughness = 0.35f;
            m_gizmoPlaneYZMaterial.Alpha = 0.36f;

            m_riftCoreMaterial.Albedo = { 3.4f, 0.56f, 4.6f };
            m_riftCoreMaterial.Roughness = 0.08f;
            m_riftCoreMaterial.Metallic = 0.18f;
            m_riftCoreMaterial.Emissive = { 1.0f, 0.18f, 1.45f };
            m_riftCoreMaterial.EmissiveIntensity = 2.8f;

            m_riftCyanMaterial.Albedo = { 0.16f, 2.65f, 4.4f };
            m_riftCyanMaterial.Roughness = 0.16f;
            m_riftCyanMaterial.Metallic = 0.06f;
            m_riftCyanMaterial.Emissive = { 0.06f, 0.75f, 1.0f };
            m_riftCyanMaterial.EmissiveIntensity = 1.7f;

            m_riftMagentaMaterial.Albedo = { 4.3f, 0.18f, 2.95f };
            m_riftMagentaMaterial.Roughness = 0.18f;
            m_riftMagentaMaterial.Metallic = 0.08f;
            m_riftMagentaMaterial.Emissive = { 1.0f, 0.08f, 0.68f };
            m_riftMagentaMaterial.EmissiveIntensity = 1.8f;

            m_riftVioletMaterial.Albedo = { 1.05f, 0.38f, 4.8f };
            m_riftVioletMaterial.Roughness = 0.14f;
            m_riftVioletMaterial.Metallic = 0.12f;
            m_riftVioletMaterial.Emissive = { 0.48f, 0.14f, 1.0f };
            m_riftVioletMaterial.EmissiveIntensity = 1.55f;

            m_riftObsidianMaterial.Albedo = { 0.035f, 0.04f, 0.075f };
            m_riftObsidianMaterial.Roughness = 0.28f;
            m_riftObsidianMaterial.Metallic = 0.72f;

            m_vfxParticleMaterial.Albedo = { 0.18f, 1.45f, 2.6f };
            m_vfxParticleMaterial.Alpha = 0.42f;
            m_vfxParticleMaterial.Roughness = 0.08f;
            m_vfxParticleMaterial.Emissive = { 0.10f, 0.85f, 1.0f };
            m_vfxParticleMaterial.EmissiveIntensity = 2.25f;

            m_vfxHotParticleMaterial.Albedo = { 4.2f, 0.32f, 2.8f };
            m_vfxHotParticleMaterial.Alpha = 0.52f;
            m_vfxHotParticleMaterial.Roughness = 0.06f;
            m_vfxHotParticleMaterial.Emissive = { 1.0f, 0.12f, 0.72f };
            m_vfxHotParticleMaterial.EmissiveIntensity = 2.7f;

            m_vfxRibbonMaterial.Albedo = { 0.28f, 1.85f, 3.0f };
            m_vfxRibbonMaterial.Alpha = 0.34f;
            m_vfxRibbonMaterial.Roughness = 0.10f;
            m_vfxRibbonMaterial.Emissive = { 0.16f, 0.92f, 1.0f };
            m_vfxRibbonMaterial.EmissiveIntensity = 2.2f;

            m_vfxLightningMaterial.Albedo = { 3.6f, 4.5f, 5.2f };
            m_vfxLightningMaterial.Alpha = 0.72f;
            m_vfxLightningMaterial.Roughness = 0.04f;
            m_vfxLightningMaterial.Emissive = { 0.72f, 0.96f, 1.0f };
            m_vfxLightningMaterial.EmissiveIntensity = 4.0f;

            m_vfxFogMaterial.Albedo = { 0.18f, 0.55f, 1.0f };
            m_vfxFogMaterial.Alpha = 0.22f;
            m_vfxFogMaterial.Roughness = 0.5f;
            m_vfxFogMaterial.Emissive = { 0.05f, 0.22f, 0.46f };
            m_vfxFogMaterial.EmissiveIntensity = 0.9f;
        }

        void ReloadGltfAssets()
        {
            if (Disparity::GltfLoader::LoadScene("Assets/Meshes/SampleTriangle.gltf", m_gltfSceneAsset))
            {
                LoadGltfRuntimeAssets();
                RegisterGltfMeshes();
            }
        }

        void LoadGltfRuntimeAssets()
        {
            m_gltfMeshes.clear();
            m_gltfMaterials.clear();
            m_gltfMesh = 0;

            for (const Disparity::MeshData& meshData : m_gltfSceneAsset.Meshes)
            {
                const Disparity::MeshHandle handle = m_renderer->CreateMesh(meshData);
                m_gltfMeshes.push_back(handle);
                if (m_gltfMesh == 0)
                {
                    m_gltfMesh = handle;
                }
            }

            for (const Disparity::GltfMaterialInfo& materialInfo : m_gltfSceneAsset.Materials)
            {
                Disparity::Material material = materialInfo.MaterialData;
                if (!materialInfo.BaseColorTexturePath.empty())
                {
                    material.BaseColorTexture = m_renderer->CreateTextureFromFile(materialInfo.BaseColorTexturePath);
                }
                m_gltfMaterials.push_back(material);
            }
        }

        void RegisterGltfMeshes()
        {
            for (size_t index = 0; index < m_gltfMeshes.size(); ++index)
            {
                m_meshes["gltf_" + std::to_string(index)] = m_gltfMeshes[index];
            }

            if (!m_gltfMeshes.empty())
            {
                m_meshes["gltf_sample"] = m_gltfMeshes.front();
            }
        }

        void AppendImportedGltfSceneObjects()
        {
            m_gltfNodeSceneIndices.assign(m_gltfSceneAsset.Nodes.size(), InvalidIndex);
            if (m_gltfMeshes.empty())
            {
                return;
            }

            if (m_gltfSceneAsset.Nodes.empty())
            {
                Disparity::NamedSceneObject object;
                object.Name = "glTF_Primitive_0";
                object.MeshName = "gltf_0";
                object.Object.Mesh = m_gltfMeshes.front();
                object.Object.TransformData.Position = { 0.0f, 1.45f, -4.0f };
                object.Object.TransformData.Scale = { 1.15f, 1.15f, 1.15f };
                object.Object.MaterialData = ResolveGltfMaterial(0);
                m_scene.Add(std::move(object));
                return;
            }

            for (size_t nodeIndex = 0; nodeIndex < m_gltfSceneAsset.Nodes.size(); ++nodeIndex)
            {
                const Disparity::GltfNodeInfo& node = m_gltfSceneAsset.Nodes[nodeIndex];
                if (node.Mesh < 0 || node.Mesh >= static_cast<int>(m_gltfMeshes.size()))
                {
                    continue;
                }

                Disparity::Transform parentTransform;
                const int parentIndex = FindGltfParent(static_cast<int>(nodeIndex));
                if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < m_gltfSceneAsset.Nodes.size())
                {
                    parentTransform = NodeToTransform(m_gltfSceneAsset.Nodes[static_cast<size_t>(parentIndex)]);
                }

                Disparity::NamedSceneObject object;
                object.Name = "glTF_" + std::to_string(nodeIndex) + "_" + (node.Name.empty() ? "Node" : node.Name);
                object.MeshName = "gltf_" + std::to_string(node.Mesh);
                object.Object.Mesh = m_gltfMeshes[static_cast<size_t>(node.Mesh)];
                object.Object.TransformData = CombineTransforms(parentTransform, NodeToTransform(node));
                object.Object.TransformData.Position.z -= 7.0f;
                object.Object.TransformData.Position.y += 1.35f;
                object.Object.TransformData.Scale = Scale(object.Object.TransformData.Scale, 1.25f);
                object.Object.MaterialData = ResolveGltfMaterial(static_cast<size_t>(node.Mesh));

                m_gltfNodeSceneIndices[nodeIndex] = m_scene.Count();
                m_scene.Add(std::move(object));
            }
        }

        Disparity::Transform NodeToTransform(const Disparity::GltfNodeInfo& node) const
        {
            Disparity::Transform transform;
            transform.Position = node.Translation;
            transform.Rotation = QuaternionToEuler(node.Rotation);
            transform.Scale = node.Scale;
            return transform;
        }

        int FindGltfParent(int childNode) const
        {
            for (size_t nodeIndex = 0; nodeIndex < m_gltfSceneAsset.Nodes.size(); ++nodeIndex)
            {
                const Disparity::GltfNodeInfo& node = m_gltfSceneAsset.Nodes[nodeIndex];
                if (std::find(node.Children.begin(), node.Children.end(), childNode) != node.Children.end())
                {
                    return static_cast<int>(nodeIndex);
                }
            }
            return -1;
        }

        Disparity::Material ResolveGltfMaterial(size_t primitiveIndex) const
        {
            if (primitiveIndex < m_gltfSceneAsset.MeshPrimitives.size())
            {
                const int materialIndex = m_gltfSceneAsset.MeshPrimitives[primitiveIndex].Material;
                if (materialIndex >= 0 && materialIndex < static_cast<int>(m_gltfMaterials.size()))
                {
                    return m_gltfMaterials[static_cast<size_t>(materialIndex)];
                }
            }

            Disparity::Material material;
            material.Albedo = { 0.72f, 0.92f, 1.0f };
            material.Roughness = 0.28f;
            material.Metallic = 0.0f;
            material.DoubleSided = true;
            return material;
        }

        void BuildFallbackScene()
        {
            m_scene.Clear();

            Disparity::NamedSceneObject terrain;
            terrain.Name = "Terrain";
            terrain.MeshName = "terrain";
            terrain.Object.Mesh = m_terrainMesh;
            terrain.Object.MaterialData.Albedo = { 0.18f, 0.25f, 0.19f };
            terrain.Object.MaterialData.Roughness = 0.90f;
            m_scene.Add(terrain);

            for (int z = -2; z <= 2; ++z)
            {
                for (int x = -2; x <= 2; ++x)
                {
                    if ((x == 0 && z == 0) || ((x + z) % 2 == 0))
                    {
                        continue;
                    }

                    Disparity::NamedSceneObject block;
                    block.Name = "Block_" + std::to_string(x) + "_" + std::to_string(z);
                    block.MeshName = "cube";
                    block.Object.Mesh = m_cubeMesh;
                    block.Object.TransformData.Position = { static_cast<float>(x) * 4.0f, 0.65f, static_cast<float>(z) * 4.0f };
                    block.Object.TransformData.Scale = { 1.0f, 1.1f + static_cast<float>((x * x + z * z) % 3) * 0.35f, 1.0f };
                    block.Object.MaterialData.Albedo = {
                        0.28f + static_cast<float>(x + 2) * 0.06f,
                        0.34f,
                        0.52f + static_cast<float>(z + 2) * 0.04f
                    };
                    block.Object.MaterialData.Roughness = 0.46f;
                    block.Object.MaterialData.Metallic = 0.08f;
                    m_scene.Add(block);
                }
            }
        }

        void UpdatePlayer(float dt)
        {
            DirectX::XMFLOAT3 moveInput = {};
            if (Disparity::Input::IsKeyDown('W'))
            {
                moveInput.z += 1.0f;
            }
            if (Disparity::Input::IsKeyDown('S'))
            {
                moveInput.z -= 1.0f;
            }
            if (Disparity::Input::IsKeyDown('D'))
            {
                moveInput.x += 1.0f;
            }
            if (Disparity::Input::IsKeyDown('A'))
            {
                moveInput.x -= 1.0f;
            }

            MovePlayerWithInput(moveInput, dt);
            m_playerBobOffset = m_playerBob.SampleOffset(dt);
            SyncPlayerTransformToRegistry();
        }

        void MovePlayerWithInput(const DirectX::XMFLOAT3& moveInput, float dt)
        {
            const DirectX::XMFLOAT3 normalizedInput = NormalizeFlat(moveInput);
            if (dt > 0.0f && (std::abs(normalizedInput.x) > 0.0f || std::abs(normalizedInput.z) > 0.0f))
            {
                const DirectX::XMFLOAT3 cameraForward = {
                    std::sin(m_cameraYaw),
                    0.0f,
                    std::cos(m_cameraYaw)
                };
                const DirectX::XMFLOAT3 cameraRight = {
                    std::cos(m_cameraYaw),
                    0.0f,
                    -std::sin(m_cameraYaw)
                };

                DirectX::XMFLOAT3 movement = Add(Scale(cameraForward, normalizedInput.z), Scale(cameraRight, normalizedInput.x));
                movement = NormalizeFlat(movement);

                constexpr float movementSpeed = 5.5f;
                m_playerPosition = Add(m_playerPosition, Scale(movement, movementSpeed * dt));
                m_playerYaw = std::atan2(movement.x, movement.z);
            }
        }

        void SyncPlayerTransformToRegistry()
        {
            if (Disparity::TransformComponent* transform = m_registry.GetTransform(m_playerEntity))
            {
                transform->Value.Position = m_playerPosition;
                transform->Value.Rotation = { 0.0f, m_playerYaw, 0.0f };
            }
        }

        void AnimateScene(float dt)
        {
            m_sceneAnimationTime += dt;
            ApplyGltfAnimation();
            for (size_t index = 0; index < m_scene.GetObjects().size(); ++index)
            {
                Disparity::NamedSceneObject& object = m_scene.GetObjects()[index];
                if (object.Name.find("Imported") != std::string::npos || object.MeshName == "gltf_sample")
                {
                    object.Object.TransformData.Rotation.y = m_sceneAnimationTime * 0.75f;
                    object.Object.TransformData.Position.y = 1.45f + std::sin(m_sceneAnimationTime * 2.0f) * 0.18f;
                    SyncSceneObjectToRegistry(index);
                }
            }
        }

        void ApplyGltfAnimation()
        {
            if (!m_gltfAnimationPlayback || m_gltfSceneAsset.Animations.empty() || m_gltfNodeSceneIndices.empty())
            {
                return;
            }

            const Disparity::GltfAnimationClipInfo& clip = m_gltfSceneAsset.Animations.front();
            for (const Disparity::GltfAnimationChannelInfo& channel : clip.Channels)
            {
                if (channel.TargetNode < 0 ||
                    channel.Sampler < 0 ||
                    channel.Sampler >= static_cast<int>(clip.Samplers.size()) ||
                    static_cast<size_t>(channel.TargetNode) >= m_gltfNodeSceneIndices.size())
                {
                    continue;
                }

                const size_t sceneIndex = m_gltfNodeSceneIndices[static_cast<size_t>(channel.TargetNode)];
                if (sceneIndex == InvalidIndex || sceneIndex >= m_scene.GetObjects().size())
                {
                    continue;
                }

                const Disparity::GltfAnimationSamplerInfo& sampler = clip.Samplers[static_cast<size_t>(channel.Sampler)];
                Disparity::Transform& transform = m_scene.GetObjects()[sceneIndex].Object.TransformData;

                if (channel.TargetPath == "translation" && !sampler.OutputTranslations.empty())
                {
                    transform.Position = SampleVec3(sampler.InputTimes, sampler.OutputTranslations, m_sceneAnimationTime);
                    transform.Position.z -= 7.0f;
                    transform.Position.y += 1.35f;
                }
                else if (channel.TargetPath == "scale" && !sampler.OutputScales.empty())
                {
                    transform.Scale = Scale(SampleVec3(sampler.InputTimes, sampler.OutputScales, m_sceneAnimationTime), 1.25f);
                }
                else if (channel.TargetPath == "rotation" && !sampler.OutputRotations.empty())
                {
                    transform.Rotation = QuaternionToEuler(SampleQuat(sampler.InputTimes, sampler.OutputRotations, m_sceneAnimationTime));
                }

                SyncSceneObjectToRegistry(sceneIndex);
            }
        }

        DirectX::XMFLOAT3 SampleVec3(
            const std::vector<float>& times,
            const std::vector<DirectX::XMFLOAT3>& values,
            float time) const
        {
            if (values.empty())
            {
                return {};
            }
            if (times.empty() || values.size() == 1)
            {
                return values.front();
            }

            const float duration = std::max(times.back(), 0.001f);
            const float localTime = std::fmod(time, duration);
            for (size_t index = 0; index + 1 < times.size() && index + 1 < values.size(); ++index)
            {
                if (localTime <= times[index + 1])
                {
                    const float span = std::max(times[index + 1] - times[index], 0.001f);
                    const float alpha = std::clamp((localTime - times[index]) / span, 0.0f, 1.0f);
                    return {
                        values[index].x + (values[index + 1].x - values[index].x) * alpha,
                        values[index].y + (values[index + 1].y - values[index].y) * alpha,
                        values[index].z + (values[index + 1].z - values[index].z) * alpha
                    };
                }
            }

            return values.back();
        }

        DirectX::XMFLOAT4 SampleQuat(
            const std::vector<float>& times,
            const std::vector<DirectX::XMFLOAT4>& values,
            float time) const
        {
            if (values.empty())
            {
                return { 0.0f, 0.0f, 0.0f, 1.0f };
            }
            if (times.empty() || values.size() == 1)
            {
                return values.front();
            }

            const float duration = std::max(times.back(), 0.001f);
            const float localTime = std::fmod(time, duration);
            for (size_t index = 0; index + 1 < times.size() && index + 1 < values.size(); ++index)
            {
                if (localTime <= times[index + 1])
                {
                    const float span = std::max(times[index + 1] - times[index], 0.001f);
                    const float alpha = std::clamp((localTime - times[index]) / span, 0.0f, 1.0f);
                    const DirectX::XMVECTOR a = DirectX::XMLoadFloat4(&values[index]);
                    const DirectX::XMVECTOR b = DirectX::XMLoadFloat4(&values[index + 1]);
                    DirectX::XMFLOAT4 result = {};
                    DirectX::XMStoreFloat4(&result, DirectX::XMQuaternionSlerp(a, b, alpha));
                    return result;
                }
            }

            return values.back();
        }

        void UpdateCamera()
        {
            const float cosPitch = std::cos(m_cameraPitch);
            const DirectX::XMFLOAT3 lookDirection = {
                std::sin(m_cameraYaw) * cosPitch,
                std::sin(m_cameraPitch),
                std::cos(m_cameraYaw) * cosPitch
            };

            const DirectX::XMFLOAT3 target = Add(m_playerPosition, { 0.0f, 1.15f, 0.0f });
            DirectX::XMFLOAT3 position = Add(target, Scale(lookDirection, -m_cameraDistance));
            position.y = std::max(position.y, 1.0f);

            m_camera.LookAt(position, target, { 0.0f, 1.0f, 0.0f });
        }

        void UpdateEditorCamera(float dt, bool editorCapturesMouse, bool editorCapturesKeyboard)
        {
            if (!m_editorCameraEnabled)
            {
                m_editorCameraTarget = Add(m_playerPosition, { 0.0f, 1.1f, 0.0f });
                m_editorLastMousePosition = Disparity::Input::GetMousePosition();
            }
            else
            {
                const DirectX::XMFLOAT2 mouseDelta = Disparity::Input::GetMouseDelta();
                const DirectX::XMFLOAT2 mousePosition = Disparity::Input::GetMousePosition();
                if (!editorCapturesMouse && Disparity::Input::IsMouseButtonDown(1))
                {
                    const DirectX::XMFLOAT2 cursorDelta = {
                        mousePosition.x - m_editorLastMousePosition.x,
                        mousePosition.y - m_editorLastMousePosition.y
                    };
                    const float deltaX = std::abs(cursorDelta.x) > 0.0f ? cursorDelta.x : mouseDelta.x;
                    const float deltaY = std::abs(cursorDelta.y) > 0.0f ? cursorDelta.y : mouseDelta.y;
                    m_editorCameraYaw += deltaX * 0.003f;
                    m_editorCameraPitch = std::clamp(m_editorCameraPitch - deltaY * 0.0025f, -0.95f, 1.15f);
                }
                const bool editorFlyActive = !editorCapturesMouse && Disparity::Input::IsMouseButtonDown(1);
                m_editorLastMousePosition = mousePosition;

                if ((!editorCapturesKeyboard || editorFlyActive) && dt > 0.0f)
                {
                    DirectX::XMFLOAT3 moveInput = {};
                    if (Disparity::Input::IsKeyDown(VK_UP)) { moveInput.z += 1.0f; }
                    if (Disparity::Input::IsKeyDown(VK_DOWN)) { moveInput.z -= 1.0f; }
                    if (Disparity::Input::IsKeyDown(VK_RIGHT)) { moveInput.x += 1.0f; }
                    if (Disparity::Input::IsKeyDown(VK_LEFT)) { moveInput.x -= 1.0f; }
                    if (Disparity::Input::IsKeyDown(VK_PRIOR)) { moveInput.y += 1.0f; }
                    if (Disparity::Input::IsKeyDown(VK_NEXT)) { moveInput.y -= 1.0f; }
                    if (editorFlyActive)
                    {
                        if (Disparity::Input::IsKeyDown('W')) { moveInput.z += 1.0f; }
                        if (Disparity::Input::IsKeyDown('S')) { moveInput.z -= 1.0f; }
                        if (Disparity::Input::IsKeyDown('D')) { moveInput.x += 1.0f; }
                        if (Disparity::Input::IsKeyDown('A')) { moveInput.x -= 1.0f; }
                        if (Disparity::Input::IsKeyDown('E')) { moveInput.y += 1.0f; }
                        if (Disparity::Input::IsKeyDown('Q')) { moveInput.y -= 1.0f; }
                    }

                    const DirectX::XMFLOAT3 normalizedInput = NormalizeFlat({ moveInput.x, 0.0f, moveInput.z });
                    const DirectX::XMFLOAT3 forward = { std::sin(m_editorCameraYaw), 0.0f, std::cos(m_editorCameraYaw) };
                    const DirectX::XMFLOAT3 right = { std::cos(m_editorCameraYaw), 0.0f, -std::sin(m_editorCameraYaw) };
                    DirectX::XMFLOAT3 movement = Add(Scale(forward, normalizedInput.z), Scale(right, normalizedInput.x));
                    movement.y += moveInput.y;
                    if (Length(movement) > 0.0001f)
                    {
                        movement = Scale(movement, 1.0f / Length(movement));
                        const float speed = Disparity::Input::IsKeyDown(VK_SHIFT) ? 15.0f : 7.5f;
                        m_editorCameraTarget = Add(m_editorCameraTarget, Scale(movement, speed * dt));
                    }
                }
            }

            const float cosPitch = std::cos(m_editorCameraPitch);
            const DirectX::XMFLOAT3 lookDirection = {
                std::sin(m_editorCameraYaw) * cosPitch,
                std::sin(m_editorCameraPitch),
                std::cos(m_editorCameraYaw) * cosPitch
            };
            DirectX::XMFLOAT3 position = Add(m_editorCameraTarget, Scale(lookDirection, -m_editorCameraDistance));
            position.y = std::max(position.y, 0.75f);
            m_editorCamera.LookAt(position, m_editorCameraTarget, { 0.0f, 1.0f, 0.0f });
        }

        void UpdateShowcaseCamera(float dt)
        {
            if (!m_showcaseMode)
            {
                return;
            }

            m_showcaseTime += dt;
            const float orbit = m_showcaseTime * 0.24f;
            const float radius = 10.8f + std::sin(m_showcaseTime * 0.31f) * 1.4f;
            const float height = 3.85f + std::sin(m_showcaseTime * 0.53f) * 0.65f;
            const DirectX::XMFLOAT3 target = Add(m_riftPosition, { 0.0f, 0.12f + std::sin(m_showcaseTime * 0.72f) * 0.22f, 0.0f });
            const DirectX::XMFLOAT3 position = {
                target.x + std::sin(orbit) * radius,
                height,
                target.z + std::cos(orbit) * radius
            };
            m_showcaseCamera.LookAt(position, target, { 0.0f, 1.0f, 0.0f });
        }

        float ApplyShotEasing(float value, const TrailerShotKey& a, const TrailerShotKey& b) const
        {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            const float shaped = SmoothStep01(clamped);
            const float blend = std::clamp((std::max(0.0f, a.EaseOut) + std::max(0.0f, b.EaseIn)) * 0.5f, 0.0f, 1.0f);
            return clamped + (shaped - clamped) * blend;
        }

        void UpdateTrailerCamera(float dt)
        {
            if (!m_trailerMode && !m_trailerKeys.empty())
            {
                const TrailerShotKey& key = m_trailerKeys.front();
                m_trailerCamera.LookAt(key.Position, key.Target, { 0.0f, 1.0f, 0.0f });
                return;
            }

            if (!m_trailerMode || m_trailerKeys.empty())
            {
                return;
            }

            m_trailerTime += dt;
            const float duration = std::max(0.001f, m_trailerKeys.back().Time);
            float playbackTime = std::fmod(std::max(0.0f, m_trailerTime), duration);
            if (m_runtimeVerification.Enabled && m_runtimeVerificationStartedTrailer)
            {
                const uint32_t elapsedFrames = m_runtimeVerificationFrame > m_runtimeVerificationTrailerStartFrame
                    ? m_runtimeVerificationFrame - m_runtimeVerificationTrailerStartFrame
                    : 0u;
                playbackTime = std::fmod(static_cast<float>(elapsedFrames) * (1.0f / 60.0f), duration);
            }

            TrailerShotKey a = m_trailerKeys.front();
            TrailerShotKey b = m_trailerKeys.back();
            for (size_t index = 1; index < m_trailerKeys.size(); ++index)
            {
                if (playbackTime <= m_trailerKeys[index].Time)
                {
                    a = m_trailerKeys[index - 1];
                    b = m_trailerKeys[index];
                    break;
                }
            }

            const float segmentDuration = std::max(0.001f, b.Time - a.Time);
            const float t = ApplyShotEasing((playbackTime - a.Time) / segmentDuration, a, b);
            m_trailerCamera.LookAt(Lerp(a.Position, b.Position, t), Lerp(a.Target, b.Target, t), { 0.0f, 1.0f, 0.0f });
            m_trailerFocus = a.Focus + (b.Focus - a.Focus) * t;
            m_trailerDofStrength = a.DofStrength + (b.DofStrength - a.DofStrength) * t;
            m_trailerLensDirt = a.LensDirt + (b.LensDirt - a.LensDirt) * t;
            m_trailerLetterbox = a.Letterbox + (b.Letterbox - a.Letterbox) * t;
            m_trailerRendererPulse = a.RendererPulse + (b.RendererPulse - a.RendererPulse) * t;
            m_trailerAudioCue = a.AudioCue + (b.AudioCue - a.AudioCue) * t;
            ApplyPresentationRendererSettings();

            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.TrailerFrames;
            }
        }

        const Disparity::Camera& GetRenderCamera() const
        {
            if (m_trailerMode)
            {
                return m_trailerCamera;
            }

            if (m_showcaseMode)
            {
                return m_showcaseCamera;
            }

            return m_editorCameraEnabled ? m_editorCamera : m_camera;
        }

        float ShowcaseVisualTime() const
        {
            if (m_runtimeVerification.Enabled)
            {
                return static_cast<float>(m_runtimeVerificationFrame) * (1.0f / 60.0f);
            }

            return m_sceneAnimationTime;
        }

        void SetShowcaseMode(bool enabled)
        {
            if (m_showcaseMode == enabled)
            {
                return;
            }

            if (enabled)
            {
                if (m_trailerMode)
                {
                    SetTrailerMode(false);
                }
                m_showcaseSavedEditorVisible = m_editorVisible;
                m_editorVisible = false;
                m_showcaseTime = 0.0f;
                if (m_renderer)
                {
                    m_showcaseSavedRendererSettings = m_renderer->GetSettings();
                    Disparity::RendererSettings settings = *m_showcaseSavedRendererSettings;
                    settings.ToneMapping = true;
                    settings.Bloom = true;
                    settings.SSAO = true;
                    settings.AntiAliasing = true;
                    settings.TemporalAA = true;
                    settings.ClusteredLighting = true;
                    settings.Exposure = 1.28f;
                    settings.BloomStrength = 1.16f;
                    settings.BloomThreshold = 0.28f;
                    settings.SsaoStrength = 0.74f;
                    settings.AntiAliasingStrength = 1.0f;
                    settings.TemporalBlend = 0.18f;
                    settings.ColorSaturation = 1.32f;
                    settings.ColorContrast = 1.16f;
                    settings.DepthOfField = true;
                    settings.DepthOfFieldFocus = 0.986f;
                    settings.DepthOfFieldRange = 0.032f;
                    settings.DepthOfFieldStrength = 0.34f;
                    settings.LensDirt = true;
                    settings.LensDirtStrength = 0.72f;
                    settings.VignetteStrength = 0.18f;
                    settings.CinematicOverlay = true;
                    settings.LetterboxAmount = 0.055f;
                    settings.TitleSafeOpacity = 0.0f;
                    settings.FilmGrainStrength = 0.018f;
                    settings.PostDebugView = 0;
                    m_renderer->SetSettings(settings);
                }
                m_showcaseMode = true;
                UpdateShowcaseCamera(0.0f);
                PlayCinematicCue("SFX", 660.0f, 0.22f, 0.9f);
                SetStatus("Showcase mode enabled");
                return;
            }

            m_showcaseMode = false;
            m_editorVisible = m_showcaseSavedEditorVisible;
            if (m_renderer && m_showcaseSavedRendererSettings.has_value())
            {
                m_renderer->SetSettings(*m_showcaseSavedRendererSettings);
            }
            m_showcaseSavedRendererSettings.reset();
            PlayCinematicCue("UI", 440.0f, 0.16f, 0.65f);
            SetStatus("Showcase mode disabled");
        }

        void SetTrailerMode(bool enabled)
        {
            if (m_trailerMode == enabled)
            {
                return;
            }

            if (enabled)
            {
                if (m_showcaseMode)
                {
                    SetShowcaseMode(false);
                }

                m_trailerSavedEditorVisible = m_editorVisible;
                m_editorVisible = false;
                m_trailerTime = 0.0f;
                m_trailerMode = true;
                if (m_renderer)
                {
                    m_trailerSavedRendererSettings = m_renderer->GetSettings();
                }
                UpdateTrailerCamera(0.0f);
                ApplyPresentationRendererSettings();
                PlayCinematicCue("SFX", 784.0f, 0.24f, 0.95f);
                SetStatus("Trailer/photo mode enabled");
                return;
            }

            m_trailerMode = false;
            m_editorVisible = m_trailerSavedEditorVisible;
            if (m_renderer && m_trailerSavedRendererSettings.has_value())
            {
                m_renderer->SetSettings(*m_trailerSavedRendererSettings);
            }
            m_trailerSavedRendererSettings.reset();
            PlayCinematicCue("UI", 392.0f, 0.16f, 0.65f);
            SetStatus("Trailer/photo mode disabled");
        }

        void ApplyPresentationRendererSettings()
        {
            if (!m_renderer || (!m_showcaseMode && !m_trailerMode))
            {
                return;
            }

            Disparity::RendererSettings settings = m_renderer->GetSettings();
            settings.ToneMapping = true;
            settings.Bloom = true;
            settings.SSAO = true;
            settings.AntiAliasing = true;
            settings.TemporalAA = true;
            settings.ClusteredLighting = true;
            settings.DepthOfField = true;
            settings.LensDirt = true;
            settings.CinematicOverlay = true;
            settings.Exposure = m_trailerMode ? 1.22f : settings.Exposure;
            settings.BloomStrength = std::max(settings.BloomStrength, 1.08f + m_riftBeatPulse * 0.32f);
            settings.BloomThreshold = std::min(settings.BloomThreshold, 0.32f);
            settings.SsaoStrength = std::max(settings.SsaoStrength, 0.72f);
            settings.ColorSaturation = std::max(settings.ColorSaturation, 1.22f);
            settings.ColorContrast = std::max(settings.ColorContrast, 1.12f);
            settings.DepthOfFieldFocus = m_trailerMode ? m_trailerFocus : 0.986f;
            settings.DepthOfFieldRange = m_trailerMode ? 0.022f : 0.032f;
            settings.DepthOfFieldStrength = m_trailerMode ? m_trailerDofStrength : 0.34f;
            settings.LensDirtStrength = (m_trailerMode ? m_trailerLensDirt : 0.72f) + m_riftBeatPulse * 0.22f;
            settings.VignetteStrength = m_trailerMode ? 0.24f : 0.18f;
            settings.LetterboxAmount = m_trailerMode ? m_trailerLetterbox : 0.055f;
            settings.TitleSafeOpacity = m_trailerMode ? 0.46f : 0.0f;
            settings.FilmGrainStrength = m_trailerMode ? 0.026f : 0.018f;
            settings.PresentationPulse = std::max(m_riftBeatPulse, m_trailerMode ? m_trailerRendererPulse : 0.0f);
            settings.PostDebugView = 0;
            m_renderer->SetSettings(settings);
        }

        void UpdateRiftBeat(float)
        {
            const float visualTime = ShowcaseVisualTime();
            const float beatWave = 0.5f + 0.5f * std::sin(visualTime * Pi * 2.0f * 1.12f);
            m_riftBeatPulse = std::pow(std::clamp(beatWave, 0.0f, 1.0f), 7.0f);
            ApplyPresentationRendererSettings();

            const int beatIndex = static_cast<int>(std::floor(visualTime * 1.12f));
            if ((m_showcaseMode || m_trailerMode) && beatIndex != m_lastRiftBeatIndex)
            {
                m_lastRiftBeatIndex = beatIndex;
                const float frequency = 520.0f + static_cast<float>((beatIndex % 5 + 5) % 5) * 58.0f;
                PlayCinematicCue("SFX", frequency, 0.08f, 0.28f);
                if (m_runtimeVerification.Enabled)
                {
                    ++m_runtimeEditorStats.AudioBeatPulses;
                }
            }
        }

        void PlayCinematicCue(const std::string& busName, float frequencyHz, float durationSeconds, float volume)
        {
            if (!m_cinematicAudioCues)
            {
                return;
            }

            Disparity::AudioSystem::PlaySpatialTone(busName, frequencyHz, durationSeconds, volume, m_riftPosition);
        }

        bool ParseShotFloat3(const std::string& text, DirectX::XMFLOAT3& value) const
        {
            std::istringstream stream(text);
            char commaA = 0;
            char commaB = 0;
            DirectX::XMFLOAT3 parsed = {};
            if (!(stream >> parsed.x >> commaA >> parsed.y >> commaB >> parsed.z) || commaA != ',' || commaB != ',')
            {
                return false;
            }

            value = parsed;
            return true;
        }

        std::string FormatShotFloat3(const DirectX::XMFLOAT3& value) const
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3) << value.x << ',' << value.y << ',' << value.z;
            return stream.str();
        }

        void AddDefaultTrailerShots()
        {
            m_trailerKeys = {
                TrailerShotKey{ 0.0f, { -7.4f, 3.3f, -0.6f }, Add(m_riftPosition, { 0.0f, 0.3f, 0.0f }), 0.982f, 0.50f, 0.62f, 0.070f, 0.28f, 0.52f, 0.20f, 0.10f, "opening_rift" },
                TrailerShotKey{ 2.4f, { -2.6f, 2.0f, 2.8f }, Add(m_riftPosition, { 0.2f, 0.1f, 0.0f }), 0.989f, 0.38f, 0.68f, 0.080f, 0.38f, 0.42f, 0.34f, 0.25f, "hero_orbit" },
                TrailerShotKey{ 4.9f, { 4.8f, 3.9f, -2.2f }, Add(m_riftPosition, { -0.1f, 0.5f, 0.2f }), 0.978f, 0.56f, 0.78f, 0.090f, 0.42f, 0.55f, 0.50f, 0.40f, "rift_peak" },
                TrailerShotKey{ 7.6f, { 0.4f, 1.8f, -15.4f }, Add(m_riftPosition, { 0.0f, 0.0f, 0.0f }), 0.992f, 0.44f, 0.72f, 0.075f, 0.30f, 0.46f, 0.28f, 0.30f, "pullback" },
                TrailerShotKey{ 10.2f, { -7.4f, 3.3f, -0.6f }, Add(m_riftPosition, { 0.0f, 0.3f, 0.0f }), 0.982f, 0.50f, 0.62f, 0.070f, 0.25f, 0.48f, 0.18f, 0.00f, "loop_out" }
            };
        }

        void LoadTrailerShotAsset()
        {
            m_trailerKeys.clear();
            const std::filesystem::path resolvedPath = Disparity::FileSystem::FindAssetPath("Assets/Cinematics/Showcase.dshot");
            std::ifstream file(resolvedPath);
            if (!file)
            {
                AddDefaultTrailerShots();
                return;
            }

            std::string line;
            while (std::getline(file, line))
            {
                line = Trim(line);
                if (line.empty() || line.front() == '#')
                {
                    continue;
                }

                if (line.rfind("key ", 0) == 0)
                {
                    line = Trim(line.substr(4));
                }

                std::vector<std::string> fields;
                size_t begin = 0;
                while (begin <= line.size())
                {
                    const size_t separator = line.find('|', begin);
                    fields.push_back(Trim(line.substr(begin, separator == std::string::npos ? std::string::npos : separator - begin)));
                    if (separator == std::string::npos)
                    {
                        break;
                    }
                    begin = separator + 1;
                }

                if (fields.size() < 7)
                {
                    continue;
                }

                TrailerShotKey key;
                if (!ParseShotFloat3(fields[1], key.Position) || !ParseShotFloat3(fields[2], key.Target))
                {
                    continue;
                }

                try
                {
                    key.Time = std::stof(fields[0]);
                    key.Focus = std::stof(fields[3]);
                    key.DofStrength = std::stof(fields[4]);
                    key.LensDirt = std::stof(fields[5]);
                    key.Letterbox = std::stof(fields[6]);
                    if (fields.size() > 7) { key.EaseIn = std::stof(fields[7]); }
                    if (fields.size() > 8) { key.EaseOut = std::stof(fields[8]); }
                    if (fields.size() > 9) { key.RendererPulse = std::stof(fields[9]); }
                    if (fields.size() > 10) { key.AudioCue = std::stof(fields[10]); }
                    if (fields.size() > 11) { key.Bookmark = fields[11]; }
                }
                catch (...)
                {
                    continue;
                }

                m_trailerKeys.push_back(key);
            }

            std::sort(m_trailerKeys.begin(), m_trailerKeys.end(), [](const TrailerShotKey& a, const TrailerShotKey& b) {
                return a.Time < b.Time;
            });

            if (m_trailerKeys.size() < 2)
            {
                AddDefaultTrailerShots();
            }
        }

        bool SaveTrailerShotAsset(const std::filesystem::path& path)
        {
            if (path.has_parent_path())
            {
                std::filesystem::create_directories(path.parent_path());
            }

            std::sort(m_trailerKeys.begin(), m_trailerKeys.end(), [](const TrailerShotKey& a, const TrailerShotKey& b) {
                return a.Time < b.Time;
            });

            std::ofstream file(path, std::ios::trunc);
            if (!file)
            {
                return false;
            }

            file << "# DISPARITY cinematic shot track v3\n";
            file << "# key time|position(x,y,z)|target(x,y,z)|focus|dof_strength|lens_dirt|letterbox|ease_in|ease_out|renderer_pulse|audio_cue|bookmark\n";
            for (const TrailerShotKey& key : m_trailerKeys)
            {
                file << "key "
                    << std::fixed << std::setprecision(3) << key.Time << '|'
                    << FormatShotFloat3(key.Position) << '|'
                    << FormatShotFloat3(key.Target) << '|'
                    << std::setprecision(4) << key.Focus << '|'
                    << key.DofStrength << '|'
                    << key.LensDirt << '|'
                    << key.Letterbox << '|'
                    << key.EaseIn << '|'
                    << key.EaseOut << '|'
                    << key.RendererPulse << '|'
                    << key.AudioCue << '|'
                    << key.Bookmark << '\n';
            }

            return file.good();
        }

        Disparity::Transform BeamTransform(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float thickness) const
        {
            const DirectX::XMFLOAT3 delta = Subtract(end, start);
            const float length = std::max(0.001f, Length(delta));
            Disparity::Transform transform;
            transform.Position = Scale(Add(start, end), 0.5f);
            transform.Rotation = {
                -std::asin(std::clamp(delta.y / length, -1.0f, 1.0f)),
                std::atan2(delta.x, delta.z),
                0.0f
            };
            transform.Scale = { thickness, thickness, length };
            return transform;
        }

        Disparity::Transform BillboardTransform(const DirectX::XMFLOAT3& position, float size, float roll = 0.0f) const
        {
            const DirectX::XMFLOAT3 cameraPosition = GetRenderCamera().GetPosition();
            const DirectX::XMFLOAT3 toCamera = Normalize(Subtract(cameraPosition, position));
            Disparity::Transform transform;
            transform.Position = position;
            transform.Rotation = { Pi * 0.5f, std::atan2(toCamera.x, toCamera.z), roll };
            transform.Scale = { size, size, size };
            return transform;
        }

        void DrawRiftVfx(Disparity::Renderer& renderer)
        {
            const float visualTime = ShowcaseVisualTime();
            const float modeBoost = (m_showcaseMode || m_trailerMode) ? 1.0f : 0.64f;
            m_lastRiftVfxStats = RiftVfxSystemStats{
                7u,
                28u,
                12u,
                8u,
                35u,
                5u
            };
            uint32_t drawCount = 0;

            for (int fogIndex = 0; fogIndex < 7; ++fogIndex)
            {
                const float n = static_cast<float>(fogIndex);
                const float angle = n * 0.897f + visualTime * (0.05f + n * 0.004f);
                const float radius = 2.4f + n * 0.42f;
                const DirectX::XMFLOAT3 position = Add(m_riftPosition, {
                    std::sin(angle) * radius,
                    -0.55f + std::sin(visualTime * 0.27f + n) * 0.45f,
                    std::cos(angle) * radius * 0.72f
                });
                Disparity::Transform fog = BillboardTransform(position, (2.6f + n * 0.18f) * modeBoost, angle * 0.17f);
                renderer.DrawMesh(m_vfxQuadMesh, fog, m_vfxFogMaterial);
                ++drawCount;
            }

            for (int particleIndex = 0; particleIndex < 28; ++particleIndex)
            {
                const float n = static_cast<float>(particleIndex);
                const float seed = n * 12.9898f;
                const float angle = n * 0.83f + visualTime * (0.58f + std::fmod(n, 5.0f) * 0.03f);
                const float radius = 1.35f + std::fmod(n * 1.91f, 3.4f);
                const float orbitY = std::sin(visualTime * 1.18f + seed) * (0.95f + std::fmod(n, 3.0f) * 0.22f);
                const DirectX::XMFLOAT3 position = Add(m_riftPosition, {
                    std::sin(angle) * radius,
                    orbitY,
                    std::cos(angle * 1.11f) * radius * 0.72f
                });
                const float size = (0.14f + std::fmod(n, 6.0f) * 0.018f + m_riftBeatPulse * 0.16f) * modeBoost;
                const Disparity::Material& material = particleIndex % 3 == 0 ? m_vfxHotParticleMaterial : m_vfxParticleMaterial;
                renderer.DrawMesh(m_vfxQuadMesh, BillboardTransform(position, size, angle), material);
                ++drawCount;
            }

            for (int ribbonIndex = 0; ribbonIndex < 12; ++ribbonIndex)
            {
                const float n = static_cast<float>(ribbonIndex);
                const float angle = visualTime * 0.64f + n * Pi * 2.0f / 12.0f;
                const float radius = 3.0f + std::sin(visualTime * 0.41f + n) * 0.34f;
                const DirectX::XMFLOAT3 position = Add(m_riftPosition, {
                    std::sin(angle) * radius,
                    std::cos(angle * 1.7f) * 0.55f,
                    std::cos(angle) * radius * 0.74f
                });
                Disparity::Transform ribbon;
                ribbon.Position = position;
                ribbon.Rotation = { 0.08f * std::sin(angle), angle + Pi * 0.5f, 0.22f * std::sin(visualTime + n) };
                ribbon.Scale = { 0.055f * modeBoost, 0.035f * modeBoost, (1.1f + m_riftBeatPulse * 0.5f) * modeBoost };
                renderer.DrawMesh(m_cubeMesh, ribbon, m_vfxRibbonMaterial);
                ++drawCount;
            }

            for (int arcIndex = 0; arcIndex < 8; ++arcIndex)
            {
                const float n = static_cast<float>(arcIndex);
                const float angleA = visualTime * 0.92f + n * Pi * 0.25f;
                const float angleB = angleA + 0.52f + std::sin(visualTime * 1.5f + n) * 0.12f;
                const DirectX::XMFLOAT3 start = Add(m_riftPosition, {
                    std::sin(angleA) * 1.0f,
                    std::sin(visualTime * 1.3f + n) * 0.72f,
                    std::cos(angleA) * 0.8f
                });
                const DirectX::XMFLOAT3 end = Add(m_riftPosition, {
                    std::sin(angleB) * (2.8f + m_riftBeatPulse * 0.7f),
                    std::cos(visualTime * 1.1f + n) * 1.25f,
                    std::cos(angleB) * (2.1f + m_riftBeatPulse * 0.35f)
                });
                renderer.DrawMesh(m_cubeMesh, BeamTransform(start, end, 0.035f + m_riftBeatPulse * 0.035f), m_vfxLightningMaterial);
                ++drawCount;
            }

            if (m_runtimeVerification.Enabled)
            {
                m_runtimeEditorStats.RiftVfxDraws += drawCount;
            }
        }

        void RequestHighResolutionCapture()
        {
            if (!m_renderer)
            {
                return;
            }

            std::filesystem::create_directories("Saved/Captures");
            m_highResCaptureSourcePath = "Saved/Captures/DISPARITY_photo_source.ppm";
            m_highResCaptureOutputPath = "Saved/Captures/DISPARITY_photo_2x.ppm";
            m_highResCaptureNativePngPath = "Saved/Captures/DISPARITY_photo_source.png";
            m_highResCapturePending = true;
            if (m_runtimeVerification.Enabled)
            {
                m_runtimeBudgetSkipFrames = std::max(m_runtimeBudgetSkipFrames, 4u);
            }
            m_renderer->RequestFrameCapture(m_highResCaptureSourcePath);
            m_renderer->RequestFrameCapture(m_highResCaptureNativePngPath);
            SetStatus("Queued 2x trailer/photo capture plus PNG export");
        }

        bool ReadPpmImage(const std::filesystem::path& path, PpmImage& image) const
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return false;
            }

            std::string magic;
            uint32_t maxValue = 0;
            file >> magic >> image.Width >> image.Height >> maxValue;
            if (magic != "P6" || image.Width == 0 || image.Height == 0 || maxValue != 255)
            {
                return false;
            }

            file.get();
            const size_t byteCount = static_cast<size_t>(image.Width) * static_cast<size_t>(image.Height) * 3u;
            image.Pixels.resize(byteCount);
            file.read(reinterpret_cast<char*>(image.Pixels.data()), static_cast<std::streamsize>(image.Pixels.size()));
            return file.good() || file.gcount() == static_cast<std::streamsize>(image.Pixels.size());
        }

        bool WriteUpscaledPpm(const PpmImage& image, const std::filesystem::path& path, uint32_t scale) const
        {
            if (image.Width == 0 || image.Height == 0 || image.Pixels.empty() || scale < 2)
            {
                return false;
            }

            if (path.has_parent_path())
            {
                std::filesystem::create_directories(path.parent_path());
            }

            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                return false;
            }

            const uint32_t outWidth = image.Width * scale;
            const uint32_t outHeight = image.Height * scale;
            output << "P6\n" << outWidth << " " << outHeight << "\n255\n";
            for (uint32_t y = 0; y < outHeight; ++y)
            {
                const uint32_t sourceY = y / scale;
                for (uint32_t x = 0; x < outWidth; ++x)
                {
                    const uint32_t sourceX = x / scale;
                    const size_t sourceIndex = (static_cast<size_t>(sourceY) * image.Width + sourceX) * 3u;
                    output.write(reinterpret_cast<const char*>(&image.Pixels[sourceIndex]), 3);
                }
            }

            return output.good();
        }

        void CompleteHighResolutionCaptureIfReady()
        {
            if (!m_highResCapturePending || !m_renderer || !m_renderer->HasLastFrameCapture())
            {
                return;
            }

            const Disparity::FrameCaptureResult& capture = m_renderer->GetLastFrameCapture();
            if (!capture.Success || capture.Path.lexically_normal() != m_highResCaptureSourcePath.lexically_normal())
            {
                return;
            }

            PpmImage image;
            if (!ReadPpmImage(m_highResCaptureSourcePath, image) || !WriteUpscaledPpm(image, m_highResCaptureOutputPath, 2))
            {
                m_highResCapturePending = false;
                SetStatus("High-res capture failed");
                if (m_runtimeVerification.Enabled)
                {
                    AddRuntimeVerificationFailure("high-resolution capture upscaling failed.");
                }
                return;
            }

            m_highResCapturePending = false;
            if (m_runtimeVerification.Enabled)
            {
                m_runtimeBudgetSkipFrames = std::max(m_runtimeBudgetSkipFrames, 4u);
            }
            ++m_runtimeEditorStats.HighResCaptures;
            SetStatus("Wrote " + m_highResCaptureOutputPath.string());
            if (m_runtimeVerification.Enabled)
            {
                AddRuntimeVerificationNote("Validated 2x high-resolution capture workflow.");
            }
        }

        void DrawShowcaseRift(Disparity::Renderer& renderer)
        {
            const float visualTime = ShowcaseVisualTime();
            const float pulse = 0.5f + 0.5f * std::sin(visualTime * 2.7f);
            const float fastPulse = 0.5f + 0.5f * std::sin(visualTime * 5.4f);
            const float showcaseBoost = m_showcaseMode ? 1.22f : 1.0f;

            Disparity::Transform core;
            core.Position = m_riftPosition;
            core.Rotation = { visualTime * 0.47f, visualTime * 0.79f, visualTime * 0.36f };
            const float coreScale = (0.54f + pulse * 0.16f) * showcaseBoost;
            core.Scale = { coreScale, coreScale, coreScale };
            renderer.DrawMesh(m_cubeMesh, core, m_riftCoreMaterial);

            for (int ringIndex = 0; ringIndex < 4; ++ringIndex)
            {
                const float ring = static_cast<float>(ringIndex);
                Disparity::Transform ringTransform;
                ringTransform.Position = Add(m_riftPosition, { 0.0f, (ring - 1.5f) * 0.05f, 0.0f });
                ringTransform.Rotation = {
                    Pi * 0.5f + std::sin(visualTime * 0.33f + ring) * 0.48f,
                    visualTime * (0.22f + ring * 0.06f),
                    ring * Pi * 0.25f + visualTime * (0.34f + ring * 0.04f)
                };
                const float radius = (2.16f + ring * 0.28f + pulse * 0.08f) * showcaseBoost;
                ringTransform.Scale = { radius, radius, radius };
                const Disparity::Material& ringMaterial = ringIndex % 2 == 0 ? m_riftCyanMaterial : m_riftMagentaMaterial;
                renderer.DrawMesh(m_gizmoRingMesh, ringTransform, ringMaterial);
            }

            constexpr int ShardCount = 14;
            for (int shardIndex = 0; shardIndex < ShardCount; ++shardIndex)
            {
                const float shard = static_cast<float>(shardIndex);
                const float alpha = shard / static_cast<float>(ShardCount);
                const float angle = alpha * Pi * 2.0f + visualTime * (0.43f + alpha * 0.27f);
                const float verticalAngle = visualTime * 0.71f + alpha * Pi * 4.0f;
                const float radius = (2.95f + std::sin(verticalAngle) * 0.45f) * showcaseBoost;
                Disparity::Transform shardTransform;
                shardTransform.Position = Add(m_riftPosition, {
                    std::sin(angle) * radius,
                    std::sin(verticalAngle) * 0.9f,
                    std::cos(angle) * radius * 0.76f
                });
                shardTransform.Rotation = {
                    visualTime * (0.91f + alpha),
                    angle,
                    visualTime * (0.58f + alpha * 0.4f)
                };
                const float shardScale = 0.18f + (shardIndex % 4) * 0.04f + fastPulse * 0.035f;
                shardTransform.Scale = { shardScale * 0.72f, shardScale * 1.85f, shardScale * 0.72f };
                const Disparity::Material& shardMaterial = shardIndex % 3 == 0
                    ? m_riftVioletMaterial
                    : (shardIndex % 3 == 1 ? m_riftCyanMaterial : m_riftMagentaMaterial);
                renderer.DrawMesh(m_cubeMesh, shardTransform, shardMaterial);
            }

            constexpr int SpireCount = 6;
            for (int spireIndex = 0; spireIndex < SpireCount; ++spireIndex)
            {
                const float spire = static_cast<float>(spireIndex);
                const float angle = spire / static_cast<float>(SpireCount) * Pi * 2.0f + Pi * 0.16f;
                const DirectX::XMFLOAT3 base = Add(m_riftPosition, {
                    std::sin(angle) * 4.25f,
                    -1.28f,
                    std::cos(angle) * 3.35f
                });

                Disparity::Transform spireTransform;
                spireTransform.Position = base;
                spireTransform.Rotation = { 0.0f, angle, 0.08f * std::sin(visualTime + spire) };
                spireTransform.Scale = { 0.38f, 2.15f + (spireIndex % 2) * 0.55f, 0.38f };
                renderer.DrawMesh(m_cubeMesh, spireTransform, m_riftObsidianMaterial);

                Disparity::Transform capTransform = spireTransform;
                capTransform.Position = Add(base, { 0.0f, spireTransform.Scale.y + 0.18f, 0.0f });
                capTransform.Scale = { 0.22f, 0.22f, 0.22f };
                const Disparity::Material& capMaterial = spireIndex % 2 == 0 ? m_riftCyanMaterial : m_riftMagentaMaterial;
                renderer.DrawMesh(m_cubeMesh, capTransform, capMaterial);
            }
        }

        Disparity::Transform PlayerBodyTransform() const
        {
            Disparity::Transform body;
            body.Position = Add(m_playerPosition, { 0.0f, 0.85f + m_playerBobOffset, 0.0f });
            body.Rotation = { 0.0f, m_playerYaw, 0.0f };
            body.Scale = { 0.75f, 1.55f, 0.55f };
            return body;
        }

        void DrawPlayer(Disparity::Renderer& renderer)
        {
            renderer.DrawMeshWithId(m_cubeMesh, PlayerBodyTransform(), m_playerBodyMaterial, EditorPickPlayerId);

            Disparity::Transform head;
            head.Position = Add(m_playerPosition, { 0.0f, 1.85f + m_playerBobOffset, 0.0f });
            head.Rotation = { 0.0f, m_playerYaw, 0.0f };
            head.Scale = { 0.55f, 0.45f, 0.55f };
            renderer.DrawMeshWithId(m_cubeMesh, head, m_playerHeadMaterial, EditorPickPlayerId);
        }

        void DrawSelectionOutline(Disparity::Renderer& renderer)
        {
            Disparity::Material outlineMaterial;
            outlineMaterial.Albedo = { 1.65f, 1.08f, 0.18f };
            outlineMaterial.Roughness = 0.18f;
            outlineMaterial.Metallic = 0.0f;
            outlineMaterial.Alpha = 0.34f;

            if (m_selectedPlayer)
            {
                Disparity::Transform body = PlayerBodyTransform();
                body.Scale = Scale(body.Scale, 1.08f);
                renderer.DrawMeshWithId(m_cubeMesh, body, outlineMaterial, EditorPickPlayerId);

                Disparity::Transform head;
                head.Position = Add(m_playerPosition, { 0.0f, 1.85f + m_playerBobOffset, 0.0f });
                head.Rotation = { 0.0f, m_playerYaw, 0.0f };
                head.Scale = { 0.62f, 0.52f, 0.62f };
                renderer.DrawMeshWithId(m_cubeMesh, head, outlineMaterial, EditorPickPlayerId);
                return;
            }

            if (m_selectedIndex >= m_scene.Count())
            {
                return;
            }

            const std::vector<size_t> selectedIndices = GetSelectedSceneIndices();
            for (const size_t selectedIndex : selectedIndices)
            {
                const Disparity::NamedSceneObject& selected = m_scene.GetObjects()[selectedIndex];
                Disparity::Transform outlineTransform = selected.Object.TransformData;
                outlineTransform.Scale = Scale(outlineTransform.Scale, 1.06f);
                renderer.DrawMeshWithId(selected.Object.Mesh, outlineTransform, outlineMaterial, SceneObjectPickId(selectedIndex));
            }
        }

        bool TryGetSelectionPivot(DirectX::XMFLOAT3& outPivot) const
        {
            if (m_selectedPlayer)
            {
                outPivot = Add(m_playerPosition, { 0.0f, 1.05f, 0.0f });
                return true;
            }

            const std::vector<size_t> selectedIndices = GetSelectedSceneIndices();
            if (selectedIndices.empty())
            {
                return false;
            }

            DirectX::XMFLOAT3 pivot = {};
            for (const size_t selectedIndex : selectedIndices)
            {
                pivot = Add(pivot, m_scene.GetObjects()[selectedIndex].Object.TransformData.Position);
            }

            outPivot = Scale(pivot, 1.0f / static_cast<float>(selectedIndices.size()));
            return true;
        }

        void DrawSelectionGizmoHandles(Disparity::Renderer& renderer)
        {
            DirectX::XMFLOAT3 pivot = {};
            if (!TryGetSelectionPivot(pivot) || m_cubeMesh == 0)
            {
                return;
            }

            const float screenScale = GizmoScreenScale(pivot);
            Disparity::Transform center;
            center.Position = pivot;
            center.Scale = { 0.18f * screenScale, 0.18f * screenScale, 0.18f * screenScale };
            renderer.DrawMesh(m_cubeMesh, center, m_gizmoCenterMaterial);

            const float handleDistance = GizmoHandleDistance(pivot);
            const float handleSize = (m_gizmoMode == GizmoMode::Scale ? 0.24f : 0.19f) * screenScale;
            const std::array<std::pair<GizmoAxis, Disparity::Material*>, 3> handles = {
                std::pair<GizmoAxis, Disparity::Material*>{ GizmoAxis::X, &m_gizmoXMaterial },
                std::pair<GizmoAxis, Disparity::Material*>{ GizmoAxis::Y, &m_gizmoYMaterial },
                std::pair<GizmoAxis, Disparity::Material*>{ GizmoAxis::Z, &m_gizmoZMaterial }
            };

            if (m_gizmoMode == GizmoMode::Rotate)
            {
                for (const auto& [axis, material] : handles)
                {
                    Disparity::Material drawMaterial = IsGizmoAxisHighlighted(axis) ? HighlightGizmoMaterial(*material) : *material;
                    Disparity::Transform ring;
                    ring.Position = pivot;
                    ring.Rotation = GizmoRingRotation(axis);
                    ring.Scale = { handleDistance, handleDistance, handleDistance };
                    renderer.DrawMeshWithId(m_gizmoRingMesh, ring, drawMaterial, GizmoAxisPickId(axis));

                    const DirectX::XMFLOAT3 axisVector = AxisVector(axis);
                    Disparity::Transform marker;
                    marker.Position = Add(pivot, Scale(axisVector, handleDistance));
                    marker.Scale = { handleSize * 0.8f, handleSize * 0.8f, handleSize * 0.8f };
                    renderer.DrawMeshWithId(m_cubeMesh, marker, drawMaterial, GizmoAxisPickId(axis));
                }
                return;
            }

            if (m_gizmoMode == GizmoMode::Translate && !m_selectedPlayer && m_gizmoPlaneHandleMesh != 0)
            {
                const std::array<std::pair<GizmoPlane, Disparity::Material*>, 3> planes = {
                    std::pair<GizmoPlane, Disparity::Material*>{ GizmoPlane::XY, &m_gizmoPlaneXYMaterial },
                    std::pair<GizmoPlane, Disparity::Material*>{ GizmoPlane::XZ, &m_gizmoPlaneXZMaterial },
                    std::pair<GizmoPlane, Disparity::Material*>{ GizmoPlane::YZ, &m_gizmoPlaneYZMaterial }
                };

                for (const auto& [plane, material] : planes)
                {
                    Disparity::Material drawMaterial = IsGizmoPlaneHighlighted(plane) ? HighlightGizmoMaterial(*material) : *material;
                    Disparity::Transform planeHandle;
                    planeHandle.Position = GizmoPlaneCenter(pivot, plane);
                    planeHandle.Rotation = GizmoPlaneRotation(plane);
                    const float planeSize = screenScale * 0.34f;
                    planeHandle.Scale = { planeSize, planeSize, planeSize };
                    renderer.DrawMeshWithId(m_gizmoPlaneHandleMesh, planeHandle, drawMaterial, GizmoPlanePickId(plane));
                }
            }

            for (const auto& [axis, material] : handles)
            {
                const DirectX::XMFLOAT3 axisVector = AxisVector(axis);
                Disparity::Material drawMaterial = IsGizmoAxisHighlighted(axis) ? HighlightGizmoMaterial(*material) : *material;

                Disparity::Transform marker;
                marker.Position = Add(pivot, Scale(axisVector, handleDistance));
                marker.Scale = { handleSize, handleSize, handleSize };
                renderer.DrawMeshWithId(m_cubeMesh, marker, drawMaterial, GizmoAxisPickId(axis));

                Disparity::Transform tick;
                tick.Position = Add(pivot, Scale(axisVector, handleDistance * 0.55f));
                tick.Scale = { handleSize * 0.62f, handleSize * 0.62f, handleSize * 0.62f };
                renderer.DrawMeshWithId(m_cubeMesh, tick, drawMaterial, GizmoAxisPickId(axis));
            }
        }

        void DrawDockspace()
        {
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        }

        void DrawMainMenu()
        {
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("DISPARITY"))
                {
                    if (ImGui::MenuItem("Showcase Mode", "F7", m_showcaseMode))
                    {
                        SetShowcaseMode(!m_showcaseMode);
                    }
                    if (ImGui::MenuItem("Trailer/Photo Mode", "F8", m_trailerMode))
                    {
                        SetTrailerMode(!m_trailerMode);
                    }
                    if (ImGui::MenuItem("High-Res Capture", "F9"))
                    {
                        RequestHighResolutionCapture();
                    }
                    if (ImGui::MenuItem("Reload Scene", "F5"))
                    {
                        ReloadSceneAndScript();
                    }
                    if (ImGui::MenuItem("Save Runtime Scene", "F6"))
                    {
                        SaveRuntimeScene();
                    }
                    if (ImGui::MenuItem("Audio Test", "F2"))
                    {
                        Disparity::AudioSystem::PlayNotification();
                        SetStatus("Played generated audio test tone");
                    }
                    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo()))
                    {
                        UndoEdit();
                    }
                    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo()))
                    {
                        RedoEdit();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Edit"))
                {
                    if (ImGui::MenuItem("Copy", "Ctrl+C", false, CanCopySelection()))
                    {
                        CopySelectedObject();
                    }
                    if (ImGui::MenuItem("Paste", "Ctrl+V", false, m_sceneClipboard.has_value()))
                    {
                        PasteSceneObject();
                    }
                    if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, CanCopySelection()))
                    {
                        DuplicateSelectedObject();
                    }
                    if (ImGui::MenuItem("Delete", "Del", false, CanCopySelection()))
                    {
                        DeleteSelectedObject();
                    }
                    ImGui::EndMenu();
                }

                if (!m_statusMessage.empty() && m_statusTimer > 0.0f)
                {
                    ImGui::TextDisabled("%s", m_statusMessage.c_str());
                }

                ImGui::EndMainMenuBar();
            }
        }

        void DrawViewportPanel()
        {
            ImGui::SetNextWindowPos(ImVec2(12.0f, 464.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(280.0f, 190.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Viewport"))
            {
                ImGui::End();
                return;
            }

            bool showcaseMode = m_showcaseMode;
            if (ImGui::Checkbox("Showcase mode", &showcaseMode))
            {
                SetShowcaseMode(showcaseMode);
            }
            bool trailerMode = m_trailerMode;
            if (ImGui::Checkbox("Trailer/photo mode", &trailerMode))
            {
                SetTrailerMode(trailerMode);
            }
            if (ImGui::Button("Capture PPM+PNG"))
            {
                RequestHighResolutionCapture();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", m_highResCaptureOutputPath.empty() ? "No capture" : m_highResCaptureOutputPath.string().c_str());
            ImGui::Checkbox("Editor camera", &m_editorCameraEnabled);
            if (ImGui::Button("Frame Selection"))
            {
                FrameSelectedObject();
            }
            ImGui::SameLine();
            if (ImGui::Button("Frame Player"))
            {
                m_editorCameraTarget = Add(m_playerPosition, { 0.0f, 1.1f, 0.0f });
                m_editorCameraEnabled = true;
            }

            ImGui::SliderFloat("Orbit distance", &m_editorCameraDistance, 2.5f, 40.0f);
            ImGui::DragFloat3("Target", &m_editorCameraTarget.x, 0.08f);
            const char* gizmoModes[] = { "Translate", "Rotate", "Scale" };
            int gizmoModeIndex = static_cast<int>(m_gizmoMode);
            if (ImGui::Combo("Gizmo mode", &gizmoModeIndex, gizmoModes, IM_ARRAYSIZE(gizmoModes)))
            {
                m_gizmoMode = static_cast<GizmoMode>(gizmoModeIndex);
                m_gizmoStatus = std::string(GizmoModeName(m_gizmoMode)) + " mode";
            }
            const char* gizmoSpaces[] = { "World", "Local" };
            int gizmoSpaceIndex = static_cast<int>(m_gizmoSpace);
            if (ImGui::Combo("Gizmo space", &gizmoSpaceIndex, gizmoSpaces, IM_ARRAYSIZE(gizmoSpaces)))
            {
                m_gizmoSpace = static_cast<GizmoSpace>(gizmoSpaceIndex);
                m_gizmoStatus = std::string(GizmoSpaceName(m_gizmoSpace)) + " space";
            }
            ImGui::Text("Pick: %s", m_lastPickStatus.c_str());
            ImGui::Text("Hover: %s", DescribeEditorPick(m_hoverPick).c_str());
            ImGui::Text("GPU pick: %s", m_lastGpuPickStatus.c_str());
            ImGui::Text("Viewport: %.0fx%.0f", m_editorViewport.Width, m_editorViewport.Height);
            if (m_renderer && m_renderer->GetEditorViewportShaderResourceView())
            {
                const float previewWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
                const float previewHeight = previewWidth * 9.0f / 16.0f;
                ImGui::Image(
                    reinterpret_cast<ImTextureID>(m_renderer->GetEditorViewportShaderResourceView()),
                    ImVec2(previewWidth, previewHeight));
            }
            ImGui::Text("Gizmo: %s", m_gizmoStatus.c_str());
            ImGui::TextDisabled("F7 showcase, F8 trailer/photo, F9 2x capture. Tab releases mouse; right-drag plus WASD/QE flies.");
            ImGui::End();
        }

        void DrawHierarchyPanel()
        {
            ImGui::SetNextWindowPos(ImVec2(12.0f, 32.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(280.0f, 420.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Hierarchy"))
            {
                ImGui::End();
                return;
            }

            if (ImGui::Selectable("Player", m_selectedPlayer))
            {
                m_selectedPlayer = true;
                m_multiSelection.clear();
            }

            ImGui::SeparatorText("Scene");
            const auto& objects = m_scene.GetObjects();
            for (size_t index = 0; index < objects.size(); ++index)
            {
                const bool selected = IsSceneObjectSelected(index);
                if (ImGui::Selectable(objects[index].Name.c_str(), selected))
                {
                    SelectSceneObject(index, Disparity::Input::IsKeyDown(VK_CONTROL));
                }
            }

            ImGui::SeparatorText("Actions");
            const std::vector<size_t> selectedIndices = GetSelectedSceneIndices();
            ImGui::Text("Selected objects: %zu", selectedIndices.size());
            const bool canEditSelection = CanCopySelection();
            if (ImGui::Button("Copy") && canEditSelection)
            {
                CopySelectedObject();
            }
            ImGui::SameLine();
            if (ImGui::Button("Paste") && m_sceneClipboard.has_value())
            {
                PasteSceneObject();
            }
            if (ImGui::Button("Duplicate") && canEditSelection)
            {
                DuplicateSelectedObject();
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete") && canEditSelection)
            {
                DeleteSelectedObject();
            }

            ImGui::End();
        }

        void DrawInspectorPanel()
        {
            ImGui::SetNextWindowPos(ImVec2(304.0f, 32.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(340.0f, 420.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Inspector"))
            {
                ImGui::End();
                return;
            }

            if (m_selectedPlayer)
            {
                ImGui::TextUnformatted("Player");
                const EditState before = CaptureEditState();
                bool changed = false;
                changed |= ImGui::DragFloat3("Player position", &m_playerPosition.x, 0.05f);
                changed |= ImGui::DragFloat("Yaw", &m_playerYaw, 0.02f);
                changed |= DrawPlayerGizmo();
                changed |= DrawMaterialEditor("Body Material", m_playerBodyMaterial);
                if (changed)
                {
                    PushUndoState(before, "Edit Player");
                }
            }
            else if (m_scene.Count() > 0 && m_selectedIndex < m_scene.Count())
            {
                Disparity::NamedSceneObject& selected = m_scene.GetObjects()[m_selectedIndex];
                ImGui::Text("Name: %s", selected.Name.c_str());
                ImGui::Text("Mesh: %s", selected.MeshName.c_str());
                ImGui::Text("Stable ID: %llu", static_cast<unsigned long long>(selected.StableId));
                const EditState before = CaptureEditState();
                bool changed = false;
                changed |= ImGui::DragFloat3("Object position", &selected.Object.TransformData.Position.x, 0.05f);
                changed |= ImGui::DragFloat3("Rotation", &selected.Object.TransformData.Rotation.x, 0.02f);
                changed |= ImGui::DragFloat3("Scale", &selected.Object.TransformData.Scale.x, 0.05f, 0.05f, 50.0f);
                changed |= DrawTransformGizmo(selected.Object.TransformData);
                changed |= DrawMaterialEditor("Material", selected.Object.MaterialData);
                DrawPrefabOverridesPanel(selected);
                if (changed)
                {
                    PushUndoState(before, "Edit " + selected.Name);
                    SyncSceneObjectToRegistry(m_selectedIndex);
                }
            }

            ImGui::End();
        }

        bool DrawMaterialEditor(const char* label, Disparity::Material& material)
        {
            bool changed = false;
            if (ImGui::TreeNode(label))
            {
                changed |= ImGui::ColorEdit3("Albedo", &material.Albedo.x);
                changed |= ImGui::SliderFloat("Roughness", &material.Roughness, 0.02f, 1.0f);
                changed |= ImGui::SliderFloat("Metallic", &material.Metallic, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Alpha", &material.Alpha, 0.0f, 1.0f);
                ImGui::TreePop();
            }

            return changed;
        }

        bool DrawTransformGizmo(Disparity::Transform& transform)
        {
            bool changed = false;
            constexpr float Step = 0.25f;
            if (ImGui::TreeNode("Transform Gizmo"))
            {
                ImGui::TextUnformatted("Move");
                changed |= AxisButton("-X", transform.Position.x, -Step);
                ImGui::SameLine();
                changed |= AxisButton("+X", transform.Position.x, Step);
                ImGui::SameLine();
                changed |= AxisButton("-Y", transform.Position.y, -Step);
                ImGui::SameLine();
                changed |= AxisButton("+Y", transform.Position.y, Step);
                ImGui::SameLine();
                changed |= AxisButton("-Z", transform.Position.z, -Step);
                ImGui::SameLine();
                changed |= AxisButton("+Z", transform.Position.z, Step);

                ImGui::TextUnformatted("Scale");
                changed |= AxisButton("S-", transform.Scale.y, -0.1f, 0.05f);
                ImGui::SameLine();
                changed |= AxisButton("S+", transform.Scale.y, 0.1f, 0.05f);
                ImGui::SameLine();
                changed |= AxisButton("Yaw-", transform.Rotation.y, -0.08f);
                ImGui::SameLine();
                changed |= AxisButton("Yaw+", transform.Rotation.y, 0.08f);
                ImGui::TreePop();
            }

            return changed;
        }

        bool DrawPlayerGizmo()
        {
            bool changed = false;
            Disparity::Transform playerTransform;
            playerTransform.Position = m_playerPosition;
            playerTransform.Rotation.y = m_playerYaw;
            if (DrawTransformGizmo(playerTransform))
            {
                m_playerPosition = playerTransform.Position;
                m_playerYaw = playerTransform.Rotation.y;
                changed = true;
            }
            return changed;
        }

        bool AxisButton(const char* label, float& value, float delta, float minimum = -FLT_MAX)
        {
            if (!ImGui::SmallButton(label))
            {
                return false;
            }

            value = std::max(minimum, value + delta);
            return true;
        }

        void DrawPrefabOverridesPanel(const Disparity::NamedSceneObject& selected)
        {
            if (!ImGui::TreeNode("Prefab Overrides"))
            {
                return;
            }

            Disparity::Prefab prefab;
            const bool loaded = Disparity::PrefabIO::Load("Assets/Prefabs/Beacon.dprefab", prefab);
            if (!loaded)
            {
                ImGui::TextUnformatted("Beacon prefab not loaded");
                ImGui::TreePop();
                return;
            }

            const auto differs = [](float a, float b) {
                return std::abs(a - b) > 0.001f;
            };
            uint32_t overrides = 0;
            overrides += selected.MeshName != prefab.MeshName ? 1u : 0u;
            overrides += differs(selected.Object.TransformData.Scale.x, prefab.TransformData.Scale.x) ? 1u : 0u;
            overrides += differs(selected.Object.TransformData.Scale.y, prefab.TransformData.Scale.y) ? 1u : 0u;
            overrides += differs(selected.Object.TransformData.Scale.z, prefab.TransformData.Scale.z) ? 1u : 0u;
            overrides += differs(selected.Object.MaterialData.Albedo.x, prefab.MaterialData.Albedo.x) ? 1u : 0u;
            overrides += differs(selected.Object.MaterialData.Albedo.y, prefab.MaterialData.Albedo.y) ? 1u : 0u;
            overrides += differs(selected.Object.MaterialData.Albedo.z, prefab.MaterialData.Albedo.z) ? 1u : 0u;
            overrides += differs(selected.Object.MaterialData.Roughness, prefab.MaterialData.Roughness) ? 1u : 0u;
            overrides += differs(selected.Object.MaterialData.Metallic, prefab.MaterialData.Metallic) ? 1u : 0u;
            overrides += differs(selected.Object.MaterialData.Alpha, prefab.MaterialData.Alpha) ? 1u : 0u;

            ImGui::Text("Beacon overrides: %u", overrides);
            ImGui::Text("Dependency: Assets/Prefabs/Beacon.dprefab");
            if (ImGui::Button("Apply To Beacon Prefab##InspectorPrefab"))
            {
                SaveSelectedPrefab("Assets/Prefabs/Beacon.dprefab");
                WatchAssets();
            }
            ImGui::SameLine();
            if (ImGui::Button("Revert From Beacon##InspectorPrefab"))
            {
                RevertSelectedFromPrefab("Assets/Prefabs/Beacon.dprefab");
            }
            ImGui::TreePop();
        }

        void DrawAssetsPanel()
        {
            ImGui::SetNextWindowPos(ImVec2(656.0f, 32.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(340.0f, 400.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Assets"))
            {
                ImGui::End();
                return;
            }

            ImGui::Checkbox("Hot reload assets", &m_hotReloadEnabled);
            ImGui::SameLine();
            if (ImGui::Button("Rescan Database"))
            {
                WatchAssets();
                SetStatus("Rescanned asset database");
            }
            if (ImGui::Button("Reload Scene + Script"))
            {
                ReloadSceneAndScript();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Runtime Scene"))
            {
                SaveRuntimeScene();
            }

            const bool canPrefab = !m_selectedPlayer && m_scene.Count() > 0 && m_selectedIndex < m_scene.Count();
            if (ImGui::Button("Apply Selection To Beacon Prefab") && canPrefab)
            {
                SaveSelectedPrefab("Assets/Prefabs/Beacon.dprefab");
                WatchAssets();
            }
            if (ImGui::Button("Save Selection As Runtime Prefab") && canPrefab)
            {
                const std::filesystem::path path = std::filesystem::path("Saved") / (m_scene.GetObjects()[m_selectedIndex].Name + ".dprefab");
                SaveSelectedPrefab(path);
            }

            ImGui::SeparatorText("Asset Database");
            const auto dependencyGraph = m_assetDatabase.BuildDependencyGraph();
            size_t dependencyEdges = 0;
            for (const auto& [path, dependencies] : dependencyGraph)
            {
                (void)path;
                dependencyEdges += dependencies.size();
            }
            ImGui::Text("Assets: %zu", m_assetDatabase.GetRecords().size());
            ImGui::SameLine();
            ImGui::Text("Dirty: %zu", m_assetDatabase.DirtyCount());
            ImGui::Text("Dependency nodes: %zu edges: %zu", dependencyGraph.size(), dependencyEdges);
            if (ImGui::Button("Cook Dirty Assets"))
            {
                const size_t cooked = m_assetDatabase.CookDirtyAssets();
                WatchAssets();
                SetStatus("Cooked " + std::to_string(cooked) + " asset metadata file(s)");
            }
            ImGui::SameLine();
            if (ImGui::Button("Export glTF Materials"))
            {
                ExportGltfMaterialAssets();
            }

            const std::map<Disparity::AssetKind, size_t> assetCounts = m_assetDatabase.CountByKind();
            for (const auto& [kind, count] : assetCounts)
            {
                ImGui::Text("%s: %zu", Disparity::AssetDatabase::KindToString(kind), count);
            }

            if (ImGui::BeginTable("AssetTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 145.0f)))
            {
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 48.0f);
                ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Path");
                ImGui::TableSetupColumn("Deps", ImGuiTableColumnFlags_WidthFixed, 44.0f);
                ImGui::TableHeadersRow();

                for (const Disparity::AssetRecord& record : m_assetDatabase.GetRecords())
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(record.Dirty ? "Dirty" : "Cooked");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(Disparity::AssetDatabase::KindToString(record.Kind));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(record.Path.string().c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", record.Dependencies.size());
                }

                ImGui::EndTable();
            }

            ImGui::SeparatorText("glTF Metadata");
            ImGui::Text("Meshes: %zu", m_gltfSceneAsset.Meshes.size());
            ImGui::Text("Primitives: %zu", m_gltfSceneAsset.MeshPrimitives.size());
            ImGui::Text("Materials: %zu", m_gltfSceneAsset.Materials.size());
            ImGui::Text("Nodes: %zu", m_gltfSceneAsset.Nodes.size());
            ImGui::Text("Skins: %zu", m_gltfSceneAsset.Skins.size());
            ImGui::Text("Animations: %zu", m_gltfSceneAsset.Animations.size());
            ImGui::Checkbox("glTF animation playback", &m_gltfAnimationPlayback);
            if (!m_gltfSceneAsset.Animations.empty())
            {
                const Disparity::GltfAnimationClipInfo& clip = m_gltfSceneAsset.Animations.front();
                ImGui::Text("Clip: %s", clip.Name.c_str());
                ImGui::Text("Samplers: %zu", clip.Samplers.size());
                ImGui::Text("Channels: %zu", clip.Channels.size());
            }

            ImGui::End();
        }

        void DrawRendererPanel()
        {
            ImGui::SetNextWindowPos(ImVec2(656.0f, 344.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(340.0f, 420.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Renderer"))
            {
                ImGui::End();
                return;
            }

            Disparity::RendererSettings settings = m_renderer->GetSettings();
            const EditState before = CaptureEditState();
            bool changed = false;
            changed |= ImGui::Checkbox("VSync", &settings.VSync);
            changed |= ImGui::Checkbox("Tone mapping", &settings.ToneMapping);
            changed |= ImGui::Checkbox("Shadow maps", &settings.Shadows);
            changed |= ImGui::Checkbox("CSM coverage", &settings.CascadedShadows);
            changed |= ImGui::Checkbox("Clustered lights", &settings.ClusteredLighting);
            changed |= ImGui::Checkbox("Bloom", &settings.Bloom);
            changed |= ImGui::Checkbox("SSAO", &settings.SSAO);
            changed |= ImGui::Checkbox("Anti-aliasing", &settings.AntiAliasing);
            changed |= ImGui::Checkbox("Temporal AA", &settings.TemporalAA);
            changed |= ImGui::Checkbox("Depth of field", &settings.DepthOfField);
            changed |= ImGui::Checkbox("Lens dirt pass", &settings.LensDirt);
            changed |= ImGui::Checkbox("Cinematic overlay", &settings.CinematicOverlay);
            changed |= ImGui::SliderFloat("Exposure", &settings.Exposure, 0.1f, 4.0f);
            changed |= ImGui::SliderFloat("Shadow strength", &settings.ShadowStrength, 0.0f, 0.95f);
            changed |= ImGui::SliderFloat("Bloom strength", &settings.BloomStrength, 0.0f, 1.25f);
            changed |= ImGui::SliderFloat("Bloom threshold", &settings.BloomThreshold, 0.05f, 2.0f);
            changed |= ImGui::SliderFloat("SSAO strength", &settings.SsaoStrength, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("AA strength", &settings.AntiAliasingStrength, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("TAA blend", &settings.TemporalBlend, 0.0f, 0.35f);
            changed |= ImGui::SliderFloat("Saturation", &settings.ColorSaturation, 0.0f, 2.0f);
            changed |= ImGui::SliderFloat("Contrast", &settings.ColorContrast, 0.0f, 2.0f);
            changed |= ImGui::SliderFloat("DOF focus", &settings.DepthOfFieldFocus, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("DOF range", &settings.DepthOfFieldRange, 0.001f, 0.25f);
            changed |= ImGui::SliderFloat("DOF strength", &settings.DepthOfFieldStrength, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Lens dirt strength", &settings.LensDirtStrength, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Vignette", &settings.VignetteStrength, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Letterbox", &settings.LetterboxAmount, 0.0f, 0.25f);
            changed |= ImGui::SliderFloat("Title safe", &settings.TitleSafeOpacity, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Film grain", &settings.FilmGrainStrength, 0.0f, 0.25f);

            if (ImGui::Button(m_showcaseMode ? "Stop Showcase" : "Start Showcase"))
            {
                SetShowcaseMode(!m_showcaseMode);
            }
            ImGui::SameLine();
            if (ImGui::Button(m_trailerMode ? "Stop Trailer" : "Start Trailer"))
            {
                SetTrailerMode(!m_trailerMode);
            }

            const char* debugViews[] = { "Final", "Bloom", "SSAO mask", "AA edges", "Depth", "DOF", "Lens dirt" };
            int postDebugView = static_cast<int>(settings.PostDebugView);
            if (ImGui::Combo("Post debug", &postDebugView, debugViews, IM_ARRAYSIZE(debugViews)))
            {
                settings.PostDebugView = static_cast<uint32_t>(postDebugView);
                changed = true;
            }

            if (changed)
            {
                PushUndoState(before, "Renderer Settings");
                m_renderer->SetSettings(settings);
            }

            ImGui::Text("Shadow map: %u", settings.ShadowMapSize);
            ImGui::Text("Point lights: 8");
            ImGui::End();
        }

        void DrawShotDirectorPanel()
        {
            ImGui::SetNextWindowPos(ImVec2(1008.0f, 32.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(380.0f, 360.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Shot Director"))
            {
                ImGui::End();
                return;
            }

            const float duration = m_trailerKeys.empty() ? 0.0f : std::max(0.1f, m_trailerKeys.back().Time);
            float scrubTime = std::clamp(m_trailerTime, 0.0f, duration);
            if (ImGui::SliderFloat("Scrub", &scrubTime, 0.0f, duration))
            {
                m_trailerTime = scrubTime;
                UpdateTrailerCamera(0.0f);
            }
            ImGui::Text("Keys: %zu", m_trailerKeys.size());
            ImGui::SameLine();
            if (ImGui::Button("Reload##ShotDirector"))
            {
                LoadTrailerShotAsset();
                SetStatus("Reloaded shot track");
            }
            ImGui::SameLine();
            if (ImGui::Button("Save##ShotDirector"))
            {
                const bool saved = SaveTrailerShotAsset("Assets/Cinematics/Showcase.dshot");
                WatchAssets();
                SetStatus(saved ? "Saved shot track" : "Shot track save failed");
            }

            if (ImGui::Button("Add Key##ShotDirector"))
            {
                TrailerShotKey key;
                key.Time = duration + 1.2f;
                key.Position = GetRenderCamera().GetPosition();
                key.Target = m_riftPosition;
                key.Focus = m_trailerFocus;
                key.DofStrength = m_trailerDofStrength;
                key.LensDirt = m_trailerLensDirt;
                key.Letterbox = m_trailerLetterbox;
                m_trailerKeys.push_back(key);
                SetStatus("Added shot key");
            }
            ImGui::SameLine();
            if (ImGui::Button("Capture Camera##ShotDirector"))
            {
                TrailerShotKey key;
                key.Time = scrubTime;
                key.Position = GetRenderCamera().GetPosition();
                key.Target = m_editorCameraEnabled ? m_editorCameraTarget : m_riftPosition;
                key.Focus = m_trailerFocus;
                key.DofStrength = m_trailerDofStrength;
                key.LensDirt = m_trailerLensDirt;
                key.Letterbox = m_trailerLetterbox;
                m_trailerKeys.push_back(key);
                SetStatus("Captured current camera as shot key");
            }

            if (ImGui::BeginTable("ShotKeyTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 205.0f)))
            {
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 62.0f);
                ImGui::TableSetupColumn("Position");
                ImGui::TableSetupColumn("Target");
                ImGui::TableSetupColumn("Lens", ImGuiTableColumnFlags_WidthFixed, 94.0f);
                ImGui::TableSetupColumn("Track", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                ImGui::TableHeadersRow();

                for (size_t index = 0; index < m_trailerKeys.size(); ++index)
                {
                    TrailerShotKey& key = m_trailerKeys[index];
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("Time", &key.Time, 0.03f, 0.0f, 120.0f, "%.2f");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat3("Position", &key.Position.x, 0.05f);
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat3("Target", &key.Target.x, 0.05f);
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("Focus", &key.Focus, 0.001f, 0.0f, 1.0f, "%.3f");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("DOF", &key.DofStrength, 0.01f, 0.0f, 1.0f, "%.2f");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("Dirt", &key.LensDirt, 0.01f, 0.0f, 1.0f, "%.2f");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("Bars", &key.Letterbox, 0.002f, 0.0f, 0.25f, "%.3f");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("Ease In", &key.EaseIn, 0.01f, 0.0f, 1.0f, "%.2f");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("Ease Out", &key.EaseOut, 0.01f, 0.0f, 1.0f, "%.2f");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("Pulse", &key.RendererPulse, 0.01f, 0.0f, 1.0f, "%.2f");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("Cue", &key.AudioCue, 0.01f, 0.0f, 1.0f, "%.2f");
                    if (!key.Bookmark.empty())
                    {
                        ImGui::TextUnformatted(key.Bookmark.c_str());
                    }
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            if (!m_trailerKeys.empty() && ImGui::Button("Duplicate Last##ShotDirector"))
            {
                TrailerShotKey key = m_trailerKeys.back();
                key.Time += 0.8f;
                m_trailerKeys.push_back(key);
            }
            ImGui::SameLine();
            if (!m_trailerKeys.empty() && ImGui::Button("Delete Last##ShotDirector"))
            {
                m_trailerKeys.pop_back();
            }

            ImGui::End();
        }

        void DrawAudioPanel()
        {
            ImGui::SetNextWindowPos(ImVec2(1008.0f, 324.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(340.0f, 300.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Audio Mixer"))
            {
                ImGui::End();
                return;
            }

            float masterVolume = Disparity::AudioSystem::GetMasterVolume();
            if (ImGui::SliderFloat("Master", &masterVolume, 0.0f, 1.0f))
            {
                Disparity::AudioSystem::SetMasterVolume(masterVolume);
            }
            Disparity::AudioBackendInfo backendInfo = Disparity::AudioSystem::GetBackendInfo();
            ImGui::Text("Backend: %s", backendInfo.ActiveBackend.c_str());
            ImGui::Text("XAudio2 runtime: %s", backendInfo.XAudio2Available ? "available" : "not found");
            const Disparity::AudioAnalysis analysis = Disparity::AudioSystem::GetAnalysis();
            ImGui::Text("Analysis: peak %.2f  RMS %.2f  voices %u",
                analysis.Peak,
                analysis.Rms,
                analysis.ActiveVoices);
            ImGui::ProgressBar(std::clamp(analysis.BeatEnvelope, 0.0f, 1.0f), ImVec2(-FLT_MIN, 0.0f), "Beat envelope");
            bool preferXAudio2 = backendInfo.XAudio2Preferred;
            if (ImGui::Checkbox("Prefer XAudio2", &preferXAudio2))
            {
                Disparity::AudioSystem::PreferXAudio2(preferXAudio2);
            }
            ImGui::Checkbox("Cinematic cue tones", &m_cinematicAudioCues);

            if (ImGui::Button("Low Tone"))
            {
                Disparity::AudioSystem::PlayToneOnBus("SFX", 330.0f, 0.22f, 1.0f);
            }
            ImGui::SameLine();
            if (ImGui::Button("High Tone"))
            {
                Disparity::AudioSystem::PlayToneOnBus("UI", 880.0f, 0.18f, 0.8f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Spatial"))
            {
                Disparity::AudioSystem::PlaySpatialTone("SFX", 520.0f, 0.35f, 1.0f, Add(m_playerPosition, { 4.0f, 1.2f, 0.0f }));
            }

            ImGui::SeparatorText("Buses");
            for (Disparity::AudioBus bus : Disparity::AudioSystem::GetBuses())
            {
                bool muted = bus.Muted;
                float volume = bus.Volume;
                float send = bus.Send;
                ImGui::PushID(bus.Name.c_str());
                if (ImGui::Checkbox("M", &muted))
                {
                    Disparity::AudioSystem::SetBusMuted(bus.Name, muted);
                }
                ImGui::SameLine();
                if (ImGui::SliderFloat(bus.Name.c_str(), &volume, 0.0f, 1.0f))
                {
                    Disparity::AudioSystem::SetBusVolume(bus.Name, volume);
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(72.0f);
                if (ImGui::SliderFloat("Send", &send, 0.0f, 1.0f, "%.2f"))
                {
                    Disparity::AudioSystem::SetBusSend(bus.Name, send);
                }
                ImGui::PopID();
            }

            ImGui::SeparatorText("Meters");
            for (const Disparity::AudioBusMeter& meter : Disparity::AudioSystem::GetMeters())
            {
                ImGui::Text("%s voices %u", meter.BusName.c_str(), meter.ActiveVoices);
                ImGui::SameLine();
                ImGui::ProgressBar(std::clamp(meter.Peak, 0.0f, 1.0f), ImVec2(120.0f, 0.0f));
            }

            if (ImGui::Button("Store Snapshot"))
            {
                m_audioSnapshot = Disparity::AudioSystem::CaptureSnapshot();
                SetStatus("Stored audio mixer snapshot");
            }
            ImGui::SameLine();
            if (ImGui::Button("Recall") && m_audioSnapshot.has_value())
            {
                Disparity::AudioSystem::ApplySnapshot(*m_audioSnapshot);
                SetStatus("Recalled audio mixer snapshot");
            }

            ImGui::TextDisabled("WinMM prototype backend with production mixer-facing controls");
            ImGui::End();
        }

        void DrawProfilerPanel()
        {
            ImGui::SetNextWindowPos(ImVec2(1008.0f, 32.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(300.0f, 280.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Profiler"))
            {
                ImGui::End();
                return;
            }

            const Disparity::ProfileSnapshot snapshot = Disparity::Profiler::GetSnapshot();
            ImGui::Text("%s %s (%s)", Disparity::Version::Name, Disparity::Version::ToString().c_str(), Disparity::Version::BuildConfiguration());
            ImGui::Text("FPS: %.1f", snapshot.FramesPerSecond);
            ImGui::Text("Frame: %.2f ms", snapshot.FrameMilliseconds);
            ImGui::Text("Job workers: %u", Disparity::JobSystem::WorkerCount());
            if (m_renderer)
            {
                ImGui::Text("Draw calls: %u total / %u scene / %u shadow",
                    m_renderer->GetFrameDrawCalls(),
                    m_renderer->GetSceneDrawCalls(),
                    m_renderer->GetShadowDrawCalls());
                if (m_renderer->IsGpuTimingAvailable())
                {
                    ImGui::Text("GPU frame: %.3f ms", m_renderer->GetGpuFrameMilliseconds());
                }
                else
                {
                    ImGui::TextDisabled("GPU frame timing: warming up");
                }
            }
            ImGui::Separator();
            for (const Disparity::ProfileRecord& record : snapshot.Records)
            {
                ImGui::Text("%s: %.3f ms", record.Name.c_str(), record.Milliseconds);
            }

            if (m_renderer && ImGui::TreeNode("Render Graph"))
            {
                const Disparity::RenderGraph& graph = m_renderer->GetRenderGraph();
                const auto findResourceName = [&graph](uint32_t resourceId) -> const char* {
                    for (const Disparity::RenderGraphResource& resource : graph.GetResources())
                    {
                        if (resource.Id == resourceId)
                        {
                            return resource.Name.c_str();
                        }
                    }
                    return "Unknown";
                };

                const Disparity::RendererFrameGraphDiagnostics diagnostics = m_renderer->GetFrameGraphDiagnostics();
                ImGui::Text("Resources: %zu", graph.GetResources().size());
                ImGui::Text("Scheduled passes: %zu", graph.GetExecutionOrder().size());
                ImGui::Text("Callbacks: %u/%u  Barriers: %u  Handles: %u",
                    diagnostics.GraphCallbacksExecuted,
                    diagnostics.GraphCallbacksBound,
                    diagnostics.TransitionBarriers,
                    diagnostics.ResourceHandles);
                ImGui::Text("Pending captures: %u", diagnostics.PendingCaptureRequests);
                for (const uint32_t passId : graph.GetExecutionOrder())
                {
                    if (passId >= graph.GetPasses().size())
                    {
                        continue;
                    }

                    const Disparity::RenderGraphPass& pass = graph.GetPasses()[passId];
                    ImGui::BulletText("#%u %s  CPU %.3f ms  GPU %.3f ms  R:%zu W:%zu",
                        pass.ExecutionOrder,
                        pass.Name.c_str(),
                        pass.LastCpuMilliseconds,
                        pass.LastGpuMilliseconds,
                        pass.Reads.size(),
                        pass.Writes.size());
                }

                if (ImGui::TreeNode("Culled Passes"))
                {
                    for (const Disparity::RenderGraphPass& pass : graph.GetPasses())
                    {
                        if (pass.Culled)
                        {
                            ImGui::BulletText("%s: %s", pass.Name.c_str(), pass.CullReason.c_str());
                        }
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Resource Transitions"))
                {
                    for (const Disparity::RenderGraphBarrier& barrier : graph.GetBarriers())
                    {
                        const char* fromPassName = barrier.FromPass < graph.GetPasses().size() ? graph.GetPasses()[barrier.FromPass].Name.c_str() : "External";
                        const char* toPassName = barrier.ToPass < graph.GetPasses().size() ? graph.GetPasses()[barrier.ToPass].Name.c_str() : "External";
                        ImGui::BulletText(
                            "%s: %s %s -> %s %s",
                            findResourceName(barrier.ResourceId),
                            fromPassName,
                            barrier.FromAccess.c_str(),
                            toPassName,
                            barrier.ToAccess.c_str());
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Resource Lifetimes"))
                {
                    for (const Disparity::RenderGraphResourceLifetime& lifetime : graph.GetResourceLifetimes())
                    {
                        ImGui::BulletText(
                            "%s: pass %u -> %u%s",
                            findResourceName(lifetime.ResourceId),
                            lifetime.FirstPass,
                            lifetime.LastPass,
                            lifetime.External ? " external" : "");
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Alias Candidates"))
                {
                    for (const Disparity::RenderGraphAliasCandidate& alias : graph.GetAliasCandidates())
                    {
                        ImGui::BulletText("%s <-> %s", findResourceName(alias.FirstResourceId), findResourceName(alias.SecondResourceId));
                    }
                    ImGui::TreePop();
                }

                const std::vector<std::string> validation = graph.Validate();
                ImGui::Text("Validation: %s", validation.empty() ? "OK" : "Issues");
                for (const std::string& issue : validation)
                {
                    ImGui::BulletText("%s", issue.c_str());
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Command History"))
            {
                for (const std::string& command : m_commandHistory)
                {
                    ImGui::BulletText("%s", command.c_str());
                }
                ImGui::TreePop();
            }

            ImGui::End();
        }

        void CycleSelection()
        {
            if (m_scene.Count() == 0)
            {
                m_selectedIndex = 0;
                return;
            }

            m_selectedIndex = (m_selectedIndex + 1) % m_scene.Count();
            m_selectedPlayer = false;
            m_multiSelection.clear();
        }

        void ReloadSceneAndScript()
        {
            const bool sceneLoaded = m_scene.Load("Assets/Scenes/Prototype.dscene", m_meshes);
            const bool scriptLoaded = Disparity::ScriptRunner::RunSceneScript("Assets/Scripts/Prototype.dscript", m_scene, m_meshes);
            AppendImportedGltfSceneObjects();
            BuildRuntimeRegistry();
            m_multiSelection.clear();
            SetStatus(sceneLoaded && scriptLoaded ? "Reloaded scene + script" : "Reload attempted; check asset paths");
        }

        void SaveRuntimeScene()
        {
            const bool saved = m_scene.Save("Saved/PrototypeRuntime.dscene");
            SetStatus(saved ? "Saved runtime scene snapshot" : "Scene save failed");
        }

        void ExportGltfMaterialAssets()
        {
            size_t savedCount = 0;
            for (size_t index = 0; index < m_gltfSceneAsset.Materials.size(); ++index)
            {
                const Disparity::GltfMaterialInfo& gltfMaterial = m_gltfSceneAsset.Materials[index];
                Disparity::MaterialAsset materialAsset;
                materialAsset.Name = gltfMaterial.Name.empty() ? "GltfMaterial_" + std::to_string(index) : gltfMaterial.Name;
                materialAsset.MaterialData = gltfMaterial.MaterialData;
                materialAsset.BaseColorTexturePath = gltfMaterial.BaseColorTexturePath;
                materialAsset.NormalTexturePath = gltfMaterial.NormalTexturePath;
                materialAsset.MetallicRoughnessTexturePath = gltfMaterial.MetallicRoughnessTexturePath;
                materialAsset.EmissiveTexturePath = gltfMaterial.EmissiveTexturePath;
                materialAsset.OcclusionTexturePath = gltfMaterial.OcclusionTexturePath;

                const std::filesystem::path path = std::filesystem::path("Assets") / "Materials" / "Imported" / (SafeFileStem(materialAsset.Name) + ".dmat");
                if (Disparity::MaterialAssetIO::Save(path, materialAsset))
                {
                    ++savedCount;
                }
            }

            WatchAssets();
            SetStatus("Exported " + std::to_string(savedCount) + " glTF material asset(s)");
        }

        void UpdateStatusTimer(float dt)
        {
            if (m_statusTimer > 0.0f)
            {
                m_statusTimer = std::max(0.0f, m_statusTimer - dt);
            }
        }

        void SetStatus(std::string message)
        {
            m_statusMessage = std::move(message);
            m_statusTimer = 2.5f;
        }

        void AddRuntimeVerificationNote(std::string note)
        {
            m_runtimeVerificationNotes.push_back(std::move(note));
        }

        void AddRuntimeVerificationFailure(std::string failure)
        {
            if (std::find(m_runtimeVerificationFailures.begin(), m_runtimeVerificationFailures.end(), failure) == m_runtimeVerificationFailures.end())
            {
                m_runtimeVerificationFailures.push_back(std::move(failure));
            }
        }

        void UpdateRuntimeVerification(float dt)
        {
            if (!m_runtimeVerification.Enabled || m_runtimeVerificationFinished)
            {
                return;
            }

            ++m_runtimeVerificationFrame;
            m_runtimeVerificationElapsed += dt;

            ApplyRuntimeVerificationInputPlayback();
            CollectRuntimeBudgetStats();
            ExerciseRuntimeVerification();
            if (m_runtimeVerificationFrame >= m_runtimeVerification.TargetFrames || m_runtimeVerificationElapsed >= 20.0f)
            {
                CompleteRuntimeVerification();
            }
        }

        void ExerciseRuntimeVerification()
        {
            if (m_runtimeVerificationFrame == 2)
            {
                ValidateRuntimeVerificationState("startup");
            }

            if (!m_runtimeVerificationReloadedScene && m_runtimeVerificationFrame >= 8)
            {
                ReloadSceneAndScript();
                ++m_runtimeEditorStats.SceneReloads;
                AddRuntimeVerificationNote("Reloaded scene and script.");
                m_runtimeVerificationReloadedScene = true;
            }

            if (!m_runtimeVerificationSavedScene && m_runtimeVerificationFrame >= 14)
            {
                SaveRuntimeScene();
                if (!std::filesystem::exists("Saved/PrototypeRuntime.dscene"))
                {
                    AddRuntimeVerificationFailure("Runtime scene snapshot was not written.");
                }
                ++m_runtimeEditorStats.SceneSaves;
                AddRuntimeVerificationNote("Saved runtime scene snapshot.");
                m_runtimeVerificationSavedScene = true;
            }

            if (!m_runtimeVerificationExercisedRenderer && m_runtimeVerificationFrame >= 20 && m_renderer)
            {
                m_runtimeVerificationOriginalRendererSettings = m_renderer->GetSettings();
                Disparity::RendererSettings settings = *m_runtimeVerificationOriginalRendererSettings;
                for (uint32_t debugView = 0; debugView <= 6; ++debugView)
                {
                    settings.PostDebugView = debugView;
                    settings.Bloom = debugView == 1 || !m_runtimeVerificationOriginalRendererSettings->Bloom;
                    settings.SSAO = debugView == 2 || !m_runtimeVerificationOriginalRendererSettings->SSAO;
                    settings.AntiAliasing = debugView == 3 || !m_runtimeVerificationOriginalRendererSettings->AntiAliasing;
                    settings.DepthOfField = debugView == 5 || m_runtimeVerificationOriginalRendererSettings->DepthOfField;
                    settings.LensDirt = debugView == 6 || m_runtimeVerificationOriginalRendererSettings->LensDirt;
                    m_renderer->SetSettings(settings);
                    ++m_runtimeEditorStats.PostDebugViews;
                }
                m_renderer->SetSettings(*m_runtimeVerificationOriginalRendererSettings);
                AddRuntimeVerificationNote("Cycled renderer post debug settings and restored defaults.");
                m_runtimeVerificationExercisedRenderer = true;
            }

            if (!m_runtimeVerificationCycledSelection && m_runtimeVerificationFrame >= 26)
            {
                CycleSelection();
                AddRuntimeVerificationNote("Cycled editor selection.");
                m_runtimeVerificationCycledSelection = true;
            }

            if (!m_runtimeVerificationValidatedEditorPrecision && m_runtimeVerificationFrame >= 32)
            {
                ValidateRuntimeEditorPrecision();
                ValidateRuntimeGizmoConstraints();
                ValidateRuntimeAudioSnapshot();
                ValidateRuntimeV20ProductionBatch();
                m_runtimeVerificationValidatedEditorPrecision = true;
            }

            if (!m_runtimeVerificationRequestedHighResCapture && m_runtimeVerificationFrame >= 34)
            {
                RequestHighResolutionCapture();
                m_runtimeVerificationRequestedHighResCapture = true;
                AddRuntimeVerificationNote("Requested high-resolution trailer/photo capture.");
            }

            if (!m_runtimeVerificationStartedShowcase && m_runtimeVerificationFrame >= 38)
            {
                SetShowcaseMode(true);
                m_runtimeVerificationStartedShowcase = true;
                AddRuntimeVerificationNote("Started cinematic showcase mode.");
            }

            if (m_showcaseMode)
            {
                ++m_runtimeEditorStats.ShowcaseFrames;
            }

            constexpr uint32_t showcaseStopFrame = 52u;
            if (m_runtimeVerificationStartedShowcase && !m_runtimeVerificationStoppedShowcase && m_runtimeVerificationFrame >= showcaseStopFrame)
            {
                SetShowcaseMode(false);
                m_runtimeVerificationStoppedShowcase = true;
                AddRuntimeVerificationNote("Stopped cinematic showcase mode and restored renderer settings.");
            }

            if (!m_runtimeVerificationStartedTrailer && m_runtimeVerificationFrame >= 54)
            {
                SetTrailerMode(true);
                m_runtimeVerificationStartedTrailer = true;
                m_runtimeVerificationTrailerStartFrame = m_runtimeVerificationFrame;
                AddRuntimeVerificationNote("Started authored trailer/photo shot playback.");
            }

            const uint32_t trailerStopFrame = std::max(72u, m_runtimeVerification.TargetFrames > 12u ? m_runtimeVerification.TargetFrames - 12u : 72u);
            if (m_runtimeVerificationStartedTrailer && !m_runtimeVerificationStoppedTrailer && m_runtimeVerificationFrame >= trailerStopFrame)
            {
                SetTrailerMode(false);
                m_runtimeVerificationStoppedTrailer = true;
                AddRuntimeVerificationNote("Stopped trailer/photo shot playback and restored renderer settings.");
            }

            const uint32_t captureRequestFrame = m_runtimeVerification.TargetFrames > 6u ? m_runtimeVerification.TargetFrames - 4u : m_runtimeVerification.TargetFrames;
            if (m_runtimeVerification.CaptureFrame && !m_runtimeVerificationCaptureRequested && m_runtimeVerificationFrame >= captureRequestFrame && m_renderer)
            {
                m_renderer->RequestFrameCapture(m_runtimeVerification.CapturePath);
                AddRuntimeVerificationNote("Requested frame capture.");
                m_runtimeVerificationCaptureRequested = true;
            }

            if (m_runtimeVerification.CaptureFrame && m_runtimeVerificationCaptureRequested && !m_runtimeVerificationCaptureValidated && m_renderer && m_renderer->HasLastFrameCapture())
            {
                ValidateRuntimeFrameCapture();
            }
        }

        void ApplyRuntimeVerificationInputPlayback()
        {
            if (!m_runtimeVerification.InputPlayback)
            {
                return;
            }

            if (m_runtimeReplaySteps.empty())
            {
                return;
            }

            if (!m_runtimePlayback.Started && m_runtimeVerificationFrame >= m_runtimeReplayStartFrame)
            {
                m_runtimePlayback.Started = true;
                m_runtimePlayback.StartPosition = m_playerPosition;
                m_runtimePlayback.LastPosition = m_playerPosition;
                AddRuntimeVerificationNote("Started deterministic input playback.");
            }

            if (!m_runtimePlayback.Started || m_runtimePlayback.Finished)
            {
                return;
            }

            if (m_runtimeVerificationFrame > m_runtimeReplayEndFrame)
            {
                FinishRuntimeReplayPlayback();
                return;
            }

            const RuntimeReplayStep* activeStep = nullptr;
            for (const RuntimeReplayStep& step : m_runtimeReplaySteps)
            {
                if (m_runtimeVerificationFrame >= step.StartFrame && m_runtimeVerificationFrame <= step.EndFrame)
                {
                    activeStep = &step;
                    break;
                }
            }

            if (!activeStep)
            {
                return;
            }

            constexpr float playbackDeltaSeconds = 1.0f / 60.0f;
            const DirectX::XMFLOAT3 previousPosition = m_playerPosition;
            MovePlayerWithInput(activeStep->MoveInput, playbackDeltaSeconds);
            m_runtimePlayback.Distance += Length(Subtract(m_playerPosition, previousPosition));
            m_runtimePlayback.LastPosition = m_playerPosition;
            m_playerBobOffset = m_playerBob.SampleOffset(playbackDeltaSeconds);
            SyncPlayerTransformToRegistry();
            m_cameraYaw += activeStep->CameraYawDelta;
            m_cameraPitch = std::clamp(m_cameraPitch + activeStep->CameraPitchDelta, -0.15f, 0.95f);
            UpdateCamera();
            ++m_runtimePlayback.Steps;
        }

        void FinishRuntimeReplayPlayback()
        {
            if (m_runtimePlayback.Finished)
            {
                return;
            }

            m_runtimePlayback.Finished = true;
            m_runtimePlayback.EndPosition = m_playerPosition;
            m_runtimePlayback.NetDistance = Length(Subtract(m_runtimePlayback.EndPosition, m_runtimePlayback.StartPosition));
            const double minimumDistance = m_runtimeBaselineLoaded ? m_runtimeBaseline.MinPlaybackDistance : 1.0;
            if (m_runtimePlayback.Distance < minimumDistance)
            {
                AddRuntimeVerificationFailure("input replay path distance is less than expected.");
            }
            AddRuntimeVerificationNote("Finished deterministic input replay.");
        }

        void ValidateRuntimeEditorPrecision()
        {
            RefreshEditorViewportState();
            const bool previousEditorCameraEnabled = m_editorCameraEnabled;
            const bool previousSelectedPlayer = m_selectedPlayer;
            const size_t previousSelectedIndex = m_selectedIndex;
            const std::vector<size_t> previousMultiSelection = m_multiSelection;
            const float previousEditorDistance = m_editorCameraDistance;
            const DirectX::XMFLOAT3 previousEditorTarget = m_editorCameraTarget;
            const GizmoMode previousGizmoMode = m_gizmoMode;

            m_editorCameraEnabled = true;
            m_editorCameraDistance = 10.0f;
            m_editorCameraTarget = Add(m_playerPosition, { 0.0f, 1.1f, 0.0f });
            UpdateEditorCamera(0.0f, true, true);

            auto testPickAt = [this](const DirectX::XMFLOAT2& screen, EditorPickResult& outPick) {
                MouseRay ray;
                if (!BuildScreenRay(screen, ray))
                {
                    return false;
                }

                return PickEditorTarget(ray, outPick);
            };

            EditorPickResult pick;
            DirectX::XMFLOAT2 playerScreen = ProjectWorldToScreen(Add(m_playerPosition, { 0.0f, 1.1f, 0.0f }));
            if (IsScreenPointInsideViewport(playerScreen) && testPickAt(playerScreen, pick))
            {
                if (pick.Kind == EditorPickKind::Player)
                {
                    ++m_runtimeEditorStats.ObjectPickTests;
                }
            }

            const auto& objects = m_scene.GetObjects();
            for (size_t index = 0; index < objects.size() && m_runtimeEditorStats.ObjectPickTests < 4; ++index)
            {
                if (objects[index].MeshName == "terrain")
                {
                    continue;
                }

                const DirectX::XMFLOAT2 screen = ProjectWorldToScreen(objects[index].Object.TransformData.Position);
                if (!IsScreenPointInsideViewport(screen))
                {
                    continue;
                }

                if (!testPickAt(screen, pick))
                {
                    continue;
                }

                if (pick.Kind == EditorPickKind::SceneObject && pick.SceneIndex == index && pick.StableId == PickStableIdForSceneObject(index))
                {
                    ++m_runtimeEditorStats.ObjectPickTests;
                }
            }

            m_selectedPlayer = true;
            m_multiSelection.clear();
            m_gizmoMode = GizmoMode::Translate;
            DirectX::XMFLOAT3 pivot = {};
            if (TryGetSelectionPivot(pivot))
            {
                const float handleDistance = GizmoHandleDistance(pivot);
                const std::array<GizmoAxis, 3> axes = { GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z };
                for (const GizmoAxis axis : axes)
                {
                    const DirectX::XMFLOAT2 screen = ProjectWorldToScreen(Add(pivot, Scale(AxisVector(axis), handleDistance)));
                    if (!IsScreenPointInsideViewport(screen))
                    {
                        continue;
                    }

                    MouseRay ray;
                    GizmoAxis pickedAxis = GizmoAxis::None;
                    GizmoPlane pickedPlane = GizmoPlane::None;
                    if (!BuildScreenRay(screen, ray) || !TryPickGizmoHandle(ray, pickedAxis, pickedPlane))
                    {
                        continue;
                    }

                    ++m_runtimeEditorStats.GizmoPickTests;
                    if (pickedPlane != GizmoPlane::None || pickedAxis != axis)
                    {
                        ++m_runtimeEditorStats.GizmoPickFailures;
                        AddRuntimeVerificationFailure("editor precision gizmo pick mismatch.");
                    }
                }
            }

            if (m_runtimeBaselineLoaded)
            {
                if (m_runtimeEditorStats.ObjectPickTests < m_runtimeBaseline.MinEditorPickTests)
                {
                    AddRuntimeVerificationFailure("editor precision object pick count is below baseline.");
                }
                if (m_runtimeEditorStats.GizmoPickTests < m_runtimeBaseline.MinGizmoPickTests)
                {
                    AddRuntimeVerificationFailure("editor precision gizmo pick count is below baseline.");
                }
            }
            if (m_runtimeEditorStats.ObjectPickFailures > 0 || m_runtimeEditorStats.GizmoPickFailures > 0)
            {
                AddRuntimeVerificationFailure("editor precision pick validation failed.");
            }

            m_editorCameraEnabled = previousEditorCameraEnabled;
            m_selectedPlayer = previousSelectedPlayer;
            m_selectedIndex = previousSelectedIndex;
            m_multiSelection = previousMultiSelection;
            m_editorCameraDistance = previousEditorDistance;
            m_editorCameraTarget = previousEditorTarget;
            m_gizmoMode = previousGizmoMode;
            UpdateEditorCamera(0.0f, true, true);

            AddRuntimeVerificationNote("Validated editor precision picks.");
        }

        void ValidateRuntimeGizmoConstraints()
        {
            const bool previousSelectedPlayer = m_selectedPlayer;
            const size_t previousSelectedIndex = m_selectedIndex;
            const std::vector<size_t> previousMultiSelection = m_multiSelection;

            size_t candidateIndex = InvalidIndex;
            for (size_t index = 0; index < m_scene.Count(); ++index)
            {
                if (m_scene.GetObjects()[index].MeshName != "terrain")
                {
                    candidateIndex = index;
                    break;
                }
            }

            if (candidateIndex == InvalidIndex)
            {
                AddRuntimeVerificationFailure("gizmo constraint validation found no movable scene object.");
                return;
            }

            m_selectedPlayer = false;
            m_selectedIndex = candidateIndex;
            m_multiSelection.clear();

            Disparity::NamedSceneObject& selected = m_scene.GetObjects()[candidateIndex];
            const Disparity::Transform original = selected.Object.TransformData;
            const auto closeEnough = [](float a, float b) {
                return std::abs(a - b) <= 0.0001f;
            };
            const auto failConstraint = [this](const char* name) {
                ++m_runtimeEditorStats.GizmoDragFailures;
                AddRuntimeVerificationFailure(std::string("gizmo constraint validation failed for ") + name + ".");
            };

            selected.Object.TransformData = original;
            selected.Object.TransformData.Position = Add(original.Position, Scale(AxisVector(GizmoAxis::X), 0.5f));
            SyncSceneObjectToRegistry(candidateIndex);
            ++m_runtimeEditorStats.GizmoDragTests;
            if (!closeEnough(selected.Object.TransformData.Position.x, original.Position.x + 0.5f) ||
                !closeEnough(selected.Object.TransformData.Position.y, original.Position.y) ||
                !closeEnough(selected.Object.TransformData.Position.z, original.Position.z))
            {
                failConstraint("translate X");
            }

            selected.Object.TransformData = original;
            ComponentForAxis(selected.Object.TransformData.Rotation, GizmoAxis::Y) += 0.25f;
            SyncSceneObjectToRegistry(candidateIndex);
            ++m_runtimeEditorStats.GizmoDragTests;
            if (!closeEnough(selected.Object.TransformData.Rotation.x, original.Rotation.x) ||
                !closeEnough(selected.Object.TransformData.Rotation.y, original.Rotation.y + 0.25f) ||
                !closeEnough(selected.Object.TransformData.Rotation.z, original.Rotation.z))
            {
                failConstraint("rotate Y");
            }

            selected.Object.TransformData = original;
            ComponentForAxis(selected.Object.TransformData.Scale, GizmoAxis::Z) =
                std::max(0.05f, ComponentForAxis(selected.Object.TransformData.Scale, GizmoAxis::Z) + 0.25f);
            SyncSceneObjectToRegistry(candidateIndex);
            ++m_runtimeEditorStats.GizmoDragTests;
            if (!closeEnough(selected.Object.TransformData.Scale.x, original.Scale.x) ||
                !closeEnough(selected.Object.TransformData.Scale.y, original.Scale.y) ||
                !closeEnough(selected.Object.TransformData.Scale.z, std::max(0.05f, original.Scale.z + 0.25f)))
            {
                failConstraint("scale Z");
            }

            selected.Object.TransformData = original;
            SyncSceneObjectToRegistry(candidateIndex);
            m_selectedPlayer = previousSelectedPlayer;
            m_selectedIndex = previousSelectedIndex;
            m_multiSelection = previousMultiSelection;

            if (m_runtimeBaselineLoaded && m_runtimeEditorStats.GizmoDragTests < m_runtimeBaseline.MinGizmoDragTests)
            {
                AddRuntimeVerificationFailure("gizmo constraint test count is below baseline.");
            }
            if (m_runtimeEditorStats.GizmoDragFailures > 0)
            {
                AddRuntimeVerificationFailure("gizmo constraint validation failed.");
            }

            AddRuntimeVerificationNote("Validated gizmo transform constraints.");
        }

        void ValidateRuntimeAudioSnapshot()
        {
            const Disparity::AudioSnapshot original = Disparity::AudioSystem::CaptureSnapshot();
            Disparity::AudioSnapshot edited = original;
            edited.MasterVolume = 0.37f;
            for (Disparity::AudioBus& bus : edited.Buses)
            {
                if (bus.Name == "SFX")
                {
                    bus.Volume = 0.42f;
                    bus.Send = 0.35f;
                }
            }

            Disparity::AudioSystem::ApplySnapshot(edited);
            const Disparity::AudioSnapshot captured = Disparity::AudioSystem::CaptureSnapshot();
            ++m_runtimeEditorStats.AudioSnapshotTests;
            if (std::abs(captured.MasterVolume - 0.37f) > 0.001f ||
                std::abs(Disparity::AudioSystem::GetBusVolume("SFX") - 0.42f) > 0.001f ||
                std::abs(Disparity::AudioSystem::GetBusSend("SFX") - 0.35f) > 0.001f)
            {
                AddRuntimeVerificationFailure("audio snapshot validation failed.");
            }

            Disparity::AudioSystem::ApplySnapshot(original);
            if (m_runtimeBaselineLoaded && m_runtimeEditorStats.AudioSnapshotTests < m_runtimeBaseline.MinAudioSnapshotTests)
            {
                AddRuntimeVerificationFailure("audio snapshot test count is below baseline.");
            }
            AddRuntimeVerificationNote("Validated audio mixer snapshots.");
        }

        void ValidateRuntimeV20ProductionBatch()
        {
            bool asyncSuccess = false;
            std::string asyncText;
            Disparity::JobSystem::DispatchReadTextFile(
                Disparity::FileSystem::FindAssetPath("Assets/Cinematics/Showcase.dshot"),
                [&asyncSuccess, &asyncText](Disparity::JobSystem::AsyncTextReadResult result) {
                    asyncSuccess = result.Success;
                    asyncText = std::move(result.Text);
                });
            Disparity::JobSystem::WaitIdle();
            ++m_runtimeEditorStats.AsyncIoTests;
            if (!asyncSuccess || asyncText.find("key ") == std::string::npos)
            {
                AddRuntimeVerificationFailure("async text IO validation failed for the trailer shot track.");
            }

            const std::filesystem::path materialPath = "Saved/Verification/material_slots.dmat";
            Disparity::MaterialAsset materialAsset;
            materialAsset.Name = "RuntimeTextureSlotProbe";
            materialAsset.BaseColorTexturePath = "Assets/Textures/probe_base.dds";
            materialAsset.NormalTexturePath = "Assets/Textures/probe_normal.dds";
            materialAsset.MetallicRoughnessTexturePath = "Assets/Textures/probe_mr.dds";
            materialAsset.EmissiveTexturePath = "Assets/Textures/probe_emissive.dds";
            materialAsset.OcclusionTexturePath = "Assets/Textures/probe_occlusion.dds";
            ++m_runtimeEditorStats.MaterialTextureSlotTests;
            Disparity::MaterialAsset loadedMaterialAsset;
            if (!Disparity::MaterialAssetIO::Save(materialPath, materialAsset) ||
                !Disparity::MaterialAssetIO::Load(materialPath, loadedMaterialAsset) ||
                loadedMaterialAsset.BaseColorTexturePath.generic_string() != materialAsset.BaseColorTexturePath.generic_string() ||
                loadedMaterialAsset.NormalTexturePath.generic_string() != materialAsset.NormalTexturePath.generic_string() ||
                loadedMaterialAsset.MetallicRoughnessTexturePath.generic_string() != materialAsset.MetallicRoughnessTexturePath.generic_string() ||
                loadedMaterialAsset.EmissiveTexturePath.generic_string() != materialAsset.EmissiveTexturePath.generic_string() ||
                loadedMaterialAsset.OcclusionTexturePath.generic_string() != materialAsset.OcclusionTexturePath.generic_string())
            {
                AddRuntimeVerificationFailure("material texture slot serialization validation failed.");
            }

            const std::filesystem::path prefabPath = "Saved/Verification/prefab_variant.dprefab";
            Disparity::Prefab prefabVariant;
            prefabVariant.Name = "BeaconRuntimeVariant";
            prefabVariant.MeshName = "cube";
            prefabVariant.VariantName = "NightShowcase";
            prefabVariant.ParentPrefabPath = "Assets/Prefabs/Beacon.dprefab";
            prefabVariant.NestedPrefabPaths.push_back("Assets/Prefabs/Beacon.dprefab");
            prefabVariant.TransformData.Scale = { 0.75f, 1.35f, 0.75f };
            prefabVariant.MaterialData.Albedo = { 0.24f, 0.88f, 1.0f };
            prefabVariant.MaterialData.Roughness = 0.28f;
            prefabVariant.MaterialData.Metallic = 0.18f;
            ++m_runtimeEditorStats.PrefabVariantTests;
            Disparity::Prefab loadedPrefabVariant;
            if (!Disparity::PrefabIO::Save(prefabPath, prefabVariant) ||
                !Disparity::PrefabIO::Load(prefabPath, loadedPrefabVariant) ||
                loadedPrefabVariant.VariantName != prefabVariant.VariantName ||
                loadedPrefabVariant.ParentPrefabPath.generic_string() != prefabVariant.ParentPrefabPath.generic_string() ||
                loadedPrefabVariant.NestedPrefabPaths.size() != prefabVariant.NestedPrefabPaths.size())
            {
                AddRuntimeVerificationFailure("prefab variant serialization validation failed.");
            }

            ++m_runtimeEditorStats.ShotDirectorTests;
            size_t bookmarkCount = 0;
            float maxEaseSampleDelta = 0.0f;
            for (size_t index = 1; index < m_trailerKeys.size(); ++index)
            {
                if (!m_trailerKeys[index - 1].Bookmark.empty())
                {
                    ++bookmarkCount;
                }
                const float eased = ApplyShotEasing(0.25f, m_trailerKeys[index - 1], m_trailerKeys[index]);
                maxEaseSampleDelta = std::max(maxEaseSampleDelta, std::abs(eased - 0.25f));
            }
            if (m_trailerKeys.size() < 2 || bookmarkCount == 0 || maxEaseSampleDelta <= 0.0001f)
            {
                AddRuntimeVerificationFailure("shot director easing/bookmark validation failed.");
            }

            Disparity::AudioSystem::PlayToneOnBus("UI", 440.0f, 0.04f, 0.15f);
            const Disparity::AudioAnalysis analysis = Disparity::AudioSystem::GetAnalysis();
            const Disparity::AudioBackendInfo backendInfo = Disparity::AudioSystem::GetBackendInfo();
            ++m_runtimeEditorStats.AudioAnalysisTests;
            if (backendInfo.ActiveBackend.empty() ||
                analysis.Peak < 0.0f ||
                analysis.Rms < 0.0f ||
                analysis.BeatEnvelope < 0.0f)
            {
                AddRuntimeVerificationFailure("audio analysis validation failed.");
            }

            ++m_runtimeEditorStats.VfxSystemTests;
            if (m_lastRiftVfxStats.FogCards == 0 ||
                m_lastRiftVfxStats.Particles == 0 ||
                m_lastRiftVfxStats.Ribbons == 0 ||
                m_lastRiftVfxStats.LightningArcs == 0 ||
                m_lastRiftVfxStats.SoftParticleCandidates == 0 ||
                m_lastRiftVfxStats.SortedBatches == 0)
            {
                AddRuntimeVerificationFailure("rift VFX system stats validation failed.");
            }

            Disparity::Transform blendA;
            blendA.Position = { 0.0f, 0.0f, 0.0f };
            blendA.Scale = { 1.0f, 1.0f, 1.0f };
            Disparity::Transform blendB;
            blendB.Position = { 4.0f, 2.0f, -2.0f };
            blendB.Rotation = { 0.0f, 1.0f, 0.0f };
            blendB.Scale = { 2.0f, 2.0f, 2.0f };
            const Disparity::Transform blended = Disparity::BlendTransforms(blendA, blendB, 0.25f);
            Disparity::SkinningPalette palette;
            palette.Resize(4);
            palette.MarkUploaded();
            ++m_runtimeEditorStats.AnimationSkinningTests;
            if (std::abs(blended.Position.x - 1.0f) > 0.001f ||
                std::abs(blended.Position.y - 0.5f) > 0.001f ||
                !palette.IsReady() ||
                palette.UploadGeneration != 1 ||
                palette.JointMatrices.size() != 4)
            {
                AddRuntimeVerificationFailure("animation blending/skinning palette validation failed.");
            }

            AddRuntimeVerificationNote("Validated v20 production batch systems.");
        }

        void ValidateRuntimeScenarioCoverage()
        {
            if (!m_runtimeBaselineLoaded)
            {
                return;
            }

            if (m_runtimeEditorStats.SceneReloads < m_runtimeBaseline.MinSceneReloads)
            {
                AddRuntimeVerificationFailure("scene reload count is below baseline.");
            }
            if (m_runtimeEditorStats.SceneSaves < m_runtimeBaseline.MinSceneSaves)
            {
                AddRuntimeVerificationFailure("scene save count is below baseline.");
            }
            if (m_runtimeEditorStats.PostDebugViews < m_runtimeBaseline.MinPostDebugViews)
            {
                AddRuntimeVerificationFailure("post debug view count is below baseline.");
            }
            if (m_runtimeEditorStats.ShowcaseFrames < m_runtimeBaseline.MinShowcaseFrames)
            {
                AddRuntimeVerificationFailure("showcase frame count is below baseline.");
            }
            if (m_runtimeEditorStats.TrailerFrames < m_runtimeBaseline.MinTrailerFrames)
            {
                AddRuntimeVerificationFailure("trailer frame count is below baseline.");
            }
            if (m_runtimeEditorStats.HighResCaptures < m_runtimeBaseline.MinHighResCaptures)
            {
                AddRuntimeVerificationFailure("high-resolution capture count is below baseline.");
            }
            if (m_runtimeEditorStats.RiftVfxDraws < m_runtimeBaseline.MinRiftVfxDraws)
            {
                AddRuntimeVerificationFailure("rift VFX draw count is below baseline.");
            }
            if (m_runtimeEditorStats.AudioBeatPulses < m_runtimeBaseline.MinAudioBeatPulses)
            {
                AddRuntimeVerificationFailure("audio beat pulse count is below baseline.");
            }
            if (m_runtimeEditorStats.AudioSnapshotTests < m_runtimeBaseline.MinAudioSnapshotTests)
            {
                AddRuntimeVerificationFailure("audio snapshot test count is below baseline.");
            }
            if (m_runtimeEditorStats.AsyncIoTests < m_runtimeBaseline.MinAsyncIoTests)
            {
                AddRuntimeVerificationFailure("async IO test count is below baseline.");
            }
            if (m_runtimeEditorStats.MaterialTextureSlotTests < m_runtimeBaseline.MinMaterialTextureSlotTests)
            {
                AddRuntimeVerificationFailure("material texture slot test count is below baseline.");
            }
            if (m_runtimeEditorStats.PrefabVariantTests < m_runtimeBaseline.MinPrefabVariantTests)
            {
                AddRuntimeVerificationFailure("prefab variant test count is below baseline.");
            }
            if (m_runtimeEditorStats.ShotDirectorTests < m_runtimeBaseline.MinShotDirectorTests)
            {
                AddRuntimeVerificationFailure("shot director test count is below baseline.");
            }
            if (m_runtimeEditorStats.AudioAnalysisTests < m_runtimeBaseline.MinAudioAnalysisTests)
            {
                AddRuntimeVerificationFailure("audio analysis test count is below baseline.");
            }
            if (m_runtimeEditorStats.VfxSystemTests < m_runtimeBaseline.MinVfxSystemTests)
            {
                AddRuntimeVerificationFailure("VFX system test count is below baseline.");
            }
            if (m_runtimeEditorStats.AnimationSkinningTests < m_runtimeBaseline.MinAnimationSkinningTests)
            {
                AddRuntimeVerificationFailure("animation/skinning test count is below baseline.");
            }
        }

        void CollectRuntimeBudgetStats()
        {
            if (!m_runtimeVerification.EnforceBudgets || m_runtimeVerificationFrame < 8 || m_runtimeVerificationCaptureRequested)
            {
                return;
            }
            if (m_runtimeBudgetSkipFrames > 0 || m_highResCapturePending)
            {
                if (m_runtimeBudgetSkipFrames > 0)
                {
                    --m_runtimeBudgetSkipFrames;
                }
                return;
            }

            const Disparity::ProfileSnapshot snapshot = Disparity::Profiler::GetSnapshot();
            if (snapshot.FrameMilliseconds > 0.0001)
            {
                ++m_runtimeBudgetStats.CpuSamples;
                m_runtimeBudgetStats.CpuFrameMaxMs = std::max(m_runtimeBudgetStats.CpuFrameMaxMs, snapshot.FrameMilliseconds);
                m_runtimeBudgetStats.CpuFrameAverageMs += (snapshot.FrameMilliseconds - m_runtimeBudgetStats.CpuFrameAverageMs) /
                    static_cast<double>(m_runtimeBudgetStats.CpuSamples);
            }

            if (m_renderer && m_renderer->IsGpuTimingAvailable())
            {
                const double gpuMs = m_renderer->GetGpuFrameMilliseconds();
                ++m_runtimeBudgetStats.GpuSamples;
                m_runtimeBudgetStats.GpuFrameMaxMs = std::max(m_runtimeBudgetStats.GpuFrameMaxMs, gpuMs);
                m_runtimeBudgetStats.GpuFrameAverageMs += (gpuMs - m_runtimeBudgetStats.GpuFrameAverageMs) /
                    static_cast<double>(m_runtimeBudgetStats.GpuSamples);
            }

            if (!m_renderer)
            {
                return;
            }

            for (const Disparity::RenderGraphPass& pass : m_renderer->GetRenderGraph().GetPasses())
            {
                if (pass.LastCpuMilliseconds > m_runtimeBudgetStats.PassCpuMaxMs)
                {
                    m_runtimeBudgetStats.PassCpuMaxMs = pass.LastCpuMilliseconds;
                    m_runtimeBudgetStats.PassCpuMaxName = pass.Name;
                }
                if (pass.LastGpuMilliseconds > m_runtimeBudgetStats.PassGpuMaxMs)
                {
                    m_runtimeBudgetStats.PassGpuMaxMs = pass.LastGpuMilliseconds;
                    m_runtimeBudgetStats.PassGpuMaxName = pass.Name;
                }
            }
        }

        void ValidateRuntimeBudgets()
        {
            if (!m_runtimeVerification.EnforceBudgets)
            {
                return;
            }

            if (m_runtimeBudgetStats.CpuSamples == 0)
            {
                AddRuntimeVerificationFailure("performance budget had no CPU frame samples.");
            }
            if (m_runtimeBudgetStats.CpuFrameMaxMs > m_runtimeVerification.CpuFrameBudgetMs)
            {
                AddRuntimeVerificationFailure("CPU frame budget exceeded.");
            }
            if (m_runtimeBudgetStats.PassCpuMaxMs > m_runtimeVerification.PassBudgetMs)
            {
                AddRuntimeVerificationFailure("render graph CPU pass budget exceeded.");
            }
            if (m_runtimeBudgetStats.GpuSamples > 0 && m_runtimeBudgetStats.GpuFrameMaxMs > m_runtimeVerification.GpuFrameBudgetMs)
            {
                AddRuntimeVerificationFailure("GPU frame budget exceeded.");
            }
            if (m_runtimeBudgetStats.GpuSamples > 0 && m_runtimeBudgetStats.PassGpuMaxMs > m_runtimeVerification.PassBudgetMs)
            {
                AddRuntimeVerificationFailure("render graph GPU pass budget exceeded.");
            }
        }

        void ValidateRuntimeFrameCapture()
        {
            if (!m_renderer)
            {
                return;
            }

            const Disparity::FrameCaptureResult& capture = m_renderer->GetLastFrameCapture();
            if (capture.Path.lexically_normal() != m_runtimeVerification.CapturePath.lexically_normal())
            {
                return;
            }

            m_runtimeCapture = capture;
            m_runtimeVerificationCaptureValidated = true;

            if (!capture.Success)
            {
                AddRuntimeVerificationFailure("frame capture failed: " + capture.Error);
                return;
            }

            const uint64_t pixelCount = static_cast<uint64_t>(capture.Width) * static_cast<uint64_t>(capture.Height);
            if (capture.Width != m_renderer->GetWidth() || capture.Height != m_renderer->GetHeight())
            {
                AddRuntimeVerificationFailure("frame capture dimensions do not match renderer dimensions.");
            }
            if (pixelCount == 0 || capture.NonBlackPixels < pixelCount / 20u)
            {
                AddRuntimeVerificationFailure("frame capture appears mostly blank.");
            }
            if (capture.AverageLuma < 2.0 || capture.AverageLuma > 245.0)
            {
                AddRuntimeVerificationFailure("frame capture average luminance is outside expected bounds.");
            }
            if (capture.RgbChecksum == 0)
            {
                AddRuntimeVerificationFailure("frame capture checksum is invalid.");
            }
            if (m_runtimeBaselineLoaded)
            {
                const double nonBlackRatio = pixelCount > 0
                    ? static_cast<double>(capture.NonBlackPixels) / static_cast<double>(pixelCount)
                    : 0.0;
                if (capture.Width != m_runtimeBaseline.ExpectedCaptureWidth || capture.Height != m_runtimeBaseline.ExpectedCaptureHeight)
                {
                    AddRuntimeVerificationFailure("frame capture dimensions do not match the baseline.");
                }
                if (nonBlackRatio < m_runtimeBaseline.MinNonBlackRatio)
                {
                    AddRuntimeVerificationFailure("frame capture nonblack pixel ratio is below baseline.");
                }
                if (std::abs(capture.AverageLuma - m_runtimeBaseline.ExpectedAverageLuma) > m_runtimeBaseline.AverageLumaTolerance)
                {
                    AddRuntimeVerificationFailure("frame capture luminance drifted outside baseline tolerance.");
                }
            }

            AddRuntimeVerificationNote("Validated frame capture " + capture.Path.string() + ".");
        }

        void ValidateRuntimeVerificationState(const char* stage)
        {
            if (!m_application)
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": application pointer is null.");
            }
            if (!m_renderer)
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": renderer pointer is null.");
                return;
            }

            if (m_renderer->GetWidth() == 0 || m_renderer->GetHeight() == 0)
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": renderer dimensions are invalid.");
            }
            if (m_cubeMesh == 0 || m_planeMesh == 0 || m_terrainMesh == 0 || m_gizmoRingMesh == 0 || m_gizmoPlaneHandleMesh == 0 || m_vfxQuadMesh == 0)
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": a required procedural mesh handle is missing.");
            }
            if (m_scene.Count() == 0)
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": scene has no objects.");
            }
            if (m_sceneEntities.size() != m_scene.Count())
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": ECS scene entity count does not match scene object count.");
            }
            bool importedGltfFound = false;
            bool importedGltfDoubleSided = false;
            for (const Disparity::NamedSceneObject& object : m_scene.GetObjects())
            {
                if (object.MeshName == "gltf_sample" || object.MeshName.rfind("gltf_", 0) == 0)
                {
                    importedGltfFound = true;
                    importedGltfDoubleSided = importedGltfDoubleSided || object.Object.MaterialData.DoubleSided;
                }
            }
            if (!importedGltfFound)
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": imported glTF triangle is missing from the scene.");
            }
            else if (!importedGltfDoubleSided)
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": imported glTF triangle is not marked double-sided.");
            }
            if (!std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Shaders/Basic.hlsl")) ||
                !std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Shaders/PostProcess.hlsl")))
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": required shader assets are missing.");
            }
            if (!std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Scenes/Prototype.dscene")) ||
                !std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Scripts/Prototype.dscript")))
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": required scene or script asset is missing.");
            }
            if (m_runtimeVerification.InputPlayback &&
                !std::filesystem::exists(Disparity::FileSystem::FindAssetPath(m_runtimeVerification.ReplayPath)))
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": required replay asset is missing.");
            }
            if (m_runtimeVerification.UseBaseline &&
                !std::filesystem::exists(Disparity::FileSystem::FindAssetPath(m_runtimeVerification.BaselinePath)))
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": required baseline asset is missing.");
            }

            if (m_runtimeVerificationFrame > 3 && m_renderer->GetFrameDrawCalls() == 0)
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": renderer reported zero frame draw calls after warmup.");
            }
            if (m_runtimeVerificationFrame > 3 && m_renderer->GetSceneDrawCalls() == 0)
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": renderer reported zero scene draw calls after warmup.");
            }
            if (m_runtimeVerificationFrame > 3 && m_renderer->GetSettings().Shadows && m_renderer->GetShadowDrawCalls() == 0)
            {
                AddRuntimeVerificationFailure(std::string(stage) + ": shadows are enabled but zero shadow draw calls were recorded.");
            }

            const Disparity::RenderGraph& graph = m_renderer->GetRenderGraph();
            if (m_runtimeVerificationFrame > 3)
            {
                if (graph.GetPasses().empty() || graph.GetResources().empty() || graph.GetExecutionOrder().empty())
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": render graph has no compiled passes/resources/order.");
                }

                for (const std::string& error : graph.Validate())
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": render graph validation: " + error);
                }

                const Disparity::RendererFrameGraphDiagnostics graphDiagnostics = m_renderer->GetFrameGraphDiagnostics();
                if (!graphDiagnostics.DispatchOrderValid)
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": renderer pass dispatch did not match compiled graph order.");
                }
                if (graphDiagnostics.ExecutedPasses != graph.GetExecutionOrder().size())
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": renderer did not execute every compiled graph pass.");
                }
                if (m_runtimeBaselineLoaded &&
                    graphDiagnostics.TransientAllocations < m_runtimeBaseline.MinRenderGraphAllocations)
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": render graph transient allocation count is below baseline.");
                }
                if (m_runtimeBaselineLoaded &&
                    graphDiagnostics.AliasedResources < m_runtimeBaseline.MinRenderGraphAliasedResources)
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": render graph alias allocation count is below baseline.");
                }
                if (graphDiagnostics.GraphCallbacksBound == 0 ||
                    graphDiagnostics.GraphCallbacksExecuted < graphDiagnostics.GraphCallbacksBound)
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": render graph callbacks were not executed for every bound pass.");
                }
                if (m_runtimeBaselineLoaded &&
                    graphDiagnostics.GraphCallbacksBound < m_runtimeBaseline.MinRenderGraphCallbacks)
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": render graph callback count is below baseline.");
                }
                if (m_runtimeBaselineLoaded &&
                    graphDiagnostics.TransitionBarriers < m_runtimeBaseline.MinRenderGraphBarriers)
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": render graph transition barrier count is below baseline.");
                }
                if (graphDiagnostics.ResourceHandles < graph.GetResourceAllocations().size())
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": render graph resource handle diagnostics are incomplete.");
                }
                if (m_runtimeEditorStats.GpuPickReadbacks > 0 &&
                    (graphDiagnostics.ObjectIdReadbackRingSize < 2 ||
                        graphDiagnostics.ObjectIdReadbackCompletions == 0 ||
                        graphDiagnostics.ObjectIdReadbackCompletions > graphDiagnostics.ObjectIdReadbackRequests))
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": object-ID readback ring diagnostics are invalid.");
                }

                const Disparity::EditorViewportResourcesInfo editorResources = m_renderer->GetEditorViewportResources();
                if (m_runtimeBaselineLoaded && m_runtimeBaseline.RequireEditorGpuPickResources &&
                    (!editorResources.ViewportTargetReady ||
                        !editorResources.ObjectIdTargetReady ||
                        !editorResources.ObjectDepthTargetReady ||
                        !m_renderer->GetEditorViewportShaderResourceView() ||
                        editorResources.Width != m_renderer->GetWidth() ||
                        editorResources.Height != m_renderer->GetHeight()))
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": editor viewport/object-ID/depth GPU resources are not ready.");
                }
            }
        }

        void CompleteRuntimeVerification()
        {
            if (m_runtimeVerification.InputPlayback && !m_runtimePlayback.Finished)
            {
                AddRuntimeVerificationFailure("input playback did not finish before runtime verification completed.");
            }
            if (m_runtimeVerification.CaptureFrame && !m_runtimeVerificationCaptureValidated)
            {
                AddRuntimeVerificationFailure("frame capture was not validated before runtime verification completed.");
            }
            if (m_showcaseMode)
            {
                SetShowcaseMode(false);
            }
            if (m_trailerMode)
            {
                SetTrailerMode(false);
            }
            ValidateRuntimeBudgets();
            ValidateRuntimeVerificationState("final");
            ValidateRuntimeScenarioCoverage();
            if (m_runtimeVerificationOriginalRendererSettings.has_value() && m_renderer)
            {
                m_renderer->SetSettings(*m_runtimeVerificationOriginalRendererSettings);
            }

            const bool passed = m_runtimeVerificationFailures.empty();
            WriteRuntimeVerificationReport(passed);
            m_runtimeVerificationFinished = true;
            if (m_application)
            {
                m_application->RequestExit(passed ? 0 : 20);
            }
        }

        void WriteRuntimeVerificationReport(bool passed) const
        {
            const std::filesystem::path reportPath = m_runtimeVerification.ReportPath;
            if (reportPath.has_parent_path())
            {
                std::filesystem::create_directories(reportPath.parent_path());
            }

            std::ofstream report(reportPath, std::ios::trunc);
            if (!report)
            {
                return;
            }

            report << (passed ? "PASS" : "FAIL") << "\n";
            report << "version=" << Disparity::Version::ToString() << "\n";
            report << "frames=" << m_runtimeVerificationFrame << "\n";
            report << "elapsed_seconds=" << m_runtimeVerificationElapsed << "\n";
            report << "scene_objects=" << m_scene.Count() << "\n";
            report << "scene_entities=" << m_sceneEntities.size() << "\n";
            report << "imported_gltf_double_sided=" << (ImportedGltfDoubleSidedReady() ? "true" : "false") << "\n";
            report << "draw_calls=" << (m_renderer ? m_renderer->GetFrameDrawCalls() : 0) << "\n";
            report << "scene_draw_calls=" << (m_renderer ? m_renderer->GetSceneDrawCalls() : 0) << "\n";
            report << "shadow_draw_calls=" << (m_renderer ? m_renderer->GetShadowDrawCalls() : 0) << "\n";
            if (m_renderer)
            {
                const Disparity::RenderGraph& graph = m_renderer->GetRenderGraph();
                const Disparity::RendererFrameGraphDiagnostics graphDiagnostics = m_renderer->GetFrameGraphDiagnostics();
                const Disparity::EditorViewportResourcesInfo editorResources = m_renderer->GetEditorViewportResources();
                report << "render_graph_passes=" << graph.GetPasses().size() << "\n";
                report << "render_graph_resources=" << graph.GetResources().size() << "\n";
                report << "render_graph_order=" << graph.GetExecutionOrder().size() << "\n";
                report << "render_graph_executed_passes=" << graphDiagnostics.ExecutedPasses << "\n";
                report << "render_graph_dispatch_valid=" << (graphDiagnostics.DispatchOrderValid ? "true" : "false") << "\n";
                report << "render_graph_allocations=" << graphDiagnostics.TransientAllocations << "\n";
                report << "render_graph_aliased_resources=" << graphDiagnostics.AliasedResources << "\n";
                report << "render_graph_barriers=" << graphDiagnostics.TransitionBarriers << "\n";
                report << "render_graph_resource_handles=" << graphDiagnostics.ResourceHandles << "\n";
                report << "render_graph_callbacks_bound=" << graphDiagnostics.GraphCallbacksBound << "\n";
                report << "render_graph_callbacks_executed=" << graphDiagnostics.GraphCallbacksExecuted << "\n";
                report << "render_graph_pending_captures=" << graphDiagnostics.PendingCaptureRequests << "\n";
                report << "object_id_readback_ring_size=" << graphDiagnostics.ObjectIdReadbackRingSize << "\n";
                report << "object_id_readback_requests=" << graphDiagnostics.ObjectIdReadbackRequests << "\n";
                report << "object_id_readback_completions=" << graphDiagnostics.ObjectIdReadbackCompletions << "\n";
                report << "object_id_readback_latency_frames=" << graphDiagnostics.ObjectIdReadbackLatencyFrames << "\n";
                report << "editor_viewport_ready=" << (editorResources.ViewportTargetReady ? "true" : "false") << "\n";
                report << "editor_viewport_presented_in_imgui=" << (m_renderer->GetEditorViewportShaderResourceView() ? "true" : "false") << "\n";
                report << "editor_object_id_ready=" << (editorResources.ObjectIdTargetReady ? "true" : "false") << "\n";
                report << "editor_object_depth_ready=" << (editorResources.ObjectDepthTargetReady ? "true" : "false") << "\n";
                report << "editor_viewport_width=" << editorResources.Width << "\n";
                report << "editor_viewport_height=" << editorResources.Height << "\n";
            }
            const Disparity::AudioBackendInfo audioBackendInfo = Disparity::AudioSystem::GetBackendInfo();
            report << "audio_backend=" << audioBackendInfo.ActiveBackend << "\n";
            report << "audio_xaudio2_available=" << (audioBackendInfo.XAudio2Available ? "true" : "false") << "\n";
            report << "audio_xaudio2_preferred=" << (audioBackendInfo.XAudio2Preferred ? "true" : "false") << "\n";
            const Disparity::AudioAnalysis audioAnalysis = Disparity::AudioSystem::GetAnalysis();
            report << "audio_analysis_peak=" << audioAnalysis.Peak << "\n";
            report << "audio_analysis_rms=" << audioAnalysis.Rms << "\n";
            report << "audio_analysis_beat_envelope=" << audioAnalysis.BeatEnvelope << "\n";
            report << "audio_analysis_voices=" << audioAnalysis.ActiveVoices << "\n";
            report << "audio_analysis_content_driven=" << (audioAnalysis.ContentDriven ? "true" : "false") << "\n";
            report << "playback_steps=" << m_runtimePlayback.Steps << "\n";
            report << "playback_distance=" << m_runtimePlayback.Distance << "\n";
            report << "playback_net_distance=" << m_runtimePlayback.NetDistance << "\n";
            report << "editor_pick_tests=" << m_runtimeEditorStats.ObjectPickTests << "\n";
            report << "editor_pick_failures=" << m_runtimeEditorStats.ObjectPickFailures << "\n";
            report << "gizmo_pick_tests=" << m_runtimeEditorStats.GizmoPickTests << "\n";
            report << "gizmo_pick_failures=" << m_runtimeEditorStats.GizmoPickFailures << "\n";
            report << "gpu_pick_readbacks=" << m_runtimeEditorStats.GpuPickReadbacks << "\n";
            report << "gpu_pick_fallbacks=" << m_runtimeEditorStats.GpuPickFallbacks << "\n";
            report << "gizmo_drag_tests=" << m_runtimeEditorStats.GizmoDragTests << "\n";
            report << "gizmo_drag_failures=" << m_runtimeEditorStats.GizmoDragFailures << "\n";
            report << "scene_reload_tests=" << m_runtimeEditorStats.SceneReloads << "\n";
            report << "scene_save_tests=" << m_runtimeEditorStats.SceneSaves << "\n";
            report << "post_debug_view_tests=" << m_runtimeEditorStats.PostDebugViews << "\n";
            report << "showcase_frames=" << m_runtimeEditorStats.ShowcaseFrames << "\n";
            report << "trailer_frames=" << m_runtimeEditorStats.TrailerFrames << "\n";
            report << "high_res_capture_tests=" << m_runtimeEditorStats.HighResCaptures << "\n";
            report << "rift_vfx_draws=" << m_runtimeEditorStats.RiftVfxDraws << "\n";
            report << "rift_vfx_fog_cards=" << m_lastRiftVfxStats.FogCards << "\n";
            report << "rift_vfx_particles=" << m_lastRiftVfxStats.Particles << "\n";
            report << "rift_vfx_ribbons=" << m_lastRiftVfxStats.Ribbons << "\n";
            report << "rift_vfx_lightning_arcs=" << m_lastRiftVfxStats.LightningArcs << "\n";
            report << "rift_vfx_soft_particle_candidates=" << m_lastRiftVfxStats.SoftParticleCandidates << "\n";
            report << "rift_vfx_sorted_batches=" << m_lastRiftVfxStats.SortedBatches << "\n";
            report << "audio_beat_pulses=" << m_runtimeEditorStats.AudioBeatPulses << "\n";
            report << "high_res_capture_path=" << m_highResCaptureOutputPath.string() << "\n";
            report << "native_png_capture_path=" << m_highResCaptureNativePngPath.string() << "\n";
            report << "audio_snapshot_tests=" << m_runtimeEditorStats.AudioSnapshotTests << "\n";
            report << "async_io_tests=" << m_runtimeEditorStats.AsyncIoTests << "\n";
            report << "material_texture_slot_tests=" << m_runtimeEditorStats.MaterialTextureSlotTests << "\n";
            report << "prefab_variant_tests=" << m_runtimeEditorStats.PrefabVariantTests << "\n";
            report << "shot_director_tests=" << m_runtimeEditorStats.ShotDirectorTests << "\n";
            report << "audio_analysis_tests=" << m_runtimeEditorStats.AudioAnalysisTests << "\n";
            report << "vfx_system_tests=" << m_runtimeEditorStats.VfxSystemTests << "\n";
            report << "animation_skinning_tests=" << m_runtimeEditorStats.AnimationSkinningTests << "\n";
            report << "cpu_frame_samples=" << m_runtimeBudgetStats.CpuSamples << "\n";
            report << "cpu_frame_max_ms=" << m_runtimeBudgetStats.CpuFrameMaxMs << "\n";
            report << "cpu_frame_avg_ms=" << m_runtimeBudgetStats.CpuFrameAverageMs << "\n";
            report << "gpu_frame_samples=" << m_runtimeBudgetStats.GpuSamples << "\n";
            report << "gpu_frame_max_ms=" << m_runtimeBudgetStats.GpuFrameMaxMs << "\n";
            report << "gpu_frame_avg_ms=" << m_runtimeBudgetStats.GpuFrameAverageMs << "\n";
            report << "pass_cpu_max_ms=" << m_runtimeBudgetStats.PassCpuMaxMs << "\n";
            report << "pass_cpu_max_name=" << m_runtimeBudgetStats.PassCpuMaxName << "\n";
            report << "pass_gpu_max_ms=" << m_runtimeBudgetStats.PassGpuMaxMs << "\n";
            report << "pass_gpu_max_name=" << m_runtimeBudgetStats.PassGpuMaxName << "\n";
            report << "capture_path=" << m_runtimeCapture.Path.string() << "\n";
            report << "capture_success=" << (m_runtimeCapture.Success ? "true" : "false") << "\n";
            report << "capture_width=" << m_runtimeCapture.Width << "\n";
            report << "capture_height=" << m_runtimeCapture.Height << "\n";
            report << "capture_checksum=" << m_runtimeCapture.RgbChecksum << "\n";
            report << "capture_non_black_pixels=" << m_runtimeCapture.NonBlackPixels << "\n";
            report << "capture_bright_pixels=" << m_runtimeCapture.BrightPixels << "\n";
            report << "capture_average_luma=" << m_runtimeCapture.AverageLuma << "\n";
            report << "replay_path=" << m_runtimeVerification.ReplayPath.string() << "\n";
            report << "replay_loaded_steps=" << m_runtimeReplaySteps.size() << "\n";
            report << "baseline_path=" << m_runtimeVerification.BaselinePath.string() << "\n";
            report << "baseline_loaded=" << (m_runtimeBaselineLoaded ? "true" : "false") << "\n";
            report << "baseline_expected_luma=" << m_runtimeBaseline.ExpectedAverageLuma << "\n";
            report << "baseline_luma_tolerance=" << m_runtimeBaseline.AverageLumaTolerance << "\n";
            report << "budget_cpu_frame_ms=" << m_runtimeVerification.CpuFrameBudgetMs << "\n";
            report << "budget_gpu_frame_ms=" << m_runtimeVerification.GpuFrameBudgetMs << "\n";
            report << "budget_pass_ms=" << m_runtimeVerification.PassBudgetMs << "\n";

            report << "\nnotes:\n";
            for (const std::string& note : m_runtimeVerificationNotes)
            {
                report << "- " << note << "\n";
            }

            report << "\nfailures:\n";
            if (m_runtimeVerificationFailures.empty())
            {
                report << "- none\n";
            }
            else
            {
                for (const std::string& failure : m_runtimeVerificationFailures)
                {
                    report << "- " << failure << "\n";
                }
            }
        }

        EditState CaptureEditState() const
        {
            EditState state;
            state.SceneData = m_scene;
            state.PlayerPosition = m_playerPosition;
            state.PlayerYaw = m_playerYaw;
            state.PlayerBodyMaterial = m_playerBodyMaterial;
            state.PlayerHeadMaterial = m_playerHeadMaterial;
            state.RendererSettings = m_renderer ? m_renderer->GetSettings() : Disparity::RendererSettings{};
            return state;
        }

        bool ImportedGltfDoubleSidedReady() const
        {
            for (const Disparity::NamedSceneObject& object : m_scene.GetObjects())
            {
                if ((object.MeshName == "gltf_sample" || object.MeshName.rfind("gltf_", 0) == 0) && object.Object.MaterialData.DoubleSided)
                {
                    return true;
                }
            }

            return false;
        }

        void ApplyEditState(const EditState& state)
        {
            m_scene = state.SceneData;
            m_playerPosition = state.PlayerPosition;
            m_playerYaw = state.PlayerYaw;
            m_playerBodyMaterial = state.PlayerBodyMaterial;
            m_playerHeadMaterial = state.PlayerHeadMaterial;
            if (m_renderer)
            {
                m_renderer->SetSettings(state.RendererSettings);
            }

            m_selectedIndex = std::min(m_selectedIndex, m_scene.Count() == 0 ? 0u : m_scene.Count() - 1u);
            m_multiSelection.erase(std::remove_if(m_multiSelection.begin(), m_multiSelection.end(), [this](size_t index) {
                return index >= m_scene.Count();
            }), m_multiSelection.end());
            BuildRuntimeRegistry();
        }

        void AddCommandHistory(std::string label)
        {
            if (label.empty())
            {
                return;
            }

            m_commandHistory.push_back(std::move(label));
            while (m_commandHistory.size() > 16)
            {
                m_commandHistory.pop_front();
            }
        }

        void PushUndoState(const EditState& state, std::string label = "Edit")
        {
            if (m_applyingHistory)
            {
                return;
            }

            m_undoStack.push_back(HistoryEntry{ label, state });
            while (m_undoStack.size() > 64)
            {
                m_undoStack.pop_front();
            }
            m_redoStack.clear();
            AddCommandHistory(label);
        }

        bool CanUndo() const
        {
            return !m_undoStack.empty();
        }

        bool CanRedo() const
        {
            return !m_redoStack.empty();
        }

        void UndoEdit()
        {
            if (m_undoStack.empty())
            {
                return;
            }

            m_applyingHistory = true;
            const HistoryEntry entry = m_undoStack.back();
            m_redoStack.push_back(HistoryEntry{ entry.Label, CaptureEditState() });
            m_undoStack.pop_back();
            ApplyEditState(entry.State);
            m_applyingHistory = false;
            SetStatus("Undo: " + entry.Label);
            AddCommandHistory("Undo: " + entry.Label);
        }

        void RedoEdit()
        {
            if (m_redoStack.empty())
            {
                return;
            }

            m_applyingHistory = true;
            const HistoryEntry entry = m_redoStack.back();
            m_undoStack.push_back(HistoryEntry{ entry.Label, CaptureEditState() });
            m_redoStack.pop_back();
            ApplyEditState(entry.State);
            m_applyingHistory = false;
            SetStatus("Redo: " + entry.Label);
            AddCommandHistory("Redo: " + entry.Label);
        }

        bool IntersectRaySphere(
            const DirectX::XMFLOAT3& origin,
            const DirectX::XMFLOAT3& direction,
            const DirectX::XMFLOAT3& center,
            float radius,
            float& outDistance) const
        {
            const DirectX::XMFLOAT3 toCenter = Subtract(center, origin);
            const float projection = Dot(toCenter, direction);
            const float distanceSquared = Dot(toCenter, toCenter) - projection * projection;
            const float radiusSquared = radius * radius;
            if (distanceSquared > radiusSquared)
            {
                return false;
            }

            const float halfChord = std::sqrt(std::max(0.0f, radiusSquared - distanceSquared));
            float distance = projection - halfChord;
            if (distance < 0.0f)
            {
                distance = projection + halfChord;
            }

            if (distance < 0.0f)
            {
                return false;
            }

            outDistance = distance;
            return true;
        }

        uint64_t PickStableIdForSceneObject(size_t index) const
        {
            if (index >= m_scene.Count())
            {
                return 0;
            }

            const uint64_t stableId = m_scene.GetObjects()[index].StableId;
            return stableId != 0 ? stableId : 100000000ull + static_cast<uint64_t>(index);
        }

        uint32_t SceneObjectPickId(size_t index) const
        {
            if (index > static_cast<size_t>(std::numeric_limits<uint32_t>::max() - EditorPickSceneObjectBase))
            {
                return 0;
            }
            return EditorPickSceneObjectBase + static_cast<uint32_t>(index);
        }

        uint32_t GizmoAxisPickId(GizmoAxis axis) const
        {
            return axis == GizmoAxis::None ? 0u : EditorPickGizmoAxisBase + static_cast<uint32_t>(axis);
        }

        uint32_t GizmoPlanePickId(GizmoPlane plane) const
        {
            return plane == GizmoPlane::None ? 0u : EditorPickGizmoPlaneBase + static_cast<uint32_t>(plane);
        }

        bool DecodeEditorObjectId(uint32_t objectId, float depth, EditorPickResult& outPick) const
        {
            outPick = {};
            outPick.Distance = depth;
            if (objectId == EditorPickPlayerId)
            {
                outPick.Kind = EditorPickKind::Player;
                outPick.StableId = EditorPickPlayerId;
                outPick.Name = "Player";
                return true;
            }

            if (objectId >= EditorPickSceneObjectBase && objectId < EditorPickGizmoAxisBase)
            {
                const size_t sceneIndex = static_cast<size_t>(objectId - EditorPickSceneObjectBase);
                if (sceneIndex >= m_scene.Count())
                {
                    return false;
                }

                outPick.Kind = EditorPickKind::SceneObject;
                outPick.SceneIndex = sceneIndex;
                outPick.StableId = PickStableIdForSceneObject(sceneIndex);
                outPick.Name = m_scene.GetObjects()[sceneIndex].Name;
                return true;
            }

            if (objectId >= EditorPickGizmoPlaneBase)
            {
                const GizmoPlane plane = static_cast<GizmoPlane>(objectId - EditorPickGizmoPlaneBase);
                if (plane == GizmoPlane::XY || plane == GizmoPlane::XZ || plane == GizmoPlane::YZ)
                {
                    outPick.Kind = EditorPickKind::GizmoPlane;
                    outPick.Plane = plane;
                    outPick.Name = DescribeEditorPick(outPick);
                    return true;
                }
                return false;
            }

            if (objectId >= EditorPickGizmoAxisBase)
            {
                const GizmoAxis axis = static_cast<GizmoAxis>(objectId - EditorPickGizmoAxisBase);
                if (axis == GizmoAxis::X || axis == GizmoAxis::Y || axis == GizmoAxis::Z)
                {
                    outPick.Kind = EditorPickKind::GizmoAxis;
                    outPick.Axis = axis;
                    outPick.Name = DescribeEditorPick(outPick);
                    return true;
                }
            }

            return false;
        }

        bool TryReadGpuEditorPick(EditorPickResult& outPick)
        {
            outPick = {};
            if (!m_renderer)
            {
                return false;
            }

            RefreshEditorViewportState();
            if (!m_editorViewport.MouseInside)
            {
                return false;
            }

            const DirectX::XMFLOAT2 mouse = Disparity::Input::GetMousePosition();
            const float localX = std::clamp(mouse.x - m_editorViewport.X, 0.0f, std::max(0.0f, m_editorViewport.Width - 1.0f));
            const float localY = std::clamp(mouse.y - m_editorViewport.Y, 0.0f, std::max(0.0f, m_editorViewport.Height - 1.0f));
            const Disparity::EditorObjectIdReadback readback = m_renderer->ReadEditorObjectId(
                static_cast<uint32_t>(localX),
                static_cast<uint32_t>(localY));
            ++m_runtimeEditorStats.GpuPickReadbacks;
            m_lastGpuPickStatus = readback.Error.empty()
                ? ("GPU id=" + std::to_string(readback.ObjectId) + " depth=" + std::to_string(readback.Depth))
                : ("GPU pick fallback: " + readback.Error);

            if (readback.Valid && DecodeEditorObjectId(readback.ObjectId, readback.Depth, outPick))
            {
                return true;
            }

            ++m_runtimeEditorStats.GpuPickFallbacks;
            return false;
        }

        void RefreshEditorViewportState()
        {
            if (!m_application)
            {
                m_editorViewport = {};
                return;
            }

            m_editorViewport.X = 0.0f;
            m_editorViewport.Y = 0.0f;
            m_editorViewport.Width = static_cast<float>(std::max(1u, m_application->GetWidth()));
            m_editorViewport.Height = static_cast<float>(std::max(1u, m_application->GetHeight()));
            const DirectX::XMFLOAT2 mouse = Disparity::Input::GetMousePosition();
            m_editorViewport.MouseInside =
                mouse.x >= m_editorViewport.X &&
                mouse.y >= m_editorViewport.Y &&
                mouse.x <= m_editorViewport.X + m_editorViewport.Width &&
                mouse.y <= m_editorViewport.Y + m_editorViewport.Height;
        }

        bool IsScreenPointInsideViewport(const DirectX::XMFLOAT2& screen) const
        {
            return screen.x >= m_editorViewport.X &&
                screen.y >= m_editorViewport.Y &&
                screen.x <= m_editorViewport.X + m_editorViewport.Width &&
                screen.y <= m_editorViewport.Y + m_editorViewport.Height;
        }

        bool BuildScreenRay(const DirectX::XMFLOAT2& screen, MouseRay& outRay) const
        {
            if (!m_application)
            {
                return false;
            }

            const float width = std::max(1.0f, m_editorViewport.Width);
            const float height = std::max(1.0f, m_editorViewport.Height);
            if (!IsScreenPointInsideViewport(screen))
            {
                return false;
            }

            const float localX = screen.x - m_editorViewport.X;
            const float localY = screen.y - m_editorViewport.Y;
            const DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
            const DirectX::XMMATRIX view = GetRenderCamera().GetViewMatrix();
            const DirectX::XMMATRIX projection = GetRenderCamera().GetProjectionMatrix();
            const DirectX::XMVECTOR nearPoint = DirectX::XMVector3Unproject(
                DirectX::XMVectorSet(localX, localY, 0.0f, 1.0f),
                0.0f,
                0.0f,
                width,
                height,
                0.0f,
                1.0f,
                projection,
                view,
                identity);
            const DirectX::XMVECTOR farPoint = DirectX::XMVector3Unproject(
                DirectX::XMVectorSet(localX, localY, 1.0f, 1.0f),
                0.0f,
                0.0f,
                width,
                height,
                0.0f,
                1.0f,
                projection,
                view,
                identity);
            DirectX::XMStoreFloat3(&outRay.Origin, nearPoint);
            DirectX::XMStoreFloat3(&outRay.Direction, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(farPoint, nearPoint)));
            return true;
        }

        bool BuildMouseRay(MouseRay& outRay) const
        {
            return BuildScreenRay(Disparity::Input::GetMousePosition(), outRay);
        }

        DirectX::XMFLOAT2 ProjectWorldToScreen(const DirectX::XMFLOAT3& worldPosition) const
        {
            const float width = std::max(1.0f, m_editorViewport.Width);
            const float height = std::max(1.0f, m_editorViewport.Height);
            const DirectX::XMVECTOR projected = DirectX::XMVector3Project(
                DirectX::XMLoadFloat3(&worldPosition),
                0.0f,
                0.0f,
                width,
                height,
                0.0f,
                1.0f,
                GetRenderCamera().GetProjectionMatrix(),
                GetRenderCamera().GetViewMatrix(),
                DirectX::XMMatrixIdentity());

            DirectX::XMFLOAT3 screen = {};
            DirectX::XMStoreFloat3(&screen, projected);
            return { screen.x + m_editorViewport.X, screen.y + m_editorViewport.Y };
        }

        DirectX::XMFLOAT3 PickHalfExtentsForObject(const Disparity::NamedSceneObject& object) const
        {
            if (object.MeshName == "terrain")
            {
                return { 28.0f, 0.05f, 28.0f };
            }
            if (object.MeshName == "plane")
            {
                return { 24.0f, 0.08f, 24.0f };
            }
            if (object.MeshName == "gltf_sample")
            {
                return { 0.8f, 0.8f, 0.8f };
            }

            return { 0.5f, 0.5f, 0.5f };
        }

        bool IntersectRayObb(
            const DirectX::XMFLOAT3& origin,
            const DirectX::XMFLOAT3& direction,
            const Disparity::Transform& transform,
            const DirectX::XMFLOAT3& halfExtents,
            float& outDistance) const
        {
            const DirectX::XMMATRIX world = transform.ToMatrix();
            const DirectX::XMMATRIX inverseWorld = DirectX::XMMatrixInverse(nullptr, world);
            DirectX::XMFLOAT3 localOrigin = {};
            DirectX::XMFLOAT3 localDirection = {};
            DirectX::XMStoreFloat3(&localOrigin, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&origin), inverseWorld));
            DirectX::XMStoreFloat3(&localDirection, DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&direction), inverseWorld)));

            float minimumDistance = 0.0f;
            float maximumDistance = FLT_MAX;
            const std::array<std::pair<float, float>, 3> slabs = {
                std::pair<float, float>{ localOrigin.x, localDirection.x },
                std::pair<float, float>{ localOrigin.y, localDirection.y },
                std::pair<float, float>{ localOrigin.z, localDirection.z }
            };
            const std::array<float, 3> extents = {
                std::max(0.001f, halfExtents.x),
                std::max(0.001f, halfExtents.y),
                std::max(0.001f, halfExtents.z)
            };

            for (size_t axis = 0; axis < slabs.size(); ++axis)
            {
                const float slabOrigin = slabs[axis].first;
                const float slabDirection = slabs[axis].second;
                const float extent = extents[axis];
                if (std::abs(slabDirection) < 0.0001f)
                {
                    if (slabOrigin < -extent || slabOrigin > extent)
                    {
                        return false;
                    }
                    continue;
                }

                float nearDistance = (-extent - slabOrigin) / slabDirection;
                float farDistance = (extent - slabOrigin) / slabDirection;
                if (nearDistance > farDistance)
                {
                    std::swap(nearDistance, farDistance);
                }

                minimumDistance = std::max(minimumDistance, nearDistance);
                maximumDistance = std::min(maximumDistance, farDistance);
                if (minimumDistance > maximumDistance)
                {
                    return false;
                }
            }

            if (maximumDistance < 0.0f)
            {
                return false;
            }

            outDistance = minimumDistance >= 0.0f ? minimumDistance : maximumDistance;
            return true;
        }

        bool PickEditorTarget(const MouseRay& ray, EditorPickResult& outPick) const
        {
            outPick = {};
            float bestDistance = FLT_MAX;
            float distance = 0.0f;

            Disparity::Transform playerBody = PlayerBodyTransform();
            if (IntersectRayObb(ray.Origin, ray.Direction, playerBody, { 0.5f, 0.5f, 0.5f }, distance) && distance < bestDistance)
            {
                bestDistance = distance;
                outPick.Kind = EditorPickKind::Player;
                outPick.StableId = 1;
                outPick.Distance = distance;
                outPick.Name = "Player";
            }

            Disparity::Transform playerHead;
            playerHead.Position = Add(m_playerPosition, { 0.0f, 1.85f + m_playerBobOffset, 0.0f });
            playerHead.Rotation = { 0.0f, m_playerYaw, 0.0f };
            playerHead.Scale = { 0.55f, 0.45f, 0.55f };
            if (IntersectRayObb(ray.Origin, ray.Direction, playerHead, { 0.5f, 0.5f, 0.5f }, distance) && distance < bestDistance)
            {
                bestDistance = distance;
                outPick.Kind = EditorPickKind::Player;
                outPick.StableId = 1;
                outPick.Distance = distance;
                outPick.Name = "Player";
            }

            const auto& objects = m_scene.GetObjects();
            for (size_t index = 0; index < objects.size(); ++index)
            {
                const Disparity::NamedSceneObject& object = objects[index];
                if (IntersectRayObb(ray.Origin, ray.Direction, object.Object.TransformData, PickHalfExtentsForObject(object), distance) && distance < bestDistance)
                {
                    bestDistance = distance;
                    outPick.Kind = EditorPickKind::SceneObject;
                    outPick.SceneIndex = index;
                    outPick.StableId = PickStableIdForSceneObject(index);
                    outPick.Distance = distance;
                    outPick.Name = object.Name;
                }
            }

            return outPick.Kind != EditorPickKind::None;
        }

        std::string DescribeEditorPick(const EditorPickResult& pick) const
        {
            switch (pick.Kind)
            {
            case EditorPickKind::Player:
                return "Player id=1";
            case EditorPickKind::SceneObject:
                return pick.Name + " id=" + std::to_string(pick.StableId);
            case EditorPickKind::GizmoAxis:
                return std::string("Gizmo ") + AxisName(pick.Axis);
            case EditorPickKind::GizmoPlane:
                return std::string("Gizmo ") + PlaneName(pick.Plane);
            case EditorPickKind::None:
            default:
                return "None";
            }
        }

        void ApplyEditorPick(const EditorPickResult& pick, bool additive)
        {
            if (pick.Kind == EditorPickKind::Player)
            {
                m_selectedPlayer = true;
                m_multiSelection.clear();
                m_lastPickStatus = DescribeEditorPick(pick);
                SetStatus("Picked Player");
                return;
            }

            if (pick.Kind == EditorPickKind::SceneObject && pick.SceneIndex < m_scene.Count())
            {
                SelectSceneObject(pick.SceneIndex, additive);
                m_lastPickStatus = DescribeEditorPick(pick);
                SetStatus("Picked " + pick.Name);
                return;
            }

            m_lastPickStatus = "None";
        }

        const char* GizmoModeName(GizmoMode mode) const
        {
            switch (mode)
            {
            case GizmoMode::Translate:
                return "Translate";
            case GizmoMode::Rotate:
                return "Rotate";
            case GizmoMode::Scale:
                return "Scale";
            default:
                return "Unknown";
            }
        }

        const char* GizmoSpaceName(GizmoSpace space) const
        {
            switch (space)
            {
            case GizmoSpace::World:
                return "World";
            case GizmoSpace::Local:
                return "Local";
            default:
                return "Unknown";
            }
        }

        DirectX::XMFLOAT3 BaseAxisVector(GizmoAxis axis) const
        {
            switch (axis)
            {
            case GizmoAxis::X:
                return { 1.0f, 0.0f, 0.0f };
            case GizmoAxis::Y:
                return { 0.0f, 1.0f, 0.0f };
            case GizmoAxis::Z:
                return { 0.0f, 0.0f, 1.0f };
            case GizmoAxis::None:
            default:
                return {};
            }
        }

        DirectX::XMFLOAT3 SelectionRotation() const
        {
            if (m_selectedPlayer)
            {
                return { 0.0f, m_playerYaw, 0.0f };
            }

            const std::vector<size_t> selectedIndices = GetSelectedSceneIndices();
            if (!selectedIndices.empty() && selectedIndices.front() < m_scene.Count())
            {
                return m_scene.GetObjects()[selectedIndices.front()].Object.TransformData.Rotation;
            }

            return {};
        }

        DirectX::XMFLOAT3 AxisVector(GizmoAxis axis) const
        {
            const DirectX::XMFLOAT3 baseAxis = BaseAxisVector(axis);
            if (m_gizmoSpace == GizmoSpace::Local)
            {
                return TransformDirection(baseAxis, SelectionRotation());
            }

            return baseAxis;
        }

        float GizmoScreenScale(const DirectX::XMFLOAT3& pivot) const
        {
            const float distance = Length(Subtract(GetRenderCamera().GetPosition(), pivot));
            return std::clamp(distance * 0.13f, 0.55f, 3.5f);
        }

        float GizmoHandleDistance(const DirectX::XMFLOAT3& pivot) const
        {
            const float screenScale = GizmoScreenScale(pivot);
            switch (m_gizmoMode)
            {
            case GizmoMode::Rotate:
                return 1.22f * screenScale;
            case GizmoMode::Scale:
                return 1.02f * screenScale;
            case GizmoMode::Translate:
            default:
                return 0.86f * screenScale;
            }
        }

        float GizmoHandleRadius(const DirectX::XMFLOAT3& pivot) const
        {
            const float screenScale = GizmoScreenScale(pivot);
            switch (m_gizmoMode)
            {
            case GizmoMode::Rotate:
                return 0.24f * screenScale;
            case GizmoMode::Scale:
                return 0.32f * screenScale;
            case GizmoMode::Translate:
            default:
                return 0.28f * screenScale;
            }
        }

        std::pair<DirectX::XMFLOAT3, DirectX::XMFLOAT3> GizmoPlaneAxes(GizmoPlane plane) const
        {
            switch (plane)
            {
            case GizmoPlane::XY:
                return { AxisVector(GizmoAxis::X), AxisVector(GizmoAxis::Y) };
            case GizmoPlane::XZ:
                return { AxisVector(GizmoAxis::X), AxisVector(GizmoAxis::Z) };
            case GizmoPlane::YZ:
                return { AxisVector(GizmoAxis::Y), AxisVector(GizmoAxis::Z) };
            case GizmoPlane::None:
            default:
                return { {}, {} };
            }
        }

        DirectX::XMFLOAT3 GizmoPlaneNormal(GizmoPlane plane) const
        {
            const auto [firstAxis, secondAxis] = GizmoPlaneAxes(plane);
            return Normalize(Cross(firstAxis, secondAxis));
        }

        DirectX::XMFLOAT3 GizmoPlaneCenter(const DirectX::XMFLOAT3& pivot, GizmoPlane plane) const
        {
            const auto [firstAxis, secondAxis] = GizmoPlaneAxes(plane);
            return Add(pivot, Scale(Add(firstAxis, secondAxis), GizmoHandleDistance(pivot) * 0.42f));
        }

        DirectX::XMFLOAT3 GizmoPlaneRotation(GizmoPlane plane) const
        {
            DirectX::XMFLOAT3 rotation = {};
            switch (plane)
            {
            case GizmoPlane::XY:
                rotation = { Pi * 0.5f, 0.0f, 0.0f };
                break;
            case GizmoPlane::YZ:
                rotation = { 0.0f, 0.0f, -Pi * 0.5f };
                break;
            case GizmoPlane::XZ:
            case GizmoPlane::None:
            default:
                rotation = {};
                break;
            }

            return m_gizmoSpace == GizmoSpace::Local ? Add(SelectionRotation(), rotation) : rotation;
        }

        DirectX::XMFLOAT3 GizmoRingRotation(GizmoAxis axis) const
        {
            DirectX::XMFLOAT3 rotation = {};
            switch (axis)
            {
            case GizmoAxis::X:
                rotation = { 0.0f, Pi * 0.5f, 0.0f };
                break;
            case GizmoAxis::Y:
                rotation = { -Pi * 0.5f, 0.0f, 0.0f };
                break;
            case GizmoAxis::Z:
            case GizmoAxis::None:
            default:
                rotation = {};
                break;
            }

            return m_gizmoSpace == GizmoSpace::Local ? Add(SelectionRotation(), rotation) : rotation;
        }

        float& ComponentForAxis(DirectX::XMFLOAT3& value, GizmoAxis axis) const
        {
            switch (axis)
            {
            case GizmoAxis::Y:
                return value.y;
            case GizmoAxis::Z:
                return value.z;
            case GizmoAxis::X:
            case GizmoAxis::None:
            default:
                return value.x;
            }
        }

        const char* AxisName(GizmoAxis axis) const
        {
            switch (axis)
            {
            case GizmoAxis::X:
                return "X";
            case GizmoAxis::Y:
                return "Y";
            case GizmoAxis::Z:
                return "Z";
            case GizmoAxis::None:
            default:
                return "None";
            }
        }

        const char* PlaneName(GizmoPlane plane) const
        {
            switch (plane)
            {
            case GizmoPlane::XY:
                return "XY";
            case GizmoPlane::XZ:
                return "XZ";
            case GizmoPlane::YZ:
                return "YZ";
            case GizmoPlane::None:
            default:
                return "None";
            }
        }

        bool IntersectRayPlane(
            const DirectX::XMFLOAT3& origin,
            const DirectX::XMFLOAT3& direction,
            const DirectX::XMFLOAT3& point,
            const DirectX::XMFLOAT3& normal,
            DirectX::XMFLOAT3& outHit,
            float& outDistance) const
        {
            const float denominator = Dot(direction, normal);
            if (std::abs(denominator) <= 0.0001f)
            {
                return false;
            }

            const float distance = Dot(Subtract(point, origin), normal) / denominator;
            if (distance < 0.0f)
            {
                return false;
            }

            outDistance = distance;
            outHit = Add(origin, Scale(direction, distance));
            return true;
        }

        bool IsGizmoAxisHighlighted(GizmoAxis axis) const
        {
            return (m_hoverPick.Kind == EditorPickKind::GizmoAxis && m_hoverPick.Axis == axis) ||
                (m_gizmoDragging && m_gizmoDragAxis == axis && m_gizmoDragPlane == GizmoPlane::None);
        }

        bool IsGizmoPlaneHighlighted(GizmoPlane plane) const
        {
            return (m_hoverPick.Kind == EditorPickKind::GizmoPlane && m_hoverPick.Plane == plane) ||
                (m_gizmoDragging && m_gizmoDragPlane == plane);
        }

        Disparity::Material HighlightGizmoMaterial(Disparity::Material material) const
        {
            material.Albedo = Add(Scale(material.Albedo, 1.25f), { 0.18f, 0.18f, 0.18f });
            material.Roughness = std::max(0.05f, material.Roughness * 0.65f);
            material.Alpha = std::min(1.0f, std::max(material.Alpha, 0.68f));
            return material;
        }

        bool TryPickGizmoHandle(const MouseRay& ray, GizmoAxis& outAxis, GizmoPlane& outPlane, float* outDistance = nullptr) const
        {
            outAxis = GizmoAxis::None;
            outPlane = GizmoPlane::None;
            DirectX::XMFLOAT3 pivot = {};
            if (!TryGetSelectionPivot(pivot))
            {
                return false;
            }

            float bestDistance = FLT_MAX;
            float distance = 0.0f;
            const float handleDistance = GizmoHandleDistance(pivot);
            const float handleRadius = GizmoHandleRadius(pivot);
            const std::array<std::pair<GizmoAxis, DirectX::XMFLOAT3>, 3> handles = {
                std::pair<GizmoAxis, DirectX::XMFLOAT3>{ GizmoAxis::X, Add(pivot, Scale(AxisVector(GizmoAxis::X), handleDistance)) },
                std::pair<GizmoAxis, DirectX::XMFLOAT3>{ GizmoAxis::Y, Add(pivot, Scale(AxisVector(GizmoAxis::Y), handleDistance)) },
                std::pair<GizmoAxis, DirectX::XMFLOAT3>{ GizmoAxis::Z, Add(pivot, Scale(AxisVector(GizmoAxis::Z), handleDistance)) }
            };

            for (const auto& [axis, handlePosition] : handles)
            {
                if (IntersectRaySphere(ray.Origin, ray.Direction, handlePosition, handleRadius, distance) && distance < bestDistance)
                {
                    bestDistance = distance;
                    outAxis = axis;
                }
            }

            if (m_gizmoMode == GizmoMode::Translate && !m_selectedPlayer)
            {
                const float halfSize = GizmoScreenScale(pivot) * 0.21f;
                const std::array<GizmoPlane, 3> planes = { GizmoPlane::XY, GizmoPlane::XZ, GizmoPlane::YZ };
                for (const GizmoPlane plane : planes)
                {
                    const auto [firstAxis, secondAxis] = GizmoPlaneAxes(plane);
                    const DirectX::XMFLOAT3 center = GizmoPlaneCenter(pivot, plane);
                    const DirectX::XMFLOAT3 normal = GizmoPlaneNormal(plane);
                    DirectX::XMFLOAT3 hit = {};
                    if (!IntersectRayPlane(ray.Origin, ray.Direction, center, normal, hit, distance) || distance >= bestDistance)
                    {
                        continue;
                    }

                    const DirectX::XMFLOAT3 local = Subtract(hit, center);
                    const float first = Dot(local, firstAxis);
                    const float second = Dot(local, secondAxis);
                    if (std::abs(first) <= halfSize && std::abs(second) <= halfSize)
                    {
                        bestDistance = distance;
                        outAxis = GizmoAxis::None;
                        outPlane = plane;
                    }
                }
            }

            const bool picked = outAxis != GizmoAxis::None || outPlane != GizmoPlane::None;
            if (picked && outDistance)
            {
                *outDistance = bestDistance;
            }
            return picked;
        }

        bool TryPickGizmoAxis(GizmoAxis& outAxis, GizmoPlane& outPlane)
        {
            EditorPickResult gpuPick;
            if (TryReadGpuEditorPick(gpuPick))
            {
                if (gpuPick.Kind == EditorPickKind::GizmoAxis)
                {
                    outAxis = gpuPick.Axis;
                    outPlane = GizmoPlane::None;
                    return true;
                }
                if (gpuPick.Kind == EditorPickKind::GizmoPlane)
                {
                    outAxis = GizmoAxis::None;
                    outPlane = gpuPick.Plane;
                    return true;
                }
            }

            MouseRay ray;
            if (!BuildMouseRay(ray))
            {
                outAxis = GizmoAxis::None;
                outPlane = GizmoPlane::None;
                return false;
            }

            return TryPickGizmoHandle(ray, outAxis, outPlane);
        }

        bool TryBeginGizmoDrag()
        {
            if (m_gizmoDragging)
            {
                return true;
            }

            GizmoAxis pickedAxis = GizmoAxis::None;
            GizmoPlane pickedPlane = GizmoPlane::None;
            DirectX::XMFLOAT3 pivot = {};
            if (!TryPickGizmoAxis(pickedAxis, pickedPlane) || !TryGetSelectionPivot(pivot))
            {
                return false;
            }
            if (m_selectedPlayer && m_gizmoMode == GizmoMode::Scale)
            {
                m_gizmoStatus = "Player scale is not editable";
                return false;
            }
            if (m_selectedPlayer && m_gizmoMode == GizmoMode::Rotate && pickedAxis != GizmoAxis::Y)
            {
                m_gizmoStatus = "Player rotates on Y only";
                return false;
            }

            m_gizmoDragging = true;
            m_gizmoDragMoved = false;
            m_gizmoDragAxis = pickedAxis;
            m_gizmoDragPlane = pickedPlane;
            m_gizmoDragMode = m_gizmoMode;
            m_gizmoDragSpace = m_gizmoSpace;
            m_gizmoDragAxisVector = pickedAxis == GizmoAxis::None ? DirectX::XMFLOAT3{} : AxisVector(pickedAxis);
            m_gizmoDragStartMouse = Disparity::Input::GetMousePosition();
            m_gizmoDragStartPivot = pivot;
            m_gizmoDragStartPlayerPosition = m_playerPosition;
            m_gizmoDragStartPlayerYaw = m_playerYaw;
            m_gizmoDragObjects.clear();
            m_gizmoDragBeforeState = CaptureEditState();
            if (pickedPlane != GizmoPlane::None)
            {
                MouseRay ray;
                float hitDistance = 0.0f;
                m_gizmoDragPlaneNormal = GizmoPlaneNormal(pickedPlane);
                if (!BuildMouseRay(ray) || !IntersectRayPlane(ray.Origin, ray.Direction, pivot, m_gizmoDragPlaneNormal, m_gizmoDragPlaneStartHit, hitDistance))
                {
                    m_gizmoDragging = false;
                    m_gizmoDragPlane = GizmoPlane::None;
                    m_gizmoDragPlaneNormal = {};
                    m_gizmoDragPlaneStartHit = {};
                    m_gizmoDragBeforeState.reset();
                    return false;
                }
            }
            for (const size_t selectedIndex : GetSelectedSceneIndices())
            {
                if (selectedIndex < m_scene.Count())
                {
                    const Disparity::Transform& transform = m_scene.GetObjects()[selectedIndex].Object.TransformData;
                    m_gizmoDragObjects.push_back(GizmoDragObject{
                        selectedIndex,
                        transform.Position,
                        transform.Rotation,
                        transform.Scale
                    });
                }
            }

            const char* handleName = m_gizmoDragPlane != GizmoPlane::None ? PlaneName(m_gizmoDragPlane) : AxisName(m_gizmoDragAxis);
            m_gizmoStatus = std::string(GizmoModeName(m_gizmoDragMode)) + " " + handleName + " " + GizmoSpaceName(m_gizmoDragSpace);
            return true;
        }

        void UpdateGizmoDrag()
        {
            if (!m_gizmoDragging || (m_gizmoDragAxis == GizmoAxis::None && m_gizmoDragPlane == GizmoPlane::None))
            {
                return;
            }

            if (m_gizmoDragPlane != GizmoPlane::None)
            {
                MouseRay ray;
                DirectX::XMFLOAT3 hit = {};
                float hitDistance = 0.0f;
                if (!BuildMouseRay(ray) || !IntersectRayPlane(ray.Origin, ray.Direction, m_gizmoDragStartPivot, m_gizmoDragPlaneNormal, hit, hitDistance))
                {
                    return;
                }

                DirectX::XMFLOAT3 offset = Subtract(hit, m_gizmoDragPlaneStartHit);
                if (Disparity::Input::IsKeyDown(VK_SHIFT))
                {
                    constexpr float Snap = 0.25f;
                    const auto [firstAxis, secondAxis] = GizmoPlaneAxes(m_gizmoDragPlane);
                    const float firstDelta = std::round(Dot(offset, firstAxis) / Snap) * Snap;
                    const float secondDelta = std::round(Dot(offset, secondAxis) / Snap) * Snap;
                    offset = Add(Scale(firstAxis, firstDelta), Scale(secondAxis, secondDelta));
                }

                const float movedDistance = Length(offset);
                if (m_gizmoDragMode == GizmoMode::Translate && !m_selectedPlayer)
                {
                    for (const GizmoDragObject& object : m_gizmoDragObjects)
                    {
                        if (object.SceneIndex >= m_scene.Count())
                        {
                            continue;
                        }

                        m_scene.GetObjects()[object.SceneIndex].Object.TransformData.Position = Add(object.OriginalPosition, offset);
                        SyncSceneObjectToRegistry(object.SceneIndex);
                        m_gizmoDragMoved = m_gizmoDragMoved || movedDistance > 0.001f;
                    }
                }

                m_gizmoStatus = std::string(GizmoModeName(m_gizmoDragMode)) + " " + PlaneName(m_gizmoDragPlane) + " " + std::to_string(movedDistance);
                return;
            }

            const DirectX::XMFLOAT3 axis = m_gizmoDragAxisVector;
            const DirectX::XMFLOAT2 startScreen = ProjectWorldToScreen(m_gizmoDragStartPivot);
            const DirectX::XMFLOAT2 endScreen = ProjectWorldToScreen(Add(m_gizmoDragStartPivot, axis));
            DirectX::XMFLOAT2 axisScreen = {
                endScreen.x - startScreen.x,
                endScreen.y - startScreen.y
            };
            float axisScreenLength = std::sqrt(axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y);
            if (axisScreenLength <= 0.001f)
            {
                axisScreen = { 1.0f, 0.0f };
                axisScreenLength = 1.0f;
            }
            axisScreen.x /= axisScreenLength;
            axisScreen.y /= axisScreenLength;

            const DirectX::XMFLOAT2 mouse = Disparity::Input::GetMousePosition();
            const DirectX::XMFLOAT2 mouseDelta = {
                mouse.x - m_gizmoDragStartMouse.x,
                mouse.y - m_gizmoDragStartMouse.y
            };
            const float projectedPixels = mouseDelta.x * axisScreen.x + mouseDelta.y * axisScreen.y;
            const float cameraDistance = Length(Subtract(GetRenderCamera().GetPosition(), m_gizmoDragStartPivot));
            float worldDelta = projectedPixels * std::max(0.004f, cameraDistance * 0.0035f);
            if (Disparity::Input::IsKeyDown(VK_SHIFT) && m_gizmoDragMode == GizmoMode::Translate)
            {
                constexpr float Snap = 0.25f;
                worldDelta = std::round(worldDelta / Snap) * Snap;
            }

            if (m_gizmoDragMode == GizmoMode::Translate)
            {
                const DirectX::XMFLOAT3 offset = Scale(axis, worldDelta);
                if (m_selectedPlayer)
                {
                    m_playerPosition = Add(m_gizmoDragStartPlayerPosition, offset);
                    m_gizmoDragMoved = m_gizmoDragMoved || std::abs(worldDelta) > 0.001f;
                }
                else
                {
                    for (const GizmoDragObject& object : m_gizmoDragObjects)
                    {
                        if (object.SceneIndex >= m_scene.Count())
                        {
                            continue;
                        }

                        m_scene.GetObjects()[object.SceneIndex].Object.TransformData.Position = Add(object.OriginalPosition, offset);
                        SyncSceneObjectToRegistry(object.SceneIndex);
                        m_gizmoDragMoved = m_gizmoDragMoved || std::abs(worldDelta) > 0.001f;
                    }
                }
            }
            else if (m_gizmoDragMode == GizmoMode::Rotate)
            {
                float angleDelta = worldDelta * 0.85f;
                if (Disparity::Input::IsKeyDown(VK_SHIFT))
                {
                    constexpr float Snap = Pi / 36.0f;
                    angleDelta = std::round(angleDelta / Snap) * Snap;
                }

                if (m_selectedPlayer && m_gizmoDragAxis == GizmoAxis::Y)
                {
                    m_playerYaw = m_gizmoDragStartPlayerYaw + angleDelta;
                    m_gizmoDragMoved = m_gizmoDragMoved || std::abs(angleDelta) > 0.001f;
                }
                else if (!m_selectedPlayer)
                {
                    for (const GizmoDragObject& object : m_gizmoDragObjects)
                    {
                        if (object.SceneIndex >= m_scene.Count())
                        {
                            continue;
                        }

                        Disparity::Transform& transform = m_scene.GetObjects()[object.SceneIndex].Object.TransformData;
                        transform.Rotation = object.OriginalRotation;
                        ComponentForAxis(transform.Rotation, m_gizmoDragAxis) += angleDelta;
                        SyncSceneObjectToRegistry(object.SceneIndex);
                        m_gizmoDragMoved = m_gizmoDragMoved || std::abs(angleDelta) > 0.001f;
                    }
                }
            }
            else if (m_gizmoDragMode == GizmoMode::Scale && !m_selectedPlayer)
            {
                float scaleDelta = worldDelta * 0.45f;
                if (Disparity::Input::IsKeyDown(VK_SHIFT))
                {
                    constexpr float Snap = 0.05f;
                    scaleDelta = std::round(scaleDelta / Snap) * Snap;
                }

                for (const GizmoDragObject& object : m_gizmoDragObjects)
                {
                    if (object.SceneIndex >= m_scene.Count())
                    {
                        continue;
                    }

                    Disparity::Transform& transform = m_scene.GetObjects()[object.SceneIndex].Object.TransformData;
                    transform.Scale = object.OriginalScale;
                    float& component = ComponentForAxis(transform.Scale, m_gizmoDragAxis);
                    component = std::max(0.05f, component + scaleDelta);
                    SyncSceneObjectToRegistry(object.SceneIndex);
                    m_gizmoDragMoved = m_gizmoDragMoved || std::abs(scaleDelta) > 0.001f;
                }
            }

            m_gizmoStatus = std::string(GizmoModeName(m_gizmoDragMode)) + " " + AxisName(m_gizmoDragAxis) + " " + std::to_string(worldDelta);
        }

        void EndGizmoDrag()
        {
            if (!m_gizmoDragging)
            {
                return;
            }

            if (m_gizmoDragMoved && m_gizmoDragBeforeState.has_value())
            {
                const char* handleName = m_gizmoDragPlane != GizmoPlane::None ? PlaneName(m_gizmoDragPlane) : AxisName(m_gizmoDragAxis);
                PushUndoState(*m_gizmoDragBeforeState, std::string(GizmoModeName(m_gizmoDragMode)) + " Gizmo " + handleName);
            }

            m_gizmoDragging = false;
            m_gizmoDragMoved = false;
            m_gizmoDragAxis = GizmoAxis::None;
            m_gizmoDragPlane = GizmoPlane::None;
            m_gizmoDragAxisVector = {};
            m_gizmoDragPlaneNormal = {};
            m_gizmoDragPlaneStartHit = {};
            m_gizmoDragObjects.clear();
            m_gizmoDragBeforeState.reset();
            m_gizmoStatus = "Idle";
        }

        void UpdateEditorHover(bool editorCapturesMouse)
        {
            m_hoverPick = {};
            if (!m_editorVisible || editorCapturesMouse || Disparity::Input::IsMouseCaptured() || m_gizmoDragging)
            {
                return;
            }

            if (TryReadGpuEditorPick(m_hoverPick))
            {
                return;
            }

            MouseRay ray;
            if (!BuildMouseRay(ray))
            {
                return;
            }

            GizmoAxis axis = GizmoAxis::None;
            GizmoPlane plane = GizmoPlane::None;
            float distance = 0.0f;
            if (TryPickGizmoHandle(ray, axis, plane, &distance))
            {
                m_hoverPick.Kind = plane != GizmoPlane::None ? EditorPickKind::GizmoPlane : EditorPickKind::GizmoAxis;
                m_hoverPick.Axis = axis;
                m_hoverPick.Plane = plane;
                m_hoverPick.Distance = distance;
                m_hoverPick.Name = DescribeEditorPick(m_hoverPick);
                return;
            }

            (void)PickEditorTarget(ray, m_hoverPick);
        }

        void PickSceneObjectAtMouse()
        {
            EditorPickResult gpuPick;
            if (TryReadGpuEditorPick(gpuPick))
            {
                ApplyEditorPick(gpuPick, Disparity::Input::IsKeyDown(VK_CONTROL));
                return;
            }

            MouseRay ray;
            if (!BuildMouseRay(ray))
            {
                return;
            }

            EditorPickResult pick;
            if (PickEditorTarget(ray, pick))
            {
                ApplyEditorPick(pick, Disparity::Input::IsKeyDown(VK_CONTROL));
                return;
            }

            m_lastPickStatus = "None";
        }

        void FrameSelectedObject()
        {
            m_editorCameraEnabled = true;
            if (m_selectedPlayer)
            {
                m_editorCameraTarget = Add(m_playerPosition, { 0.0f, 1.1f, 0.0f });
                return;
            }

            if (m_selectedIndex < m_scene.Count())
            {
                m_editorCameraTarget = m_scene.GetObjects()[m_selectedIndex].Object.TransformData.Position;
            }
        }

        bool IsSceneObjectSelected(size_t index) const
        {
            if (m_selectedPlayer)
            {
                return false;
            }

            if (index == m_selectedIndex)
            {
                return true;
            }

            return std::find(m_multiSelection.begin(), m_multiSelection.end(), index) != m_multiSelection.end();
        }

        void SelectSceneObject(size_t index, bool additive)
        {
            if (index >= m_scene.Count())
            {
                return;
            }

            m_selectedPlayer = false;
            if (!additive)
            {
                m_multiSelection.clear();
                m_selectedIndex = index;
                return;
            }

            if (m_multiSelection.empty() && m_selectedIndex < m_scene.Count())
            {
                m_multiSelection.push_back(m_selectedIndex);
            }

            const auto found = std::find(m_multiSelection.begin(), m_multiSelection.end(), index);
            if (found == m_multiSelection.end())
            {
                m_multiSelection.push_back(index);
                m_selectedIndex = index;
            }
            else if (m_multiSelection.size() > 1)
            {
                m_multiSelection.erase(found);
                m_selectedIndex = m_multiSelection.back();
            }
        }

        std::vector<size_t> GetSelectedSceneIndices() const
        {
            std::vector<size_t> indices;
            if (m_selectedPlayer || m_scene.Count() == 0)
            {
                return indices;
            }

            if (!m_multiSelection.empty())
            {
                indices = m_multiSelection;
            }
            else if (m_selectedIndex < m_scene.Count())
            {
                indices.push_back(m_selectedIndex);
            }

            std::sort(indices.begin(), indices.end());
            indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
            indices.erase(std::remove_if(indices.begin(), indices.end(), [this](size_t index) {
                return index >= m_scene.Count();
            }), indices.end());
            return indices;
        }

        bool CanCopySelection() const
        {
            return !GetSelectedSceneIndices().empty();
        }

        std::string MakeUniqueSceneObjectName(const std::string& requestedName) const
        {
            std::string baseName = requestedName.empty() ? "SceneObject" : requestedName;
            std::string candidate = baseName;
            int suffix = 1;

            const auto nameExists = [this](const std::string& name) {
                const auto& objects = m_scene.GetObjects();
                return std::any_of(objects.begin(), objects.end(), [&name](const Disparity::NamedSceneObject& object) {
                    return object.Name == name;
                });
            };

            while (nameExists(candidate))
            {
                candidate = baseName + "_" + std::to_string(suffix++);
            }

            return candidate;
        }

        void CopySelectedObject()
        {
            if (!CanCopySelection())
            {
                return;
            }

            m_sceneClipboard = m_scene.GetObjects()[m_selectedIndex];
            SetStatus("Copied " + m_sceneClipboard->Name);
            AddCommandHistory("Copy " + m_sceneClipboard->Name);
        }

        void PasteSceneObject(std::string label = "Paste")
        {
            if (!m_sceneClipboard.has_value())
            {
                return;
            }

            const EditState before = CaptureEditState();
            Disparity::NamedSceneObject pasted = *m_sceneClipboard;
            pasted.Name = MakeUniqueSceneObjectName(pasted.Name);
            pasted.StableId = 0;
            pasted.Object.TransformData.Position.x += 0.75f;
            pasted.Object.TransformData.Position.z += 0.75f;
            m_scene.Add(pasted);
            m_selectedPlayer = false;
            m_selectedIndex = m_scene.Count() - 1u;
            m_multiSelection.clear();
            PushUndoState(before, label + " " + m_scene.GetObjects()[m_selectedIndex].Name);
            BuildRuntimeRegistry();
            SetStatus("Pasted " + m_scene.GetObjects()[m_selectedIndex].Name);
        }

        void DuplicateSelectedObject()
        {
            if (!CanCopySelection())
            {
                return;
            }

            m_sceneClipboard = m_scene.GetObjects()[m_selectedIndex];
            PasteSceneObject("Duplicate");
            SetStatus("Duplicated " + m_scene.GetObjects()[m_selectedIndex].Name);
        }

        void DeleteSelectedObject()
        {
            if (!CanCopySelection())
            {
                return;
            }

            const EditState before = CaptureEditState();
            std::vector<size_t> selectedIndices = GetSelectedSceneIndices();
            if (selectedIndices.empty())
            {
                return;
            }

            const std::string deletedName = selectedIndices.size() == 1
                ? m_scene.GetObjects()[selectedIndices.front()].Name
                : std::to_string(selectedIndices.size()) + " objects";
            std::vector<Disparity::NamedSceneObject>& objects = m_scene.GetObjects();
            std::sort(selectedIndices.begin(), selectedIndices.end(), std::greater<size_t>());
            for (const size_t index : selectedIndices)
            {
                if (index < objects.size())
                {
                    objects.erase(objects.begin() + static_cast<std::ptrdiff_t>(index));
                }
            }

            m_multiSelection.clear();
            if (objects.empty())
            {
                m_selectedPlayer = true;
                m_selectedIndex = 0;
            }
            else
            {
                m_selectedIndex = std::min(m_selectedIndex, objects.size() - 1u);
            }

            PushUndoState(before, "Delete " + deletedName);
            BuildRuntimeRegistry();
            SetStatus("Deleted " + deletedName);
        }

        void SaveSelectedPrefab(const std::filesystem::path& path)
        {
            if (m_selectedPlayer || m_selectedIndex >= m_scene.Count())
            {
                SetStatus("Select a scene object before saving a prefab");
                return;
            }

            const Disparity::Prefab prefab = Disparity::PrefabIO::FromSceneObject(m_scene.GetObjects()[m_selectedIndex]);
            SetStatus(Disparity::PrefabIO::Save(path, prefab) ? "Saved prefab " + path.string() : "Prefab save failed");
        }

        void RevertSelectedFromPrefab(const std::filesystem::path& path)
        {
            if (m_selectedPlayer || m_selectedIndex >= m_scene.Count())
            {
                SetStatus("Select a scene object before reverting prefab overrides");
                return;
            }

            Disparity::Prefab prefab;
            if (!Disparity::PrefabIO::Load(path, prefab))
            {
                SetStatus("Prefab revert failed: " + path.string());
                return;
            }

            const EditState before = CaptureEditState();
            Disparity::NamedSceneObject& selected = m_scene.GetObjects()[m_selectedIndex];
            const std::string instanceName = selected.Name;
            const uint64_t stableId = selected.StableId;
            const DirectX::XMFLOAT3 worldPosition = selected.Object.TransformData.Position;
            selected = Disparity::PrefabIO::Instantiate(prefab, instanceName, worldPosition, m_meshes);
            selected.StableId = stableId;
            PushUndoState(before, "Revert " + instanceName + " From Prefab");
            SyncSceneObjectToRegistry(m_selectedIndex);
            WatchAssets();
            SetStatus("Reverted " + instanceName + " from " + path.string());
        }

        void BuildRuntimeRegistry()
        {
            m_registry.Clear();
            m_sceneEntities.clear();

            m_playerEntity = m_registry.CreateEntity("Player");
            Disparity::Transform playerTransform;
            playerTransform.Position = m_playerPosition;
            m_registry.AddTransform(m_playerEntity, playerTransform);
            m_registry.AddMeshRenderer(m_playerEntity, m_cubeMesh, m_playerBodyMaterial);

            m_sceneEntities.reserve(m_scene.Count());
            for (const Disparity::NamedSceneObject& object : m_scene.GetObjects())
            {
                const Disparity::Entity entity = m_registry.CreateEntity(object.Name);
                m_registry.AddTransform(entity, object.Object.TransformData);
                m_registry.AddMeshRenderer(entity, object.Object.Mesh, object.Object.MaterialData);
                m_sceneEntities.push_back(entity);
            }
        }

        void SyncSceneObjectToRegistry(size_t sceneIndex)
        {
            if (sceneIndex >= m_scene.GetObjects().size() || sceneIndex >= m_sceneEntities.size())
            {
                return;
            }

            const Disparity::NamedSceneObject& object = m_scene.GetObjects()[sceneIndex];
            const Disparity::Entity entity = m_sceneEntities[sceneIndex];
            if (Disparity::TransformComponent* transform = m_registry.GetTransform(entity))
            {
                transform->Value = object.Object.TransformData;
            }
            if (Disparity::MeshRendererComponent* renderer = m_registry.GetMeshRenderer(entity))
            {
                renderer->Mesh = object.Object.Mesh;
                renderer->MaterialData = object.Object.MaterialData;
            }
        }

        void WatchAssets()
        {
            m_hotReloader.Clear();
            if (!m_assetDatabase.Scan("Assets"))
            {
                m_hotReloader.Watch("Assets/Scenes/Prototype.dscene");
                m_hotReloader.Watch("Assets/Scripts/Prototype.dscript");
                m_hotReloader.Watch("Assets/Prefabs/Beacon.dprefab");
                m_hotReloader.Watch("Assets/Meshes/SampleTriangle.gltf");
                m_hotReloader.Watch("Assets/Materials/PlayerBody.dmat");
                m_hotReloader.Watch("Assets/Materials/PlayerHead.dmat");
                m_hotReloader.Watch("Assets/Cinematics/Showcase.dshot");
                return;
            }

            for (const Disparity::AssetRecord& record : m_assetDatabase.GetRecords())
            {
                m_hotReloader.Watch(record.Path);
                for (const std::filesystem::path& dependency : record.Dependencies)
                {
                    m_hotReloader.Watch(dependency);
                }
            }
            m_hotReloader.Watch("Assets/Cinematics/Showcase.dshot");
        }

        void PollHotReload(float dt)
        {
            if (!m_hotReloadEnabled)
            {
                return;
            }

            m_hotReloadPollTimer += dt;
            if (m_hotReloadPollTimer < 0.5f)
            {
                return;
            }

            m_hotReloadPollTimer = 0.0f;
            const std::vector<std::filesystem::path> changed = m_hotReloader.PollChanged();
            if (changed.empty())
            {
                return;
            }

            InitializeMaterials();
            LoadTrailerShotAsset();
            ReloadGltfAssets();
            ReloadSceneAndScript();
            WatchAssets();
            SetStatus("Hot reloaded " + std::to_string(changed.size()) + " asset(s)");
        }

        void LoadRuntimeVerificationAssets()
        {
            if (m_runtimeVerification.InputPlayback)
            {
                LoadRuntimeReplay();
            }

            if (m_runtimeVerification.UseBaseline)
            {
                LoadRuntimeBaseline();
            }
        }

        void LoadRuntimeReplay()
        {
            const std::filesystem::path resolvedPath = Disparity::FileSystem::FindAssetPath(m_runtimeVerification.ReplayPath);
            if (!std::filesystem::exists(resolvedPath))
            {
                AddRuntimeVerificationFailure("runtime replay asset is missing: " + m_runtimeVerification.ReplayPath.string());
                return;
            }

            std::ifstream replay(resolvedPath);
            if (!replay)
            {
                AddRuntimeVerificationFailure("runtime replay asset could not be opened: " + resolvedPath.string());
                return;
            }

            std::string line;
            uint32_t lineNumber = 0;
            while (std::getline(replay, line))
            {
                ++lineNumber;
                line = Trim(line);
                if (line.empty() || line.front() == '#')
                {
                    continue;
                }

                std::istringstream stream(line);
                std::string command;
                RuntimeReplayStep step;
                if (!(stream >> command) || command != "move" ||
                    !(stream >> step.StartFrame >> step.EndFrame >> step.MoveInput.x >> step.MoveInput.z >> step.CameraYawDelta >> step.CameraPitchDelta) ||
                    step.StartFrame > step.EndFrame)
                {
                    AddRuntimeVerificationFailure("runtime replay parse failed at line " + std::to_string(lineNumber) + ".");
                    continue;
                }

                m_runtimeReplaySteps.push_back(step);
                if (m_runtimeReplaySteps.size() == 1)
                {
                    m_runtimeReplayStartFrame = step.StartFrame;
                    m_runtimeReplayEndFrame = step.EndFrame;
                }
                else
                {
                    m_runtimeReplayStartFrame = std::min(m_runtimeReplayStartFrame, step.StartFrame);
                    m_runtimeReplayEndFrame = std::max(m_runtimeReplayEndFrame, step.EndFrame);
                }
            }

            std::sort(m_runtimeReplaySteps.begin(), m_runtimeReplaySteps.end(), [](const RuntimeReplayStep& left, const RuntimeReplayStep& right) {
                return left.StartFrame < right.StartFrame;
            });

            if (m_runtimeReplaySteps.empty())
            {
                AddRuntimeVerificationFailure("runtime replay asset contains no steps.");
                return;
            }

            AddRuntimeVerificationNote("Loaded replay asset " + resolvedPath.string() + ".");
        }

        void LoadRuntimeBaseline()
        {
            const std::filesystem::path resolvedPath = Disparity::FileSystem::FindAssetPath(m_runtimeVerification.BaselinePath);
            if (!std::filesystem::exists(resolvedPath))
            {
                AddRuntimeVerificationFailure("runtime baseline asset is missing: " + m_runtimeVerification.BaselinePath.string());
                return;
            }

            std::ifstream baseline(resolvedPath);
            if (!baseline)
            {
                AddRuntimeVerificationFailure("runtime baseline asset could not be opened: " + resolvedPath.string());
                return;
            }

            RuntimeBaseline loadedBaseline;
            std::string line;
            while (std::getline(baseline, line))
            {
                line = Trim(line);
                if (line.empty() || line.front() == '#')
                {
                    continue;
                }

                const size_t separator = line.find('=');
                if (separator == std::string::npos)
                {
                    continue;
                }

                const std::string key = Trim(line.substr(0, separator));
                const std::string value = Trim(line.substr(separator + 1));
                try
                {
                    if (key == "expected_capture_width") { loadedBaseline.ExpectedCaptureWidth = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "expected_capture_height") { loadedBaseline.ExpectedCaptureHeight = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_editor_pick_tests") { loadedBaseline.MinEditorPickTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_gizmo_pick_tests") { loadedBaseline.MinGizmoPickTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_gizmo_drag_tests") { loadedBaseline.MinGizmoDragTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_scene_reloads") { loadedBaseline.MinSceneReloads = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_scene_saves") { loadedBaseline.MinSceneSaves = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_post_debug_views") { loadedBaseline.MinPostDebugViews = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_showcase_frames") { loadedBaseline.MinShowcaseFrames = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_trailer_frames") { loadedBaseline.MinTrailerFrames = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_high_res_captures") { loadedBaseline.MinHighResCaptures = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_rift_vfx_draws") { loadedBaseline.MinRiftVfxDraws = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_audio_beat_pulses") { loadedBaseline.MinAudioBeatPulses = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_audio_snapshot_tests") { loadedBaseline.MinAudioSnapshotTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_render_graph_allocations") { loadedBaseline.MinRenderGraphAllocations = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_render_graph_aliased_resources") { loadedBaseline.MinRenderGraphAliasedResources = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_render_graph_callbacks") { loadedBaseline.MinRenderGraphCallbacks = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_render_graph_barriers") { loadedBaseline.MinRenderGraphBarriers = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_async_io_tests") { loadedBaseline.MinAsyncIoTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_material_texture_slot_tests") { loadedBaseline.MinMaterialTextureSlotTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_prefab_variant_tests") { loadedBaseline.MinPrefabVariantTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_shot_director_tests") { loadedBaseline.MinShotDirectorTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_audio_analysis_tests") { loadedBaseline.MinAudioAnalysisTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_vfx_system_tests") { loadedBaseline.MinVfxSystemTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_animation_skinning_tests") { loadedBaseline.MinAnimationSkinningTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "require_editor_gpu_pick_resources") { loadedBaseline.RequireEditorGpuPickResources = value == "1" || value == "true"; }
                    else if (key == "expected_average_luma") { loadedBaseline.ExpectedAverageLuma = std::stod(value); }
                    else if (key == "average_luma_tolerance") { loadedBaseline.AverageLumaTolerance = std::stod(value); }
                    else if (key == "min_non_black_ratio") { loadedBaseline.MinNonBlackRatio = std::stod(value); }
                    else if (key == "min_playback_distance") { loadedBaseline.MinPlaybackDistance = std::stod(value); }
                    else if (key == "max_cpu_frame_ms") { loadedBaseline.MaxCpuFrameMs = std::stod(value); }
                    else if (key == "max_gpu_frame_ms") { loadedBaseline.MaxGpuFrameMs = std::stod(value); }
                    else if (key == "max_pass_ms") { loadedBaseline.MaxPassMs = std::stod(value); }
                }
                catch (...)
                {
                    AddRuntimeVerificationFailure("runtime baseline value parse failed for key " + key + ".");
                }
            }

            m_runtimeBaseline = loadedBaseline;
            m_runtimeBaselineLoaded = true;
            m_runtimeVerification.CpuFrameBudgetMs = std::min(m_runtimeVerification.CpuFrameBudgetMs, m_runtimeBaseline.MaxCpuFrameMs);
            m_runtimeVerification.GpuFrameBudgetMs = std::min(m_runtimeVerification.GpuFrameBudgetMs, m_runtimeBaseline.MaxGpuFrameMs);
            m_runtimeVerification.PassBudgetMs = std::min(m_runtimeVerification.PassBudgetMs, m_runtimeBaseline.MaxPassMs);
            AddRuntimeVerificationNote("Loaded baseline asset " + resolvedPath.string() + ".");
        }

        Disparity::Application* m_application = nullptr;
        Disparity::Renderer* m_renderer = nullptr;
        Disparity::Camera m_camera;
        Disparity::Camera m_editorCamera;
        Disparity::Camera m_showcaseCamera;
        Disparity::Camera m_trailerCamera;
        Disparity::Registry m_registry;
        Disparity::Scene m_scene;
        Disparity::AssetDatabase m_assetDatabase;
        Disparity::AssetHotReloader m_hotReloader;
        Disparity::GltfSceneAsset m_gltfSceneAsset;
        Disparity::BobAnimation m_playerBob;
        std::unordered_map<std::string, Disparity::MeshHandle> m_meshes;
        std::vector<Disparity::Entity> m_sceneEntities;
        std::vector<Disparity::MeshHandle> m_gltfMeshes;
        std::vector<Disparity::Material> m_gltfMaterials;
        std::vector<size_t> m_gltfNodeSceneIndices;
        std::vector<size_t> m_multiSelection;
        std::deque<HistoryEntry> m_undoStack;
        std::deque<HistoryEntry> m_redoStack;
        std::deque<std::string> m_commandHistory;
        std::optional<Disparity::NamedSceneObject> m_sceneClipboard;
        std::optional<Disparity::AudioSnapshot> m_audioSnapshot;
        std::optional<EditState> m_gizmoDragBeforeState;
        std::optional<Disparity::RendererSettings> m_runtimeVerificationOriginalRendererSettings;
        std::optional<Disparity::RendererSettings> m_showcaseSavedRendererSettings;
        std::optional<Disparity::RendererSettings> m_trailerSavedRendererSettings;
        std::vector<GizmoDragObject> m_gizmoDragObjects;
        std::vector<std::string> m_runtimeVerificationNotes;
        std::vector<std::string> m_runtimeVerificationFailures;
        std::vector<RuntimeReplayStep> m_runtimeReplaySteps;
        std::vector<TrailerShotKey> m_trailerKeys;
        RuntimeBudgetStats m_runtimeBudgetStats;
        RuntimePlaybackStats m_runtimePlayback;
        EditorVerificationStats m_runtimeEditorStats;
        RuntimeBaseline m_runtimeBaseline;
        RiftVfxSystemStats m_lastRiftVfxStats;
        Disparity::FrameCaptureResult m_runtimeCapture;
        Disparity::Entity m_playerEntity = 0;
        Disparity::MeshHandle m_cubeMesh = 0;
        Disparity::MeshHandle m_planeMesh = 0;
        Disparity::MeshHandle m_terrainMesh = 0;
        Disparity::MeshHandle m_gltfMesh = 0;
        Disparity::MeshHandle m_gizmoPlaneHandleMesh = 0;
        Disparity::MeshHandle m_gizmoRingMesh = 0;
        Disparity::MeshHandle m_vfxQuadMesh = 0;
        Disparity::Material m_playerBodyMaterial;
        Disparity::Material m_playerHeadMaterial;
        Disparity::Material m_shadowMaterial;
        Disparity::Material m_gizmoXMaterial;
        Disparity::Material m_gizmoYMaterial;
        Disparity::Material m_gizmoZMaterial;
        Disparity::Material m_gizmoCenterMaterial;
        Disparity::Material m_gizmoPlaneXYMaterial;
        Disparity::Material m_gizmoPlaneXZMaterial;
        Disparity::Material m_gizmoPlaneYZMaterial;
        Disparity::Material m_riftCoreMaterial;
        Disparity::Material m_riftCyanMaterial;
        Disparity::Material m_riftMagentaMaterial;
        Disparity::Material m_riftVioletMaterial;
        Disparity::Material m_riftObsidianMaterial;
        Disparity::Material m_vfxParticleMaterial;
        Disparity::Material m_vfxHotParticleMaterial;
        Disparity::Material m_vfxRibbonMaterial;
        Disparity::Material m_vfxLightningMaterial;
        Disparity::Material m_vfxFogMaterial;
        DirectX::XMFLOAT3 m_playerPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 m_riftPosition = { 0.0f, 2.25f, -7.4f };
        DirectX::XMFLOAT3 m_gizmoDragStartPivot = {};
        DirectX::XMFLOAT3 m_gizmoDragStartPlayerPosition = {};
        DirectX::XMFLOAT3 m_gizmoDragAxisVector = {};
        DirectX::XMFLOAT3 m_gizmoDragPlaneNormal = {};
        DirectX::XMFLOAT3 m_gizmoDragPlaneStartHit = {};
        DirectX::XMFLOAT2 m_gizmoDragStartMouse = {};
        float m_playerYaw = 0.0f;
        float m_gizmoDragStartPlayerYaw = 0.0f;
        float m_cameraYaw = Pi;
        float m_cameraPitch = 0.42f;
        float m_cameraDistance = 7.0f;
        float m_editorCameraYaw = Pi;
        float m_editorCameraPitch = 0.52f;
        float m_editorCameraDistance = 9.0f;
        float m_playerBobOffset = 0.0f;
        float m_sceneAnimationTime = 0.0f;
        float m_showcaseTime = 0.0f;
        float m_trailerTime = 0.0f;
        float m_trailerFocus = 0.985f;
        float m_trailerDofStrength = 0.45f;
        float m_trailerLensDirt = 0.62f;
        float m_trailerLetterbox = 0.075f;
        float m_trailerRendererPulse = 0.0f;
        float m_trailerAudioCue = 0.0f;
        float m_riftBeatPulse = 0.0f;
        float m_statusTimer = 0.0f;
        float m_hotReloadPollTimer = 0.0f;
        float m_runtimeVerificationElapsed = 0.0f;
        int m_lastRiftBeatIndex = -1;
        size_t m_selectedIndex = 0;
        uint32_t m_runtimeVerificationFrame = 0;
        uint32_t m_runtimeReplayStartFrame = 0;
        uint32_t m_runtimeReplayEndFrame = 0;
        uint32_t m_runtimeVerificationTrailerStartFrame = 0;
        uint32_t m_runtimeBudgetSkipFrames = 0;
        DirectX::XMFLOAT3 m_editorCameraTarget = { 0.0f, 1.1f, 0.0f };
        DirectX::XMFLOAT2 m_editorLastMousePosition = {};
        EditorViewportState m_editorViewport;
        EditorPickResult m_hoverPick;
        RuntimeVerificationConfig m_runtimeVerification;
        bool m_selectedPlayer = true;
        bool m_editorVisible = true;
        bool m_showcaseSavedEditorVisible = true;
        bool m_trailerSavedEditorVisible = true;
        bool m_showcaseMode = false;
        bool m_trailerMode = false;
        bool m_cinematicAudioCues = false;
        bool m_editorCameraEnabled = false;
        bool m_hotReloadEnabled = true;
        bool m_gltfAnimationPlayback = true;
        bool m_applyingHistory = false;
        bool m_runtimeVerificationFinished = false;
        bool m_runtimeVerificationReloadedScene = false;
        bool m_runtimeVerificationSavedScene = false;
        bool m_runtimeVerificationExercisedRenderer = false;
        bool m_runtimeVerificationCycledSelection = false;
        bool m_runtimeVerificationValidatedEditorPrecision = false;
        bool m_runtimeVerificationCaptureRequested = false;
        bool m_runtimeVerificationCaptureValidated = false;
        bool m_runtimeVerificationStartedShowcase = false;
        bool m_runtimeVerificationStoppedShowcase = false;
        bool m_runtimeVerificationStartedTrailer = false;
        bool m_runtimeVerificationStoppedTrailer = false;
        bool m_runtimeVerificationRequestedHighResCapture = false;
        bool m_runtimeBaselineLoaded = false;
        bool m_highResCapturePending = false;
        bool m_gizmoDragging = false;
        bool m_gizmoDragMoved = false;
        GizmoAxis m_gizmoDragAxis = GizmoAxis::None;
        GizmoPlane m_gizmoDragPlane = GizmoPlane::None;
        GizmoMode m_gizmoMode = GizmoMode::Translate;
        GizmoMode m_gizmoDragMode = GizmoMode::Translate;
        GizmoSpace m_gizmoSpace = GizmoSpace::World;
        GizmoSpace m_gizmoDragSpace = GizmoSpace::World;
        std::string m_statusMessage;
        std::string m_lastPickStatus = "None";
        std::string m_lastGpuPickStatus = "Not sampled";
        std::string m_gizmoStatus = "Idle";
        std::filesystem::path m_highResCaptureSourcePath;
        std::filesystem::path m_highResCaptureOutputPath;
        std::filesystem::path m_highResCaptureNativePngPath;
    };
}

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE previousInstance, _In_ PWSTR commandLine, _In_ int commandShow)
{
    (void)previousInstance;
    (void)commandShow;

    RuntimeVerificationConfig runtimeVerification;
    const std::wstring arguments = commandLine ? commandLine : L"";
    runtimeVerification.Enabled = HasArgument(arguments, L"--verify-runtime");
    runtimeVerification.CaptureFrame = !HasArgument(arguments, L"--verify-no-capture");
    runtimeVerification.InputPlayback = !HasArgument(arguments, L"--verify-no-input-playback");
    runtimeVerification.EnforceBudgets = !HasArgument(arguments, L"--verify-no-budgets");
    runtimeVerification.UseBaseline = !HasArgument(arguments, L"--verify-no-baseline");
    runtimeVerification.TargetFrames = std::max(60u, ParseUnsignedArgument(arguments, L"--verify-frames=", runtimeVerification.TargetFrames));
    runtimeVerification.CpuFrameBudgetMs = std::max(1.0, ParseDoubleArgument(arguments, L"--verify-cpu-budget-ms=", runtimeVerification.CpuFrameBudgetMs));
    runtimeVerification.GpuFrameBudgetMs = std::max(1.0, ParseDoubleArgument(arguments, L"--verify-gpu-budget-ms=", runtimeVerification.GpuFrameBudgetMs));
    runtimeVerification.PassBudgetMs = std::max(1.0, ParseDoubleArgument(arguments, L"--verify-pass-budget-ms=", runtimeVerification.PassBudgetMs));
    runtimeVerification.ReplayPath = ParsePathArgument(arguments, L"--verify-replay=", runtimeVerification.ReplayPath);
    runtimeVerification.BaselinePath = ParsePathArgument(arguments, L"--verify-baseline=", runtimeVerification.BaselinePath);

    Disparity::Application app({ instance, L"DISPARITY", 1280, 720 });
    app.SetLayer(std::make_unique<DisparityGameLayer>(runtimeVerification));
    return app.Run();
}
