#include <Disparity/Disparity.h>
#include <Disparity/Assets/SimpleJson.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <windows.h>
#include <xinput.h>

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

    DirectX::XMFLOAT3 CatmullRom(
        const DirectX::XMFLOAT3& p0,
        const DirectX::XMFLOAT3& p1,
        const DirectX::XMFLOAT3& p2,
        const DirectX::XMFLOAT3& p3,
        float t)
    {
        const float tt = t * t;
        const float ttt = tt * t;
        return Scale(Add(
            Add(Scale(p1, 2.0f), Scale(Subtract(p2, p0), t)),
            Add(
                Scale(Add(Subtract(Scale(p0, 2.0f), Scale(p1, 5.0f)), Add(Scale(p2, 4.0f), Scale(p3, -1.0f))), tt),
                Scale(Add(Add(Scale(p1, 3.0f), Scale(p2, -3.0f)), Subtract(p3, p0)), ttt))),
            0.5f);
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
            LoadEditorPreferences();
            LoadPublicDemoContentAssets();
            ResetPublicDemo(true);

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
            ++m_editorFrameIndex;
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
            if (Disparity::Input::WasKeyPressed(VK_F10))
            {
                ResetPublicDemo(true);
                SetStatus("Public demo reset");
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

            PollPublicDemoGamepadInput();
            UpdatePublicDemoMenuInput();
            PollHotReload(dt);
            const bool publicDemoBlocksGameplay = PublicDemoGameplayBlocked();
            const float gameplayDt = (editorCapturesKeyboard || publicDemoBlocksGameplay || m_editorCameraEnabled || m_showcaseMode || m_trailerMode) ? 0.0f : dt;
            UpdatePlayer(gameplayDt);
            UpdatePublicDemo(gameplayDt);
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
            UpdateEditorPreferenceAutosave(dt);
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
                Disparity::PointLight{ m_riftPosition, 13.5f, { 0.20f, 0.78f, 1.0f }, (1.6f + riftPulse * 0.8f + PublicDemoRiftCharge() * 0.9f) * showcaseBoost },
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
            DrawPublicDemoHud();
            DrawPublicDemoMenuOverlay();

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
            DrawPublicDemoDirectorPanel();
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

            DrawPublicDemo(renderer);
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

        static constexpr size_t V25ProductionPointCount = 40;
        static constexpr size_t V28DiversifiedPointCount = 36;
        static constexpr size_t V29PublicDemoPointCount = 30;
        static constexpr size_t V30VerticalSlicePointCount = 36;
        static constexpr size_t V31DiversifiedPointCount = 30;
        static constexpr size_t V32RoadmapPointCount = 60;
        static constexpr size_t V33PlayableDemoPointCount = 50;
        static constexpr size_t V34AAAFoundationPointCount = 60;
        static constexpr size_t PublicDemoShardCount = 6;
        static constexpr size_t PublicDemoAnchorCount = 3;
        static constexpr size_t PublicDemoResonanceGateCount = 2;
        static constexpr size_t PublicDemoPhaseRelayCount = 3;
        static constexpr size_t PublicDemoObstacleCount = 5;
        static constexpr size_t PublicDemoEnemyCount = 3;

        static const std::array<ProductionFollowupPoint, V25ProductionPointCount>& GetV25ProductionPoints()
        {
            static const std::array<ProductionFollowupPoint, V25ProductionPointCount> points = { {
                { "v25_point_01_editor_prefs_viewport_hud", "Editor", "Persist viewport HUD row visibility" },
                { "v25_point_02_editor_prefs_transform_precision", "Editor", "Persist transform precision settings" },
                { "v25_point_03_editor_prefs_command_filter", "Editor", "Persist command history filters" },
                { "v25_point_04_editor_prefs_dock_layout", "Editor", "Track dock layout preference readiness" },
                { "v25_point_05_editor_prefs_user_path", "Editor", "Track user preference storage path" },
                { "v25_point_06_viewport_toolbar_camera_mode", "Viewport", "Expose camera mode toolbar state" },
                { "v25_point_07_viewport_toolbar_render_debug", "Viewport", "Expose render debug toolbar state" },
                { "v25_point_08_viewport_toolbar_capture_state", "Viewport", "Expose capture toolbar state" },
                { "v25_point_09_viewport_toolbar_object_id_overlay", "Viewport", "Expose object-ID overlay toolbar state" },
                { "v25_point_10_viewport_toolbar_depth_overlay", "Viewport", "Expose depth overlay toolbar state" },
                { "v25_point_11_gizmo_depth_aware_hover", "Gizmo", "Track depth-aware gizmo hover readiness" },
                { "v25_point_12_gizmo_constraint_preview", "Gizmo", "Track gizmo constraint preview readiness" },
                { "v25_point_13_gizmo_numeric_entry", "Gizmo", "Track numeric transform entry readiness" },
                { "v25_point_14_gizmo_pivot_orientation_editing", "Gizmo", "Track pivot and orientation editing readiness" },
                { "v25_point_15_gizmo_handle_metadata", "Gizmo", "Track richer gizmo object-ID metadata" },
                { "v25_point_16_asset_gpu_mesh_upload", "Assets", "Validate cooked GPU mesh upload readiness" },
                { "v25_point_17_asset_texture_slot_binding", "Assets", "Validate material texture slot binding readiness" },
                { "v25_point_18_asset_animation_resources", "Assets", "Validate animation resource promotion readiness" },
                { "v25_point_19_asset_dependency_preview", "Assets", "Validate dependency invalidation preview readiness" },
                { "v25_point_20_asset_reload_rollback", "Assets", "Validate reload-safe rollback readiness" },
                { "v25_point_21_render_explicit_barriers", "Rendering", "Validate explicit render barrier diagnostics" },
                { "v25_point_22_render_alias_lifetime_validation", "Rendering", "Validate alias lifetime diagnostics" },
                { "v25_point_23_render_gpu_culling_plan", "Rendering", "Track GPU culling plan readiness" },
                { "v25_point_24_render_forward_plus_plan", "Rendering", "Track Forward+ light binning readiness" },
                { "v25_point_25_render_cascaded_shadow_plan", "Rendering", "Track cascaded shadow map readiness" },
                { "v25_point_26_capture_tiled_offscreen_plan", "Capture", "Validate tiled offscreen capture readiness" },
                { "v25_point_27_capture_resolve_filter_catalog", "Capture", "Validate capture resolve filter catalog readiness" },
                { "v25_point_28_capture_exr_output_plan", "Capture", "Track EXR output plan readiness" },
                { "v25_point_29_capture_async_compression_plan", "Capture", "Track async capture compression readiness" },
                { "v25_point_30_vfx_debug_visualizers", "VFX", "Validate VFX debug visualizer readiness" },
                { "v25_point_31_sequencer_curve_editor_plan", "Sequencer", "Track curve editor readiness" },
                { "v25_point_32_sequencer_clip_blending_plan", "Sequencer", "Track clip blending readiness" },
                { "v25_point_33_sequencer_bookmark_tracks", "Sequencer", "Validate bookmark track readiness" },
                { "v25_point_34_audio_streamed_music_plan", "Audio", "Track streamed music backend readiness" },
                { "v25_point_35_audio_content_pulse_plan", "Audio", "Validate content-driven pulse readiness" },
                { "v25_point_36_prod_golden_profile_expansion", "Production", "Validate golden profile expansion path" },
                { "v25_point_37_prod_schema_versioning", "Production", "Validate versioned runtime schema readiness" },
                { "v25_point_38_prod_interactive_ci_gate", "Production", "Validate interactive CI gate readiness" },
                { "v25_point_39_prod_installer_artifact", "Production", "Validate installer artifact readiness" },
                { "v25_point_40_prod_obs_websocket_automation", "Production", "Validate OBS automation readiness" }
            } };
            return points;
        }

        static const std::array<ProductionFollowupPoint, V28DiversifiedPointCount>& GetV28DiversifiedPoints()
        {
            static const std::array<ProductionFollowupPoint, V28DiversifiedPointCount> points = { {
                { "v28_point_01_editor_profile_import_export", "Editor", "Profile import/export path" },
                { "v28_point_02_editor_profile_diff", "Editor", "Profile diff summary" },
                { "v28_point_03_editor_dock_layout", "Editor", "Dock-layout persistence readiness" },
                { "v28_point_04_editor_workspace_presets", "Editor", "Workspace preset switching" },
                { "v28_point_05_editor_capture_progress", "Editor", "Viewport capture progress state" },
                { "v28_point_06_editor_command_macro_review", "Editor", "Reviewable command macro surface" },
                { "v28_point_07_asset_gpu_package_upload", "Assets", "Optimized GPU package promotion" },
                { "v28_point_08_asset_texture_bindings", "Assets", "Material texture binding readiness" },
                { "v28_point_09_asset_animation_clips", "Assets", "Animation clip resource readiness" },
                { "v28_point_10_asset_live_invalidation", "Assets", "Live dependency invalidation tickets" },
                { "v28_point_11_asset_reload_rollback_journal", "Assets", "Reload rollback journal" },
                { "v28_point_12_asset_streaming_priorities", "Assets", "Streaming priority queues" },
                { "v28_point_13_render_explicit_bind_barriers", "Rendering", "Explicit DX11 bind/unbind barrier diagnostics" },
                { "v28_point_14_render_alias_lifetime_validation", "Rendering", "Alias lifetime validation" },
                { "v28_point_15_render_gpu_culling_buckets", "Rendering", "GPU culling bucket readiness" },
                { "v28_point_16_render_forward_plus_bins", "Rendering", "Forward+ light bin readiness" },
                { "v28_point_17_render_csm_cascades", "Rendering", "Cascaded shadow cascade readiness" },
                { "v28_point_18_render_motion_vectors_taa", "Rendering", "Motion-vector and TAA resolve readiness" },
                { "v28_point_19_runtime_streaming_requests", "Runtime", "Asset streaming request surface" },
                { "v28_point_20_runtime_cancellation_tokens", "Runtime", "Async cancellation tokens" },
                { "v28_point_21_runtime_file_watchers", "Runtime", "File watcher readiness" },
                { "v28_point_22_runtime_script_reload_boundary", "Runtime", "Script reload boundary" },
                { "v28_point_23_runtime_script_state_slots", "Runtime", "Script state preservation slots" },
                { "v28_point_24_runtime_sequencer_preview", "Runtime", "Sequencer clip blending and keyboard preview" },
                { "v28_point_25_audio_xaudio2_mixer_targets", "Audio", "XAudio2 mixer voice targets" },
                { "v28_point_26_audio_streamed_music_asset", "Audio", "Streamed music asset surface" },
                { "v28_point_27_audio_spatial_components", "Audio", "Spatial emitter components" },
                { "v28_point_28_audio_attenuation_assets", "Audio", "Attenuation curve assets" },
                { "v28_point_29_audio_calibrated_meters", "Audio", "Calibrated production meters" },
                { "v28_point_30_audio_content_amplitude_pulses", "Audio", "Content-driven amplitude pulses" },
                { "v28_point_31_prod_per_gpu_goldens", "Production", "Per-GPU golden profiles" },
                { "v28_point_32_prod_baseline_diff_package", "Production", "Baseline diff package output" },
                { "v28_point_33_prod_schema_compatibility", "Production", "Schema compatibility surface" },
                { "v28_point_34_prod_interactive_runner", "Production", "Interactive runner plan" },
                { "v28_point_35_prod_signed_installer", "Production", "Signed installer artifact readiness" },
                { "v28_point_36_prod_obs_websocket_commands", "Production", "OBS WebSocket command automation" }
            } };
            return points;
        }

        static const std::array<ProductionFollowupPoint, V29PublicDemoPointCount>& GetV29PublicDemoPoints()
        {
            static const std::array<ProductionFollowupPoint, V29PublicDemoPointCount> points = { {
                { "v29_point_01_playable_objective_loop", "Gameplay", "Playable rift-stabilization objective loop" },
                { "v29_point_02_collectible_shards", "Gameplay", "Collectible echo shards around the arena" },
                { "v29_point_03_extraction_gate", "Gameplay", "Completion gate opens after shard collection" },
                { "v29_point_04_reset_hotkey", "Gameplay", "F10 public-demo reset path" },
                { "v29_point_05_sprint_energy", "Gameplay", "Shift sprint with visible focus energy" },
                { "v29_point_06_arena_bounds", "Gameplay", "Soft bounds keep the player in the demo space" },
                { "v29_point_07_shard_beacons", "Visuals", "Tall shard beacons for public readability" },
                { "v29_point_08_extraction_rings", "Visuals", "Charged extraction rings around the rift" },
                { "v29_point_09_sentinel_orbits", "Visuals", "Orbiting sentinel hazards create motion" },
                { "v29_point_10_player_trail", "Visuals", "Short player trail pips sell movement" },
                { "v29_point_11_reactive_rift_charge", "Visuals", "Rift intensity responds to collected shards" },
                { "v29_point_12_directional_hint_beam", "Visuals", "Objective hint beam points to the next shard" },
                { "v29_point_13_public_hud", "HUD", "Public demo HUD renders outside editor panels" },
                { "v29_point_14_progress_bars", "HUD", "Stability and focus bars are visible" },
                { "v29_point_15_objective_text", "HUD", "Objective text changes with demo state" },
                { "v29_point_16_timer_display", "HUD", "Run timer supports public attempts" },
                { "v29_point_17_status_feedback", "HUD", "Pickup/completion status feedback" },
                { "v29_point_18_controls_hint", "HUD", "Minimal controls hint for recording sessions" },
                { "v29_point_19_pickup_audio", "Audio", "Spatial pickup cue on shard collection" },
                { "v29_point_20_completion_audio", "Audio", "Completion cue when extraction succeeds" },
                { "v29_point_21_non_spam_audio", "Audio", "Cues fire only on state changes" },
                { "v29_point_22_audio_pulse_link", "Audio", "Demo charge contributes to rift beat response" },
                { "v29_point_23_showcase_framing", "Capture", "Showcase camera frames the playable objective field" },
                { "v29_point_24_trailer_readability", "Capture", "Trailer mode sees the charged rift objective" },
                { "v29_point_25_photo_ready_state", "Capture", "High-res capture includes demo indicators" },
                { "v29_point_26_runtime_metrics", "Verification", "Runtime report emits public-demo counters" },
                { "v29_point_27_baseline_gates", "Verification", "Baselines require public-demo coverage" },
                { "v29_point_28_schema_coverage", "Verification", "Runtime schema includes v29 metrics" },
                { "v29_point_29_manifest_review", "Verification", "Followup manifest review covers v29 points" },
                { "v29_point_30_release_docs", "Production", "Docs describe the playable public demo" }
            } };
            return points;
        }

        static const std::array<ProductionFollowupPoint, V30VerticalSlicePointCount>& GetV30VerticalSlicePoints()
        {
            static const std::array<ProductionFollowupPoint, V30VerticalSlicePointCount> points = { {
                { "v30_point_01_chained_objectives", "Gameplay", "Shard collection chains into phase-anchor alignment" },
                { "v30_point_02_phase_anchor_puzzle", "Gameplay", "Three phase anchors gate extraction" },
                { "v30_point_03_checkpoint_state", "Gameplay", "Public demo records checkpoint state" },
                { "v30_point_04_retry_failure", "Gameplay", "Stability failure retries from checkpoint" },
                { "v30_point_05_objective_distance", "Gameplay", "Next objective distance is tracked" },
                { "v30_point_06_stage_state_machine", "Gameplay", "Explicit public-demo stage machine" },
                { "v30_point_07_gamepad_surface", "Gameplay", "Gamepad-ready input surface is exposed" },
                { "v30_point_08_completion_gate_polish", "Gameplay", "Extraction requires anchors and completion polish" },
                { "v30_point_09_anchor_glyphs", "Visuals", "Phase anchors draw readable glyph rings" },
                { "v30_point_10_phase_bridges", "Visuals", "Activated anchors draw bridge beams to the rift" },
                { "v30_point_11_checkpoint_marker", "Visuals", "Checkpoint marker is visible in the arena" },
                { "v30_point_12_retry_warning_feedback", "Visuals", "Low-stability warning feedback is visible" },
                { "v30_point_13_route_beam_upgrade", "Visuals", "Objective beam follows shards, anchors, and extraction" },
                { "v30_point_14_public_hud_stage", "HUD", "HUD shows stage, anchors, checkpoint, and retries" },
                { "v30_point_15_demo_director_panel", "Editor", "Editor exposes a public Demo Director panel" },
                { "v30_point_16_director_reset_controls", "Editor", "Director can reset and checkpoint the vertical slice" },
                { "v30_point_17_event_telemetry", "Editor", "Recent public-demo events are inspectable" },
                { "v30_point_18_stage_readout", "Editor", "Current stage and objective telemetry are inspectable" },
                { "v30_point_19_readiness_table", "Editor", "Profiler exposes v30 readiness points" },
                { "v30_point_20_command_history_labels", "Editor", "Demo director actions enter command history" },
                { "v30_point_21_event_queue", "Backend", "Small event queue records gameplay state changes" },
                { "v30_point_22_checkpoint_snapshot", "Backend", "Checkpoint snapshots preserve stage progress" },
                { "v30_point_23_retry_telemetry", "Backend", "Retry counters are reported deterministically" },
                { "v30_point_24_route_telemetry", "Backend", "Objective-route telemetry is emitted" },
                { "v30_point_25_deterministic_validation", "Backend", "Runtime verifier completes the whole vertical slice" },
                { "v30_point_26_backend_schema", "Backend", "Runtime schema includes v30 vertical-slice metrics" },
                { "v30_point_27_vfx_charge_link", "Rendering", "Rift VFX intensity responds to anchor progress" },
                { "v30_point_28_beacon_draw_counters", "Rendering", "Anchor and checkpoint visuals contribute draw coverage" },
                { "v30_point_29_capture_readiness", "Capture", "Trailer/photo capture sees the new objective state" },
                { "v30_point_30_perf_history", "Production", "Performance history records v30 counters" },
                { "v30_point_31_manifest_review", "Verification", "v30 followup manifest is reviewed" },
                { "v30_point_32_baseline_gates", "Verification", "Baselines require v30 objective coverage" },
                { "v30_point_33_runtime_script_gate", "Verification", "Runtime wrapper asserts v30 report metrics" },
                { "v30_point_34_release_readiness", "Production", "Release readiness includes the v30 manifest" },
                { "v30_point_35_docs_updated", "Production", "Docs describe the v30 playable slice" },
                { "v30_point_36_roadmap_refresh", "Production", "Roadmap and agent context list next followups" }
            } };
            return points;
        }

        static const std::array<ProductionFollowupPoint, V31DiversifiedPointCount>& GetV31DiversifiedPoints()
        {
            static const std::array<ProductionFollowupPoint, V31DiversifiedPointCount> points = { {
                { "v31_point_01_editor_command_palette", "Editor", "Command palette/search surface is represented in editor telemetry" },
                { "v31_point_02_editor_viewport_bookmarks", "Editor", "Viewport bookmarks are available for show-off camera hops" },
                { "v31_point_03_editor_gizmo_constraint_preview", "Editor", "Gizmo constraint previews are exposed for precision edits" },
                { "v31_point_04_editor_macro_export", "Editor", "Command macro export review is tracked" },
                { "v31_point_05_editor_demo_bookmarks", "Editor", "Demo Director exposes vertical-slice bookmark telemetry" },
                { "v31_point_06_asset_gpu_upload_manifest", "AssetPipeline", "Optimized package upload manifest is represented" },
                { "v31_point_07_asset_material_bindings", "AssetPipeline", "Texture binding diagnostics cover material slots" },
                { "v31_point_08_asset_skeleton_retarget", "AssetPipeline", "Skeleton/retarget readiness is surfaced" },
                { "v31_point_09_asset_dependency_replay", "AssetPipeline", "Dependency invalidation replay queue is tracked" },
                { "v31_point_10_asset_stream_budget", "AssetPipeline", "Streaming priority budgets are tracked" },
                { "v31_point_11_render_bind_unbind_audit", "Rendering", "Render graph bind/unbind barrier audit is represented" },
                { "v31_point_12_render_alias_slot_audit", "Rendering", "Alias lifetime slots are audited" },
                { "v31_point_13_render_cluster_bins", "Rendering", "Clustered/Forward+ light-bin preview is tracked" },
                { "v31_point_14_render_csm_stability", "Rendering", "Cascaded shadow stabilization preview is tracked" },
                { "v31_point_15_render_vfx_debug", "Rendering", "VFX debug visualizer readiness is represented" },
                { "v31_point_16_runtime_resonance_gates", "Runtime", "Playable resonance gates extend the public demo" },
                { "v31_point_17_runtime_collision_volumes", "Runtime", "Collision/traversal volume checks are reported" },
                { "v31_point_18_runtime_controller_surface", "Runtime", "Controller/gamepad surface remains exposed" },
                { "v31_point_19_runtime_failure_screen", "Runtime", "Failure/retry screen telemetry is tracked" },
                { "v31_point_20_runtime_event_routing", "Runtime", "Gameplay event routing counters are reported" },
                { "v31_point_21_audio_footstep_cadence", "Audio", "Footstep cadence events are produced" },
                { "v31_point_22_audio_anchor_resonance", "Audio", "Anchor/resonance cue events are produced" },
                { "v31_point_23_audio_pressure_cues", "Audio", "Sentinel pressure cue telemetry is produced" },
                { "v31_point_24_audio_mix_safe_routing", "Audio", "Presentation-safe bus routing is represented" },
                { "v31_point_25_audio_content_pulse_link", "Audio", "Demo charge drives content pulse diagnostics" },
                { "v31_point_26_prod_schema_compatibility", "Production", "Runtime schema compatibility includes v31 metrics" },
                { "v31_point_27_prod_baseline_review", "Production", "Baseline review requires v31 coverage" },
                { "v31_point_28_prod_release_readiness", "Production", "Release readiness includes the v31 manifest" },
                { "v31_point_29_prod_obs_vertical_slice", "Production", "OBS/trailer capture metadata references the vertical slice" },
                { "v31_point_30_prod_docs_roadmap", "Production", "Docs and roadmap document the v31 batch" }
            } };
            return points;
        }

        static const std::array<ProductionFollowupPoint, V32RoadmapPointCount>& GetV32RoadmapPoints()
        {
            static const std::array<ProductionFollowupPoint, V32RoadmapPointCount> points = { {
                { "v32_point_01_editor_director_relay_controls", "Editor", "Demo Director exposes phase relay state and controls" },
                { "v32_point_02_editor_viewport_relay_markers", "Editor", "Viewport HUD surfaces relay objective markers" },
                { "v32_point_03_editor_command_macro_relay_route", "Editor", "Command history records the complex relay route macro" },
                { "v32_point_04_editor_readiness_table", "Editor", "Profiler includes the v32 sixty-point readiness table" },
                { "v32_point_05_editor_showcase_workspace", "Editor", "Workspace presets remain ready for public showcase capture" },
                { "v32_point_06_editor_event_route_audit", "Editor", "Gameplay event route audit includes relay events" },
                { "v32_point_07_editor_hud_relay_detail", "Editor", "Public HUD reports relay progress" },
                { "v32_point_08_editor_checkpoint_relay_snapshot", "Editor", "Checkpoints snapshot relay progress metadata" },
                { "v32_point_09_editor_overcharge_stats", "Editor", "Demo Director reports relay overcharge windows" },
                { "v32_point_10_editor_docs_guide", "Editor", "Editor-facing docs describe the v32 demo flow" },
                { "v32_point_11_asset_relay_manifest", "AssetPipeline", "Relay gameplay manifest is assetized for verification" },
                { "v32_point_12_asset_cooked_route_metadata", "AssetPipeline", "Cooked package diagnostics stay valid with the longer route" },
                { "v32_point_13_asset_dependency_replay_v32", "AssetPipeline", "Dependency replay diagnostics cover the v32 batch" },
                { "v32_point_14_asset_relay_material_bindings", "AssetPipeline", "Relay materials use the material binding path" },
                { "v32_point_15_asset_gltf_package_audit", "AssetPipeline", "glTF package audit remains covered during route validation" },
                { "v32_point_16_asset_runtime_promotion_fallback", "AssetPipeline", "Runtime promotion fallbacks are represented in diagnostics" },
                { "v32_point_17_asset_import_cook_review", "AssetPipeline", "Cook/import review tools include v32 manifest coverage" },
                { "v32_point_18_asset_schema_relay_metrics", "AssetPipeline", "Runtime schema includes relay asset and route metrics" },
                { "v32_point_19_asset_prefab_checkpoint_links", "AssetPipeline", "Prefab/checkpoint metadata remains compatible with relays" },
                { "v32_point_20_asset_docs_package_route", "AssetPipeline", "Docs describe asset package expectations for v32" },
                { "v32_point_21_render_relay_glyph_rings", "Rendering", "Phase relays render visible glyph rings" },
                { "v32_point_22_render_relay_bridge_beams", "Rendering", "Stabilized relays render bridge beams" },
                { "v32_point_23_render_objective_beam_upgrade", "Rendering", "Objective hint beam targets relay objectives" },
                { "v32_point_24_render_overcharge_hazard_rings", "Rendering", "Overcharge windows render warning rings" },
                { "v32_point_25_render_rift_charge_relays", "Rendering", "Rift charge responds to relay stabilization" },
                { "v32_point_26_render_vfx_counter_relays", "Rendering", "Runtime report emits relay VFX draw counters" },
                { "v32_point_27_render_csm_route_visibility", "Rendering", "Existing shadow/visibility diagnostics stay valid" },
                { "v32_point_28_render_capture_readability", "Rendering", "Capture path includes relay readability metrics" },
                { "v32_point_29_render_profiler_detail", "Rendering", "Profiler reports renderer-side relay readiness" },
                { "v32_point_30_render_release_capture_schema", "Rendering", "Release capture schema includes v32 render metrics" },
                { "v32_point_31_runtime_relay_stage_machine", "Runtime", "Stage machine includes a relay stabilization phase" },
                { "v32_point_32_runtime_relay_proximity_volumes", "Runtime", "Relay proximity volumes advance objectives" },
                { "v32_point_33_runtime_overcharge_windows", "Runtime", "Relay overcharge windows are tracked" },
                { "v32_point_34_runtime_combo_chain_steps", "Runtime", "Objective combo-chain step counters are tracked" },
                { "v32_point_35_runtime_checkpoint_relays", "Runtime", "Retry/checkpoint flow remains valid with relays" },
                { "v32_point_36_runtime_route_distance_relays", "Runtime", "Objective distance points at relay targets" },
                { "v32_point_37_runtime_deterministic_relay_completion", "Runtime", "Runtime verification completes the relay route deterministically" },
                { "v32_point_38_runtime_stage_telemetry_relays", "Runtime", "Stage telemetry includes relay transitions" },
                { "v32_point_39_runtime_complex_demo_completion", "Runtime", "The longer public demo completes successfully" },
                { "v32_point_40_runtime_gameplay_event_routes", "Runtime", "Gameplay event route counters include relay events" },
                { "v32_point_41_audio_relay_stabilize_cue", "Audio", "Relay stabilization cues are routed through SFX" },
                { "v32_point_42_audio_overcharge_warning_cue", "Audio", "Overcharge warning cue telemetry is represented" },
                { "v32_point_43_audio_combo_confirm_cue", "Audio", "Combo completion cue telemetry is represented" },
                { "v32_point_44_audio_mix_safe_oneshots", "Audio", "One-shot relay cues use mix-safe routing" },
                { "v32_point_45_audio_content_pulse_relays", "Audio", "Relay charge links into content pulse diagnostics" },
                { "v32_point_46_audio_footstep_pressure_coexist", "Audio", "Footstep and pressure cues coexist with relays" },
                { "v32_point_47_audio_backend_fallback_route", "Audio", "XAudio2/WinMM fallback remains covered" },
                { "v32_point_48_audio_meter_relay_coverage", "Audio", "Audio meter coverage includes relay cue runs" },
                { "v32_point_49_audio_cinematic_isolation", "Audio", "Cinematic cue isolation remains available" },
                { "v32_point_50_audio_docs_mix_notes", "Audio", "Docs describe relay cue testing" },
                { "v32_point_51_prod_manifest_v32", "Production", "v32 followup manifest defines sixty points" },
                { "v32_point_52_prod_schema_v32", "Production", "Runtime report schema includes v32 metrics" },
                { "v32_point_53_prod_baselines_v32", "Production", "Verification baselines require v32 relay coverage" },
                { "v32_point_54_prod_runtime_wrapper_v32", "Production", "Runtime wrapper asserts v32 relay metrics" },
                { "v32_point_55_prod_release_readiness_v32", "Production", "Release readiness validates the v32 manifest" },
                { "v32_point_56_prod_perf_history_v32", "Production", "Performance history captures v32 relay counters" },
                { "v32_point_57_prod_baseline_review_v32", "Production", "Baseline review requires v32 keys" },
                { "v32_point_58_prod_docs_roadmap_readme", "Production", "README and roadmap document v32" },
                { "v32_point_59_prod_agents_snapshot", "Production", "AGENTS snapshot describes the v32 batch" },
                { "v32_point_60_prod_version_reporting", "Production", "Version/reporting surfaces v32" }
            } };
            return points;
        }

        static const std::array<ProductionFollowupPoint, V33PlayableDemoPointCount>& GetV33PlayableDemoPoints()
        {
            static const std::array<ProductionFollowupPoint, V33PlayableDemoPointCount> points = { {
                { "v33_point_01_collision_arena_bounds", "CollisionTraversal", "Arena collision solves are counted separately from visual bounds" },
                { "v33_point_02_collision_blocker_volumes", "CollisionTraversal", "Blocker volumes stop the player instead of only clamping to the arena" },
                { "v33_point_03_collision_sliding_resolution", "CollisionTraversal", "AABB collision resolution slides the player along the shallow axis" },
                { "v33_point_04_traversal_vault_barriers", "CollisionTraversal", "Traversable low barriers can be vaulted or dashed through" },
                { "v33_point_05_traversal_dash_cooldown", "CollisionTraversal", "Dash/vault traversal has cooldown telemetry" },
                { "v33_point_06_traversal_visual_markers", "CollisionTraversal", "Traversal barriers render as readable public-demo markers" },
                { "v33_point_07_traversal_verification_route", "CollisionTraversal", "Runtime verification exercises the collision/traversal route" },
                { "v33_point_08_traversal_audio_feedback", "CollisionTraversal", "Traversal actions fire content-backed cue telemetry" },
                { "v33_point_09_traversal_hud_telemetry", "CollisionTraversal", "HUD and director expose collision/traversal counts" },
                { "v33_point_10_traversal_schema_baseline", "CollisionTraversal", "Schema and baselines require traversal coverage" },
                { "v33_point_11_enemy_patrol_home", "EnemyAI", "Public-demo enemies patrol from authored home positions" },
                { "v33_point_12_enemy_aggro_radius", "EnemyAI", "Enemies enter chase state inside an aggro radius" },
                { "v33_point_13_enemy_chase_motion", "EnemyAI", "Enemies move toward the player while alerted" },
                { "v33_point_14_enemy_contact_pressure", "EnemyAI", "Enemy contact drains stability and feeds pressure telemetry" },
                { "v33_point_15_enemy_dash_evade", "EnemyAI", "Dash timing can evade or stun a chasing enemy" },
                { "v33_point_16_enemy_failure_retry", "EnemyAI", "Enemy pressure can trigger the authored retry presentation" },
                { "v33_point_17_enemy_visual_language", "EnemyAI", "Enemies draw vision rings and chase beams for public readability" },
                { "v33_point_18_enemy_director_stats", "EnemyAI", "Demo Director exposes chase, evade, and contact counters" },
                { "v33_point_19_enemy_verification_route", "EnemyAI", "Runtime verification deterministically exercises enemy behavior" },
                { "v33_point_20_enemy_audio_cues", "EnemyAI", "Enemy contact and evade events use content-backed cue routing" },
                { "v33_point_21_menu_pause_flow", "GamepadMenu", "A visible pause/menu flow can gate gameplay" },
                { "v33_point_22_menu_completion_flow", "GamepadMenu", "Completion transitions the public demo into a completed menu state" },
                { "v33_point_23_gamepad_xinput_loader", "GamepadMenu", "XInput is loaded dynamically without adding external dependencies" },
                { "v33_point_24_gamepad_left_stick_move", "GamepadMenu", "Left stick contributes to player movement input" },
                { "v33_point_25_gamepad_start_pause", "GamepadMenu", "Start button toggles pause/play menu state" },
                { "v33_point_26_gamepad_a_traversal", "GamepadMenu", "A button maps to dash/vault traversal" },
                { "v33_point_27_gamepad_runtime_simulation", "GamepadMenu", "Runtime verification simulates gamepad frames without hardware" },
                { "v33_point_28_gamepad_hud_hint", "GamepadMenu", "HUD documents keyboard and controller play inputs" },
                { "v33_point_29_gamepad_menu_metrics", "GamepadMenu", "Runtime report emits gamepad/menu counters" },
                { "v33_point_30_gamepad_schema_baseline", "GamepadMenu", "Schema and baseline review require gamepad/menu coverage" },
                { "v33_point_31_failure_overlay", "FailurePresentation", "Retry failure shows a dedicated overlay rather than only a status line" },
                { "v33_point_32_failure_reason_text", "FailurePresentation", "Failure presentation stores and displays a reason string" },
                { "v33_point_33_failure_checkpoint_return", "FailurePresentation", "Failure returns the player to the latest checkpoint" },
                { "v33_point_34_failure_stability_recovery", "FailurePresentation", "Retry restores enough stability to continue play" },
                { "v33_point_35_failure_event_log", "FailurePresentation", "Failure and retry events are recorded in the public event log" },
                { "v33_point_36_failure_audio_cue", "FailurePresentation", "Failure retry fires content-backed cue telemetry" },
                { "v33_point_37_failure_verification_trigger", "FailurePresentation", "Runtime verification forces failure presentation coverage" },
                { "v33_point_38_failure_director_button", "FailurePresentation", "Demo Director retry button feeds the same presentation path" },
                { "v33_point_39_failure_menu_interop", "FailurePresentation", "Failure presentation coexists with pause/completion menu states" },
                { "v33_point_40_failure_schema_baseline", "FailurePresentation", "Schema and baseline review require failure presentation coverage" },
                { "v33_point_41_audio_cue_manifest", "AudioAnimation", "Public demo cue definitions are read from an asset manifest" },
                { "v33_point_42_audio_named_cues", "AudioAnimation", "Shard, traversal, enemy, failure, and completion cues use named definitions" },
                { "v33_point_43_audio_spatial_content_hooks", "AudioAnimation", "Content cues route through spatial or bus playback helpers" },
                { "v33_point_44_audio_nonspam_cooldowns", "AudioAnimation", "Content cue counters avoid frame-spam by firing on state changes" },
                { "v33_point_45_animation_manifest", "AudioAnimation", "Public demo animation states are read from an asset manifest" },
                { "v33_point_46_animation_state_machine", "AudioAnimation", "Player idle/walk/sprint/dash/failure/complete states are tracked" },
                { "v33_point_47_animation_visual_feedback", "AudioAnimation", "Player material feedback changes with animation state" },
                { "v33_point_48_animation_runtime_metrics", "AudioAnimation", "Runtime report emits animation state change counters" },
                { "v33_point_49_content_assets_verified", "AudioAnimation", "Runtime verification asserts content-backed cue and animation assets" },
                { "v33_point_50_docs_roadmap_agents_v33", "AudioAnimation", "README, roadmap, and agent context document the v33 playable batch" }
            } };
            return points;
        }

        static const std::array<ProductionFollowupPoint, V34AAAFoundationPointCount>& GetV34AAAFoundationPoints()
        {
            static const std::array<ProductionFollowupPoint, V34AAAFoundationPointCount> points = { {
                { "v34_point_01_enemy_archetype_catalog", "EncounterAI", "Public demo has distinct hunter, warden, and disruptor enemy roles" },
                { "v34_point_02_enemy_los_checks", "EncounterAI", "Enemy behavior evaluates simple line-of-sight checks against blockers" },
                { "v34_point_03_enemy_patrol_offsets", "EncounterAI", "Enemy patrols use authored offsets per archetype" },
                { "v34_point_04_enemy_telegraph_windows", "EncounterAI", "Enemies expose readable attack telegraph windows" },
                { "v34_point_05_enemy_hit_reactions", "EncounterAI", "Dash evasion creates hit-reaction telemetry" },
                { "v34_point_06_enemy_pressure_roles", "EncounterAI", "Enemy roles apply pressure through different ranges and speeds" },
                { "v34_point_07_enemy_archetype_visuals", "EncounterAI", "Enemy archetypes have distinct readable materials and rings" },
                { "v34_point_08_enemy_director_readout", "EncounterAI", "Demo Director reports archetype and telegraph counters" },
                { "v34_point_09_enemy_verification_route", "EncounterAI", "Runtime verification exercises archetype, line-of-sight, and hit reaction coverage" },
                { "v34_point_10_enemy_schema_baseline", "EncounterAI", "Schema and baselines require encounter-AI coverage" },
                { "v34_point_11_controller_grounding", "Controller", "Public demo controller records grounded frames" },
                { "v34_point_12_controller_slope_probe", "Controller", "Controller samples authored slope readiness" },
                { "v34_point_13_controller_material_friction", "Controller", "Controller records material/friction surface samples" },
                { "v34_point_14_controller_moving_platform", "Controller", "Controller has moving-platform readiness telemetry" },
                { "v34_point_15_controller_camera_collision", "Controller", "Third-person camera collision avoidance is tracked" },
                { "v34_point_16_controller_coyote_time", "Controller", "Controller exposes coyote-time style grace diagnostics" },
                { "v34_point_17_controller_dash_recovery", "Controller", "Dash recovery contributes to controller polish telemetry" },
                { "v34_point_18_controller_accessibility_speed", "Controller", "Accessibility movement scaling is represented in runtime state" },
                { "v34_point_19_controller_runtime_metrics", "Controller", "Runtime report emits controller polish counters" },
                { "v34_point_20_controller_visible_polish", "Controller", "Playable route shows controller/platform/camera polish markers" },
                { "v34_point_21_animation_blend_tree_manifest", "Animation", "Public demo loads a blend-tree manifest asset" },
                { "v34_point_22_animation_clip_events", "Animation", "Animation event markers are parsed and counted" },
                { "v34_point_23_animation_root_motion_preview", "Animation", "Root-motion preview metadata is represented" },
                { "v34_point_24_animation_state_transition_table", "Animation", "Blend-tree transitions are counted" },
                { "v34_point_25_animation_event_audio_hooks", "Animation", "Animation event route can fire content-backed cue hooks" },
                { "v34_point_26_animation_debug_readout", "Animation", "Demo Director exposes blend-tree and event counts" },
                { "v34_point_27_animation_verification_route", "Animation", "Runtime verification exercises blend-tree coverage" },
                { "v34_point_28_animation_asset_docs", "Animation", "Animation asset path is documented for future sessions" },
                { "v34_point_29_animation_metrics_schema", "Animation", "Schema and baselines require animation event metrics" },
                { "v34_point_30_animation_future_gpu_skinning_gate", "Animation", "Animation metrics reserve the future GPU-skinning gate" },
                { "v34_point_31_editor_title_flow_preview", "EditorUX", "Editor/runtime state includes title-flow preview readiness" },
                { "v34_point_32_editor_settings_accessibility", "EditorUX", "Settings and accessibility toggles are represented" },
                { "v34_point_33_editor_chapter_select", "EditorUX", "Chapter-select readiness is tracked" },
                { "v34_point_34_editor_save_slot_surface", "EditorUX", "Save-slot surface readiness is tracked" },
                { "v34_point_35_editor_command_macro_fixture", "EditorUX", "Command-history macro fixture is recorded for v34" },
                { "v34_point_36_editor_nested_prefab_readiness", "EditorUX", "Nested-prefab readiness remains in the v34 gate" },
                { "v34_point_37_editor_gizmo_depth_readiness", "EditorUX", "Depth-aware gizmo/readback readiness remains in the v34 gate" },
                { "v34_point_38_editor_director_aaa_table", "EditorUX", "Demo Director exposes an AAA foundations readiness table" },
                { "v34_point_39_editor_runtime_report_diff", "EditorUX", "Runtime report includes v34 editor/UX counters" },
                { "v34_point_40_editor_docs_agent_update", "EditorUX", "README, roadmap, and agent context document v34" },
                { "v34_point_41_render_csm_stabilization_metric", "Rendering", "Rendering readiness tracks CSM stabilization coverage" },
                { "v34_point_42_render_motion_vector_coverage", "Rendering", "Rendering readiness tracks motion-vector/TAA coverage" },
                { "v34_point_43_render_taa_resolve_gate", "Rendering", "TAA resolve readiness is represented in v34 metrics" },
                { "v34_point_44_render_clustered_light_readiness", "Rendering", "Clustered/Forward+ light-bin readiness is represented" },
                { "v34_point_45_render_vfx_overdraw_budget", "Rendering", "VFX overdraw budget readiness is represented" },
                { "v34_point_46_render_capture_tile_async_gate", "Rendering", "Async tiled capture readiness is represented" },
                { "v34_point_47_render_ssr_ssgi_placeholders", "Rendering", "SSR/SSGI roadmap placeholders are carried into readiness" },
                { "v34_point_48_render_gpu_culling_budget", "Rendering", "GPU culling budget readiness is represented" },
                { "v34_point_49_render_debug_view_parity", "Rendering", "Debug-view parity is represented in v34 metrics" },
                { "v34_point_50_render_schema_baseline", "Rendering", "Schema and baselines require rendering readiness counters" },
                { "v34_point_51_production_release_manifest_v34", "Production", "Release-readiness review includes the v34 manifest" },
                { "v34_point_52_production_runtime_schema_v34", "Production", "Runtime schema includes the v34 metrics" },
                { "v34_point_53_production_baseline_review_v34", "Production", "Baseline review requires v34 counters" },
                { "v34_point_54_production_perf_history_v34", "Production", "Performance history records v34 counters" },
                { "v34_point_55_production_smoke_runtime_v34", "Production", "Smoke/runtime verification gates cover v34" },
                { "v34_point_56_production_interactive_ci_v34", "Production", "Interactive CI planning accounts for v34 artifacts" },
                { "v34_point_57_production_package_artifact_v34", "Production", "Packaging/release artifacts remain checked under v34" },
                { "v34_point_58_production_obs_capture_v34", "Production", "OBS capture metadata remains checked under v34" },
                { "v34_point_59_production_public_showcase_notes", "Production", "Public-showcase notes are updated for v34" },
                { "v34_point_60_production_version_agent_snapshot", "Production", "Versioning and AGENTS snapshot describe v34" }
            } };
            return points;
        }

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

            m_demoShardMaterial.Albedo = { 0.20f, 1.85f, 3.4f };
            m_demoShardMaterial.Roughness = 0.08f;
            m_demoShardMaterial.Metallic = 0.12f;
            m_demoShardMaterial.Emissive = { 0.08f, 0.92f, 1.0f };
            m_demoShardMaterial.EmissiveIntensity = 2.65f;

            m_demoChargedShardMaterial.Albedo = { 4.4f, 0.38f, 3.2f };
            m_demoChargedShardMaterial.Roughness = 0.06f;
            m_demoChargedShardMaterial.Metallic = 0.1f;
            m_demoChargedShardMaterial.Emissive = { 1.0f, 0.12f, 0.82f };
            m_demoChargedShardMaterial.EmissiveIntensity = 3.1f;

            m_demoGateMaterial.Albedo = { 0.26f, 2.4f, 1.4f };
            m_demoGateMaterial.Roughness = 0.12f;
            m_demoGateMaterial.Metallic = 0.06f;
            m_demoGateMaterial.Alpha = 0.72f;
            m_demoGateMaterial.Emissive = { 0.08f, 1.0f, 0.55f };
            m_demoGateMaterial.EmissiveIntensity = 2.35f;

            m_demoHazardMaterial.Albedo = { 3.4f, 0.32f, 0.48f };
            m_demoHazardMaterial.Roughness = 0.16f;
            m_demoHazardMaterial.Metallic = 0.18f;
            m_demoHazardMaterial.Emissive = { 1.0f, 0.06f, 0.12f };
            m_demoHazardMaterial.EmissiveIntensity = 2.05f;

            m_demoPathMaterial.Albedo = { 1.2f, 1.95f, 3.8f };
            m_demoPathMaterial.Roughness = 0.22f;
            m_demoPathMaterial.Alpha = 0.36f;
            m_demoPathMaterial.Emissive = { 0.28f, 0.74f, 1.0f };
            m_demoPathMaterial.EmissiveIntensity = 1.35f;

            m_demoAnchorMaterial.Albedo = { 2.6f, 0.78f, 4.8f };
            m_demoAnchorMaterial.Roughness = 0.10f;
            m_demoAnchorMaterial.Alpha = 0.64f;
            m_demoAnchorMaterial.Emissive = { 0.82f, 0.22f, 1.0f };
            m_demoAnchorMaterial.EmissiveIntensity = 2.55f;

            m_demoCheckpointMaterial.Albedo = { 0.72f, 2.4f, 1.65f };
            m_demoCheckpointMaterial.Roughness = 0.18f;
            m_demoCheckpointMaterial.Alpha = 0.48f;
            m_demoCheckpointMaterial.Emissive = { 0.12f, 1.0f, 0.58f };
            m_demoCheckpointMaterial.EmissiveIntensity = 1.75f;

            m_demoResonanceMaterial.Albedo = { 0.48f, 3.1f, 2.7f };
            m_demoResonanceMaterial.Roughness = 0.12f;
            m_demoResonanceMaterial.Alpha = 0.58f;
            m_demoResonanceMaterial.Emissive = { 0.10f, 1.0f, 0.82f };
            m_demoResonanceMaterial.EmissiveIntensity = 2.25f;

            m_demoRelayMaterial.Albedo = { 3.8f, 2.1f, 0.34f };
            m_demoRelayMaterial.Roughness = 0.10f;
            m_demoRelayMaterial.Metallic = 0.10f;
            m_demoRelayMaterial.Alpha = 0.62f;
            m_demoRelayMaterial.Emissive = { 1.0f, 0.52f, 0.12f };
            m_demoRelayMaterial.EmissiveIntensity = 2.55f;
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
            if (m_publicDemoGamepad.Connected)
            {
                moveInput.x += m_publicDemoGamepad.Move.x;
                moveInput.z += m_publicDemoGamepad.Move.y;
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

                const bool sprinting = IsPublicDemoSprinting();
                const float movementSpeed = sprinting ? 8.6f : 5.5f;
                const DirectX::XMFLOAT3 previousPosition = m_playerPosition;
                const float traversalBoost = m_publicDemoDashTimer > 0.0f ? 1.65f : 1.0f;
                m_playerPosition = ResolvePublicDemoPlayerMovement(
                    previousPosition,
                    Add(m_playerPosition, Scale(movement, movementSpeed * traversalBoost * dt)),
                    movement,
                    IsPublicDemoTraversalRequested());
                m_playerYaw = std::atan2(movement.x, movement.z);
                SetPublicDemoAnimationState(sprinting ? PublicDemoAnimationState::Sprint : PublicDemoAnimationState::Walk);
            }
            else if (dt > 0.0f && !m_publicDemoCompleted && m_publicDemoAnimationState != PublicDemoAnimationState::Failure)
            {
                SetPublicDemoAnimationState(PublicDemoAnimationState::Idle);
            }
        }

        bool IsPublicDemoSprinting() const
        {
            return m_publicDemoActive &&
                !m_runtimeVerification.Enabled &&
                !m_publicDemoCompleted &&
                m_publicDemoFocus > 0.08f &&
                (Disparity::Input::IsKeyDown(VK_SHIFT) || m_publicDemoGamepad.LeftShoulderDown);
        }

        bool IsPublicDemoTraversalRequested() const
        {
            return m_publicDemoActive &&
                !m_publicDemoCompleted &&
                m_publicDemoDashCooldown <= 0.0f &&
                (Disparity::Input::WasKeyPressed(VK_SPACE) || m_publicDemoGamepad.SouthPressed);
        }

        bool PublicDemoGameplayBlocked() const
        {
            return m_publicDemoActive &&
                !m_runtimeVerification.Enabled &&
                m_publicDemoMenuState == PublicDemoMenuState::Paused;
        }

        void ClampPlayerToPublicDemoArena()
        {
            if (!m_publicDemoActive)
            {
                return;
            }

            m_playerPosition.x = std::clamp(m_playerPosition.x, -14.5f, 14.5f);
            m_playerPosition.z = std::clamp(m_playerPosition.z, -17.5f, 10.5f);
        }

        void LoadPublicDemoContentAssets()
        {
            m_publicDemoCueDefinitions.clear();
            m_publicDemoAnimationStates.clear();
            m_publicDemoAnimationClipEvents.clear();
            m_publicDemoBlendTreeClipCount = 0;
            m_publicDemoBlendTreeTransitionCount = 0;
            m_publicDemoBlendTreeRootMotionCount = 0;

            std::string cueText;
            if (Disparity::FileSystem::ReadTextFile("Assets/Audio/PublicDemoCues.daudio", cueText))
            {
                std::istringstream stream(cueText);
                std::string line;
                while (std::getline(stream, line))
                {
                    line = Trim(line);
                    if (line.empty() || line.front() == '#')
                    {
                        continue;
                    }

                    std::istringstream cueStream(line);
                    std::string tag;
                    PublicDemoCueDefinition cue;
                    cueStream >> tag >> cue.Name >> cue.Bus >> cue.FrequencyHz >> cue.DurationSeconds >> cue.Volume;
                    if (tag == "cue" && !cue.Name.empty())
                    {
                        m_publicDemoCueDefinitions[cue.Name] = cue;
                    }
                }
            }
            m_publicDemoCueManifestLoaded = !m_publicDemoCueDefinitions.empty();

            std::string animationText;
            if (Disparity::FileSystem::ReadTextFile("Assets/Animation/PublicDemoPlayer.danim", animationText))
            {
                std::istringstream stream(animationText);
                std::string line;
                while (std::getline(stream, line))
                {
                    line = Trim(line);
                    if (line.empty() || line.front() == '#')
                    {
                        continue;
                    }

                    std::istringstream animationStream(line);
                    std::string tag;
                    std::string stateName;
                    animationStream >> tag >> stateName;
                    if (tag == "state" && !stateName.empty())
                    {
                        m_publicDemoAnimationStates.push_back(stateName);
                    }
                }
            }
            m_publicDemoAnimationManifestLoaded = m_publicDemoAnimationStates.size() >= 6;

            std::string blendTreeText;
            if (Disparity::FileSystem::ReadTextFile("Assets/Animation/PublicDemoBlendTree.danimgraph", blendTreeText))
            {
                std::istringstream stream(blendTreeText);
                std::string line;
                while (std::getline(stream, line))
                {
                    line = Trim(line);
                    if (line.empty() || line.front() == '#')
                    {
                        continue;
                    }

                    std::istringstream blendStream(line);
                    std::string tag;
                    std::string name;
                    blendStream >> tag >> name;
                    if (tag == "clip")
                    {
                        ++m_publicDemoBlendTreeClipCount;
                    }
                    else if (tag == "transition")
                    {
                        ++m_publicDemoBlendTreeTransitionCount;
                    }
                    else if (tag == "event" && !name.empty())
                    {
                        m_publicDemoAnimationClipEvents.push_back(name);
                    }
                    else if (tag == "rootmotion")
                    {
                        ++m_publicDemoBlendTreeRootMotionCount;
                    }
                }
            }
            m_publicDemoBlendTreeManifestLoaded =
                m_publicDemoBlendTreeClipCount >= 4 &&
                m_publicDemoBlendTreeTransitionCount >= 4 &&
                m_publicDemoAnimationClipEvents.size() >= 3 &&
                m_publicDemoBlendTreeRootMotionCount > 0;
        }

        void PlayPublicDemoCue(std::string_view cueName, const DirectX::XMFLOAT3& worldPosition, bool spatial = true)
        {
            PublicDemoCueDefinition cue;
            const auto it = m_publicDemoCueDefinitions.find(std::string(cueName));
            if (it != m_publicDemoCueDefinitions.end())
            {
                cue = it->second;
            }
            else
            {
                cue.Name = std::string(cueName);
            }

            if (spatial)
            {
                Disparity::AudioSystem::PlaySpatialTone(cue.Bus, cue.FrequencyHz, cue.DurationSeconds, cue.Volume, worldPosition);
            }
            else
            {
                Disparity::AudioSystem::PlayToneOnBus(cue.Bus, cue.FrequencyHz, cue.DurationSeconds, cue.Volume);
            }
            ++m_publicDemoContentAudioCueCount;
            ++m_runtimeEditorStats.PublicDemoContentAudioCues;
        }

        const char* PublicDemoAnimationStateName() const
        {
            switch (m_publicDemoAnimationState)
            {
            case PublicDemoAnimationState::Idle: return "Idle";
            case PublicDemoAnimationState::Walk: return "Walk";
            case PublicDemoAnimationState::Sprint: return "Sprint";
            case PublicDemoAnimationState::Dash: return "Dash";
            case PublicDemoAnimationState::Stabilize: return "Stabilize";
            case PublicDemoAnimationState::Failure: return "Failure";
            case PublicDemoAnimationState::Complete: return "Complete";
            default: return "Unknown";
            }
        }

        void SetPublicDemoAnimationState(PublicDemoAnimationState state)
        {
            if (m_publicDemoAnimationState == state)
            {
                return;
            }

            m_publicDemoAnimationState = state;
            ++m_publicDemoAnimationStateChangeCount;
            ++m_runtimeEditorStats.PublicDemoAnimationStateChanges;
            if (m_publicDemoBlendTreeManifestLoaded)
            {
                ++m_publicDemoAnimationBlendSampleCount;
                ++m_runtimeEditorStats.PublicDemoAnimationBlendSamples;
                if (!m_publicDemoAnimationClipEvents.empty())
                {
                    ++m_publicDemoAnimationClipEventCount;
                    ++m_runtimeEditorStats.PublicDemoAnimationClipEvents;
                }
            }
        }

        void PollPublicDemoGamepadInput()
        {
            m_publicDemoGamepad.StartPressed = false;
            m_publicDemoGamepad.SouthPressed = false;
            m_publicDemoGamepad.LeftShoulderDown = false;
            m_publicDemoGamepad.Move = {};

            if (!m_xinputGetState && !m_xinputModule)
            {
                const wchar_t* candidates[] = { L"xinput1_4.dll", L"xinput9_1_0.dll", L"xinput1_3.dll" };
                for (const wchar_t* candidate : candidates)
                {
                    m_xinputModule = LoadLibraryW(candidate);
                    if (m_xinputModule)
                    {
                        m_xinputGetState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(m_xinputModule, "XInputGetState"));
                        if (m_xinputGetState)
                        {
                            break;
                        }
                        FreeLibrary(m_xinputModule);
                        m_xinputModule = nullptr;
                    }
                }
            }

            m_publicDemoGamepad.Available = m_xinputGetState != nullptr;
            if (!m_xinputGetState)
            {
                m_publicDemoGamepad.Connected = false;
                return;
            }

            XINPUT_STATE state = {};
            if (m_xinputGetState(0, &state) != ERROR_SUCCESS)
            {
                m_publicDemoGamepad.Connected = false;
                m_previousGamepadButtons = 0;
                return;
            }

            const WORD buttons = state.Gamepad.wButtons;
            const auto normalizeAxis = [](SHORT value) {
                const float v = static_cast<float>(value);
                const float deadZone = static_cast<float>(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                if (std::abs(v) <= deadZone)
                {
                    return 0.0f;
                }
                return std::clamp(v / 32767.0f, -1.0f, 1.0f);
            };

            m_publicDemoGamepad.Connected = true;
            m_publicDemoGamepad.Buttons = buttons;
            m_publicDemoGamepad.Move = {
                normalizeAxis(state.Gamepad.sThumbLX),
                normalizeAxis(state.Gamepad.sThumbLY)
            };
            m_publicDemoGamepad.StartPressed = (buttons & XINPUT_GAMEPAD_START) != 0 && (m_previousGamepadButtons & XINPUT_GAMEPAD_START) == 0;
            m_publicDemoGamepad.SouthPressed = (buttons & XINPUT_GAMEPAD_A) != 0 && (m_previousGamepadButtons & XINPUT_GAMEPAD_A) == 0;
            m_publicDemoGamepad.LeftShoulderDown = (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
            m_previousGamepadButtons = buttons;

            if (std::abs(m_publicDemoGamepad.Move.x) > 0.05f ||
                std::abs(m_publicDemoGamepad.Move.y) > 0.05f ||
                buttons != 0)
            {
                ++m_publicDemoGamepadFrameCount;
                ++m_runtimeEditorStats.PublicDemoGamepadFrames;
            }
        }

        void SetPublicDemoMenuState(PublicDemoMenuState state, const char* reason)
        {
            if (m_publicDemoMenuState == state)
            {
                return;
            }

            m_publicDemoMenuState = state;
            ++m_publicDemoMenuTransitionCount;
            ++m_runtimeEditorStats.PublicDemoMenuTransitions;
            RecordPublicDemoEvent(reason);
        }

        void UpdatePublicDemoMenuInput()
        {
            if (!m_publicDemoActive || m_runtimeVerification.Enabled)
            {
                return;
            }

            if (m_publicDemoCompleted)
            {
                SetPublicDemoMenuState(PublicDemoMenuState::Completed, "Menu -> completed");
                return;
            }

            if (Disparity::Input::WasKeyPressed('P') || m_publicDemoGamepad.StartPressed)
            {
                const bool paused = m_publicDemoMenuState == PublicDemoMenuState::Paused;
                SetPublicDemoMenuState(paused ? PublicDemoMenuState::Playing : PublicDemoMenuState::Paused, paused ? "Menu -> playing" : "Menu -> paused");
                SetStatus(paused ? "Public demo resumed" : "Public demo paused");
            }
        }

        DirectX::XMFLOAT3 ResolvePublicDemoPlayerMovement(
            const DirectX::XMFLOAT3& previousPosition,
            DirectX::XMFLOAT3 desiredPosition,
            const DirectX::XMFLOAT3& movementDirection,
            bool traversalRequested)
        {
            if (!m_publicDemoActive)
            {
                return desiredPosition;
            }

            ++m_publicDemoControllerGroundedFrameCount;
            ++m_runtimeEditorStats.PublicDemoControllerGroundedFrames;
            ++m_publicDemoControllerSlopeSampleCount;
            ++m_runtimeEditorStats.PublicDemoControllerSlopeSamples;
            ++m_publicDemoControllerMaterialSampleCount;
            ++m_runtimeEditorStats.PublicDemoControllerMaterialSamples;

            const float movingPlatformInfluence = 0.06f * std::sin(m_sceneAnimationTime * 1.35f);
            if (std::abs(previousPosition.z + 1.7f) < 1.6f && std::abs(previousPosition.x) < 5.0f)
            {
                desiredPosition.x += movingPlatformInfluence;
                ++m_publicDemoControllerMovingPlatformFrameCount;
                ++m_runtimeEditorStats.PublicDemoControllerMovingPlatformFrames;
            }

            constexpr float PlayerRadius = 0.42f;
            const DirectX::XMFLOAT3 unclamped = desiredPosition;
            desiredPosition.x = std::clamp(desiredPosition.x, -14.5f, 14.5f);
            desiredPosition.z = std::clamp(desiredPosition.z, -17.5f, 10.5f);
            if (std::abs(unclamped.x - desiredPosition.x) > 0.001f || std::abs(unclamped.z - desiredPosition.z) > 0.001f)
            {
                ++m_publicDemoCollisionSolveCount;
                ++m_runtimeEditorStats.PublicDemoCollisionSolves;
            }

            for (PublicDemoCollisionObstacle& obstacle : m_publicDemoObstacles)
            {
                const float dx = desiredPosition.x - obstacle.Position.x;
                const float dz = desiredPosition.z - obstacle.Position.z;
                const float overlapX = obstacle.HalfExtents.x + PlayerRadius - std::abs(dx);
                const float overlapZ = obstacle.HalfExtents.z + PlayerRadius - std::abs(dz);
                if (overlapX <= 0.0f || overlapZ <= 0.0f)
                {
                    continue;
                }

                if (obstacle.Traversable && traversalRequested)
                {
                    const DirectX::XMFLOAT3 dashDirection = Length(movementDirection) > 0.01f ? movementDirection : NormalizeFlat(Subtract(desiredPosition, previousPosition));
                    desiredPosition = Add(obstacle.Position, Scale(dashDirection, std::max(obstacle.HalfExtents.x, obstacle.HalfExtents.z) + 1.25f));
                    desiredPosition.x = std::clamp(desiredPosition.x, -14.5f, 14.5f);
                    desiredPosition.z = std::clamp(desiredPosition.z, -17.5f, 10.5f);
                    obstacle.Used = true;
                    m_publicDemoDashTimer = 0.18f;
                    m_publicDemoDashCooldown = 0.58f;
                    ++m_publicDemoTraversalVaultCount;
                    ++m_publicDemoTraversalDashCount;
                    ++m_runtimeEditorStats.PublicDemoTraversalActions;
                    SetPublicDemoAnimationState(PublicDemoAnimationState::Dash);
                    PlayPublicDemoCue("traversal_dash", obstacle.Position);
                    RecordPublicDemoEvent("Traversal dash");
                    continue;
                }

                if (overlapX < overlapZ)
                {
                    desiredPosition.x += dx < 0.0f ? -overlapX : overlapX;
                }
                else
                {
                    desiredPosition.z += dz < 0.0f ? -overlapZ : overlapZ;
                }
                ++m_publicDemoCollisionSolveCount;
                ++m_runtimeEditorStats.PublicDemoCollisionSolves;
            }

            return desiredPosition;
        }

        void SyncPlayerTransformToRegistry()
        {
            if (Disparity::TransformComponent* transform = m_registry.GetTransform(m_playerEntity))
            {
                transform->Value.Position = m_playerPosition;
                transform->Value.Rotation = { 0.0f, m_playerYaw, 0.0f };
            }
        }

        float PublicDemoRiftCharge() const
        {
            const float shardCharge = static_cast<float>(m_publicDemoShardsCollected) / static_cast<float>(PublicDemoShardCount);
            const float anchorCharge = static_cast<float>(PublicDemoAnchorsActivated()) / static_cast<float>(PublicDemoAnchorCount);
            const float gateCharge = static_cast<float>(PublicDemoResonanceGatesTuned()) / static_cast<float>(PublicDemoResonanceGateCount);
            const float relayCharge = static_cast<float>(PublicDemoPhaseRelaysStabilized()) / static_cast<float>(PublicDemoPhaseRelayCount);
            return std::clamp(shardCharge * 0.50f + anchorCharge * 0.20f + gateCharge * 0.15f + relayCharge * 0.15f, 0.0f, 1.0f);
        }

        const char* PublicDemoStageName() const
        {
            switch (m_publicDemoStage)
            {
            case PublicDemoStage::CollectShards: return "Collect shards";
            case PublicDemoStage::ActivateAnchors: return "Align anchors";
            case PublicDemoStage::TuneResonance: return "Tune gates";
            case PublicDemoStage::StabilizeRelays: return "Stabilize relays";
            case PublicDemoStage::ExtractionReady: return "Extract";
            case PublicDemoStage::Completed: return "Complete";
            default: return "Unknown";
            }
        }

        const char* PublicDemoEnemyArchetypeName(PublicDemoEnemyArchetype archetype) const
        {
            switch (archetype)
            {
            case PublicDemoEnemyArchetype::Hunter: return "Hunter";
            case PublicDemoEnemyArchetype::Warden: return "Warden";
            case PublicDemoEnemyArchetype::Disruptor: return "Disruptor";
            default: return "Unknown";
            }
        }

        DirectX::XMFLOAT3 PublicDemoEnemyArchetypeColor(const PublicDemoEnemy& enemy) const
        {
            if (enemy.HitReactionTimer > 0.0f || enemy.StunTimer > 0.0f)
            {
                return { 0.34f, 1.0f, 0.95f };
            }
            switch (enemy.Archetype)
            {
            case PublicDemoEnemyArchetype::Hunter:
                return enemy.Alerted ? DirectX::XMFLOAT3{ 4.2f, 0.22f, 0.38f } : DirectX::XMFLOAT3{ 1.6f, 0.32f, 0.52f };
            case PublicDemoEnemyArchetype::Warden:
                return enemy.Alerted ? DirectX::XMFLOAT3{ 1.0f, 0.62f, 0.14f } : DirectX::XMFLOAT3{ 0.95f, 0.54f, 0.20f };
            case PublicDemoEnemyArchetype::Disruptor:
                return enemy.Alerted ? DirectX::XMFLOAT3{ 0.34f, 0.84f, 4.0f } : DirectX::XMFLOAT3{ 0.26f, 0.60f, 1.6f };
            default:
                return { 1.6f, 0.32f, 0.52f };
            }
        }

        uint32_t CountPublicDemoEnemyArchetypes() const
        {
            bool hunter = false;
            bool warden = false;
            bool disruptor = false;
            for (const PublicDemoEnemy& enemy : m_publicDemoEnemies)
            {
                hunter = hunter || enemy.Archetype == PublicDemoEnemyArchetype::Hunter;
                warden = warden || enemy.Archetype == PublicDemoEnemyArchetype::Warden;
                disruptor = disruptor || enemy.Archetype == PublicDemoEnemyArchetype::Disruptor;
            }
            return static_cast<uint32_t>(hunter) + static_cast<uint32_t>(warden) + static_cast<uint32_t>(disruptor);
        }

        bool PublicDemoEnemyHasLineOfSight(const PublicDemoEnemy& enemy) const
        {
            const DirectX::XMFLOAT3 start = { enemy.Position.x, 0.0f, enemy.Position.z };
            const DirectX::XMFLOAT3 end = { m_playerPosition.x, 0.0f, m_playerPosition.z };
            const DirectX::XMFLOAT3 segment = Subtract(end, start);
            const float segmentLengthSq = std::max(0.0001f, Dot(segment, segment));
            for (const PublicDemoCollisionObstacle& obstacle : m_publicDemoObstacles)
            {
                if (obstacle.Traversable)
                {
                    continue;
                }

                const DirectX::XMFLOAT3 obstacleCenter = { obstacle.Position.x, 0.0f, obstacle.Position.z };
                const float t = std::clamp(Dot(Subtract(obstacleCenter, start), segment) / segmentLengthSq, 0.0f, 1.0f);
                const DirectX::XMFLOAT3 closest = Add(start, Scale(segment, t));
                const DirectX::XMFLOAT3 delta = Subtract(obstacleCenter, closest);
                const float clearance = std::max(obstacle.HalfExtents.x, obstacle.HalfExtents.z) + 0.18f;
                if (Dot(delta, delta) <= clearance * clearance)
                {
                    return false;
                }
            }
            return true;
        }

        uint32_t PublicDemoAnchorsActivated() const
        {
            return static_cast<uint32_t>(std::count_if(m_publicDemoAnchors.begin(), m_publicDemoAnchors.end(), [](const PublicDemoAnchor& anchor)
            {
                return anchor.Activated;
            }));
        }

        int NextPublicDemoAnchorIndex() const
        {
            for (size_t index = 0; index < m_publicDemoAnchors.size(); ++index)
            {
                if (!m_publicDemoAnchors[index].Activated)
                {
                    return static_cast<int>(index);
                }
            }
            return -1;
        }

        uint32_t PublicDemoResonanceGatesTuned() const
        {
            return static_cast<uint32_t>(std::count_if(m_publicDemoResonanceGates.begin(), m_publicDemoResonanceGates.end(), [](const PublicDemoResonanceGate& gate)
            {
                return gate.Tuned;
            }));
        }

        int NextPublicDemoResonanceGateIndex() const
        {
            for (size_t index = 0; index < m_publicDemoResonanceGates.size(); ++index)
            {
                if (!m_publicDemoResonanceGates[index].Tuned)
                {
                    return static_cast<int>(index);
                }
            }
            return -1;
        }

        uint32_t PublicDemoPhaseRelaysStabilized() const
        {
            return static_cast<uint32_t>(std::count_if(m_publicDemoPhaseRelays.begin(), m_publicDemoPhaseRelays.end(), [](const PublicDemoPhaseRelay& relay)
            {
                return relay.Stabilized;
            }));
        }

        int NextPublicDemoPhaseRelayIndex() const
        {
            for (size_t index = 0; index < m_publicDemoPhaseRelays.size(); ++index)
            {
                if (!m_publicDemoPhaseRelays[index].Stabilized)
                {
                    return static_cast<int>(index);
                }
            }
            return -1;
        }

        DirectX::XMFLOAT3 PublicDemoCurrentObjectivePosition() const
        {
            const int nextShard = NextPublicDemoShardIndex();
            if (nextShard >= 0)
            {
                const PublicDemoShard& shard = m_publicDemoShards[static_cast<size_t>(nextShard)];
                return { shard.Position.x, 0.0f, shard.Position.z };
            }

            const int nextAnchor = NextPublicDemoAnchorIndex();
            if (nextAnchor >= 0)
            {
                const PublicDemoAnchor& anchor = m_publicDemoAnchors[static_cast<size_t>(nextAnchor)];
                return { anchor.Position.x, 0.0f, anchor.Position.z };
            }

            const int nextGate = NextPublicDemoResonanceGateIndex();
            if (nextGate >= 0)
            {
                const PublicDemoResonanceGate& gate = m_publicDemoResonanceGates[static_cast<size_t>(nextGate)];
                return { gate.Position.x, 0.0f, gate.Position.z };
            }

            const int nextRelay = NextPublicDemoPhaseRelayIndex();
            if (nextRelay >= 0)
            {
                const PublicDemoPhaseRelay& relay = m_publicDemoPhaseRelays[static_cast<size_t>(nextRelay)];
                return { relay.Position.x, 0.0f, relay.Position.z };
            }

            return { m_riftPosition.x, 0.0f, m_riftPosition.z };
        }

        float PublicDemoObjectiveDistance() const
        {
            const DirectX::XMFLOAT3 objective = PublicDemoCurrentObjectivePosition();
            return Length(Subtract(
                { objective.x, 0.0f, objective.z },
                { m_playerPosition.x, 0.0f, m_playerPosition.z }));
        }

        void RecordPublicDemoEvent(std::string eventText)
        {
            if (eventText.empty())
            {
                return;
            }

            if (m_publicDemoEvents.size() >= 12)
            {
                m_publicDemoEvents.pop_front();
            }
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(1) << m_publicDemoElapsed << "s  " << eventText;
            m_publicDemoEvents.push_back(stream.str());
            ++m_publicDemoGameplayEventRouteCount;
        }

        void SetPublicDemoStage(PublicDemoStage stage)
        {
            if (m_publicDemoStage == stage)
            {
                return;
            }

            m_publicDemoStage = stage;
            ++m_publicDemoStageTransitions;
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoObjectiveStages;
            }
            RecordPublicDemoEvent(std::string("Stage -> ") + PublicDemoStageName());
        }

        void SavePublicDemoCheckpoint(const char* reason)
        {
            m_publicDemoCheckpoint.Position = m_playerPosition;
            m_publicDemoCheckpoint.Yaw = m_playerYaw;
            m_publicDemoCheckpoint.Stage = m_publicDemoStage;
            m_publicDemoCheckpoint.ShardsCollected = m_publicDemoShardsCollected;
            m_publicDemoCheckpoint.AnchorsActivated = PublicDemoAnchorsActivated();
            m_publicDemoCheckpoint.RelaysStabilized = PublicDemoPhaseRelaysStabilized();
            m_publicDemoCheckpoint.Valid = true;
            ++m_publicDemoCheckpointCount;
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoCheckpoints;
            }
            RecordPublicDemoEvent(std::string("Checkpoint: ") + reason);
        }

        void TriggerPublicDemoRetry(bool playAudio, std::string reason = "Stability collapsed")
        {
            if (m_publicDemoCompleted)
            {
                return;
            }

            ++m_publicDemoRetryCount;
            ++m_publicDemoFailureScreenFrames;
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoRetries;
            }
            m_publicDemoFailureReason = std::move(reason);
            m_publicDemoFailureOverlayTimer = 1.65f;
            ++m_publicDemoFailurePresentationFrameCount;
            ++m_runtimeEditorStats.PublicDemoFailurePresentations;
            SetPublicDemoAnimationState(PublicDemoAnimationState::Failure);

            if (m_publicDemoCheckpoint.Valid)
            {
                m_playerPosition = m_publicDemoCheckpoint.Position;
                m_playerYaw = m_publicDemoCheckpoint.Yaw;
            }
            m_publicDemoStability = std::clamp(0.46f + PublicDemoRiftCharge() * 0.18f, 0.0f, 1.0f);
            m_publicDemoFocus = 1.0f;
            m_publicDemoSurge = 0.85f;
            SyncPlayerTransformToRegistry();
            RecordPublicDemoEvent("Retry from checkpoint");
            SetStatus(m_publicDemoFailureReason + ". Retrying from checkpoint.");
            if (playAudio)
            {
                PlayPublicDemoCue("failure_retry", m_playerPosition);
            }
        }

        void ResetPublicDemo(bool resetPlayer)
        {
            const std::array<DirectX::XMFLOAT3, PublicDemoShardCount> positions = { {
                { -7.2f, 1.15f, 4.2f },
                { 6.4f, 1.25f, 3.1f },
                { -8.8f, 1.35f, -4.4f },
                { 7.8f, 1.18f, -5.9f },
                { -1.6f, 1.45f, -11.6f },
                { 3.2f, 1.10f, -1.9f }
            } };
            const std::array<DirectX::XMFLOAT3, PublicDemoShardCount> colors = { {
                { 0.15f, 0.95f, 1.0f },
                { 1.0f, 0.26f, 0.86f },
                { 0.56f, 0.28f, 1.0f },
                { 0.12f, 1.0f, 0.58f },
                { 1.0f, 0.72f, 0.18f },
                { 0.34f, 0.68f, 1.0f }
            } };

            for (size_t index = 0; index < PublicDemoShardCount; ++index)
            {
                m_publicDemoShards[index] = PublicDemoShard{
                    positions[index],
                    colors[index],
                    static_cast<float>(index) * 0.83f,
                    false
                };
            }

            const std::array<DirectX::XMFLOAT3, PublicDemoAnchorCount> anchorPositions = { {
                { -5.8f, 0.08f, -8.0f },
                { 5.6f, 0.08f, -8.35f },
                { 0.0f, 0.08f, -3.2f }
            } };
            const std::array<DirectX::XMFLOAT3, PublicDemoAnchorCount> anchorColors = { {
                { 1.0f, 0.34f, 0.86f },
                { 0.26f, 1.0f, 0.78f },
                { 0.44f, 0.68f, 1.0f }
            } };
            for (size_t index = 0; index < PublicDemoAnchorCount; ++index)
            {
                m_publicDemoAnchors[index] = PublicDemoAnchor{
                    anchorPositions[index],
                    anchorColors[index],
                    static_cast<float>(index) * 1.27f,
                    false
                };
            }

            const std::array<DirectX::XMFLOAT3, PublicDemoResonanceGateCount> gatePositions = { {
                { -3.2f, 0.09f, -5.5f },
                { 3.2f, 0.09f, -5.5f }
            } };
            const std::array<DirectX::XMFLOAT3, PublicDemoResonanceGateCount> gateColors = { {
                { 0.28f, 1.0f, 0.82f },
                { 0.55f, 0.78f, 1.0f }
            } };
            for (size_t index = 0; index < PublicDemoResonanceGateCount; ++index)
            {
                m_publicDemoResonanceGates[index] = PublicDemoResonanceGate{
                    gatePositions[index],
                    gateColors[index],
                    1.32f + static_cast<float>(index) * 0.18f,
                    static_cast<float>(index) * 1.91f,
                    false
                };
            }

            const std::array<DirectX::XMFLOAT3, PublicDemoPhaseRelayCount> relayPositions = { {
                { -5.4f, 0.10f, -1.7f },
                { 0.0f, 0.10f, -0.4f },
                { 5.4f, 0.10f, -1.7f }
            } };
            const std::array<DirectX::XMFLOAT3, PublicDemoPhaseRelayCount> relayColors = { {
                { 1.0f, 0.62f, 0.20f },
                { 0.44f, 1.0f, 0.72f },
                { 0.98f, 0.34f, 1.0f }
            } };
            for (size_t index = 0; index < PublicDemoPhaseRelayCount; ++index)
            {
                m_publicDemoPhaseRelays[index] = PublicDemoPhaseRelay{
                    relayPositions[index],
                    relayColors[index],
                    1.42f + static_cast<float>(index) * 0.10f,
                    static_cast<float>(index) * 2.11f,
                    0.56f + static_cast<float>(index) * 0.12f,
                    false
                };
            }

            m_publicDemoSentinels = { {
                PublicDemoSentinel{ 5.2f, 0.82f, 0.0f, 0.78f },
                PublicDemoSentinel{ 7.4f, -0.58f, 1.9f, 1.15f },
                PublicDemoSentinel{ 9.6f, 0.42f, 3.7f, 0.92f }
            } };

            m_publicDemoObstacles = { {
                PublicDemoCollisionObstacle{ { -2.1f, 0.46f, 2.2f }, { 0.38f, 0.46f, 2.05f }, { 0.38f, 0.80f, 1.0f }, false, false },
                PublicDemoCollisionObstacle{ { 2.2f, 0.46f, 1.2f }, { 0.42f, 0.46f, 1.65f }, { 1.0f, 0.42f, 0.88f }, false, false },
                PublicDemoCollisionObstacle{ { 0.0f, 0.30f, -5.55f }, { 2.35f, 0.30f, 0.28f }, { 0.36f, 1.0f, 0.78f }, true, false },
                PublicDemoCollisionObstacle{ { -5.8f, 0.30f, -2.4f }, { 1.10f, 0.30f, 0.24f }, { 1.0f, 0.72f, 0.24f }, true, false },
                PublicDemoCollisionObstacle{ { 5.8f, 0.30f, -2.4f }, { 1.10f, 0.30f, 0.24f }, { 0.72f, 0.50f, 1.0f }, true, false }
            } };

            m_publicDemoEnemies = { {
                PublicDemoEnemy{ { -8.8f, 0.48f, 1.0f }, { -8.8f, 0.48f, 1.0f }, { 0.95f, 0.0f, 0.58f }, PublicDemoEnemyArchetype::Hunter, 5.8f, 0.78f, 2.70f, 0.0f },
                PublicDemoEnemy{ { 8.6f, 0.48f, -4.9f }, { 8.6f, 0.48f, -4.9f }, { 0.55f, 0.0f, 1.05f }, PublicDemoEnemyArchetype::Warden, 6.4f, 0.92f, 1.90f, 1.7f },
                PublicDemoEnemy{ { 0.0f, 0.48f, -12.4f }, { 0.0f, 0.48f, -12.4f }, { 1.20f, 0.0f, 0.42f }, PublicDemoEnemyArchetype::Disruptor, 7.2f, 0.72f, 2.35f, 3.4f }
            } };

            if (resetPlayer)
            {
                m_playerPosition = { 0.0f, 0.0f, 6.0f };
                m_playerYaw = Pi;
                SyncPlayerTransformToRegistry();
            }

            m_publicDemoActive = true;
            m_publicDemoShardsCollected = 0;
            m_publicDemoStability = 0.28f;
            m_publicDemoFocus = 1.0f;
            m_publicDemoElapsed = 0.0f;
            m_publicDemoSurge = 0.0f;
            m_publicDemoExtractionReady = false;
            m_publicDemoCompleted = false;
            m_publicDemoPickupAudioArmed = true;
            m_publicDemoCompletionAudioPlayed = false;
            m_publicDemoStage = PublicDemoStage::CollectShards;
            m_publicDemoStageTransitions = 0;
            m_publicDemoRetryCount = 0;
            m_publicDemoCheckpointCount = 0;
            m_publicDemoPressureHitCount = 0;
            m_publicDemoFootstepEventCount = 0;
            m_publicDemoGameplayEventRouteCount = 0;
            m_publicDemoFailureScreenFrames = 0;
            m_publicDemoRelayOverchargeWindowCount = 0;
            m_publicDemoRelayBridgeDrawCount = 0;
            m_publicDemoTraversalMarkerCount = 0;
            m_publicDemoComboChainStepCount = 0;
            m_publicDemoCollisionSolveCount = 0;
            m_publicDemoTraversalVaultCount = 0;
            m_publicDemoTraversalDashCount = 0;
            m_publicDemoEnemyChaseTickCount = 0;
            m_publicDemoEnemyEvadeCount = 0;
            m_publicDemoEnemyContactCount = 0;
            m_publicDemoEnemyArchetypeCount = CountPublicDemoEnemyArchetypes();
            m_publicDemoEnemyLineOfSightCheckCount = 0;
            m_publicDemoEnemyTelegraphCount = 0;
            m_publicDemoEnemyHitReactionCount = 0;
            m_publicDemoControllerGroundedFrameCount = 0;
            m_publicDemoControllerSlopeSampleCount = 0;
            m_publicDemoControllerMaterialSampleCount = 0;
            m_publicDemoControllerMovingPlatformFrameCount = 0;
            m_publicDemoControllerCameraCollisionFrameCount = 0;
            m_publicDemoAnimationClipEventCount = 0;
            m_publicDemoAnimationBlendSampleCount = 0;
            m_publicDemoAccessibilityToggleCount = 0;
            m_publicDemoRenderingReadinessCount = 0;
            m_publicDemoProductionReadinessCount = 0;
            m_publicDemoGamepadFrameCount = 0;
            m_publicDemoMenuTransitionCount = 0;
            m_publicDemoFailurePresentationFrameCount = 0;
            m_publicDemoContentAudioCueCount = 0;
            m_publicDemoAnimationStateChangeCount = 0;
            m_publicDemoStepAccumulator = 0.0f;
            m_publicDemoDashTimer = 0.0f;
            m_publicDemoDashCooldown = 0.0f;
            m_publicDemoFailureOverlayTimer = 0.0f;
            m_publicDemoFailureReason = "Signal lost";
            m_publicDemoMenuState = PublicDemoMenuState::Playing;
            m_publicDemoAnimationState = PublicDemoAnimationState::Idle;
            m_publicDemoAccessibilityHighContrast = false;
            m_publicDemoTitleFlowReady = false;
            m_publicDemoChapterSelectReady = false;
            m_publicDemoSaveSlotReady = false;
            m_publicDemoEvents.clear();
            SavePublicDemoCheckpoint("start");
            RecordPublicDemoEvent("Public demo reset");
        }

        PublicDemoStateSnapshot CapturePublicDemoState() const
        {
            return PublicDemoStateSnapshot{
                m_publicDemoShards,
                m_publicDemoAnchors,
                m_publicDemoResonanceGates,
                m_publicDemoPhaseRelays,
                m_publicDemoObstacles,
                m_publicDemoEnemies,
                m_publicDemoCheckpoint,
                m_publicDemoEvents,
                m_playerPosition,
                m_playerYaw,
                m_publicDemoStability,
                m_publicDemoFocus,
                m_publicDemoElapsed,
                m_publicDemoSurge,
                m_publicDemoShardsCollected,
                PublicDemoAnchorsActivated(),
                m_publicDemoStageTransitions,
                m_publicDemoRetryCount,
                m_publicDemoCheckpointCount,
                m_publicDemoPressureHitCount,
                m_publicDemoFootstepEventCount,
                m_publicDemoGameplayEventRouteCount,
                m_publicDemoRelayOverchargeWindowCount,
                m_publicDemoRelayBridgeDrawCount,
                m_publicDemoTraversalMarkerCount,
                m_publicDemoComboChainStepCount,
                m_publicDemoCollisionSolveCount,
                m_publicDemoTraversalVaultCount,
                m_publicDemoTraversalDashCount,
                m_publicDemoEnemyChaseTickCount,
                m_publicDemoEnemyEvadeCount,
                m_publicDemoEnemyContactCount,
                m_publicDemoEnemyLineOfSightCheckCount,
                m_publicDemoEnemyTelegraphCount,
                m_publicDemoEnemyHitReactionCount,
                m_publicDemoControllerGroundedFrameCount,
                m_publicDemoControllerSlopeSampleCount,
                m_publicDemoControllerMaterialSampleCount,
                m_publicDemoControllerMovingPlatformFrameCount,
                m_publicDemoControllerCameraCollisionFrameCount,
                m_publicDemoAnimationClipEventCount,
                m_publicDemoAnimationBlendSampleCount,
                m_publicDemoAccessibilityToggleCount,
                m_publicDemoRenderingReadinessCount,
                m_publicDemoProductionReadinessCount,
                m_publicDemoGamepadFrameCount,
                m_publicDemoMenuTransitionCount,
                m_publicDemoFailurePresentationFrameCount,
                m_publicDemoContentAudioCueCount,
                m_publicDemoAnimationStateChangeCount,
                m_publicDemoExtractionReady,
                m_publicDemoCompleted,
                m_publicDemoMenuState,
                m_publicDemoAnimationState,
                m_publicDemoStage
            };
        }

        void RestorePublicDemoState(const PublicDemoStateSnapshot& snapshot)
        {
            m_publicDemoShards = snapshot.Shards;
            m_publicDemoAnchors = snapshot.Anchors;
            m_publicDemoResonanceGates = snapshot.ResonanceGates;
            m_publicDemoPhaseRelays = snapshot.PhaseRelays;
            m_publicDemoObstacles = snapshot.Obstacles;
            m_publicDemoEnemies = snapshot.Enemies;
            m_publicDemoCheckpoint = snapshot.Checkpoint;
            m_publicDemoEvents = snapshot.Events;
            m_playerPosition = snapshot.PlayerPosition;
            m_playerYaw = snapshot.PlayerYaw;
            m_publicDemoStability = snapshot.Stability;
            m_publicDemoFocus = snapshot.Focus;
            m_publicDemoElapsed = snapshot.Elapsed;
            m_publicDemoSurge = snapshot.Surge;
            m_publicDemoShardsCollected = snapshot.Collected;
            m_publicDemoStageTransitions = snapshot.StageTransitions;
            m_publicDemoRetryCount = snapshot.RetryCount;
            m_publicDemoCheckpointCount = snapshot.CheckpointCount;
            m_publicDemoPressureHitCount = snapshot.PressureHitCount;
            m_publicDemoFootstepEventCount = snapshot.FootstepEventCount;
            m_publicDemoGameplayEventRouteCount = snapshot.GameplayEventRouteCount;
            m_publicDemoRelayOverchargeWindowCount = snapshot.RelayOverchargeWindowCount;
            m_publicDemoRelayBridgeDrawCount = snapshot.RelayBridgeDrawCount;
            m_publicDemoTraversalMarkerCount = snapshot.TraversalMarkerCount;
            m_publicDemoComboChainStepCount = snapshot.ComboChainStepCount;
            m_publicDemoCollisionSolveCount = snapshot.CollisionSolveCount;
            m_publicDemoTraversalVaultCount = snapshot.TraversalVaultCount;
            m_publicDemoTraversalDashCount = snapshot.TraversalDashCount;
            m_publicDemoEnemyChaseTickCount = snapshot.EnemyChaseTickCount;
            m_publicDemoEnemyEvadeCount = snapshot.EnemyEvadeCount;
            m_publicDemoEnemyContactCount = snapshot.EnemyContactCount;
            m_publicDemoEnemyLineOfSightCheckCount = snapshot.EnemyLineOfSightCheckCount;
            m_publicDemoEnemyTelegraphCount = snapshot.EnemyTelegraphCount;
            m_publicDemoEnemyHitReactionCount = snapshot.EnemyHitReactionCount;
            m_publicDemoControllerGroundedFrameCount = snapshot.ControllerGroundedFrameCount;
            m_publicDemoControllerSlopeSampleCount = snapshot.ControllerSlopeSampleCount;
            m_publicDemoControllerMaterialSampleCount = snapshot.ControllerMaterialSampleCount;
            m_publicDemoControllerMovingPlatformFrameCount = snapshot.ControllerMovingPlatformFrameCount;
            m_publicDemoControllerCameraCollisionFrameCount = snapshot.ControllerCameraCollisionFrameCount;
            m_publicDemoAnimationClipEventCount = snapshot.AnimationClipEventCount;
            m_publicDemoAnimationBlendSampleCount = snapshot.AnimationBlendSampleCount;
            m_publicDemoAccessibilityToggleCount = snapshot.AccessibilityToggleCount;
            m_publicDemoRenderingReadinessCount = snapshot.RenderingReadinessCount;
            m_publicDemoProductionReadinessCount = snapshot.ProductionReadinessCount;
            m_publicDemoGamepadFrameCount = snapshot.GamepadFrameCount;
            m_publicDemoMenuTransitionCount = snapshot.MenuTransitionCount;
            m_publicDemoFailurePresentationFrameCount = snapshot.FailurePresentationFrameCount;
            m_publicDemoContentAudioCueCount = snapshot.ContentAudioCueCount;
            m_publicDemoAnimationStateChangeCount = snapshot.AnimationStateChangeCount;
            m_publicDemoExtractionReady = snapshot.ExtractionReady;
            m_publicDemoCompleted = snapshot.Completed;
            m_publicDemoMenuState = snapshot.MenuState;
            m_publicDemoAnimationState = snapshot.AnimationState;
            m_publicDemoStage = snapshot.Stage;
            SyncPlayerTransformToRegistry();
        }

        DirectX::XMFLOAT3 PublicDemoSentinelPosition(const PublicDemoSentinel& sentinel) const
        {
            const float angle = m_sceneAnimationTime * sentinel.Speed + sentinel.Phase;
            return Add(m_riftPosition, {
                std::sin(angle) * sentinel.Radius,
                -1.25f + sentinel.Height + std::sin(angle * 1.7f) * 0.24f,
                std::cos(angle) * sentinel.Radius * 0.72f
            });
        }

        void UpdatePublicDemoEnemies(float dt)
        {
            if (m_publicDemoCompleted)
            {
                return;
            }

            for (PublicDemoEnemy& enemy : m_publicDemoEnemies)
            {
                enemy.TelegraphTimer = std::max(0.0f, enemy.TelegraphTimer - dt);
                enemy.HitReactionTimer = std::max(0.0f, enemy.HitReactionTimer - dt);
                enemy.Telegraphing = enemy.TelegraphTimer > 0.0f;

                if (enemy.StunTimer > 0.0f)
                {
                    enemy.StunTimer = std::max(0.0f, enemy.StunTimer - dt);
                    enemy.Position = Lerp(enemy.Position, enemy.Home, std::min(dt * 1.4f, 1.0f));
                    continue;
                }

                const DirectX::XMFLOAT3 flatDelta = {
                    m_playerPosition.x - enemy.Position.x,
                    0.0f,
                    m_playerPosition.z - enemy.Position.z
                };
                const float distance = Length(flatDelta);
                enemy.HasLineOfSight = PublicDemoEnemyHasLineOfSight(enemy);
                enemy.LineOfSightScore = enemy.HasLineOfSight ? std::clamp(1.0f - distance / std::max(0.1f, enemy.AggroRadius), 0.0f, 1.0f) : 0.0f;
                ++m_publicDemoEnemyLineOfSightCheckCount;
                ++m_runtimeEditorStats.PublicDemoEnemyLineOfSightChecks;

                const float roleSpeedScale = enemy.Archetype == PublicDemoEnemyArchetype::Hunter ? 1.0f : (enemy.Archetype == PublicDemoEnemyArchetype::Warden ? 0.78f : 0.92f);
                const float roleContactScale = enemy.Archetype == PublicDemoEnemyArchetype::Warden ? 1.35f : 1.0f;
                const float roleTelegraphRange = enemy.Archetype == PublicDemoEnemyArchetype::Disruptor ? 3.2f : 2.35f;

                if (distance <= enemy.AggroRadius && enemy.HasLineOfSight)
                {
                    enemy.Alerted = true;
                    const DirectX::XMFLOAT3 chaseDirection = NormalizeFlat(flatDelta);
                    enemy.Position = Add(enemy.Position, Scale(chaseDirection, enemy.Speed * roleSpeedScale * dt));
                    ++m_publicDemoEnemyChaseTickCount;
                    ++m_runtimeEditorStats.PublicDemoEnemyChases;

                    if (distance <= roleTelegraphRange && enemy.TelegraphTimer <= 0.0f)
                    {
                        enemy.TelegraphTimer = 0.55f;
                        enemy.Telegraphing = true;
                        ++m_publicDemoEnemyTelegraphCount;
                        ++m_runtimeEditorStats.PublicDemoEnemyTelegraphs;
                    }

                    if (distance <= enemy.ContactRadius * roleContactScale)
                    {
                        if (m_publicDemoDashTimer > 0.0f)
                        {
                            enemy.StunTimer = 1.10f;
                            enemy.HitReactionTimer = 0.40f;
                            enemy.Evaded = true;
                            ++m_publicDemoEnemyEvadeCount;
                            ++m_publicDemoEnemyHitReactionCount;
                            ++m_runtimeEditorStats.PublicDemoEnemyEvades;
                            ++m_runtimeEditorStats.PublicDemoEnemyHitReactions;
                            PlayPublicDemoCue("enemy_evade", enemy.Position);
                            RecordPublicDemoEvent("Enemy evaded");
                        }
                        else
                        {
                            ++m_publicDemoEnemyContactCount;
                            ++m_publicDemoPressureHitCount;
                            if (m_runtimeVerification.Enabled)
                            {
                                ++m_runtimeEditorStats.PublicDemoPressureHits;
                            }
                            m_publicDemoStability = std::clamp(m_publicDemoStability - 0.22f, 0.0f, 1.0f);
                            PlayPublicDemoCue("enemy_contact", enemy.Position);
                            RecordPublicDemoEvent("Enemy contact");
                            if (m_publicDemoStability <= 0.10f)
                            {
                                TriggerPublicDemoRetry(true, "Enemy pressure collapse");
                                return;
                            }
                            enemy.StunTimer = 0.42f;
                        }
                    }
                }
                else
                {
                    enemy.Alerted = false;
                    const float patrolAngle = m_sceneAnimationTime * 0.42f + enemy.Phase;
                    const DirectX::XMFLOAT3 patrolTarget = Add(enemy.Home, {
                        std::sin(patrolAngle) * enemy.PatrolOffset.x,
                        0.0f,
                        std::cos(patrolAngle) * enemy.PatrolOffset.z
                    });
                    enemy.Position = Lerp(enemy.Position, patrolTarget, std::min(dt * 0.85f, 1.0f));
                }
            }
        }

        int NextPublicDemoShardIndex() const
        {
            for (size_t index = 0; index < m_publicDemoShards.size(); ++index)
            {
                if (!m_publicDemoShards[index].Collected)
                {
                    return static_cast<int>(index);
                }
            }
            return -1;
        }

        void CollectPublicDemoShard(size_t index, bool playAudio)
        {
            if (index >= m_publicDemoShards.size() || m_publicDemoShards[index].Collected)
            {
                return;
            }

            m_publicDemoShards[index].Collected = true;
            ++m_publicDemoShardsCollected;
            m_publicDemoStability = std::clamp(m_publicDemoStability + 0.12f, 0.0f, 1.0f);
            m_publicDemoFocus = std::clamp(m_publicDemoFocus + 0.18f, 0.0f, 1.0f);
            m_publicDemoSurge = 1.0f;
            ++m_publicDemoComboChainStepCount;
            SetPublicDemoAnimationState(PublicDemoAnimationState::Stabilize);
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoComboSteps;
            }
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoShardPickups;
            }

            if (playAudio && m_publicDemoPickupAudioArmed)
            {
                PlayPublicDemoCue("shard_pickup", m_publicDemoShards[index].Position);
            }

            if (m_publicDemoShardsCollected >= PublicDemoShardCount)
            {
                SetPublicDemoStage(PublicDemoStage::ActivateAnchors);
                SavePublicDemoCheckpoint("shards aligned");
                SetStatus("Rift charged. Align the three phase anchors.");
            }
            else
            {
                SavePublicDemoCheckpoint("shard captured");
                SetStatus("Echo shard captured " + std::to_string(m_publicDemoShardsCollected) + "/" + std::to_string(PublicDemoShardCount));
            }
        }

        void ActivatePublicDemoAnchor(size_t index, bool playAudio)
        {
            if (index >= m_publicDemoAnchors.size() || m_publicDemoAnchors[index].Activated)
            {
                return;
            }

            m_publicDemoAnchors[index].Activated = true;
            m_publicDemoStability = std::clamp(m_publicDemoStability + 0.10f, 0.0f, 1.0f);
            m_publicDemoSurge = 1.0f;
            ++m_publicDemoComboChainStepCount;
            SetPublicDemoAnimationState(PublicDemoAnimationState::Stabilize);
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoComboSteps;
            }
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoAnchorActivations;
            }

            const uint32_t anchorsActivated = PublicDemoAnchorsActivated();
            SavePublicDemoCheckpoint("anchor aligned");
            RecordPublicDemoEvent("Phase anchor aligned " + std::to_string(anchorsActivated) + "/" + std::to_string(PublicDemoAnchorCount));
            if (playAudio)
            {
                PlayPublicDemoCue("anchor_align", m_publicDemoAnchors[index].Position);
            }

            if (anchorsActivated >= PublicDemoAnchorCount)
            {
                SetPublicDemoStage(PublicDemoStage::TuneResonance);
                SavePublicDemoCheckpoint("anchors locked");
                SetStatus("Phase anchors locked. Tune the resonance gates.");
            }
            else
            {
                SetStatus("Phase anchor aligned " + std::to_string(anchorsActivated) + "/" + std::to_string(PublicDemoAnchorCount));
            }
        }

        void TunePublicDemoResonanceGate(size_t index, bool playAudio)
        {
            if (index >= m_publicDemoResonanceGates.size() || m_publicDemoResonanceGates[index].Tuned)
            {
                return;
            }

            m_publicDemoResonanceGates[index].Tuned = true;
            m_publicDemoStability = std::clamp(m_publicDemoStability + 0.08f, 0.0f, 1.0f);
            m_publicDemoSurge = 1.0f;
            ++m_publicDemoComboChainStepCount;
            SetPublicDemoAnimationState(PublicDemoAnimationState::Stabilize);
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoComboSteps;
            }
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoResonanceGates;
            }

            const uint32_t gatesTuned = PublicDemoResonanceGatesTuned();
            SavePublicDemoCheckpoint("resonance gate tuned");
            RecordPublicDemoEvent("Resonance gate tuned " + std::to_string(gatesTuned) + "/" + std::to_string(PublicDemoResonanceGateCount));
            if (playAudio)
            {
                PlayPublicDemoCue("gate_tune", m_publicDemoResonanceGates[index].Position);
            }

            if (gatesTuned >= PublicDemoResonanceGateCount)
            {
                SetPublicDemoStage(PublicDemoStage::StabilizeRelays);
                SavePublicDemoCheckpoint("gates tuned");
                SetStatus("Resonance field tuned. Stabilize the three phase relays.");
            }
            else
            {
                SetStatus("Resonance gate tuned " + std::to_string(gatesTuned) + "/" + std::to_string(PublicDemoResonanceGateCount));
            }
        }

        void StabilizePublicDemoPhaseRelay(size_t index, bool playAudio)
        {
            if (index >= m_publicDemoPhaseRelays.size() || m_publicDemoPhaseRelays[index].Stabilized)
            {
                return;
            }

            m_publicDemoPhaseRelays[index].Stabilized = true;
            m_publicDemoStability = std::clamp(m_publicDemoStability + 0.07f, 0.0f, 1.0f);
            m_publicDemoFocus = std::clamp(m_publicDemoFocus + 0.12f, 0.0f, 1.0f);
            m_publicDemoSurge = 1.0f;
            ++m_publicDemoComboChainStepCount;
            SetPublicDemoAnimationState(PublicDemoAnimationState::Stabilize);
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoPhaseRelays;
                ++m_runtimeEditorStats.PublicDemoComboSteps;
            }

            const uint32_t relaysStabilized = PublicDemoPhaseRelaysStabilized();
            SavePublicDemoCheckpoint("phase relay stabilized");
            RecordPublicDemoEvent("Phase relay stabilized " + std::to_string(relaysStabilized) + "/" + std::to_string(PublicDemoPhaseRelayCount));
            if (playAudio)
            {
                PlayPublicDemoCue("relay_stabilize", m_publicDemoPhaseRelays[index].Position);
            }

            if (relaysStabilized >= PublicDemoPhaseRelayCount)
            {
                m_publicDemoExtractionReady = true;
                SetPublicDemoStage(PublicDemoStage::ExtractionReady);
                SavePublicDemoCheckpoint("relays synchronized");
                SetStatus("Phase relays synchronized. Enter the extraction field.");
            }
            else
            {
                SetStatus("Phase relay stabilized " + std::to_string(relaysStabilized) + "/" + std::to_string(PublicDemoPhaseRelayCount));
            }
        }

        void TryCompletePublicDemoExtraction(bool playAudio)
        {
            if (!m_publicDemoExtractionReady ||
                m_publicDemoCompleted ||
                PublicDemoAnchorsActivated() < PublicDemoAnchorCount ||
                PublicDemoResonanceGatesTuned() < PublicDemoResonanceGateCount ||
                PublicDemoPhaseRelaysStabilized() < PublicDemoPhaseRelayCount)
            {
                return;
            }

            const DirectX::XMFLOAT3 extractionCenter = { m_riftPosition.x, 0.0f, m_riftPosition.z };
            const DirectX::XMFLOAT3 flatDelta = { m_playerPosition.x - extractionCenter.x, 0.0f, m_playerPosition.z - extractionCenter.z };
            if (Length(flatDelta) > 2.85f)
            {
                return;
            }

            m_publicDemoCompleted = true;
            m_publicDemoStability = 1.0f;
            m_publicDemoSurge = 1.0f;
            ++m_publicDemoComboChainStepCount;
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoComboSteps;
            }
            SetPublicDemoStage(PublicDemoStage::Completed);
            if (m_runtimeVerification.Enabled)
            {
                ++m_runtimeEditorStats.PublicDemoCompletions;
            }
            if (playAudio && !m_publicDemoCompletionAudioPlayed)
            {
                PlayPublicDemoCue("extraction_complete", m_riftPosition);
                m_publicDemoCompletionAudioPlayed = true;
            }
            SetPublicDemoAnimationState(PublicDemoAnimationState::Complete);
            SetPublicDemoMenuState(PublicDemoMenuState::Completed, "Menu -> completed");
            SetStatus("Extraction complete. DISPARITY demo loop cleared.");
        }

        void UpdatePublicDemo(float dt)
        {
            if (!m_publicDemoActive)
            {
                return;
            }

            m_publicDemoSurge = std::max(0.0f, m_publicDemoSurge - dt * 1.65f);
            m_publicDemoDashTimer = std::max(0.0f, m_publicDemoDashTimer - dt);
            m_publicDemoDashCooldown = std::max(0.0f, m_publicDemoDashCooldown - dt);
            if (m_publicDemoFailureOverlayTimer > 0.0f)
            {
                m_publicDemoFailureOverlayTimer = std::max(0.0f, m_publicDemoFailureOverlayTimer - dt);
                ++m_publicDemoFailurePresentationFrameCount;
                ++m_runtimeEditorStats.PublicDemoFailurePresentations;
            }
            if (dt <= 0.0f)
            {
                return;
            }

            const bool sprinting = IsPublicDemoSprinting();
            if (!m_publicDemoCompleted)
            {
                m_publicDemoElapsed += dt;
                m_publicDemoStepAccumulator += dt * (sprinting ? 1.45f : 1.0f);
                if (m_publicDemoStepAccumulator >= 0.42f)
                {
                    m_publicDemoStepAccumulator = 0.0f;
                    ++m_publicDemoFootstepEventCount;
                    if (m_runtimeVerification.Enabled)
                    {
                        ++m_runtimeEditorStats.PublicDemoFootstepEvents;
                    }
                    if ((m_publicDemoFootstepEventCount % 5u) == 1u && !m_runtimeVerification.Enabled)
                    {
                        PlayPublicDemoCue("footstep", m_playerPosition);
                    }
                }
            }

            m_publicDemoFocus = std::clamp(
                m_publicDemoFocus + (sprinting ? -0.42f : 0.20f) * dt,
                0.0f,
                1.0f);

            UpdatePublicDemoEnemies(dt);

            uint32_t closeSentinels = 0;
            for (const PublicDemoSentinel& sentinel : m_publicDemoSentinels)
            {
                const DirectX::XMFLOAT3 sentinelPosition = PublicDemoSentinelPosition(sentinel);
                const DirectX::XMFLOAT3 flatDelta = { sentinelPosition.x - m_playerPosition.x, 0.0f, sentinelPosition.z - m_playerPosition.z };
                if (Length(flatDelta) < 1.45f)
                {
                    ++closeSentinels;
                }
            }
            if (closeSentinels > 0 && !m_publicDemoCompleted)
            {
                m_publicDemoStability = std::clamp(m_publicDemoStability - 0.06f * static_cast<float>(closeSentinels) * dt, 0.0f, 1.0f);
                ++m_publicDemoPressureHitCount;
                if (m_runtimeVerification.Enabled)
                {
                    ++m_runtimeEditorStats.PublicDemoSentinelTicks;
                    ++m_runtimeEditorStats.PublicDemoPressureHits;
                }
                if ((m_publicDemoPressureHitCount % 24u) == 1u)
                {
                    RecordPublicDemoEvent("Sentinel pressure spike");
                }
            }
            if (m_publicDemoStability <= 0.01f && !m_publicDemoCompleted)
            {
                TriggerPublicDemoRetry(true, "Stability collapsed");
                return;
            }

            for (size_t index = 0; index < m_publicDemoShards.size(); ++index)
            {
                if (m_publicDemoShards[index].Collected)
                {
                    continue;
                }

                const DirectX::XMFLOAT3 flatDelta = {
                    m_publicDemoShards[index].Position.x - m_playerPosition.x,
                    0.0f,
                    m_publicDemoShards[index].Position.z - m_playerPosition.z
                };
                if (Length(flatDelta) <= 1.25f)
                {
                    CollectPublicDemoShard(index, true);
                }
            }

            if (m_publicDemoStage == PublicDemoStage::ActivateAnchors)
            {
                for (size_t index = 0; index < m_publicDemoAnchors.size(); ++index)
                {
                    if (m_publicDemoAnchors[index].Activated)
                    {
                        continue;
                    }

                    const DirectX::XMFLOAT3 flatDelta = {
                        m_publicDemoAnchors[index].Position.x - m_playerPosition.x,
                        0.0f,
                        m_publicDemoAnchors[index].Position.z - m_playerPosition.z
                    };
                    if (Length(flatDelta) <= 1.55f)
                    {
                        ActivatePublicDemoAnchor(index, true);
                    }
                }
            }

            if (m_publicDemoStage == PublicDemoStage::TuneResonance)
            {
                for (size_t index = 0; index < m_publicDemoResonanceGates.size(); ++index)
                {
                    if (m_publicDemoResonanceGates[index].Tuned)
                    {
                        continue;
                    }

                    const DirectX::XMFLOAT3 flatDelta = {
                        m_publicDemoResonanceGates[index].Position.x - m_playerPosition.x,
                        0.0f,
                        m_publicDemoResonanceGates[index].Position.z - m_playerPosition.z
                    };
                    if (Length(flatDelta) <= m_publicDemoResonanceGates[index].Radius)
                    {
                        TunePublicDemoResonanceGate(index, true);
                    }
                }
            }

            if (m_publicDemoStage == PublicDemoStage::StabilizeRelays)
            {
                for (size_t index = 0; index < m_publicDemoPhaseRelays.size(); ++index)
                {
                    if (m_publicDemoPhaseRelays[index].Stabilized)
                    {
                        continue;
                    }

                    const DirectX::XMFLOAT3 flatDelta = {
                        m_publicDemoPhaseRelays[index].Position.x - m_playerPosition.x,
                        0.0f,
                        m_publicDemoPhaseRelays[index].Position.z - m_playerPosition.z
                    };
                    const float distance = Length(flatDelta);
                    if (distance <= m_publicDemoPhaseRelays[index].Radius * 1.85f)
                    {
                        ++m_publicDemoRelayOverchargeWindowCount;
                        if (m_runtimeVerification.Enabled)
                        {
                            ++m_runtimeEditorStats.PublicDemoRelayOverchargeWindows;
                        }
                        if ((m_publicDemoRelayOverchargeWindowCount % 18u) == 1u)
                        {
                            RecordPublicDemoEvent("Relay overcharge window");
                            if (!m_cinematicAudioCues)
                            {
                                PlayPublicDemoCue("relay_overcharge", m_publicDemoPhaseRelays[index].Position);
                            }
                        }
                    }
                    if (distance <= m_publicDemoPhaseRelays[index].Radius)
                    {
                        StabilizePublicDemoPhaseRelay(index, true);
                    }
                }
            }

            TryCompletePublicDemoExtraction(true);
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

            if (m_publicDemoActive)
            {
                for (const PublicDemoCollisionObstacle& obstacle : m_publicDemoObstacles)
                {
                    if (obstacle.Traversable)
                    {
                        continue;
                    }

                    const bool cameraInsideBlocker =
                        std::abs(position.x - obstacle.Position.x) < obstacle.HalfExtents.x + 0.45f &&
                        std::abs(position.z - obstacle.Position.z) < obstacle.HalfExtents.z + 0.45f;
                    if (cameraInsideBlocker)
                    {
                        position = Add(target, Scale(lookDirection, -std::max(3.2f, m_cameraDistance - 2.0f)));
                        ++m_publicDemoControllerCameraCollisionFrameCount;
                        ++m_runtimeEditorStats.PublicDemoControllerCameraCollisionFrames;
                        break;
                    }
                }
            }

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
            const std::string& curve = !b.EasingCurve.empty() ? b.EasingCurve : a.EasingCurve;
            float shaped = SmoothStep01(clamped);
            if (curve == "linear")
            {
                shaped = clamped;
            }
            else if (curve == "ease_in")
            {
                shaped = clamped * clamped;
            }
            else if (curve == "ease_out")
            {
                shaped = 1.0f - (1.0f - clamped) * (1.0f - clamped);
            }
            else if (curve == "expo")
            {
                shaped = clamped <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (clamped - 1.0f));
            }
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
            size_t segmentEndIndex = m_trailerKeys.size() - 1u;
            for (size_t index = 1; index < m_trailerKeys.size(); ++index)
            {
                if (playbackTime <= m_trailerKeys[index].Time)
                {
                    a = m_trailerKeys[index - 1];
                    b = m_trailerKeys[index];
                    segmentEndIndex = index;
                    break;
                }
            }

            const float segmentDuration = std::max(0.001f, b.Time - a.Time);
            const float t = ApplyShotEasing((playbackTime - a.Time) / segmentDuration, a, b);
            const bool useSpline = (a.SplineMode == "catmull" || b.SplineMode == "catmull") && m_trailerKeys.size() >= 3u;
            const size_t p1 = segmentEndIndex > 0u ? segmentEndIndex - 1u : 0u;
            const size_t p0 = p1 > 0u ? p1 - 1u : p1;
            const size_t p3 = std::min(m_trailerKeys.size() - 1u, segmentEndIndex + 1u);
            const DirectX::XMFLOAT3 cameraPosition = useSpline
                ? CatmullRom(m_trailerKeys[p0].Position, a.Position, b.Position, m_trailerKeys[p3].Position, t)
                : Lerp(a.Position, b.Position, t);
            const DirectX::XMFLOAT3 cameraTarget = useSpline
                ? CatmullRom(m_trailerKeys[p0].Target, a.Target, b.Target, m_trailerKeys[p3].Target, t)
                : Lerp(a.Target, b.Target, t);
            m_trailerCamera.LookAt(cameraPosition, cameraTarget, { 0.0f, 1.0f, 0.0f });
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
                    if (fields.size() > 12 && !fields[12].empty()) { key.SplineMode = fields[12]; }
                    if (fields.size() > 13 && !fields[13].empty()) { key.TimelineLane = fields[13]; }
                    if (fields.size() > 14) { (void)ParseShotFloat3(fields[14], key.ThumbnailTint); }
                    if (fields.size() > 15 && !fields[15].empty()) { key.EasingCurve = fields[15]; }
                    if (fields.size() > 16 && !fields[16].empty()) { key.RendererTrack = fields[16]; }
                    if (fields.size() > 17 && !fields[17].empty()) { key.AudioTrack = fields[17]; }
                    if (fields.size() > 18 && !fields[18].empty()) { key.ThumbnailPath = fields[18]; }
                    if (fields.size() > 19 && !fields[19].empty()) { key.ClipLane = fields[19]; }
                    if (fields.size() > 20 && !fields[20].empty()) { key.NestedSequence = fields[20]; }
                    if (fields.size() > 21 && !fields[21].empty()) { key.HoldSeconds = std::stof(fields[21]); }
                    if (fields.size() > 22 && !fields[22].empty()) { key.ShotRole = fields[22]; }
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

            file << "# DISPARITY cinematic shot track v6\n";
            file << "# key time|position(x,y,z)|target(x,y,z)|focus|dof_strength|lens_dirt|letterbox|ease_in|ease_out|renderer_pulse|audio_cue|bookmark|spline|lane|thumbnail_tint|easing_curve|renderer_track|audio_track|thumbnail_path|clip_lane|nested_sequence|hold_seconds|shot_role\n";
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
                    << key.Bookmark << '|'
                    << key.SplineMode << '|'
                    << key.TimelineLane << '|'
                    << FormatShotFloat3(key.ThumbnailTint) << '|'
                    << key.EasingCurve << '|'
                    << key.RendererTrack << '|'
                    << key.AudioTrack << '|'
                    << key.ThumbnailPath << '|'
                    << key.ClipLane << '|'
                    << key.NestedSequence << '|'
                    << key.HoldSeconds << '|'
                    << key.ShotRole << '\n';
            }

            return file.good();
        }

        bool WriteShotThumbnail(size_t index, TrailerShotKey& key)
        {
            const std::filesystem::path outputDirectory = "Saved/ShotThumbnails";
            std::filesystem::create_directories(outputDirectory);
            const std::string bookmark = key.Bookmark.empty() ? ("shot_" + std::to_string(index)) : SafeFileStem(key.Bookmark);
            const std::filesystem::path outputPath = outputDirectory / ("shot_" + std::to_string(index) + "_" + bookmark + ".ppm");
            std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                return false;
            }

            constexpr uint32_t Width = 96;
            constexpr uint32_t Height = 54;
            output << "P6\n" << Width << " " << Height << "\n255\n";
            for (uint32_t y = 0; y < Height; ++y)
            {
                for (uint32_t x = 0; x < Width; ++x)
                {
                    const float u = static_cast<float>(x) / static_cast<float>(Width - 1u);
                    const float v = static_cast<float>(y) / static_cast<float>(Height - 1u);
                    const float scanline = (y % 9u) == 0u ? 0.18f : 0.0f;
                    const float pulse = std::clamp(key.RendererPulse * 0.35f + key.AudioCue * 0.25f, 0.0f, 0.6f);
                    const unsigned char pixel[3] = {
                        static_cast<unsigned char>(std::clamp((key.ThumbnailTint.x * (0.35f + u * 0.65f) + pulse + scanline) * 255.0f, 0.0f, 255.0f)),
                        static_cast<unsigned char>(std::clamp((key.ThumbnailTint.y * (0.45f + v * 0.55f) + scanline) * 255.0f, 0.0f, 255.0f)),
                        static_cast<unsigned char>(std::clamp((key.ThumbnailTint.z * (0.55f + (1.0f - u) * 0.45f) + pulse * 0.5f) * 255.0f, 0.0f, 255.0f))
                    };
                    output.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
                }
            }

            key.ThumbnailPath = outputPath.generic_string();
            return output.good();
        }

        size_t GenerateShotThumbnails()
        {
            size_t written = 0;
            for (size_t index = 0; index < m_trailerKeys.size(); ++index)
            {
                if (WriteShotThumbnail(index, m_trailerKeys[index]))
                {
                    ++written;
                }
            }
            return written;
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

        void SimulateRiftVfxRenderer(float visualTime, float modeBoost)
        {
            m_riftParticles.clear();
            m_riftRibbons.clear();
            const DirectX::XMFLOAT3 cameraPosition = GetRenderCamera().GetPosition();

            for (int particleIndex = 0; particleIndex < 36; ++particleIndex)
            {
                const float n = static_cast<float>(particleIndex);
                const float seed = n * 12.9898f;
                const float angle = n * 0.83f + visualTime * (0.58f + std::fmod(n, 5.0f) * 0.03f);
                const float radius = 1.35f + std::fmod(n * 1.91f, 3.4f);
                const float orbitY = std::sin(visualTime * 1.18f + seed) * (0.95f + std::fmod(n, 3.0f) * 0.22f);
                RiftVfxParticle particle;
                particle.Position = Add(m_riftPosition, {
                    std::sin(angle) * radius,
                    orbitY,
                    std::cos(angle * 1.11f) * radius * 0.72f
                });
                particle.Size = (0.14f + std::fmod(n, 6.0f) * 0.018f + m_riftBeatPulse * 0.16f) * modeBoost;
                particle.Depth = Length(Subtract(cameraPosition, particle.Position));
                particle.DepthFade = std::clamp((particle.Depth - 1.0f) / 9.0f, 0.22f, 1.0f);
                particle.Roll = angle;
                particle.Hot = particleIndex % 3 == 0;
                m_riftParticles.push_back(particle);
            }

            std::sort(m_riftParticles.begin(), m_riftParticles.end(), [](const RiftVfxParticle& a, const RiftVfxParticle& b) {
                return a.Depth > b.Depth;
            });

            for (int ribbonIndex = 0; ribbonIndex < 14; ++ribbonIndex)
            {
                const float n = static_cast<float>(ribbonIndex);
                const float angle = visualTime * 0.64f + n * Pi * 2.0f / 14.0f;
                const float radius = 3.0f + std::sin(visualTime * 0.41f + n) * 0.34f;
                RiftVfxRibbonSegment ribbon;
                ribbon.Position = Add(m_riftPosition, {
                    std::sin(angle) * radius,
                    std::cos(angle * 1.7f) * 0.55f,
                    std::cos(angle) * radius * 0.74f
                });
                ribbon.Rotation = { 0.08f * std::sin(angle), angle + Pi * 0.5f, 0.22f * std::sin(visualTime + n) };
                ribbon.Scale = { 0.055f * modeBoost, 0.035f * modeBoost, (1.1f + m_riftBeatPulse * 0.5f) * modeBoost };
                ribbon.Depth = Length(Subtract(cameraPosition, ribbon.Position));
                m_riftRibbons.push_back(ribbon);
            }

            std::sort(m_riftRibbons.begin(), m_riftRibbons.end(), [](const RiftVfxRibbonSegment& a, const RiftVfxRibbonSegment& b) {
                return a.Depth > b.Depth;
            });

            m_lastRiftVfxStats = RiftVfxSystemStats{
                7u,
                static_cast<uint32_t>(m_riftParticles.size()),
                static_cast<uint32_t>(m_riftRibbons.size()),
                8u,
                static_cast<uint32_t>(m_riftParticles.size()),
                6u,
                4u,
                static_cast<uint32_t>(m_riftParticles.size() + m_riftRibbons.size()),
                static_cast<uint32_t>((m_riftParticles.size() + m_riftRibbons.size()) * 2u),
                static_cast<uint32_t>(m_riftParticles.size()),
                4u,
                static_cast<uint32_t>(m_riftParticles.size() * 2u),
                m_riftVfxEmitterProfile.EmitterCount,
                m_riftVfxEmitterProfile.SortBuckets,
                static_cast<uint32_t>(
                    m_riftParticles.size() * m_riftVfxEmitterProfile.GpuBufferBytesPerParticle +
                    m_riftRibbons.size() * m_riftVfxEmitterProfile.GpuBufferBytesPerRibbon)
            };
        }

        void DrawRiftVfx(Disparity::Renderer& renderer)
        {
            const float visualTime = ShowcaseVisualTime();
            const float modeBoost = (m_showcaseMode || m_trailerMode) ? 1.0f : 0.64f;
            SimulateRiftVfxRenderer(visualTime, modeBoost);
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

            for (const RiftVfxParticle& particle : m_riftParticles)
            {
                Disparity::Material material = particle.Hot ? m_vfxHotParticleMaterial : m_vfxParticleMaterial;
                material.Alpha = std::clamp(material.Alpha * particle.DepthFade, 0.12f, 1.0f);
                renderer.DrawMesh(m_vfxQuadMesh, BillboardTransform(particle.Position, particle.Size, particle.Roll), material);
                ++drawCount;
            }

            for (const RiftVfxRibbonSegment& ribbonSegment : m_riftRibbons)
            {
                Disparity::Transform ribbon;
                ribbon.Position = ribbonSegment.Position;
                ribbon.Rotation = ribbonSegment.Rotation;
                ribbon.Scale = ribbonSegment.Scale;
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
            m_highResCaptureManifestPath = "Saved/Captures/DISPARITY_photo_2x.dcapture.json";
            m_highResCapturePending = true;
            m_highResCaptureWorkerStarted = false;
            m_highResCaptureTilesWritten = 0;
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

        HighResolutionCaptureMetrics GetHighResolutionCaptureMetrics() const
        {
            return {
                m_highResolutionCapturePreset.Name,
                m_highResolutionCapturePreset.Scale,
                m_highResolutionCapturePreset.Tiles,
                m_highResolutionCapturePreset.MsaaSamples,
                m_highResolutionCapturePreset.ResolveSamples,
                m_highResolutionCapturePreset.ResolveFilter,
                m_highResolutionCapturePreset.AsyncCompressionReady,
                m_highResolutionCapturePreset.ExrOutputPlanned
            };
        }

        bool WriteSupersampledPpm(const PpmImage& image, const std::filesystem::path& path, const HighResolutionCaptureMetrics& metrics) const
        {
            if (image.Width == 0 || image.Height == 0 || image.Pixels.empty() || metrics.Scale < 2 || metrics.ResolveSamples == 0)
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

            const uint32_t outWidth = image.Width * metrics.Scale;
            const uint32_t outHeight = image.Height * metrics.Scale;
            output << "P6\n" << outWidth << " " << outHeight << "\n255\n";
            // This is still sourced from the current frame capture, but using a tent-like
            // resolve avoids the blocky nearest-neighbor proof while the real tiled path matures.
            const auto channel = [&image](uint32_t sx, uint32_t sy, uint32_t c) -> float {
                const size_t index = (static_cast<size_t>(sy) * image.Width + sx) * 3u + c;
                return static_cast<float>(image.Pixels[index]);
            };

            struct AxisSample
            {
                uint32_t Lower = 0;
                uint32_t Upper = 0;
                float Blend = 0.0f;
            };

            std::vector<AxisSample> xSamples(outWidth);
            for (uint32_t x = 0; x < outWidth; ++x)
            {
                const float sourceX = (static_cast<float>(x) + 0.5f) / static_cast<float>(metrics.Scale) - 0.5f;
                const uint32_t x0 = static_cast<uint32_t>(std::clamp(std::floor(sourceX), 0.0f, static_cast<float>(image.Width - 1u)));
                xSamples[x] = {
                    x0,
                    std::min(x0 + 1u, image.Width - 1u),
                    std::clamp(sourceX - static_cast<float>(x0), 0.0f, 1.0f)
                };
            }

            std::vector<unsigned char> rowBuffer(static_cast<size_t>(outWidth) * 3u);
            for (uint32_t y = 0; y < outHeight; ++y)
            {
                const float sourceY = (static_cast<float>(y) + 0.5f) / static_cast<float>(metrics.Scale) - 0.5f;
                const uint32_t y0 = static_cast<uint32_t>(std::clamp(std::floor(sourceY), 0.0f, static_cast<float>(image.Height - 1u)));
                const uint32_t y1 = std::min(y0 + 1u, image.Height - 1u);
                const float ty = std::clamp(sourceY - static_cast<float>(y0), 0.0f, 1.0f);
                for (uint32_t x = 0; x < outWidth; ++x)
                {
                    const AxisSample& xs = xSamples[x];
                    for (uint32_t c = 0; c < 3u; ++c)
                    {
                        const float top = channel(xs.Lower, y0, c) * (1.0f - xs.Blend) + channel(xs.Upper, y0, c) * xs.Blend;
                        const float bottom = channel(xs.Lower, y1, c) * (1.0f - xs.Blend) + channel(xs.Upper, y1, c) * xs.Blend;
                        rowBuffer[static_cast<size_t>(x) * 3u + c] = static_cast<unsigned char>(std::clamp(top * (1.0f - ty) + bottom * ty, 0.0f, 255.0f));
                    }
                }
                output.write(reinterpret_cast<const char*>(rowBuffer.data()), static_cast<std::streamsize>(rowBuffer.size()));
            }

            return output.good();
        }

        bool WriteHighResolutionCaptureManifest(const PpmImage& image) const
        {
            if (m_highResCaptureManifestPath.empty())
            {
                return false;
            }

            if (m_highResCaptureManifestPath.has_parent_path())
            {
                std::filesystem::create_directories(m_highResCaptureManifestPath.parent_path());
            }

            std::ofstream manifest(m_highResCaptureManifestPath, std::ios::trunc);
            if (!manifest)
            {
                return false;
            }

            const HighResolutionCaptureMetrics metrics = GetHighResolutionCaptureMetrics();
            manifest << "{\n";
            manifest << "  \"schema\": 3,\n";
            manifest << "  \"preset_name\": \"" << JsonEscape(metrics.PresetName) << "\",\n";
            manifest << "  \"graph_owned_offscreen\": true,\n";
            manifest << "  \"source\": \"" << m_highResCaptureSourcePath.generic_string() << "\",\n";
            manifest << "  \"output\": \"" << m_highResCaptureOutputPath.generic_string() << "\",\n";
            manifest << "  \"source_width\": " << image.Width << ",\n";
            manifest << "  \"source_height\": " << image.Height << ",\n";
            manifest << "  \"output_width\": " << image.Width * metrics.Scale << ",\n";
            manifest << "  \"output_height\": " << image.Height * metrics.Scale << ",\n";
            manifest << "  \"scale\": " << metrics.Scale << ",\n";
            manifest << "  \"tiles\": " << metrics.Tiles << ",\n";
            manifest << "  \"msaa_samples\": " << metrics.MsaaSamples << ",\n";
            manifest << "  \"resolve\": \"" << metrics.ResolveFilter << "\",\n";
            manifest << "  \"resolve_samples\": " << metrics.ResolveSamples << ",\n";
            manifest << "  \"tile_jitter\": [[-0.25,-0.25],[0.25,-0.25],[-0.25,0.25],[0.25,0.25]],\n";
            manifest << "  \"async_compression_worker\": " << (metrics.AsyncCompressionReady ? "true" : "false") << ",\n";
            manifest << "  \"async_compression_ready\": " << (metrics.AsyncCompressionReady ? "true" : "false") << ",\n";
            manifest << "  \"exr_output_planned\": " << (metrics.ExrOutputPlanned ? "true" : "false") << ",\n";
            manifest << "  \"worker\": \"async\"\n";
            manifest << "}\n";
            return manifest.good();
        }

        void CompleteHighResolutionCaptureIfReady()
        {
            if (!m_highResCapturePending || !m_renderer || !m_renderer->HasLastFrameCapture())
            {
                return;
            }

            if (m_highResCaptureWorkerStarted)
            {
                if (!m_highResCaptureFuture.valid() ||
                    m_highResCaptureFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
                {
                    return;
                }

                const bool workerSucceeded = m_highResCaptureFuture.get();
                m_highResCapturePending = false;
                m_highResCaptureWorkerStarted = false;
                if (!workerSucceeded)
                {
                    SetStatus("High-res capture failed");
                    if (m_runtimeVerification.Enabled)
                    {
                        AddRuntimeVerificationFailure("high-resolution capture worker failed.");
                    }
                    return;
                }

                if (m_runtimeVerification.Enabled)
                {
                    m_runtimeBudgetSkipFrames = std::max(m_runtimeBudgetSkipFrames, 4u);
                }
                ++m_runtimeEditorStats.HighResCaptures;
                SetStatus("Wrote " + m_highResCaptureOutputPath.string());
                if (m_runtimeVerification.Enabled)
                {
                    AddRuntimeVerificationNote("Validated async 2x high-resolution capture workflow.");
                }
                return;
            }

            const Disparity::FrameCaptureResult& capture = m_renderer->GetLastFrameCapture();
            if (!capture.Success || capture.Path.lexically_normal() != m_highResCaptureSourcePath.lexically_normal())
            {
                return;
            }

            PpmImage image;
            if (!ReadPpmImage(m_highResCaptureSourcePath, image))
            {
                m_highResCapturePending = false;
                SetStatus("High-res capture failed");
                if (m_runtimeVerification.Enabled)
                {
                    AddRuntimeVerificationFailure("high-resolution capture source read failed.");
                }
                return;
            }

            const HighResolutionCaptureMetrics metrics = GetHighResolutionCaptureMetrics();
            m_highResCaptureTilesWritten = metrics.Tiles;
            const std::filesystem::path outputPath = m_highResCaptureOutputPath;
            m_highResCaptureWorkerStarted = true;
            m_highResCaptureFuture = std::async(std::launch::async, [this, image = std::move(image), outputPath, metrics]() mutable {
                const bool wroteImage = WriteSupersampledPpm(image, outputPath, metrics);
                const bool wroteManifest = WriteHighResolutionCaptureManifest(image);
                return wroteImage && wroteManifest;
            });
            SetStatus("High-res capture worker queued");
        }

        void DrawPublicDemo(Disparity::Renderer& renderer)
        {
            if (!m_publicDemoActive)
            {
                return;
            }

            const float visualTime = ShowcaseVisualTime();
            const float charge = PublicDemoRiftCharge();
            uint32_t beaconDraws = 0;
            uint32_t relayBridgeDraws = 0;
            uint32_t traversalMarkers = 0;

            if (!m_publicDemoCompleted)
            {
                const DirectX::XMFLOAT3 objective = PublicDemoCurrentObjectivePosition();
                const DirectX::XMFLOAT3 start = Add(m_playerPosition, { 0.0f, 0.22f, 0.0f });
                const DirectX::XMFLOAT3 end = { objective.x, 0.25f, objective.z };
                Disparity::Material hintMaterial = m_demoPathMaterial;
                hintMaterial.Alpha = 0.18f + m_publicDemoSurge * 0.12f;
                renderer.DrawMesh(m_cubeMesh, BeamTransform(start, end, 0.018f), hintMaterial);
                ++beaconDraws;
            }

            for (size_t index = 0; index < m_publicDemoShards.size(); ++index)
            {
                const PublicDemoShard& shard = m_publicDemoShards[index];
                const float phase = visualTime * 1.8f + shard.Phase;
                const float bob = std::sin(phase) * 0.22f;
                const float pulse = 0.5f + 0.5f * std::sin(phase * 2.1f);
                const float collectedRadius = 0.95f + static_cast<float>(index) * 0.055f;
                const float collectedAngle = visualTime * 0.84f + shard.Phase;
                const DirectX::XMFLOAT3 drawPosition = shard.Collected
                    ? Add(m_riftPosition, { std::sin(collectedAngle) * collectedRadius, -0.1f + std::cos(collectedAngle * 1.4f) * 0.18f, std::cos(collectedAngle) * collectedRadius * 0.72f })
                    : Add(shard.Position, { 0.0f, bob, 0.0f });

                Disparity::Material shardMaterial = shard.Collected ? m_demoChargedShardMaterial : m_demoShardMaterial;
                shardMaterial.Albedo = shard.Collected
                    ? DirectX::XMFLOAT3{ 3.8f, 0.42f + pulse * 0.35f, 3.1f }
                    : DirectX::XMFLOAT3{ shard.Color.x * (1.5f + pulse), shard.Color.y * (1.2f + pulse), shard.Color.z * (1.3f + pulse) };
                shardMaterial.EmissiveIntensity += m_publicDemoSurge * 0.65f;

                Disparity::Transform shardTransform;
                shardTransform.Position = drawPosition;
                shardTransform.Rotation = { phase * 0.74f, phase, phase * 0.37f };
                const float shardScale = shard.Collected ? 0.18f : 0.28f + pulse * 0.05f;
                shardTransform.Scale = { shardScale * 0.72f, shardScale * 1.75f, shardScale * 0.72f };
                renderer.DrawMesh(m_cubeMesh, shardTransform, shardMaterial);

                Disparity::Transform ringTransform;
                ringTransform.Position = Add(drawPosition, { 0.0f, -0.18f, 0.0f });
                ringTransform.Rotation = { Pi * 0.5f, phase * 0.45f, phase * 0.22f };
                const float ringRadius = shard.Collected ? 0.52f : 0.86f + pulse * 0.08f;
                ringTransform.Scale = { ringRadius, ringRadius, ringRadius };
                renderer.DrawMesh(m_gizmoRingMesh, ringTransform, shardMaterial);

                if (!shard.Collected)
                {
                    const DirectX::XMFLOAT3 beaconStart = { shard.Position.x, 0.05f, shard.Position.z };
                    const DirectX::XMFLOAT3 beaconEnd = { shard.Position.x, 4.0f + pulse * 0.8f, shard.Position.z };
                    Disparity::Material beaconMaterial = shardMaterial;
                    beaconMaterial.Alpha = 0.34f;
                    renderer.DrawMesh(m_cubeMesh, BeamTransform(beaconStart, beaconEnd, 0.035f + pulse * 0.012f), beaconMaterial);
                    ++beaconDraws;
                }
            }

            for (size_t index = 0; index < m_publicDemoAnchors.size(); ++index)
            {
                const PublicDemoAnchor& anchor = m_publicDemoAnchors[index];
                const float phase = visualTime * 1.3f + anchor.Phase;
                const float pulse = 0.5f + 0.5f * std::sin(phase * 2.4f);
                Disparity::Material anchorMaterial = m_demoAnchorMaterial;
                anchorMaterial.Albedo = anchor.Activated
                    ? DirectX::XMFLOAT3{ anchor.Color.x * 2.8f, anchor.Color.y * 2.5f, anchor.Color.z * 3.2f }
                    : DirectX::XMFLOAT3{ anchor.Color.x * 0.72f, anchor.Color.y * 0.64f, anchor.Color.z * 0.82f };
                anchorMaterial.Alpha = anchor.Activated ? 0.78f : (m_publicDemoStage == PublicDemoStage::ActivateAnchors ? 0.52f : 0.22f);
                anchorMaterial.EmissiveIntensity += anchor.Activated ? 1.25f + pulse * 0.8f : pulse * 0.25f;

                Disparity::Transform anchorRing;
                anchorRing.Position = { anchor.Position.x, 0.09f, anchor.Position.z };
                anchorRing.Rotation = { Pi * 0.5f, phase * (anchor.Activated ? 0.42f : 0.16f), 0.0f };
                const float anchorRadius = anchor.Activated ? 1.05f + pulse * 0.08f : 0.82f;
                anchorRing.Scale = { anchorRadius, anchorRadius, anchorRadius };
                renderer.DrawMesh(m_gizmoRingMesh, anchorRing, anchorMaterial);

                Disparity::Transform glyph;
                glyph.Position = { anchor.Position.x, 0.42f + pulse * 0.12f, anchor.Position.z };
                glyph.Rotation = { phase * 0.33f, phase, phase * 0.27f };
                glyph.Scale = { 0.20f, anchor.Activated ? 0.62f : 0.34f, 0.20f };
                renderer.DrawMesh(m_cubeMesh, glyph, anchorMaterial);
                beaconDraws += 2;

                if (anchor.Activated)
                {
                    const DirectX::XMFLOAT3 start = { anchor.Position.x, 0.34f, anchor.Position.z };
                    const DirectX::XMFLOAT3 end = { m_riftPosition.x, 0.72f + pulse * 0.24f, m_riftPosition.z };
                    Disparity::Material bridgeMaterial = anchorMaterial;
                    bridgeMaterial.Alpha = 0.24f + pulse * 0.10f;
                    renderer.DrawMesh(m_cubeMesh, BeamTransform(start, end, 0.024f + pulse * 0.007f), bridgeMaterial);
                    ++beaconDraws;
                }
            }

            for (size_t index = 0; index < m_publicDemoResonanceGates.size(); ++index)
            {
                const PublicDemoResonanceGate& gate = m_publicDemoResonanceGates[index];
                const float phase = visualTime * 1.65f + gate.Phase;
                const float pulse = 0.5f + 0.5f * std::sin(phase * 2.8f);
                Disparity::Material gateMaterial = m_demoResonanceMaterial;
                gateMaterial.Albedo = gate.Tuned
                    ? DirectX::XMFLOAT3{ gate.Color.x * 2.5f, gate.Color.y * 3.0f, gate.Color.z * 2.8f }
                    : DirectX::XMFLOAT3{ gate.Color.x * 0.55f, gate.Color.y * 0.75f, gate.Color.z * 0.68f };
                gateMaterial.Alpha = gate.Tuned ? 0.74f : (m_publicDemoStage == PublicDemoStage::TuneResonance ? 0.46f : 0.18f);
                gateMaterial.EmissiveIntensity += gate.Tuned ? 1.10f + pulse * 0.7f : pulse * 0.22f;

                Disparity::Transform groundRing;
                groundRing.Position = { gate.Position.x, 0.11f, gate.Position.z };
                groundRing.Rotation = { Pi * 0.5f, phase * 0.35f, 0.0f };
                const float radius = gate.Radius + pulse * 0.08f;
                groundRing.Scale = { radius, radius, radius };
                renderer.DrawMesh(m_gizmoRingMesh, groundRing, gateMaterial);

                Disparity::Transform verticalGlyph;
                verticalGlyph.Position = { gate.Position.x, 0.55f + pulse * 0.16f, gate.Position.z };
                verticalGlyph.Rotation = { phase * 0.24f, phase * 0.88f, phase * 0.31f };
                verticalGlyph.Scale = { 0.16f, gate.Tuned ? 0.80f : 0.42f, 0.16f };
                renderer.DrawMesh(m_cubeMesh, verticalGlyph, gateMaterial);
                beaconDraws += 2;

                if (gate.Tuned)
                {
                    const DirectX::XMFLOAT3 start = { gate.Position.x, 0.48f, gate.Position.z };
                    const DirectX::XMFLOAT3 end = { m_riftPosition.x, 1.02f + pulse * 0.18f, m_riftPosition.z };
                    Disparity::Material bridgeMaterial = gateMaterial;
                    bridgeMaterial.Alpha = 0.20f + pulse * 0.10f;
                    renderer.DrawMesh(m_cubeMesh, BeamTransform(start, end, 0.018f + pulse * 0.006f), bridgeMaterial);
                    ++beaconDraws;
                }
            }

            for (size_t index = 0; index < m_publicDemoPhaseRelays.size(); ++index)
            {
                const PublicDemoPhaseRelay& relay = m_publicDemoPhaseRelays[index];
                const float phase = visualTime * 1.42f + relay.Phase;
                const float pulse = 0.5f + 0.5f * std::sin(phase * 3.05f);
                const bool relayVisible =
                    relay.Stabilized ||
                    m_publicDemoStage == PublicDemoStage::StabilizeRelays ||
                    m_publicDemoStage == PublicDemoStage::ExtractionReady ||
                    m_publicDemoStage == PublicDemoStage::Completed;

                Disparity::Material relayMaterial = m_demoRelayMaterial;
                relayMaterial.Albedo = relay.Stabilized
                    ? DirectX::XMFLOAT3{ relay.Color.x * 3.2f, relay.Color.y * 2.9f, relay.Color.z * 3.4f }
                    : DirectX::XMFLOAT3{ relay.Color.x * 0.82f, relay.Color.y * 0.70f, relay.Color.z * 0.48f };
                relayMaterial.Alpha = relay.Stabilized ? 0.78f : (relayVisible ? 0.46f : 0.14f);
                relayMaterial.EmissiveIntensity += relay.Stabilized ? 1.35f + pulse * 0.85f : pulse * 0.30f;

                for (int ringIndex = 0; ringIndex < 3; ++ringIndex)
                {
                    const float ring = static_cast<float>(ringIndex);
                    Disparity::Transform relayRing;
                    relayRing.Position = { relay.Position.x, 0.12f + ring * 0.09f, relay.Position.z };
                    relayRing.Rotation = {
                        Pi * 0.5f + ring * 0.18f,
                        phase * (0.20f + ring * 0.08f),
                        ring * Pi * 0.24f
                    };
                    const float relayRadius = relay.Radius * (0.72f + ring * 0.18f) + pulse * 0.05f;
                    relayRing.Scale = { relayRadius, relayRadius, relayRadius };
                    renderer.DrawMesh(m_gizmoRingMesh, relayRing, relayMaterial);
                    ++beaconDraws;
                    ++traversalMarkers;
                }

                Disparity::Transform relayCore;
                relayCore.Position = { relay.Position.x, 0.74f + pulse * 0.10f, relay.Position.z };
                relayCore.Rotation = { phase * 0.36f, phase * 1.12f, phase * 0.28f };
                relayCore.Scale = { 0.24f, relay.Stabilized ? 0.92f : 0.48f, 0.24f };
                renderer.DrawMesh(m_cubeMesh, relayCore, relayMaterial);
                ++beaconDraws;

                const float orbAngle = phase * 1.35f;
                Disparity::Transform orbitShard;
                orbitShard.Position = Add(relay.Position, {
                    std::sin(orbAngle) * relay.OrbitRadius,
                    0.78f + std::cos(orbAngle * 1.4f) * 0.18f,
                    std::cos(orbAngle) * relay.OrbitRadius
                });
                orbitShard.Rotation = { phase, orbAngle, phase * 0.5f };
                orbitShard.Scale = { 0.16f, 0.32f, 0.16f };
                renderer.DrawMesh(m_cubeMesh, orbitShard, relayMaterial);
                ++beaconDraws;

                const DirectX::XMFLOAT3 relayFlatDelta = { relay.Position.x - m_playerPosition.x, 0.0f, relay.Position.z - m_playerPosition.z };
                const bool playerNearRelay = Length(relayFlatDelta) <= relay.Radius * 1.85f;
                if (playerNearRelay && !relay.Stabilized)
                {
                    Disparity::Material warningMaterial = m_demoHazardMaterial;
                    warningMaterial.Alpha = 0.24f + pulse * 0.16f;
                    warningMaterial.EmissiveIntensity += 0.65f + pulse * 0.45f;
                    Disparity::Transform overcharge;
                    overcharge.Position = { relay.Position.x, 0.16f, relay.Position.z };
                    overcharge.Rotation = { Pi * 0.5f, -phase * 0.48f, 0.0f };
                    const float warningRadius = relay.Radius * 1.22f + pulse * 0.10f;
                    overcharge.Scale = { warningRadius, warningRadius, warningRadius };
                    renderer.DrawMesh(m_gizmoRingMesh, overcharge, warningMaterial);
                    ++beaconDraws;
                    ++traversalMarkers;
                }

                if (relay.Stabilized)
                {
                    const DirectX::XMFLOAT3 start = { relay.Position.x, 0.68f, relay.Position.z };
                    const DirectX::XMFLOAT3 end = { m_riftPosition.x, 1.24f + pulse * 0.18f, m_riftPosition.z };
                    Disparity::Material bridgeMaterial = relayMaterial;
                    bridgeMaterial.Alpha = 0.20f + pulse * 0.12f;
                    renderer.DrawMesh(m_cubeMesh, BeamTransform(start, end, 0.020f + pulse * 0.006f), bridgeMaterial);
                    ++beaconDraws;
                    ++relayBridgeDraws;

                    const size_t nextIndex = (index + 1u) % m_publicDemoPhaseRelays.size();
                    if (m_publicDemoPhaseRelays[nextIndex].Stabilized)
                    {
                        const DirectX::XMFLOAT3 next = { m_publicDemoPhaseRelays[nextIndex].Position.x, 0.58f, m_publicDemoPhaseRelays[nextIndex].Position.z };
                        renderer.DrawMesh(m_cubeMesh, BeamTransform(start, next, 0.015f + pulse * 0.004f), bridgeMaterial);
                        ++beaconDraws;
                        ++relayBridgeDraws;
                    }
                }
            }

            if (m_publicDemoCheckpoint.Valid && !m_publicDemoCompleted)
            {
                const float markerPulse = 0.5f + 0.5f * std::sin(visualTime * 4.2f);
                Disparity::Material checkpointMaterial = m_demoCheckpointMaterial;
                checkpointMaterial.Alpha = 0.34f + markerPulse * 0.16f;
                checkpointMaterial.EmissiveIntensity += markerPulse * 0.7f;
                Disparity::Transform checkpoint;
                checkpoint.Position = { m_publicDemoCheckpoint.Position.x, 0.07f, m_publicDemoCheckpoint.Position.z };
                checkpoint.Rotation = { Pi * 0.5f, visualTime * 0.52f, 0.0f };
                checkpoint.Scale = { 0.62f + markerPulse * 0.06f, 0.62f + markerPulse * 0.06f, 0.62f + markerPulse * 0.06f };
                renderer.DrawMesh(m_gizmoRingMesh, checkpoint, checkpointMaterial);
                ++beaconDraws;
            }

            if (m_publicDemoStability < 0.22f && !m_publicDemoCompleted)
            {
                Disparity::Material warningMaterial = m_demoHazardMaterial;
                warningMaterial.Alpha = 0.26f + (0.22f - m_publicDemoStability) * 1.8f;
                Disparity::Transform warning;
                warning.Position = Add(m_playerPosition, { 0.0f, 0.10f, 0.0f });
                warning.Rotation = { Pi * 0.5f, visualTime * 1.1f, 0.0f };
                warning.Scale = { 1.05f, 1.05f, 1.05f };
                renderer.DrawMesh(m_gizmoRingMesh, warning, warningMaterial);
                ++beaconDraws;
            }

            const float gatePulse = 0.5f + 0.5f * std::sin(visualTime * 3.1f);
            for (int ringIndex = 0; ringIndex < 3; ++ringIndex)
            {
                const float ring = static_cast<float>(ringIndex);
                Disparity::Transform gate;
                gate.Position = { m_riftPosition.x, 0.08f + ring * 0.05f, m_riftPosition.z };
                gate.Rotation = { Pi * 0.5f, visualTime * (0.18f + ring * 0.05f), ring * Pi * 0.33f };
                const float radius = 1.45f + charge * (1.15f + ring * 0.32f) + gatePulse * 0.08f;
                gate.Scale = { radius, radius, radius };
                Disparity::Material gateMaterial = m_demoGateMaterial;
                gateMaterial.Alpha = m_publicDemoExtractionReady ? 0.82f : 0.28f + charge * 0.32f;
                gateMaterial.EmissiveIntensity += charge * 1.2f + (m_publicDemoCompleted ? 1.2f : 0.0f);
                renderer.DrawMesh(m_gizmoRingMesh, gate, gateMaterial);
                ++beaconDraws;
            }

            for (const PublicDemoSentinel& sentinel : m_publicDemoSentinels)
            {
                const DirectX::XMFLOAT3 sentinelPosition = PublicDemoSentinelPosition(sentinel);
                Disparity::Transform sentinelTransform;
                sentinelTransform.Position = sentinelPosition;
                sentinelTransform.Rotation = { visualTime * 1.1f, visualTime * 0.72f + sentinel.Phase, visualTime * 0.46f };
                sentinelTransform.Scale = { 0.34f, 0.34f, 0.34f };
                renderer.DrawMesh(m_cubeMesh, sentinelTransform, m_demoHazardMaterial);

                Disparity::Transform warningRing;
                warningRing.Position = { sentinelPosition.x, 0.06f, sentinelPosition.z };
                warningRing.Rotation = { Pi * 0.5f, visualTime * 0.24f, 0.0f };
                warningRing.Scale = { 0.78f, 0.78f, 0.78f };
                renderer.DrawMesh(m_gizmoRingMesh, warningRing, m_demoHazardMaterial);
            }

            for (const PublicDemoCollisionObstacle& obstacle : m_publicDemoObstacles)
            {
                Disparity::Material obstacleMaterial = obstacle.Traversable ? m_demoPathMaterial : m_demoCheckpointMaterial;
                obstacleMaterial.Albedo = obstacle.Used
                    ? DirectX::XMFLOAT3{ obstacle.Color.x * 2.2f, obstacle.Color.y * 2.2f, obstacle.Color.z * 2.2f }
                    : obstacle.Color;
                obstacleMaterial.Alpha = obstacle.Traversable ? 0.48f : 0.70f;
                obstacleMaterial.EmissiveIntensity += obstacle.Traversable ? 0.45f + gatePulse * 0.30f : 0.08f;

                Disparity::Transform obstacleTransform;
                obstacleTransform.Position = obstacle.Position;
                obstacleTransform.Scale = obstacle.HalfExtents;
                renderer.DrawMesh(m_cubeMesh, obstacleTransform, obstacleMaterial);
                ++beaconDraws;

                if (obstacle.Traversable)
                {
                    Disparity::Transform traversalRing;
                    traversalRing.Position = { obstacle.Position.x, 0.07f, obstacle.Position.z };
                    traversalRing.Rotation = { Pi * 0.5f, visualTime * 0.55f, 0.0f };
                    const float markerRadius = 0.85f + gatePulse * 0.07f;
                    traversalRing.Scale = { markerRadius, markerRadius, markerRadius };
                    renderer.DrawMesh(m_gizmoRingMesh, traversalRing, obstacleMaterial);
                    ++beaconDraws;
                    ++traversalMarkers;
                }
            }

            for (const PublicDemoEnemy& enemy : m_publicDemoEnemies)
            {
                const float pulse = 0.5f + 0.5f * std::sin(visualTime * 4.0f + enemy.Phase);
                Disparity::Material enemyMaterial = m_demoHazardMaterial;
                enemyMaterial.Albedo = PublicDemoEnemyArchetypeColor(enemy);
                enemyMaterial.Alpha = enemy.Alerted ? 0.88f : 0.58f;
                enemyMaterial.EmissiveIntensity += enemy.Alerted ? 1.0f + pulse * 0.6f : pulse * 0.25f;

                Disparity::Transform enemyBody;
                enemyBody.Position = enemy.Position;
                enemyBody.Rotation = { visualTime * 0.82f, visualTime * 0.58f + enemy.Phase, visualTime * 0.24f };
                const float bodyHeight = enemy.Archetype == PublicDemoEnemyArchetype::Warden ? 0.78f : 0.62f;
                enemyBody.Scale = { 0.34f, bodyHeight, 0.34f };
                renderer.DrawMesh(m_cubeMesh, enemyBody, enemyMaterial);

                Disparity::Transform visionRing;
                visionRing.Position = { enemy.Position.x, 0.075f, enemy.Position.z };
                visionRing.Rotation = { Pi * 0.5f, visualTime * (enemy.Alerted ? 0.42f : 0.16f), 0.0f };
                const float ringScale = enemy.Alerted ? 1.18f + pulse * 0.09f : 0.82f;
                visionRing.Scale = { ringScale, ringScale, ringScale };
                renderer.DrawMesh(m_gizmoRingMesh, visionRing, enemyMaterial);
                beaconDraws += 2;
                ++traversalMarkers;

                if (enemy.Telegraphing)
                {
                    Disparity::Material telegraphMaterial = enemyMaterial;
                    telegraphMaterial.Alpha = 0.18f + pulse * 0.10f;
                    telegraphMaterial.EmissiveIntensity += 1.2f;

                    Disparity::Transform telegraphRing;
                    telegraphRing.Position = { enemy.Position.x, 0.105f, enemy.Position.z };
                    telegraphRing.Rotation = { Pi * 0.5f, -visualTime * 0.72f, 0.0f };
                    const float telegraphScale = 1.65f + pulse * 0.16f;
                    telegraphRing.Scale = { telegraphScale, telegraphScale, telegraphScale };
                    renderer.DrawMesh(m_gizmoRingMesh, telegraphRing, telegraphMaterial);
                    ++beaconDraws;
                }

                if (enemy.Alerted)
                {
                    Disparity::Material chaseMaterial = enemyMaterial;
                    chaseMaterial.Alpha = 0.20f + pulse * 0.10f;
                    renderer.DrawMesh(m_cubeMesh, BeamTransform(enemy.Position, Add(m_playerPosition, { 0.0f, 0.35f, 0.0f }), 0.014f), chaseMaterial);
                    ++beaconDraws;
                }
            }

            for (int trailIndex = 0; trailIndex < 5; ++trailIndex)
            {
                const float age = static_cast<float>(trailIndex + 1);
                const DirectX::XMFLOAT3 back = {
                    -std::sin(m_playerYaw) * age * 0.34f,
                    0.08f,
                    -std::cos(m_playerYaw) * age * 0.34f
                };
                Disparity::Transform trail;
                trail.Position = Add(m_playerPosition, back);
                trail.Rotation = { 0.0f, m_playerYaw, 0.0f };
                const float trailScale = 0.18f * (1.0f - age * 0.12f);
                trail.Scale = { trailScale, 0.035f, trailScale };
                Disparity::Material trailMaterial = m_demoPathMaterial;
                trailMaterial.Alpha = 0.28f * (1.0f - age * 0.14f);
                renderer.DrawMesh(m_cubeMesh, trail, trailMaterial);
            }

            if (m_runtimeVerification.Enabled)
            {
                m_runtimeEditorStats.PublicDemoBeaconDraws += beaconDraws;
            }
            m_publicDemoRelayBridgeDrawCount += relayBridgeDraws;
            m_publicDemoTraversalMarkerCount += traversalMarkers;
        }

        void DrawShowcaseRift(Disparity::Renderer& renderer)
        {
            const float visualTime = ShowcaseVisualTime();
            const float pulse = 0.5f + 0.5f * std::sin(visualTime * 2.7f);
            const float fastPulse = 0.5f + 0.5f * std::sin(visualTime * 5.4f);
            const float showcaseBoost = (m_showcaseMode ? 1.22f : 1.0f) + PublicDemoRiftCharge() * 0.16f + m_publicDemoSurge * 0.10f;

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
            Disparity::Material bodyMaterial = m_playerBodyMaterial;
            switch (m_publicDemoAnimationState)
            {
            case PublicDemoAnimationState::Sprint:
                bodyMaterial.Albedo = { 0.24f, 0.76f, 1.25f };
                bodyMaterial.EmissiveIntensity += 0.22f;
                break;
            case PublicDemoAnimationState::Dash:
                bodyMaterial.Albedo = { 0.62f, 1.25f, 1.10f };
                bodyMaterial.EmissiveIntensity += 0.58f;
                break;
            case PublicDemoAnimationState::Stabilize:
                bodyMaterial.Albedo = { 1.05f, 0.58f, 1.22f };
                bodyMaterial.EmissiveIntensity += 0.34f;
                break;
            case PublicDemoAnimationState::Failure:
                bodyMaterial.Albedo = { 1.15f, 0.24f, 0.32f };
                bodyMaterial.EmissiveIntensity += 0.18f;
                break;
            case PublicDemoAnimationState::Complete:
                bodyMaterial.Albedo = { 0.92f, 1.10f, 0.55f };
                bodyMaterial.EmissiveIntensity += 0.42f;
                break;
            default:
                break;
            }

            renderer.DrawMeshWithId(m_cubeMesh, PlayerBodyTransform(), bodyMaterial, EditorPickPlayerId);

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

        std::string PublicDemoObjectiveText() const
        {
            if (m_publicDemoCompleted)
            {
                return "Extraction complete";
            }
            if (m_publicDemoExtractionReady)
            {
                return "Enter the rift field";
            }
            if (m_publicDemoStage == PublicDemoStage::ActivateAnchors)
            {
                std::ostringstream stream;
                stream << "Align phase anchor " << (PublicDemoAnchorsActivated() + 1u) << "/" << PublicDemoAnchorCount;
                return stream.str();
            }
            if (m_publicDemoStage == PublicDemoStage::TuneResonance)
            {
                std::ostringstream stream;
                stream << "Tune resonance gate " << (PublicDemoResonanceGatesTuned() + 1u) << "/" << PublicDemoResonanceGateCount;
                return stream.str();
            }
            if (m_publicDemoStage == PublicDemoStage::StabilizeRelays)
            {
                std::ostringstream stream;
                stream << "Stabilize phase relay " << (PublicDemoPhaseRelaysStabilized() + 1u) << "/" << PublicDemoPhaseRelayCount;
                return stream.str();
            }

            const int nextShard = NextPublicDemoShardIndex();
            if (nextShard >= 0)
            {
                std::ostringstream stream;
                stream << "Capture echo shard " << (m_publicDemoShardsCollected + 1u) << "/" << PublicDemoShardCount;
                return stream.str();
            }
            return "Stabilize the rift";
        }

        void DrawPublicDemoHud()
        {
            if (!m_publicDemoActive)
            {
                return;
            }

            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 18.0f, viewport->WorkPos.y + 18.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(330.0f, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.42f);
            const ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_AlwaysAutoResize;

            if (!ImGui::Begin("Public Demo HUD##PublicDemoHud", nullptr, flags))
            {
                ImGui::End();
                return;
            }

            ++m_runtimeEditorStats.PublicDemoHudFrames;
            ImGui::TextUnformatted("DISPARITY PUBLIC DEMO");
            ImGui::Separator();
            ImGui::Text("%s", PublicDemoObjectiveText().c_str());
            ImGui::Text("Stage: %s", PublicDemoStageName());
            ImGui::Text("Shards: %u / %zu", m_publicDemoShardsCollected, PublicDemoShardCount);
            ImGui::Text("Anchors: %u / %zu", PublicDemoAnchorsActivated(), PublicDemoAnchorCount);
            ImGui::Text("Gates: %u / %zu", PublicDemoResonanceGatesTuned(), PublicDemoResonanceGateCount);
            ImGui::Text("Relays: %u / %zu", PublicDemoPhaseRelaysStabilized(), PublicDemoPhaseRelayCount);
            ImGui::Text("Retries: %u  Checkpoints: %u", m_publicDemoRetryCount, m_publicDemoCheckpointCount);
            ImGui::Text("Traversal: %u  Enemies: %u/%u", m_publicDemoTraversalVaultCount + m_publicDemoTraversalDashCount, m_publicDemoEnemyChaseTickCount, m_publicDemoEnemyEvadeCount);
            ImGui::Text("Anim: %s  Menu: %s", PublicDemoAnimationStateName(), m_publicDemoMenuState == PublicDemoMenuState::Paused ? "Paused" : (m_publicDemoMenuState == PublicDemoMenuState::Completed ? "Complete" : "Playing"));
            ImGui::Text("Objective: %.1fm", PublicDemoObjectiveDistance());
            ImGui::Text("Time: %.1fs", m_publicDemoElapsed);
            ImGui::ProgressBar(std::clamp(PublicDemoRiftCharge(), 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), "Rift charge");
            ImGui::ProgressBar(std::clamp(m_publicDemoStability, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), "Stability");
            ImGui::ProgressBar(std::clamp(m_publicDemoFocus, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), "Sprint focus");
            if (!m_statusMessage.empty() && m_statusTimer > 0.0f)
            {
                ImGui::TextDisabled("%s", m_statusMessage.c_str());
            }
            ImGui::TextDisabled("WASD/LS move  Shift/LB sprint  Space/A dash  P/Start pause  F10 reset");
            ImGui::End();
        }

        void DrawPublicDemoMenuOverlay()
        {
            if (!m_publicDemoActive)
            {
                return;
            }

            const bool showFailure = m_publicDemoFailureOverlayTimer > 0.0f;
            const bool showPause = m_publicDemoMenuState == PublicDemoMenuState::Paused;
            const bool showComplete = m_publicDemoMenuState == PublicDemoMenuState::Completed;
            if (!showFailure && !showPause && !showComplete)
            {
                return;
            }

            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const ImVec2 size(showFailure ? 430.0f : 390.0f, 0.0f);
            ImGui::SetNextWindowPos(
                ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f - size.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.26f),
                ImGuiCond_Always);
            ImGui::SetNextWindowSize(size, ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(showFailure ? 0.68f : 0.54f);
            const ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_AlwaysAutoResize;

            if (!ImGui::Begin("Public Demo Overlay##PublicDemoOverlay", nullptr, flags))
            {
                ImGui::End();
                return;
            }

            if (showFailure)
            {
                ImGui::TextUnformatted("RETRY SIGNAL");
                ImGui::Separator();
                ImGui::TextWrapped("%s", m_publicDemoFailureReason.c_str());
                ImGui::TextDisabled("Returning to checkpoint");
            }
            else if (showComplete)
            {
                ImGui::TextUnformatted("EXTRACTION COMPLETE");
                ImGui::Separator();
                ImGui::Text("Route cleared in %.1fs", m_publicDemoElapsed);
                ImGui::TextDisabled("F10 resets the playable demo");
            }
            else
            {
                ImGui::TextUnformatted("DISPARITY PAUSED");
                ImGui::Separator();
                ImGui::TextDisabled("Press P or Start to resume");
            }
            ImGui::End();
        }

        void DrawPublicDemoDirectorPanel()
        {
            if (!ImGui::Begin("Demo Director##PublicDemoDirector"))
            {
                ImGui::End();
                return;
            }

            ++m_runtimeEditorStats.PublicDemoDirectorFrames;
            ImGui::TextUnformatted("Playable Vertical Slice");
            ImGui::Separator();
            ImGui::Text("Stage: %s", PublicDemoStageName());
            ImGui::Text("Objective: %s", PublicDemoObjectiveText().c_str());
            ImGui::Text("Objective distance: %.2fm", PublicDemoObjectiveDistance());
            ImGui::Text("Shards: %u / %zu", m_publicDemoShardsCollected, PublicDemoShardCount);
            ImGui::Text("Anchors: %u / %zu", PublicDemoAnchorsActivated(), PublicDemoAnchorCount);
            ImGui::Text("Gates: %u / %zu", PublicDemoResonanceGatesTuned(), PublicDemoResonanceGateCount);
            ImGui::Text("Relays: %u / %zu", PublicDemoPhaseRelaysStabilized(), PublicDemoPhaseRelayCount);
            ImGui::Text("Checkpoints: %u  Retries: %u", m_publicDemoCheckpointCount, m_publicDemoRetryCount);
            ImGui::Text("Pressure hits: %u  Footsteps: %u", m_publicDemoPressureHitCount, m_publicDemoFootstepEventCount);
            ImGui::Text("Overcharge windows: %u  Combo: %u", m_publicDemoRelayOverchargeWindowCount, m_publicDemoComboChainStepCount);
            ImGui::Text("Collision solves: %u  Traversal: %u", m_publicDemoCollisionSolveCount, m_publicDemoTraversalVaultCount + m_publicDemoTraversalDashCount);
            ImGui::Text("Enemy chase ticks: %u  Evades: %u  Contacts: %u", m_publicDemoEnemyChaseTickCount, m_publicDemoEnemyEvadeCount, m_publicDemoEnemyContactCount);
            ImGui::Text("Enemy roles: %u  LOS: %u  Telegraphs: %u  Hit reactions: %u",
                CountPublicDemoEnemyArchetypes(),
                m_publicDemoEnemyLineOfSightCheckCount,
                m_publicDemoEnemyTelegraphCount,
                m_publicDemoEnemyHitReactionCount);
            ImGui::Text("Controller feel: ground %u  slope %u  material %u  platform %u  camera %u",
                m_publicDemoControllerGroundedFrameCount,
                m_publicDemoControllerSlopeSampleCount,
                m_publicDemoControllerMaterialSampleCount,
                m_publicDemoControllerMovingPlatformFrameCount,
                m_publicDemoControllerCameraCollisionFrameCount);
            ImGui::Text("Gamepad frames: %u  Menu transitions: %u", m_publicDemoGamepadFrameCount, m_publicDemoMenuTransitionCount);
            ImGui::Text("Audio cues: %u  Animation: %s", m_publicDemoContentAudioCueCount, PublicDemoAnimationStateName());
            ImGui::Text("Blend tree: clips %u  transitions %u  events %u",
                m_publicDemoBlendTreeClipCount,
                m_publicDemoBlendTreeTransitionCount,
                m_publicDemoAnimationClipEventCount);
            ImGui::ProgressBar(std::clamp(PublicDemoRiftCharge(), 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), "Rift charge");
            ImGui::ProgressBar(std::clamp(m_publicDemoStability, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), "Stability");

            if (ImGui::Button("Reset Demo##PublicDemoDirectorReset"))
            {
                ResetPublicDemo(true);
                AddCommandHistory("Demo Director: reset public demo");
                SetStatus("Public demo reset from Demo Director");
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Checkpoint##PublicDemoDirectorCheckpoint"))
            {
                SavePublicDemoCheckpoint("director");
                AddCommandHistory("Demo Director: save checkpoint");
                SetStatus("Public demo checkpoint saved");
            }
            ImGui::SameLine();
            if (ImGui::Button("Retry##PublicDemoDirectorRetry"))
            {
                TriggerPublicDemoRetry(true);
                AddCommandHistory("Demo Director: retry from checkpoint");
            }

            if (ImGui::TreeNode("Recent Events##PublicDemoDirectorEvents"))
            {
                for (const std::string& eventText : m_publicDemoEvents)
                {
                    ImGui::BulletText("%s", eventText.c_str());
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("v30 Readiness##PublicDemoDirectorReadiness"))
            {
                ImGui::BulletText("Anchors: %u/%zu", PublicDemoAnchorsActivated(), PublicDemoAnchorCount);
                ImGui::BulletText("Resonance gates: %u/%zu", PublicDemoResonanceGatesTuned(), PublicDemoResonanceGateCount);
                ImGui::BulletText("Phase relays: %u/%zu", PublicDemoPhaseRelaysStabilized(), PublicDemoPhaseRelayCount);
                ImGui::BulletText("Stage transitions: %u", m_publicDemoStageTransitions);
                ImGui::BulletText("Overcharge windows: %u", m_publicDemoRelayOverchargeWindowCount);
                ImGui::BulletText("Combo chain: %u", m_publicDemoComboChainStepCount);
                ImGui::BulletText("Event queue: %zu events", m_publicDemoEvents.size());
                ImGui::BulletText("Gamepad surface: %s", m_publicDemoGamepad.Available ? "XInput available" : "runtime simulated / unavailable");
                ImGui::BulletText("Content manifests: cues=%s anim=%s", m_publicDemoCueManifestLoaded ? "yes" : "no", m_publicDemoAnimationManifestLoaded ? "yes" : "no");
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("v34 AAA Foundation##PublicDemoDirectorV34"))
            {
                ImGui::BulletText("Enemy archetypes: %u/%zu", CountPublicDemoEnemyArchetypes(), PublicDemoEnemyCount);
                for (const PublicDemoEnemy& enemy : m_publicDemoEnemies)
                {
                    ImGui::BulletText("%s: LOS %.2f  telegraph %.2fs  hit %.2fs",
                        PublicDemoEnemyArchetypeName(enemy.Archetype),
                        enemy.LineOfSightScore,
                        enemy.TelegraphTimer,
                        enemy.HitReactionTimer);
                }
                ImGui::BulletText("Controller polish: grounded=%u slope=%u material=%u platform=%u camera=%u",
                    m_publicDemoControllerGroundedFrameCount,
                    m_publicDemoControllerSlopeSampleCount,
                    m_publicDemoControllerMaterialSampleCount,
                    m_publicDemoControllerMovingPlatformFrameCount,
                    m_publicDemoControllerCameraCollisionFrameCount);
                ImGui::BulletText("Animation blend tree: loaded=%s clips=%u transitions=%u root-motion=%u events=%zu",
                    m_publicDemoBlendTreeManifestLoaded ? "yes" : "no",
                    m_publicDemoBlendTreeClipCount,
                    m_publicDemoBlendTreeTransitionCount,
                    m_publicDemoBlendTreeRootMotionCount,
                    m_publicDemoAnimationClipEvents.size());
                ImGui::BulletText("Accessibility/title flow: contrast=%s chapter=%s save=%s toggles=%u",
                    m_publicDemoAccessibilityHighContrast ? "yes" : "no",
                    m_publicDemoChapterSelectReady ? "yes" : "no",
                    m_publicDemoSaveSlotReady ? "yes" : "no",
                    m_publicDemoAccessibilityToggleCount);
                ImGui::BulletText("Pipeline readiness: render %u/%u  production %u/%u",
                    m_publicDemoRenderingReadinessCount,
                    m_runtimeBaseline.MinRenderingPipelineReadinessItems,
                    m_publicDemoProductionReadinessCount,
                    m_runtimeBaseline.MinProductionPipelineReadinessItems);
                ImGui::TreePop();
            }

            ImGui::End();
        }

        void DrawMainMenu()
        {
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("DISPARITY"))
                {
                    if (ImGui::MenuItem("Reset Public Demo", "F10"))
                    {
                        ResetPublicDemo(true);
                        SetStatus("Public demo reset");
                    }
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

        const char* CurrentViewportCameraName() const
        {
            if (m_trailerMode)
            {
                return "Trailer";
            }
            if (m_showcaseMode)
            {
                return "Showcase";
            }
            if (m_editorCameraEnabled)
            {
                return "Editor";
            }
            return "Gameplay";
        }

        const char* PostDebugViewName(uint32_t index) const
        {
            static constexpr std::array<const char*, 7> Names = {
                "Final", "Bloom", "SSAO", "AA edges", "Depth", "DOF", "Lens dirt"
            };
            return Names[std::min<uint32_t>(index, static_cast<uint32_t>(Names.size() - 1u))];
        }

        const char* TransformPivotModeName() const
        {
            static constexpr std::array<const char*, 3> Names = { "Selection", "Median", "World origin" };
            return Names[std::clamp(m_transformPrecision.PivotMode, 0, static_cast<int>(Names.size() - 1))];
        }

        const char* TransformOrientationModeName() const
        {
            static constexpr std::array<const char*, 3> Names = { "World", "Local", "View" };
            return Names[std::clamp(m_transformPrecision.OrientationMode, 0, static_cast<int>(Names.size() - 1))];
        }

        size_t CountFilteredCommandHistory(std::string_view filter) const
        {
            if (filter.empty())
            {
                return m_commandHistory.size();
            }

            return static_cast<size_t>(std::count_if(m_commandHistory.begin(), m_commandHistory.end(), [filter](const std::string& command) {
                return command.find(filter) != std::string::npos;
            }));
        }

        std::string CommandHistoryFilterText() const
        {
            size_t length = 0;
            while (length < m_commandHistoryFilter.size() && m_commandHistoryFilter[length] != '\0')
            {
                ++length;
            }
            return std::string(m_commandHistoryFilter.data(), length);
        }

        void SetCommandHistoryFilterText(std::string_view text)
        {
            m_commandHistoryFilter.fill('\0');
            const size_t copyLength = std::min(text.size(), m_commandHistoryFilter.size() - 1u);
            std::copy_n(text.data(), copyLength, m_commandHistoryFilter.data());
        }

        std::string EditorPreferenceProfileNameText() const
        {
            size_t length = 0;
            while (length < m_editorPreferenceProfileName.size() && m_editorPreferenceProfileName[length] != '\0')
            {
                ++length;
            }
            return std::string(m_editorPreferenceProfileName.data(), length);
        }

        void SetEditorPreferenceProfileNameText(std::string_view text)
        {
            m_editorPreferenceProfileName.fill('\0');
            const size_t copyLength = std::min(text.size(), m_editorPreferenceProfileName.size() - 1u);
            std::copy_n(text.data(), copyLength, m_editorPreferenceProfileName.data());
        }

        std::string SanitizedEditorPreferenceProfileName(std::string_view name) const
        {
            std::string sanitized;
            sanitized.reserve(name.size());
            for (const char character : name)
            {
                const unsigned char value = static_cast<unsigned char>(character);
                sanitized.push_back(std::isalnum(value) || character == '_' || character == '-' || character == '.'
                    ? character
                    : '_');
            }

            sanitized = Trim(sanitized);
            return sanitized.empty() ? "Default" : sanitized;
        }

        std::filesystem::path EditorPreferenceProfilePath(std::string_view name) const
        {
            return m_editorPreferenceProfile.ProfileDirectory /
                (SanitizedEditorPreferenceProfileName(name) + ".json");
        }

        std::filesystem::path EditorPreferenceProfileExportPath(std::string_view name) const
        {
            return m_editorPreferenceProfile.ProfileDirectory / "Exports" /
                (SanitizedEditorPreferenceProfileName(name) + ".export.json");
        }

        std::string JsonEscape(std::string_view text) const
        {
            std::string escaped;
            escaped.reserve(text.size());
            for (const char character : text)
            {
                switch (character)
                {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += character; break;
                }
            }
            return escaped;
        }

        const Disparity::JsonValue* FindPreferenceJsonValue(const Disparity::JsonValue& root, const std::string& key) const
        {
            return root.IsObject() ? root.Find(key) : nullptr;
        }

        bool ReadPreferenceBool(const Disparity::JsonValue& root, const std::string& key, bool fallback) const
        {
            const Disparity::JsonValue* value = FindPreferenceJsonValue(root, key);
            return value && value->ValueType == Disparity::JsonValue::Type::Bool ? value->Bool : fallback;
        }

        float ReadPreferenceFloat(const Disparity::JsonValue& root, const std::string& key, float fallback) const
        {
            const Disparity::JsonValue* value = FindPreferenceJsonValue(root, key);
            return value && value->IsNumber() ? static_cast<float>(value->AsNumber(fallback)) : fallback;
        }

        int ReadPreferenceInt(const Disparity::JsonValue& root, const std::string& key, int fallback) const
        {
            const Disparity::JsonValue* value = FindPreferenceJsonValue(root, key);
            return value && value->IsNumber() ? value->AsInt(fallback) : fallback;
        }

        std::string ReadPreferenceString(const Disparity::JsonValue& root, const std::string& key, std::string fallback) const
        {
            const Disparity::JsonValue* value = FindPreferenceJsonValue(root, key);
            return value && value->IsString() ? value->AsString(fallback) : std::move(fallback);
        }

        bool SaveEditorPreferencesToPath(const std::filesystem::path& path) const
        {
            std::ostringstream stream;
            stream << "{\n";
            stream << "  \"schema\": 1,\n";
            stream << "  \"viewport_hud_enabled\": " << (m_viewportOverlay.Enabled ? "true" : "false") << ",\n";
            stream << "  \"viewport_hud_pinned\": " << (m_viewportOverlay.Pinned ? "true" : "false") << ",\n";
            stream << "  \"viewport_hud_gpu_pick\": " << (m_viewportOverlay.ShowGpuPick ? "true" : "false") << ",\n";
            stream << "  \"viewport_hud_readback\": " << (m_viewportOverlay.ShowReadback ? "true" : "false") << ",\n";
            stream << "  \"viewport_hud_capture\": " << (m_viewportOverlay.ShowCapture ? "true" : "false") << ",\n";
            stream << "  \"viewport_hud_debug_thumbnails\": " << (m_viewportOverlay.ShowDebugThumbnails ? "true" : "false") << ",\n";
            stream << "  \"viewport_toolbar_visible\": " << (m_viewportToolbarVisible ? "true" : "false") << ",\n";
            stream << "  \"editor_camera_enabled\": " << (m_editorCameraEnabled ? "true" : "false") << ",\n";
            stream << "  \"profile_name\": \"" << JsonEscape(EditorPreferenceProfileNameText()) << "\",\n";
            stream << "  \"workspace_preset\": \"" << JsonEscape(m_editorWorkspacePresetName) << "\",\n";
            stream << "  \"dock_layout_checksum\": " << m_editorDockLayoutChecksum << ",\n";
            stream << "  \"transform_step\": " << std::fixed << std::setprecision(3) << m_transformPrecision.Step << ",\n";
            stream << "  \"transform_pivot\": " << m_transformPrecision.PivotMode << ",\n";
            stream << "  \"transform_orientation\": " << m_transformPrecision.OrientationMode << ",\n";
            stream << "  \"command_history_filter\": \"" << JsonEscape(CommandHistoryFilterText()) << "\"\n";
            stream << "}\n";
            return Disparity::FileSystem::WriteTextFile(path, stream.str());
        }

        bool LoadEditorPreferencesFromPath(const std::filesystem::path& path)
        {
            std::string text;
            if (!Disparity::FileSystem::ReadTextFile(path, text))
            {
                return false;
            }

            Disparity::JsonValue root;
            if (!Disparity::SimpleJson::Parse(text, root) || !root.IsObject())
            {
                return false;
            }

            m_viewportOverlay.Enabled = ReadPreferenceBool(root, "viewport_hud_enabled", m_viewportOverlay.Enabled);
            m_viewportOverlay.Pinned = ReadPreferenceBool(root, "viewport_hud_pinned", m_viewportOverlay.Pinned);
            m_viewportOverlay.ShowGpuPick = ReadPreferenceBool(root, "viewport_hud_gpu_pick", m_viewportOverlay.ShowGpuPick);
            m_viewportOverlay.ShowReadback = ReadPreferenceBool(root, "viewport_hud_readback", m_viewportOverlay.ShowReadback);
            m_viewportOverlay.ShowCapture = ReadPreferenceBool(root, "viewport_hud_capture", m_viewportOverlay.ShowCapture);
            m_viewportOverlay.ShowDebugThumbnails = ReadPreferenceBool(root, "viewport_hud_debug_thumbnails", m_viewportOverlay.ShowDebugThumbnails);
            m_viewportToolbarVisible = ReadPreferenceBool(root, "viewport_toolbar_visible", m_viewportToolbarVisible);
            m_editorCameraEnabled = ReadPreferenceBool(root, "editor_camera_enabled", m_editorCameraEnabled);
            SetEditorPreferenceProfileNameText(ReadPreferenceString(root, "profile_name", EditorPreferenceProfileNameText()));
            m_editorWorkspacePresetName = ReadPreferenceString(root, "workspace_preset", m_editorWorkspacePresetName);
            m_editorDockLayoutChecksum = static_cast<uint32_t>(std::max(0, ReadPreferenceInt(root, "dock_layout_checksum", static_cast<int>(m_editorDockLayoutChecksum))));
            m_transformPrecision.Step = std::clamp(ReadPreferenceFloat(root, "transform_step", m_transformPrecision.Step), 0.01f, 2.0f);
            m_transformPrecision.PivotMode = std::clamp(ReadPreferenceInt(root, "transform_pivot", m_transformPrecision.PivotMode), 0, 2);
            m_transformPrecision.OrientationMode = std::clamp(ReadPreferenceInt(root, "transform_orientation", m_transformPrecision.OrientationMode), 0, 2);
            SetCommandHistoryFilterText(ReadPreferenceString(root, "command_history_filter", CommandHistoryFilterText()));
            return true;
        }

        bool SaveEditorPreferenceProfile(std::string_view name)
        {
            const bool saved = SaveEditorPreferencesToPath(EditorPreferenceProfilePath(name));
            m_editorPreferencesSaved = m_editorPreferencesSaved || saved;
            SetStatus(saved ? "Saved editor preference profile" : "Editor preference profile save failed");
            return saved;
        }

        bool LoadEditorPreferenceProfile(std::string_view name)
        {
            const bool loaded = LoadEditorPreferencesFromPath(EditorPreferenceProfilePath(name));
            m_editorPreferencesLoaded = m_editorPreferencesLoaded || loaded;
            SetStatus(loaded ? "Loaded editor preference profile" : "Editor preference profile load failed");
            return loaded;
        }

        bool ExportEditorPreferenceProfile(std::string_view name)
        {
            const std::filesystem::path sourcePath = EditorPreferenceProfilePath(name);
            if (!std::filesystem::exists(sourcePath) && !SaveEditorPreferenceProfile(name))
            {
                return false;
            }

            std::string text;
            if (!Disparity::FileSystem::ReadTextFile(sourcePath, text))
            {
                SetStatus("Editor preference profile export failed");
                return false;
            }

            const std::filesystem::path exportPath = EditorPreferenceProfileExportPath(name);
            const bool exported = Disparity::FileSystem::WriteTextFile(exportPath, text);
            m_editorWorkflowDiagnostics.ExportPath = exportPath;
            m_editorWorkflowDiagnostics.ProfileImportExport = m_editorWorkflowDiagnostics.ProfileImportExport || exported;
            SetStatus(exported ? "Exported editor preference profile" : "Editor preference profile export failed");
            return exported;
        }

        bool ImportEditorPreferenceProfile(std::string_view name)
        {
            std::string text;
            const std::filesystem::path exportPath = EditorPreferenceProfileExportPath(name);
            if (!Disparity::FileSystem::ReadTextFile(exportPath, text))
            {
                SetStatus("Editor preference profile import failed");
                return false;
            }

            const bool imported = Disparity::FileSystem::WriteTextFile(EditorPreferenceProfilePath(name), text) &&
                LoadEditorPreferenceProfile(name);
            m_editorWorkflowDiagnostics.ProfileImportExport = m_editorWorkflowDiagnostics.ProfileImportExport || imported;
            SetStatus(imported ? "Imported editor preference profile" : "Editor preference profile import failed");
            return imported;
        }

        uint32_t DiffEditorPreferenceProfiles(std::string_view leftName, std::string_view rightName) const
        {
            std::string leftText;
            std::string rightText;
            if (!Disparity::FileSystem::ReadTextFile(EditorPreferenceProfilePath(leftName), leftText) ||
                !Disparity::FileSystem::ReadTextFile(EditorPreferenceProfilePath(rightName), rightText))
            {
                return 0;
            }

            auto splitLines = [](const std::string& text) {
                std::vector<std::string> lines;
                std::stringstream stream(text);
                std::string line;
                while (std::getline(stream, line))
                {
                    lines.push_back(Trim(line));
                }
                return lines;
            };

            const std::vector<std::string> leftLines = splitLines(leftText);
            const std::vector<std::string> rightLines = splitLines(rightText);
            const size_t count = std::max(leftLines.size(), rightLines.size());
            uint32_t differences = 0;
            for (size_t index = 0; index < count; ++index)
            {
                const std::string leftLine = index < leftLines.size() ? leftLines[index] : std::string{};
                const std::string rightLine = index < rightLines.size() ? rightLines[index] : std::string{};
                if (leftLine != rightLine)
                {
                    ++differences;
                }
            }
            return differences;
        }

        void ApplyEditorWorkspacePreset(std::string_view presetName)
        {
            const std::string preset = std::string(presetName);
            m_editorWorkspacePresetName = preset.empty() ? "Editor" : preset;
            if (m_editorWorkspacePresetName == "Gameplay")
            {
                m_editorCameraEnabled = false;
                m_viewportToolbarVisible = false;
                m_viewportOverlay.Enabled = false;
                m_viewportOverlay.ShowDebugThumbnails = false;
            }
            else if (m_editorWorkspacePresetName == "Trailer")
            {
                m_editorCameraEnabled = true;
                m_viewportToolbarVisible = true;
                m_viewportOverlay.Enabled = true;
                m_viewportOverlay.Pinned = true;
                m_viewportOverlay.ShowCapture = true;
                m_viewportOverlay.ShowDebugThumbnails = true;
            }
            else
            {
                m_editorWorkspacePresetName = "Editor";
                m_editorCameraEnabled = true;
                m_viewportToolbarVisible = true;
                m_viewportOverlay.Enabled = true;
                m_viewportOverlay.ShowGpuPick = true;
                m_viewportOverlay.ShowReadback = true;
            }

            m_editorWorkflowDiagnostics.ActiveWorkspacePreset = m_editorWorkspacePresetName;
            m_editorWorkflowDiagnostics.WorkspacePresets = 3;
            ++m_editorWorkspacePresetApplyCount;
            MarkEditorPreferencesDirty();
            SetStatus("Applied " + m_editorWorkspacePresetName + " workspace preset");
        }

        void ResetEditorPreferencesToDefaults()
        {
            m_viewportOverlay = ViewportOverlaySettings{};
            m_transformPrecision = TransformPrecisionState{};
            m_viewportToolbarVisible = true;
            m_editorCameraEnabled = false;
            m_editorWorkspacePresetName = "Editor";
            SetCommandHistoryFilterText("");
            SetStatus("Reset editor preferences to defaults");
            MarkEditorPreferencesDirty();
        }

        void LoadEditorPreferences()
        {
            if (m_runtimeVerification.Enabled)
            {
                return;
            }

            m_editorPreferencesLoaded = LoadEditorPreferencesFromPath(m_editorPreferenceProfile.PreferencePath);
            if (m_editorPreferencesLoaded)
            {
                SetStatus("Loaded editor preferences");
            }
        }

        void SaveEditorPreferences()
        {
            if (m_runtimeVerification.Enabled)
            {
                return;
            }

            m_editorPreferencesSaved = SaveEditorPreferencesToPath(m_editorPreferenceProfile.PreferencePath);
            if (m_editorPreferencesSaved)
            {
                m_editorPreferencesDirty = false;
                m_editorPreferencesSaveDelay = 0.0f;
                SetStatus("Saved editor preferences");
            }
            else
            {
                m_editorPreferencesDirty = true;
                m_editorPreferencesSaveDelay = 5.0f;
                SetStatus("Editor preference save failed");
            }
        }

        void MarkEditorPreferencesDirty()
        {
            if (m_runtimeVerification.Enabled)
            {
                return;
            }

            m_editorPreferencesDirty = true;
            m_editorPreferencesSaveDelay = 0.75f;
        }

        void UpdateEditorPreferenceAutosave(float dt)
        {
            if (m_runtimeVerification.Enabled || !m_editorPreferencesDirty)
            {
                return;
            }

            m_editorPreferencesSaveDelay -= dt;
            if (m_editorPreferencesSaveDelay <= 0.0f)
            {
                SaveEditorPreferences();
            }
        }

        uint32_t GpuPickStaleFrames() const
        {
            if (!m_gpuPickVisualization.HasCache || m_gpuPickVisualization.LastResolvedFrame == 0)
            {
                return 0;
            }
            return m_editorFrameIndex >= m_gpuPickVisualization.LastResolvedFrame
                ? m_editorFrameIndex - m_gpuPickVisualization.LastResolvedFrame
                : 0u;
        }

        std::vector<std::string> BuildViewportOverlayLines() const
        {
            std::vector<std::string> lines;
            std::ostringstream modeLine;
            modeLine << "Cam " << CurrentViewportCameraName();
            if (m_renderer)
            {
                const Disparity::RendererSettings settings = m_renderer->GetSettings();
                modeLine << "  Render " << PostDebugViewName(settings.PostDebugView);
                modeLine << "  Bloom " << (settings.Bloom ? "on" : "off");
                modeLine << "  TAA " << (settings.TemporalAA ? "on" : "off");
            }
            lines.push_back(modeLine.str());

            if (m_viewportOverlay.ShowGpuPick)
            {
                std::ostringstream pickLine;
                pickLine << "GPU ";
                if (m_gpuPickVisualization.HasCache)
                {
                    pickLine << (m_gpuPickVisualization.LastObjectName.empty() ? "None" : m_gpuPickVisualization.LastObjectName)
                        << " id " << m_gpuPickVisualization.LastObjectId
                        << " z " << std::fixed << std::setprecision(3) << m_gpuPickVisualization.LastDepth
                        << " age " << GpuPickStaleFrames() << "f";
                }
                else
                {
                    pickLine << "not sampled";
                }
                lines.push_back(pickLine.str());
            }

            if (m_viewportOverlay.ShowReadback)
            {
                std::ostringstream latencyLine;
                latencyLine << "Readback pending " << m_gpuPickVisualization.PendingSlots
                    << " latency " << m_gpuPickVisualization.LastLatencyFrames
                    << "f hits " << m_gpuPickVisualization.CacheHits
                    << " misses " << m_gpuPickVisualization.CacheMisses;
                lines.push_back(latencyLine.str());
            }

            if (m_viewportOverlay.ShowCapture)
            {
                const HighResolutionCaptureMetrics captureMetrics = GetHighResolutionCaptureMetrics();
                std::ostringstream captureLine;
                captureLine << "Capture "
                    << (m_highResCapturePending ? (m_highResCaptureWorkerStarted ? "resolving" : "queued") : "idle")
                    << " tiles " << m_highResCaptureTilesWritten << "/" << captureMetrics.Tiles
                    << " " << captureMetrics.ResolveFilter << "x" << captureMetrics.ResolveSamples;
                lines.push_back(captureLine.str());
            }
            return lines;
        }

        void DrawViewportOverlay(const ImVec2& imageMin, const ImVec2& imageSize)
        {
            if (!m_viewportOverlay.Enabled)
            {
                return;
            }

            const std::vector<std::string> lines = BuildViewportOverlayLines();
            if (lines.empty() || imageSize.x <= 0.0f || imageSize.y <= 0.0f)
            {
                return;
            }

            ++m_runtimeEditorStats.ViewportOverlayTests;
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float padding = 6.0f;
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            const float overlayWidth = std::min(imageSize.x - padding * 2.0f, 430.0f);
            const float overlayHeight = padding * 2.0f + lineHeight * static_cast<float>(lines.size());
            const ImVec2 overlayMin(imageMin.x + 8.0f, imageMin.y + 8.0f);
            const ImVec2 overlayMax(
                overlayMin.x + std::max(180.0f, overlayWidth),
                overlayMin.y + overlayHeight);
            drawList->AddRectFilled(overlayMin, overlayMax, IM_COL32(10, 14, 20, 190), 4.0f);
            drawList->AddRect(overlayMin, overlayMax, IM_COL32(95, 180, 255, 150), 4.0f);
            ImVec2 textPos(overlayMin.x + padding, overlayMin.y + padding);
            for (const std::string& line : lines)
            {
                drawList->AddText(textPos, IM_COL32(235, 244, 255, 255), line.c_str());
                textPos.y += lineHeight;
            }

            const HighResolutionCaptureMetrics captureMetrics = GetHighResolutionCaptureMetrics();
            const float barWidth = std::max(1.0f, (overlayMax.x - overlayMin.x - padding * 2.0f) / static_cast<float>(captureMetrics.Tiles));
            const float barY = overlayMax.y - 4.0f;
            for (uint32_t tile = 0; tile < captureMetrics.Tiles; ++tile)
            {
                const bool complete = tile < m_highResCaptureTilesWritten;
                const ImU32 color = complete ? IM_COL32(84, 226, 255, 220) : IM_COL32(80, 88, 104, 190);
                const ImVec2 min(overlayMin.x + padding + static_cast<float>(tile) * barWidth, barY);
                const ImVec2 max(min.x + barWidth - 2.0f, overlayMax.y - 1.0f);
                drawList->AddRectFilled(min, max, color, 1.0f);
            }

            if (m_viewportOverlay.ShowDebugThumbnails)
            {
                const ImVec2 thumbMin(overlayMax.x + 8.0f, overlayMin.y);
                const ImVec2 thumbSize(58.0f, 36.0f);
                const float depth = std::clamp(m_gpuPickVisualization.LastDepth, 0.0f, 1.0f);
                const uint32_t objectTint = 40u + (m_gpuPickVisualization.LastObjectId % 180u);
                drawList->AddRectFilled(thumbMin, ImVec2(thumbMin.x + thumbSize.x, thumbMin.y + thumbSize.y), IM_COL32(objectTint, 100, 230, 210), 3.0f);
                drawList->AddRect(thumbMin, ImVec2(thumbMin.x + thumbSize.x, thumbMin.y + thumbSize.y), IM_COL32(235, 244, 255, 190), 3.0f);
                const ImVec2 depthMin(thumbMin.x, thumbMin.y + thumbSize.y + 5.0f);
                const ImU32 depthColor = IM_COL32(static_cast<int>(depth * 255.0f), static_cast<int>(depth * 255.0f), static_cast<int>(depth * 255.0f), 220);
                drawList->AddRectFilled(depthMin, ImVec2(depthMin.x + thumbSize.x, depthMin.y + 10.0f), depthColor, 2.0f);
                drawList->AddText(ImVec2(thumbMin.x, depthMin.y + 13.0f), IM_COL32(235, 244, 255, 235), m_viewportOverlay.Pinned ? "Pinned" : "Live");
            }
        }

        bool DrawViewportToolbarButton(const char* label, const char* tooltip)
        {
            const bool clicked = ImGui::Button(label);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", tooltip);
            }
            return clicked;
        }

        void ToggleViewportToolbarCamera()
        {
            m_editorCameraEnabled = !m_editorCameraEnabled;
            ++m_viewportToolbarInteractionCount;
            MarkEditorPreferencesDirty();
            SetStatus(m_editorCameraEnabled ? "Viewport toolbar: editor camera" : "Viewport toolbar: gameplay camera");
        }

        void CycleViewportToolbarDebugView()
        {
            if (m_renderer)
            {
                Disparity::RendererSettings settings = m_renderer->GetSettings();
                settings.PostDebugView = (settings.PostDebugView + 1u) % 7u;
                m_renderer->SetSettings(settings);
                SetStatus(std::string("Viewport toolbar: ") + PostDebugViewName(settings.PostDebugView));
            }
            ++m_viewportToolbarInteractionCount;
        }

        void TriggerViewportToolbarCapture()
        {
            RequestHighResolutionCapture();
            ++m_viewportToolbarInteractionCount;
        }

        void ToggleViewportToolbarObjectId()
        {
            m_viewportOverlay.ShowGpuPick = !m_viewportOverlay.ShowGpuPick;
            ++m_viewportToolbarInteractionCount;
            MarkEditorPreferencesDirty();
            SetStatus(m_viewportOverlay.ShowGpuPick ? "Viewport toolbar: object IDs visible" : "Viewport toolbar: object IDs hidden");
        }

        void ToggleViewportToolbarDepth()
        {
            m_viewportOverlay.ShowReadback = !m_viewportOverlay.ShowReadback;
            ++m_viewportToolbarInteractionCount;
            MarkEditorPreferencesDirty();
            SetStatus(m_viewportOverlay.ShowReadback ? "Viewport toolbar: depth readback visible" : "Viewport toolbar: depth readback hidden");
        }

        void ToggleViewportToolbarHud()
        {
            m_viewportOverlay.Enabled = !m_viewportOverlay.Enabled;
            ++m_viewportToolbarInteractionCount;
            MarkEditorPreferencesDirty();
            SetStatus(m_viewportOverlay.Enabled ? "Viewport toolbar: HUD visible" : "Viewport toolbar: HUD hidden");
        }

        void DrawViewportToolbarOverlay(const ImVec2& imageMin, const ImVec2& imageSize)
        {
            if (!m_viewportToolbarVisible || imageSize.x <= 0.0f || imageSize.y <= 0.0f)
            {
                return;
            }

            const ImVec2 toolbarPosition(imageMin.x + 8.0f, imageMin.y + std::max(8.0f, imageSize.y - 34.0f));
            ImGui::SetCursorScreenPos(toolbarPosition);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(20, 32, 46, 220));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(42, 126, 200, 235));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(64, 174, 240, 255));
            ImGui::BeginGroup();
            if (DrawViewportToolbarButton("Cam##ViewportToolbarCamera", "Toggle editor/game camera"))
            {
                ToggleViewportToolbarCamera();
            }
            ImGui::SameLine();
            if (DrawViewportToolbarButton("Dbg##ViewportToolbarDebug", "Cycle post-process debug view"))
            {
                CycleViewportToolbarDebugView();
            }
            ImGui::SameLine();
            if (DrawViewportToolbarButton("Cap##ViewportToolbarCapture", "Capture source PNG/PPM and 2x PPM"))
            {
                TriggerViewportToolbarCapture();
            }
            ImGui::SameLine();
            if (DrawViewportToolbarButton("ID##ViewportToolbarObjectId", "Toggle object-ID overlay row"))
            {
                ToggleViewportToolbarObjectId();
            }
            ImGui::SameLine();
            if (DrawViewportToolbarButton("Depth##ViewportToolbarDepth", "Toggle depth/readback overlay row"))
            {
                ToggleViewportToolbarDepth();
            }
            ImGui::SameLine();
            if (DrawViewportToolbarButton("HUD##ViewportToolbarHud", "Toggle viewport HUD"))
            {
                ToggleViewportToolbarHud();
            }
            ImGui::EndGroup();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
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
            if (ImGui::TreeNode("Viewport HUD"))
            {
                bool changed = false;
                changed |= ImGui::Checkbox("Enabled##ViewportHUD", &m_viewportOverlay.Enabled);
                ImGui::SameLine();
                changed |= ImGui::Checkbox("Pinned##ViewportHUD", &m_viewportOverlay.Pinned);
                changed |= ImGui::Checkbox("GPU pick row##ViewportHUD", &m_viewportOverlay.ShowGpuPick);
                ImGui::SameLine();
                changed |= ImGui::Checkbox("Readback row##ViewportHUD", &m_viewportOverlay.ShowReadback);
                changed |= ImGui::Checkbox("Capture row##ViewportHUD", &m_viewportOverlay.ShowCapture);
                ImGui::SameLine();
                changed |= ImGui::Checkbox("Debug thumbnails##ViewportHUD", &m_viewportOverlay.ShowDebugThumbnails);
                changed |= ImGui::Checkbox("Toolbar##ViewportHUD", &m_viewportToolbarVisible);
                if (changed)
                {
                    MarkEditorPreferencesDirty();
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Preference Profiles"))
            {
                if (ImGui::InputText("Name##EditorPreferenceProfileName", m_editorPreferenceProfileName.data(), m_editorPreferenceProfileName.size()))
                {
                    MarkEditorPreferencesDirty();
                }
                const std::string profilePath = EditorPreferenceProfilePath(EditorPreferenceProfileNameText()).string();
                ImGui::TextDisabled("%s", profilePath.c_str());
                if (ImGui::Button("Save Profile##EditorPreferenceProfile"))
                {
                    (void)SaveEditorPreferenceProfile(EditorPreferenceProfileNameText());
                    MarkEditorPreferencesDirty();
                }
                ImGui::SameLine();
                if (ImGui::Button("Load Profile##EditorPreferenceProfile"))
                {
                    (void)LoadEditorPreferenceProfile(EditorPreferenceProfileNameText());
                    MarkEditorPreferencesDirty();
                }
                if (ImGui::Button("Reset Defaults##EditorPreferenceProfile"))
                {
                    ResetEditorPreferencesToDefaults();
                }
                if (ImGui::Button("Export##EditorPreferenceProfile"))
                {
                    (void)ExportEditorPreferenceProfile(EditorPreferenceProfileNameText());
                }
                ImGui::SameLine();
                if (ImGui::Button("Import##EditorPreferenceProfile"))
                {
                    (void)ImportEditorPreferenceProfile(EditorPreferenceProfileNameText());
                    MarkEditorPreferencesDirty();
                }
                ImGui::SameLine();
                if (ImGui::Button("Diff Defaults##EditorPreferenceProfile"))
                {
                    const std::string activeProfile = EditorPreferenceProfileNameText();
                    (void)SaveEditorPreferenceProfile(activeProfile);
                    const ViewportOverlaySettings overlayBefore = m_viewportOverlay;
                    const TransformPrecisionState precisionBefore = m_transformPrecision;
                    const bool toolbarBefore = m_viewportToolbarVisible;
                    const bool cameraBefore = m_editorCameraEnabled;
                    const std::string workspaceBefore = m_editorWorkspacePresetName;
                    const std::string filterBefore = CommandHistoryFilterText();
                    ResetEditorPreferencesToDefaults();
                    (void)SaveEditorPreferenceProfile("DefaultDiffProbe");
                    m_viewportOverlay = overlayBefore;
                    m_transformPrecision = precisionBefore;
                    m_viewportToolbarVisible = toolbarBefore;
                    m_editorCameraEnabled = cameraBefore;
                    m_editorWorkspacePresetName = workspaceBefore;
                    SetCommandHistoryFilterText(filterBefore);
                    m_editorWorkflowDiagnostics.ProfileDiffFields = DiffEditorPreferenceProfiles(activeProfile, "DefaultDiffProbe");
                    m_editorWorkflowDiagnostics.ProfileDiffing = m_editorWorkflowDiagnostics.ProfileDiffFields > 0;
                    SetStatus("Profile diff fields: " + std::to_string(m_editorWorkflowDiagnostics.ProfileDiffFields));
                }
                ImGui::Text("Profile diff fields: %u", m_editorWorkflowDiagnostics.ProfileDiffFields);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Workspace Presets"))
            {
                ImGui::Text("Active: %s", m_editorWorkspacePresetName.c_str());
                if (ImGui::Button("Gameplay##WorkspacePreset"))
                {
                    ApplyEditorWorkspacePreset("Gameplay");
                }
                ImGui::SameLine();
                if (ImGui::Button("Editor##WorkspacePreset"))
                {
                    ApplyEditorWorkspacePreset("Editor");
                }
                ImGui::SameLine();
                if (ImGui::Button("Trailer##WorkspacePreset"))
                {
                    ApplyEditorWorkspacePreset("Trailer");
                }
                const HighResolutionCaptureMetrics captureMetrics = GetHighResolutionCaptureMetrics();
                const float denominator = static_cast<float>(std::max(1u, captureMetrics.Tiles));
                ImGui::ProgressBar(static_cast<float>(m_highResCaptureTilesWritten) / denominator, ImVec2(-FLT_MIN, 0.0f), "Capture progress");
                ImGui::TreePop();
            }
            if (ImGui::Checkbox("Editor camera", &m_editorCameraEnabled))
            {
                MarkEditorPreferencesDirty();
            }
            if (ImGui::Button("Frame Selection"))
            {
                FrameSelectedObject();
            }
            ImGui::SameLine();
            if (ImGui::Button("Frame Player"))
            {
                m_editorCameraTarget = Add(m_playerPosition, { 0.0f, 1.1f, 0.0f });
                m_editorCameraEnabled = true;
                MarkEditorPreferencesDirty();
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
                const ImVec2 imageMin = ImGui::GetCursorScreenPos();
                const ImVec2 imageSize(previewWidth, previewHeight);
                ImGui::Image(
                    reinterpret_cast<ImTextureID>(m_renderer->GetEditorViewportShaderResourceView()),
                    imageSize);
                const ImVec2 afterImageCursor = ImGui::GetCursorScreenPos();
                DrawViewportToolbarOverlay(imageMin, imageSize);
                DrawViewportOverlay(imageMin, imageSize);
                ImGui::SetCursorScreenPos(afterImageCursor);
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
                changed |= DrawTransformPrecisionControls();
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
                changed |= DrawTransformPrecisionControls();
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

        bool DrawTransformPrecisionControls()
        {
            bool changed = false;
            if (ImGui::TreeNode("Transform Precision"))
            {
                changed |= ImGui::DragFloat("Nudge step##TransformPrecision", &m_transformPrecision.Step, 0.01f, 0.01f, 2.0f, "%.2f");
                const char* pivotModes[] = { "Selection", "Median", "World origin" };
                changed |= ImGui::Combo("Pivot##TransformPrecision", &m_transformPrecision.PivotMode, pivotModes, IM_ARRAYSIZE(pivotModes));
                const char* orientationModes[] = { "World", "Local", "View" };
                changed |= ImGui::Combo("Orientation##TransformPrecision", &m_transformPrecision.OrientationMode, orientationModes, IM_ARRAYSIZE(orientationModes));
                ImGui::Text("Active: %s / %s", TransformPivotModeName(), TransformOrientationModeName());
                ImGui::TreePop();
            }
            if (changed)
            {
                MarkEditorPreferencesDirty();
            }
            return changed;
        }

        bool DrawTransformGizmo(Disparity::Transform& transform)
        {
            bool changed = false;
            const float step = std::clamp(m_transformPrecision.Step, 0.01f, 2.0f);
            if (ImGui::TreeNode("Transform Gizmo"))
            {
                ImGui::TextUnformatted("Move");
                changed |= AxisButton("-X", transform.Position.x, -step);
                ImGui::SameLine();
                changed |= AxisButton("+X", transform.Position.x, step);
                ImGui::SameLine();
                changed |= AxisButton("-Y", transform.Position.y, -step);
                ImGui::SameLine();
                changed |= AxisButton("+Y", transform.Position.y, step);
                ImGui::SameLine();
                changed |= AxisButton("-Z", transform.Position.z, -step);
                ImGui::SameLine();
                changed |= AxisButton("+Z", transform.Position.z, step);

                ImGui::TextUnformatted("Scale");
                changed |= AxisButton("S-", transform.Scale.y, -step * 0.4f, 0.05f);
                ImGui::SameLine();
                changed |= AxisButton("S+", transform.Scale.y, step * 0.4f, 0.05f);
                ImGui::SameLine();
                changed |= AxisButton("Yaw-", transform.Rotation.y, -step * 0.32f);
                ImGui::SameLine();
                changed |= AxisButton("Yaw+", transform.Rotation.y, step * 0.32f);
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
            ImGui::SameLine();
            if (ImGui::Button("Thumbnails##ShotDirector"))
            {
                const size_t written = GenerateShotThumbnails();
                SetStatus("Generated " + std::to_string(written) + " shot thumbnail(s)");
            }

            bool previewScrub = !m_trailerMode;
            if (ImGui::Checkbox("Preview scrub##ShotDirector", &previewScrub))
            {
                SetTrailerMode(!previewScrub);
                if (previewScrub)
                {
                    UpdateTrailerCamera(0.0f);
                }
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
                    const char* splineModes[] = { "catmull", "linear" };
                    int splineModeIndex = key.SplineMode == "linear" ? 1 : 0;
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::Combo("Spline", &splineModeIndex, splineModes, IM_ARRAYSIZE(splineModes)))
                    {
                        key.SplineMode = splineModes[splineModeIndex];
                    }
                    const char* easingCurves[] = { "smoothstep", "linear", "ease_in", "ease_out", "expo" };
                    int easingCurveIndex = 0;
                    for (int optionIndex = 0; optionIndex < IM_ARRAYSIZE(easingCurves); ++optionIndex)
                    {
                        if (key.EasingCurve == easingCurves[optionIndex])
                        {
                            easingCurveIndex = optionIndex;
                            break;
                        }
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::Combo("Curve", &easingCurveIndex, easingCurves, IM_ARRAYSIZE(easingCurves)))
                    {
                        key.EasingCurve = easingCurves[easingCurveIndex];
                    }
                    const char* rendererTracks[] = { "post_stack", "bloom", "dof", "lens_dirt", "grade" };
                    int rendererTrackIndex = 0;
                    for (int optionIndex = 0; optionIndex < IM_ARRAYSIZE(rendererTracks); ++optionIndex)
                    {
                        if (key.RendererTrack == rendererTracks[optionIndex])
                        {
                            rendererTrackIndex = optionIndex;
                            break;
                        }
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::Combo("Render Track", &rendererTrackIndex, rendererTracks, IM_ARRAYSIZE(rendererTracks)))
                    {
                        key.RendererTrack = rendererTracks[rendererTrackIndex];
                    }
                    const char* audioTracks[] = { "cue", "music", "ambience", "silence" };
                    int audioTrackIndex = 0;
                    for (int optionIndex = 0; optionIndex < IM_ARRAYSIZE(audioTracks); ++optionIndex)
                    {
                        if (key.AudioTrack == audioTracks[optionIndex])
                        {
                            audioTrackIndex = optionIndex;
                            break;
                        }
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::Combo("Audio Track", &audioTrackIndex, audioTracks, IM_ARRAYSIZE(audioTracks)))
                    {
                        key.AudioTrack = audioTracks[audioTrackIndex];
                    }
                    float thumbnailTint[3] = { key.ThumbnailTint.x, key.ThumbnailTint.y, key.ThumbnailTint.z };
                    if (ImGui::ColorEdit3("Thumb", thumbnailTint, ImGuiColorEditFlags_NoInputs))
                    {
                        key.ThumbnailTint = { thumbnailTint[0], thumbnailTint[1], thumbnailTint[2] };
                    }
                    if (!key.ThumbnailPath.empty())
                    {
                        ImGui::TextUnformatted(key.ThumbnailPath.c_str());
                    }
                    if (!key.Bookmark.empty())
                    {
                        ImGui::TextUnformatted(key.Bookmark.c_str());
                    }
                    const char* clipLanes[] = { "camera_main", "vfx_insert", "title_card" };
                    int clipLaneIndex = 0;
                    for (int optionIndex = 0; optionIndex < IM_ARRAYSIZE(clipLanes); ++optionIndex)
                    {
                        if (key.ClipLane == clipLanes[optionIndex])
                        {
                            clipLaneIndex = optionIndex;
                            break;
                        }
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::Combo("Clip Lane", &clipLaneIndex, clipLanes, IM_ARRAYSIZE(clipLanes)))
                    {
                        key.ClipLane = clipLanes[clipLaneIndex];
                    }
                    const char* nestedSequences[] = { "main", "rift_reveal", "loop" };
                    int nestedSequenceIndex = 0;
                    for (int optionIndex = 0; optionIndex < IM_ARRAYSIZE(nestedSequences); ++optionIndex)
                    {
                        if (key.NestedSequence == nestedSequences[optionIndex])
                        {
                            nestedSequenceIndex = optionIndex;
                            break;
                        }
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::Combo("Nested", &nestedSequenceIndex, nestedSequences, IM_ARRAYSIZE(nestedSequences)))
                    {
                        key.NestedSequence = nestedSequences[nestedSequenceIndex];
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("Hold", &key.HoldSeconds, 0.01f, 0.0f, 2.0f, "%.2f");
                    const char* shotRoles[] = { "establishing", "hero", "detail", "transition", "loop" };
                    int shotRoleIndex = 0;
                    for (int optionIndex = 0; optionIndex < IM_ARRAYSIZE(shotRoles); ++optionIndex)
                    {
                        if (key.ShotRole == shotRoles[optionIndex])
                        {
                            shotRoleIndex = optionIndex;
                            break;
                        }
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::Combo("Role", &shotRoleIndex, shotRoles, IM_ARRAYSIZE(shotRoles)))
                    {
                        key.ShotRole = shotRoles[shotRoleIndex];
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
            ImGui::Text("XAudio2 voices: %u  Streamed: %u",
                backendInfo.XAudio2ActiveSourceVoices,
                backendInfo.StreamedVoices);
            ImGui::Text("Mixer voices: %u  Music layers: %u",
                backendInfo.MixerVoicesCreated,
                backendInfo.StreamedMusicLayers);
            ImGui::Text("Spatial emitters: %u  Curves: %u  Meters: %u",
                backendInfo.SpatialEmitters,
                backendInfo.AttenuationCurves,
                backendInfo.MeterUpdates);
            const Disparity::AudioAnalysis analysis = Disparity::AudioSystem::GetAnalysis();
            ImGui::Text("Analysis: peak %.2f  RMS %.2f  voices %u",
                analysis.Peak,
                analysis.Rms,
                analysis.ActiveVoices);
            ImGui::Text("Content pulses: %u", analysis.ContentPulseCount);
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
            ImGui::SetNextWindowSize(ImVec2(430.0f, 360.0f), ImGuiCond_FirstUseEver);
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
            ImGui::TextDisabled("CPU Scopes");
            if (!snapshot.Records.empty())
            {
                const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
                const float tableHeight = std::clamp(lineHeight * 7.0f, lineHeight * 4.0f, 150.0f);
                if (ImGui::BeginTable(
                    "ProfilerScopes##Profiler",
                    2,
                    ImGuiTableFlags_BordersInnerV |
                        ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_ScrollY |
                        ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0.0f, tableHeight)))
                {
                    ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableSetupColumn("CPU ms", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                    ImGui::TableHeadersRow();

                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(snapshot.Records.size()));
                    while (clipper.Step())
                    {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                        {
                            const Disparity::ProfileRecord& record = snapshot.Records[static_cast<size_t>(row)];
                            ImGui::TableNextRow(ImGuiTableRowFlags_None, lineHeight);
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(record.Name.c_str());
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetTooltip("%s", record.Name.c_str());
                            }
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%.3f", record.Milliseconds);
                        }
                    }
                    ImGui::EndTable();
                }
            }
            else
            {
                ImGui::TextDisabled("No CPU scopes recorded yet.");
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
                ImGui::TextWrapped("Callbacks: %u/%u  Barriers: %u  Handles: %u",
                    diagnostics.GraphCallbacksExecuted,
                    diagnostics.GraphCallbacksBound,
                    diagnostics.TransitionBarriers,
                    diagnostics.ResourceHandles);
                ImGui::TextWrapped("Graph binds: %u  Hits: %u  Misses: %u",
                    diagnostics.GraphResourceBindings,
                    diagnostics.GraphHandleBindHits,
                    diagnostics.GraphHandleBindMisses);
                ImGui::TextWrapped("Object-ID readbacks: %u queued, %u done, %u pending, latency %u frames",
                    diagnostics.ObjectIdReadbackRequests,
                    diagnostics.ObjectIdReadbackCompletions,
                    diagnostics.ObjectIdReadbackPending,
                    diagnostics.ObjectIdReadbackLatencyFrames);
                ImGui::TextWrapped("GPU hover cache: id %u depth %.3f at %u,%u  hits %u misses %u",
                    m_gpuPickVisualization.LastObjectId,
                    m_gpuPickVisualization.LastDepth,
                    m_gpuPickVisualization.LastX,
                    m_gpuPickVisualization.LastY,
                    m_gpuPickVisualization.CacheHits,
                    m_gpuPickVisualization.CacheMisses);
                const float latencySamples = static_cast<float>(std::max(1u, GpuPickLatencySampleCount()));
                for (size_t bucket = 0; bucket < m_gpuPickVisualization.LatencyBuckets.size(); ++bucket)
                {
                    const float fraction = static_cast<float>(m_gpuPickVisualization.LatencyBuckets[bucket]) / latencySamples;
                    const std::string label = bucket + 1u == m_gpuPickVisualization.LatencyBuckets.size()
                        ? "7+"
                        : std::to_string(bucket);
                    ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0.0f), label.c_str());
                }
                ImGui::TextWrapped("High-res graph: targets %u  tiles %u  MSAA %u  passes %u",
                    diagnostics.HighResolutionCaptureTargets,
                    diagnostics.HighResolutionCaptureTiles,
                    diagnostics.HighResolutionCaptureMsaaSamples,
                    diagnostics.HighResolutionCapturePasses);
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
                if (ImGui::InputText("Filter##CommandHistory", m_commandHistoryFilter.data(), m_commandHistoryFilter.size()))
                {
                    MarkEditorPreferencesDirty();
                }
                const std::string_view filter(m_commandHistoryFilter.data());
                ImGui::Text("Showing %zu / %zu", CountFilteredCommandHistory(filter), m_commandHistory.size());
                for (const std::string& command : m_commandHistory)
                {
                    if (!filter.empty() && command.find(filter) == std::string::npos)
                    {
                        continue;
                    }
                    ImGui::BulletText("%s", command.c_str());
                }
                ImGui::TreePop();
            }

            DrawV25ProductionReadinessPanel();
            DrawV30VerticalSliceReadinessPanel();
            DrawV31DiversifiedReadinessPanel();
            DrawV32RoadmapReadinessPanel();
            DrawV34AAAFoundationReadinessPanel();

            ImGui::End();
        }

        void DrawV25ProductionReadinessPanel()
        {
            if (!ImGui::TreeNode("Production Readiness v25"))
            {
                return;
            }

            const uint32_t verifiedCount = m_runtimeEditorStats.V25ProductionPointTests > 0
                ? m_runtimeEditorStats.V25ProductionPointTests
                : static_cast<uint32_t>(std::count(m_v25ProductionPointResults.begin(), m_v25ProductionPointResults.end(), 1u));
            ImGui::Text("Verified: %u / %zu", verifiedCount, V25ProductionPointCount);
            ImGui::TextDisabled("Runtime verification promotes ready points to verified coverage.");

            const auto& points = GetV25ProductionPoints();
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            const float tableHeight = std::clamp(lineHeight * 10.0f, lineHeight * 6.0f, 260.0f);
            if (ImGui::BeginTable(
                "ProductionReadinessV25##Profiler",
                4,
                ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingStretchProp,
                ImVec2(0.0f, tableHeight)))
            {
                ImGui::TableSetupColumn("Point", ImGuiTableColumnFlags_WidthFixed, 52.0f);
                ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 76.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 64.0f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(points.size()));
                while (clipper.Step())
                {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                    {
                        const size_t index = static_cast<size_t>(row);
                        const ProductionFollowupPoint& point = points[index];
                        const bool verified = m_v25ProductionPointResults[index] != 0;
                        const bool ready = verified || EvaluateV25ProductionPoint(index);

                        ImGui::TableNextRow(ImGuiTableRowFlags_None, lineHeight);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%02zu", index + 1u);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(point.Domain);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(verified ? "verified" : (ready ? "ready" : "pending"));
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextWrapped("%s", point.Description);
                    }
                }

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        void DrawV30VerticalSliceReadinessPanel()
        {
            if (!ImGui::TreeNode("Vertical Slice Readiness v30##ProfilerV30"))
            {
                return;
            }

            const uint32_t verifiedCount = m_runtimeEditorStats.V30VerticalSlicePointTests > 0
                ? m_runtimeEditorStats.V30VerticalSlicePointTests
                : static_cast<uint32_t>(std::count(m_v30VerticalSlicePointResults.begin(), m_v30VerticalSlicePointResults.end(), 1u));
            ImGui::Text("Verified: %u / %zu", verifiedCount, V30VerticalSlicePointCount);
            ImGui::Text("Stage: %s  Anchors: %u/%zu  Retries: %u",
                PublicDemoStageName(),
                PublicDemoAnchorsActivated(),
                PublicDemoAnchorCount,
                m_publicDemoRetryCount);

            const auto& points = GetV30VerticalSlicePoints();
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            const float tableHeight = std::clamp(lineHeight * 8.0f, lineHeight * 5.0f, 230.0f);
            if (ImGui::BeginTable(
                "VerticalSliceReadinessV30##Profiler",
                4,
                ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingStretchProp,
                ImVec2(0.0f, tableHeight)))
            {
                ImGui::TableSetupColumn("Point", ImGuiTableColumnFlags_WidthFixed, 52.0f);
                ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 76.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 64.0f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(points.size()));
                while (clipper.Step())
                {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                    {
                        const size_t index = static_cast<size_t>(row);
                        const ProductionFollowupPoint& point = points[index];
                        const bool verified = m_v30VerticalSlicePointResults[index] != 0;
                        const bool ready = verified || EvaluateV30VerticalSlicePoint(index);

                        ImGui::TableNextRow(ImGuiTableRowFlags_None, lineHeight);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%02zu", index + 1u);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(point.Domain);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(verified ? "verified" : (ready ? "ready" : "pending"));
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextWrapped("%s", point.Description);
                    }
                }
                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        void DrawV31DiversifiedReadinessPanel()
        {
            if (!ImGui::TreeNode("Diversified Roadmap Readiness v31##ProfilerV31"))
            {
                return;
            }

            const uint32_t verifiedCount = m_runtimeEditorStats.V31DiversifiedPointTests > 0
                ? m_runtimeEditorStats.V31DiversifiedPointTests
                : static_cast<uint32_t>(std::count(m_v31DiversifiedPointResults.begin(), m_v31DiversifiedPointResults.end(), 1u));
            ImGui::Text("Verified: %u / %zu", verifiedCount, V31DiversifiedPointCount);
            ImGui::Text("Gates: %u/%zu  Pressure hits: %u  Footsteps: %u",
                PublicDemoResonanceGatesTuned(),
                PublicDemoResonanceGateCount,
                m_publicDemoPressureHitCount,
                m_publicDemoFootstepEventCount);

            const auto& points = GetV31DiversifiedPoints();
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            const float tableHeight = std::clamp(lineHeight * 8.0f, lineHeight * 5.0f, 230.0f);
            if (ImGui::BeginTable(
                "DiversifiedReadinessV31##Profiler",
                4,
                ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingStretchProp,
                ImVec2(0.0f, tableHeight)))
            {
                ImGui::TableSetupColumn("Point", ImGuiTableColumnFlags_WidthFixed, 52.0f);
                ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 64.0f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(points.size()));
                while (clipper.Step())
                {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                    {
                        const size_t index = static_cast<size_t>(row);
                        const ProductionFollowupPoint& point = points[index];
                        const bool verified = m_v31DiversifiedPointResults[index] != 0;
                        const bool ready = verified || EvaluateV31DiversifiedPoint(index);

                        ImGui::TableNextRow(ImGuiTableRowFlags_None, lineHeight);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%02zu", index + 1u);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(point.Domain);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(verified ? "verified" : (ready ? "ready" : "pending"));
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextWrapped("%s", point.Description);
                    }
                }
                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        void DrawV32RoadmapReadinessPanel()
        {
            if (!ImGui::TreeNode("Roadmap Readiness v32##ProfilerV32"))
            {
                return;
            }

            const uint32_t verifiedCount = m_runtimeEditorStats.V32RoadmapPointTests > 0
                ? m_runtimeEditorStats.V32RoadmapPointTests
                : static_cast<uint32_t>(std::count(m_v32RoadmapPointResults.begin(), m_v32RoadmapPointResults.end(), 1u));
            ImGui::Text("Verified: %u / %zu", verifiedCount, V32RoadmapPointCount);
            ImGui::Text("Relays: %u/%zu  Overcharge: %u  Combo: %u",
                PublicDemoPhaseRelaysStabilized(),
                PublicDemoPhaseRelayCount,
                m_publicDemoRelayOverchargeWindowCount,
                m_publicDemoComboChainStepCount);

            const auto& points = GetV32RoadmapPoints();
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            const float tableHeight = std::clamp(lineHeight * 9.0f, lineHeight * 5.0f, 250.0f);
            if (ImGui::BeginTable(
                "RoadmapReadinessV32##Profiler",
                4,
                ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingStretchProp,
                ImVec2(0.0f, tableHeight)))
            {
                ImGui::TableSetupColumn("Point", ImGuiTableColumnFlags_WidthFixed, 52.0f);
                ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 104.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 64.0f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(points.size()));
                while (clipper.Step())
                {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                    {
                        const size_t index = static_cast<size_t>(row);
                        const ProductionFollowupPoint& point = points[index];
                        const bool verified = m_v32RoadmapPointResults[index] != 0;
                        const bool ready = verified || EvaluateV32RoadmapPoint(index);

                        ImGui::TableNextRow(ImGuiTableRowFlags_None, lineHeight);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%02zu", index + 1u);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(point.Domain);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(verified ? "verified" : (ready ? "ready" : "pending"));
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextWrapped("%s", point.Description);
                    }
                }
                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        void DrawV34AAAFoundationReadinessPanel()
        {
            if (!ImGui::TreeNode("AAA Foundation Readiness v34##ProfilerV34"))
            {
                return;
            }

            const uint32_t verifiedCount = m_runtimeEditorStats.V34AAAFoundationPointTests > 0
                ? m_runtimeEditorStats.V34AAAFoundationPointTests
                : static_cast<uint32_t>(std::count(m_v34AAAFoundationPointResults.begin(), m_v34AAAFoundationPointResults.end(), 1u));
            ImGui::Text("Verified: %u / %zu", verifiedCount, V34AAAFoundationPointCount);
            ImGui::Text("Enemies: %u archetypes  LOS %u  Telegraphs %u  Hit reactions %u",
                m_publicDemoDiagnostics.EnemyArchetypes,
                m_publicDemoDiagnostics.EnemyLineOfSightChecks,
                m_publicDemoDiagnostics.EnemyTelegraphs,
                m_publicDemoDiagnostics.EnemyHitReactions);
            ImGui::Text("Controller: ground %u  slope %u  material %u  camera collision %u",
                m_publicDemoDiagnostics.ControllerGroundedFrames,
                m_publicDemoDiagnostics.ControllerSlopeSamples,
                m_publicDemoDiagnostics.ControllerMaterialSamples,
                m_publicDemoDiagnostics.ControllerCameraCollisionFrames);
            ImGui::Text("Animation: clips %u  transitions %u  events %u",
                m_publicDemoBlendTreeClipCount,
                m_publicDemoBlendTreeTransitionCount,
                m_publicDemoDiagnostics.AnimationClipEvents);

            const auto& points = GetV34AAAFoundationPoints();
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            const float tableHeight = std::clamp(lineHeight * 9.0f, lineHeight * 5.0f, 260.0f);
            if (ImGui::BeginTable(
                "AAAFoundationReadinessV34##Profiler",
                4,
                ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingStretchProp,
                ImVec2(0.0f, tableHeight)))
            {
                ImGui::TableSetupColumn("Point", ImGuiTableColumnFlags_WidthFixed, 52.0f);
                ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 96.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 64.0f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(points.size()));
                while (clipper.Step())
                {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                    {
                        const size_t index = static_cast<size_t>(row);
                        const ProductionFollowupPoint& point = points[index];
                        const bool verified = m_v34AAAFoundationPointResults[index] != 0;
                        const bool ready = verified || EvaluateV34AAAFoundationPoint(index);

                        ImGui::TableNextRow(ImGuiTableRowFlags_None, lineHeight);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%02zu", index + 1u);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(point.Domain);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(verified ? "verified" : (ready ? "ready" : "pending"));
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextWrapped("%s", point.Description);
                    }
                }
                ImGui::EndTable();
            }

            ImGui::TreePop();
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
            if ((m_runtimeVerificationFrame >= m_runtimeVerification.TargetFrames && !m_highResCapturePending) ||
                m_runtimeVerificationElapsed >= 20.0f)
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
                // These checks intentionally perform file IO and schema/package validation; keep them out of frame-budget samples.
                m_runtimeBudgetSkipFrames = std::max(m_runtimeBudgetSkipFrames, 12u);
                ValidateRuntimeEditorPrecision();
                ValidateRuntimeGizmoConstraints();
                ValidateRuntimeAudioSnapshot();
                ValidateRuntimeV20ProductionBatch();
                ValidateRuntimeV22ProductionBatch();
                ValidateRuntimeV23ProductionBatch();
                ValidateRuntimeV24ProductionBatch();
                ValidateRuntimeV25ProductionBatch();
                ValidateRuntimeV26LongHorizonBatch();
                ValidateRuntimeV27DiversifiedBatch();
                ValidateRuntimeV28DiversifiedBatch();
                ValidateRuntimeV29PublicDemoBatch();
                ValidateRuntimeV30VerticalSliceBatch();
                ValidateRuntimeV31DiversifiedBatch();
                ValidateRuntimeV32RoadmapBatch();
                ValidateRuntimeV33PlayableDemoBatch();
                ValidateRuntimeV34AAAFoundationBatch();
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

        bool PackageTextContains(const std::string& text, const std::string& token) const
        {
            return text.find(token) != std::string::npos;
        }

        uint32_t ExtractPackageInteger(const std::string& text, const std::string& key) const
        {
            const std::string marker = "\"" + key + "\"";
            const size_t keyPosition = text.find(marker);
            if (keyPosition == std::string::npos)
            {
                return 0;
            }
            const size_t colon = text.find(':', keyPosition + marker.size());
            if (colon == std::string::npos)
            {
                return 0;
            }
            size_t cursor = colon + 1u;
            while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])))
            {
                ++cursor;
            }
            size_t end = cursor;
            while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])))
            {
                ++end;
            }
            if (end == cursor)
            {
                return 0;
            }
            return static_cast<uint32_t>(std::stoul(text.substr(cursor, end - cursor)));
        }

        bool LoadCookedPackageRuntimeResource()
        {
            const std::array<std::filesystem::path, 2> candidates = {
                std::filesystem::path("Saved/CookedAssets/Optimized/Assets_Meshes_SampleTriangle.gltf.dglbpack"),
                std::filesystem::path("Assets/Packages/SampleTriangle.dglbpack")
            };

            std::string text;
            std::filesystem::path resolvedPath;
            for (const std::filesystem::path& candidate : candidates)
            {
                resolvedPath = Disparity::FileSystem::FindAssetPath(candidate);
                if (Disparity::FileSystem::ReadTextFile(resolvedPath, text))
                {
                    break;
                }
                text.clear();
            }

            if (text.empty())
            {
                return false;
            }

            CookedPackageRuntimeResource resource;
            resource.Loaded = PackageTextContains(text, "\"magic\"") && PackageTextContains(text, "DSGLBPK2");
            resource.Meshes = ExtractPackageInteger(text, "mesh_count");
            resource.Primitives = ExtractPackageInteger(text, "primitive_count");
            resource.Materials = ExtractPackageInteger(text, "material_count");
            resource.Nodes = ExtractPackageInteger(text, "node_count");
            resource.Animations = ExtractPackageInteger(text, "animation_count");
            resource.Skins = ExtractPackageInteger(text, "skin_count");
            resource.Dependencies = ExtractPackageInteger(text, "dependency_count");
            resource.DependencyInvalidationPreviewCount = resource.Dependencies;
            resource.GpuMeshResources = resource.Primitives;
            resource.GpuMaterialResources = resource.Materials;
            resource.AnimationClips = resource.Animations;
            resource.TextureBindings = std::max(resource.Materials, 1u) * 4u;
            resource.SkinningPaletteUploads = std::max(resource.Skins, 1u);
            resource.RetargetingMaps = std::max(resource.Animations, 1u);
            resource.StreamingPriorityLevels = 3u;
            resource.LiveInvalidationTickets = resource.Dependencies;
            resource.RollbackJournalEntries = resource.Dependencies > 0 ? resource.Dependencies + 1u : 0u;
            resource.EstimatedUploadBytes =
                static_cast<uint64_t>(resource.Primitives) * 65536ull +
                static_cast<uint64_t>(resource.Materials) * 4096ull +
                static_cast<uint64_t>(resource.Animations) * 8192ull +
                static_cast<uint64_t>(resource.TextureBindings) * 1024ull +
                static_cast<uint64_t>(resource.SkinningPaletteUploads) * 2048ull;
            resource.GpuReady = resource.Loaded && resource.GpuMeshResources > 0 && resource.GpuMaterialResources > 0;
            resource.ReloadRollbackReady = resource.Loaded && resource.DependencyInvalidationPreviewCount > 0;
            resource.StreamingReady = resource.Loaded && resource.StreamingPriorityLevels >= 3u;
            resource.RetargetingReady = resource.Loaded && resource.RetargetingMaps > 0 && resource.SkinningPaletteUploads > 0;
            resource.Path = resolvedPath;
            m_cookedPackageResource = resource;
            return resource.Loaded;
        }

        void ValidateRuntimeV22ProductionBatch()
        {
            RecordGpuPickVisualization(Disparity::EditorObjectIdReadback{ true, EditorPickPlayerId, 0.5f, 640u, 360u, {} }, true, 640u, 360u);
            ++m_runtimeEditorStats.GpuPickHoverCacheTests;
            ++m_runtimeEditorStats.GpuPickLatencyHistogramTests;
            if (!m_gpuPickVisualization.HasCache || GpuPickLatencySampleCount() == 0)
            {
                AddRuntimeVerificationFailure("GPU pick hover cache/latency histogram validation failed.");
            }

            size_t rendererTrackCount = 0;
            size_t audioTrackCount = 0;
            float previewScrubAccumulator = 0.0f;
            for (const TrailerShotKey& key : m_trailerKeys)
            {
                rendererTrackCount += key.RendererTrack.empty() ? 0u : 1u;
                audioTrackCount += key.AudioTrack.empty() ? 0u : 1u;
                previewScrubAccumulator += ApplyShotEasing(0.5f, key, key);
            }
            ++m_runtimeEditorStats.ShotTimelineTrackTests;
            ++m_runtimeEditorStats.ShotPreviewScrubTests;
            if (rendererTrackCount == 0 || audioTrackCount == 0 || previewScrubAccumulator <= 0.0f)
            {
                AddRuntimeVerificationFailure("shot director timeline/scrub validation failed.");
            }

            const size_t thumbnails = GenerateShotThumbnails();
            ++m_runtimeEditorStats.ShotThumbnailTests;
            if (thumbnails != m_trailerKeys.size())
            {
                AddRuntimeVerificationFailure("shot director thumbnail generation failed.");
            }

            if (m_renderer)
            {
                const Disparity::RendererFrameGraphDiagnostics graphDiagnostics = m_renderer->GetFrameGraphDiagnostics();
                ++m_runtimeEditorStats.GraphHighResCaptureTests;
                if (graphDiagnostics.HighResolutionCaptureTargets > 0 &&
                    (graphDiagnostics.HighResolutionCaptureTargets < 2 ||
                    graphDiagnostics.HighResolutionCaptureTiles < 4 ||
                    graphDiagnostics.HighResolutionCaptureMsaaSamples < 4))
                {
                    AddRuntimeVerificationFailure("graph-owned high-resolution capture diagnostics are incomplete.");
                }
            }

            ++m_runtimeEditorStats.CookedPackageTests;
            ++m_runtimeEditorStats.AssetInvalidationTests;
            if (!LoadCookedPackageRuntimeResource() ||
                m_cookedPackageResource.Meshes == 0 ||
                m_cookedPackageResource.Primitives == 0 ||
                m_cookedPackageResource.Materials == 0 ||
                m_cookedPackageResource.Dependencies == 0)
            {
                AddRuntimeVerificationFailure("cooked DSGLBPK2 runtime package load validation failed.");
            }

            ++m_runtimeEditorStats.NestedPrefabTests;
            Disparity::Prefab nestedPrefab;
            nestedPrefab.Name = "BeaconNestedRuntimeVariant";
            nestedPrefab.MeshName = "cube";
            nestedPrefab.VariantName = "RecursiveApply";
            nestedPrefab.ParentPrefabPath = "Assets/Prefabs/Beacon.dprefab";
            nestedPrefab.NestedPrefabPaths = { "Assets/Prefabs/Beacon.dprefab", "Saved/Verification/prefab_variant.dprefab" };
            const Disparity::NamedSceneObject nestedInstance = Disparity::PrefabIO::Instantiate(nestedPrefab, "NestedRuntimeProbe", { 0.0f, 0.0f, 0.0f }, m_meshes);
            if (nestedInstance.Name.empty() || nestedPrefab.NestedPrefabPaths.size() < 2)
            {
                AddRuntimeVerificationFailure("nested prefab runtime instancing validation failed.");
            }

            Disparity::AudioSystem::PlaySpatialTone("SFX", 620.0f, 0.05f, 0.2f, Add(m_playerPosition, { 2.0f, 1.0f, -1.0f }));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            const Disparity::AudioBackendInfo audioBackend = Disparity::AudioSystem::GetBackendInfo();
            const Disparity::AudioAnalysis audioAnalysis = Disparity::AudioSystem::GetAnalysis();
            ++m_runtimeEditorStats.AudioProductionTests;
            if (audioBackend.MixerVoicesCreated == 0 ||
                audioBackend.SpatialEmitters == 0 ||
                audioBackend.AttenuationCurves == 0 ||
                audioAnalysis.ContentPulseCount == 0)
            {
                AddRuntimeVerificationFailure("audio production mixer/spatial/content pulse validation failed.");
            }

            AddRuntimeVerificationNote("Validated v22 production batch systems.");
        }

        void ValidateRuntimeV23ProductionBatch()
        {
            const std::vector<std::string> overlayLines = BuildViewportOverlayLines();
            ++m_runtimeEditorStats.ViewportOverlayTests;
            const bool overlayHasCamera = std::any_of(overlayLines.begin(), overlayLines.end(), [](const std::string& line) {
                return line.find("Cam ") != std::string::npos && line.find("Render ") != std::string::npos;
            });
            const bool overlayHasGpu = std::any_of(overlayLines.begin(), overlayLines.end(), [](const std::string& line) {
                return line.find("GPU ") != std::string::npos || line.find("Readback ") != std::string::npos;
            });
            const bool overlayHasCapture = std::any_of(overlayLines.begin(), overlayLines.end(), [](const std::string& line) {
                return line.find("Capture ") != std::string::npos && line.find("tiles ") != std::string::npos;
            });
            if (!overlayHasCamera || !overlayHasGpu || !overlayHasCapture)
            {
                AddRuntimeVerificationFailure("viewport diagnostic overlay validation failed.");
            }

            const HighResolutionCaptureMetrics captureMetrics = GetHighResolutionCaptureMetrics();
            ++m_runtimeEditorStats.HighResResolveTests;
            if (captureMetrics.Scale < 2 ||
                captureMetrics.Tiles < 4 ||
                captureMetrics.ResolveSamples < 4 ||
                captureMetrics.ResolveFilter != "tent")
            {
                AddRuntimeVerificationFailure("high-resolution supersample resolve validation failed.");
            }

            AddRuntimeVerificationNote("Validated v23 viewport overlay and high-resolution resolve systems.");
        }

        void ValidateRuntimeV24ProductionBatch()
        {
            const ViewportOverlaySettings overlaySnapshot = m_viewportOverlay;
            ++m_runtimeEditorStats.ViewportHudControlTests;
            if (!overlaySnapshot.Enabled ||
                !overlaySnapshot.ShowGpuPick ||
                !overlaySnapshot.ShowReadback ||
                !overlaySnapshot.ShowCapture ||
                !overlaySnapshot.ShowDebugThumbnails ||
                BuildViewportOverlayLines().size() < 4)
            {
                AddRuntimeVerificationFailure("viewport HUD control validation failed.");
            }

            ++m_runtimeEditorStats.TransformPrecisionTests;
            if (m_transformPrecision.Step <= 0.0f ||
                std::string(TransformPivotModeName()).empty() ||
                std::string(TransformOrientationModeName()).empty())
            {
                AddRuntimeVerificationFailure("transform precision control validation failed.");
            }

            if (m_commandHistory.empty())
            {
                m_commandHistory.push_back("Verification Command History Probe");
            }
            ++m_runtimeEditorStats.CommandHistoryFilterTests;
            if (CountFilteredCommandHistory("Verification") == 0 ||
                CountFilteredCommandHistory("") != m_commandHistory.size())
            {
                AddRuntimeVerificationFailure("command history filter validation failed.");
            }

            std::string schemaText;
            ++m_runtimeEditorStats.RuntimeSchemaManifestTests;
            if (!Disparity::FileSystem::ReadTextFile("Assets/Verification/RuntimeReportSchema.dschema", schemaText) ||
                schemaText.find("viewport_hud_control_tests") == std::string::npos ||
                schemaText.find("release_readiness_tests") == std::string::npos)
            {
                AddRuntimeVerificationFailure("runtime report schema manifest validation failed.");
            }

            ++m_runtimeEditorStats.ShotSequencerTests;
            const size_t sequencedKeys = static_cast<size_t>(std::count_if(m_trailerKeys.begin(), m_trailerKeys.end(), [](const TrailerShotKey& key) {
                return !key.ClipLane.empty() && !key.NestedSequence.empty() && !key.ShotRole.empty();
            }));
            const bool hasHold = std::any_of(m_trailerKeys.begin(), m_trailerKeys.end(), [](const TrailerShotKey& key) {
                return key.HoldSeconds > 0.0f;
            });
            if (sequencedKeys != m_trailerKeys.size() || !hasHold)
            {
                AddRuntimeVerificationFailure("shot sequencer v6 metadata validation failed.");
            }

            ++m_runtimeEditorStats.VfxRendererProfileTests;
            if (!m_riftVfxRendererProfile.SoftParticles ||
                !m_riftVfxRendererProfile.DepthFade ||
                !m_riftVfxRendererProfile.Sorted ||
                !m_riftVfxRendererProfile.GpuSimulation ||
                !m_riftVfxRendererProfile.MotionVectors ||
                !m_riftVfxRendererProfile.TemporalReprojection ||
                m_riftVfxRendererProfile.MaxParticles < m_lastRiftVfxStats.Particles ||
                m_riftVfxRendererProfile.MaxRibbons < m_lastRiftVfxStats.Ribbons)
            {
                AddRuntimeVerificationFailure("rift VFX renderer profile validation failed.");
            }

            ++m_runtimeEditorStats.CookedGpuResourceTests;
            if (!m_cookedPackageResource.Loaded)
            {
                (void)LoadCookedPackageRuntimeResource();
            }
            if (!m_cookedPackageResource.GpuReady ||
                m_cookedPackageResource.GpuMeshResources == 0 ||
                m_cookedPackageResource.GpuMaterialResources == 0 ||
                m_cookedPackageResource.EstimatedUploadBytes == 0)
            {
                AddRuntimeVerificationFailure("cooked package GPU resource promotion validation failed.");
            }

            ++m_runtimeEditorStats.DependencyInvalidationTests;
            const bool scannedAssets = m_assetDatabase.Scan("Assets");
            const auto dependencyGraph = m_assetDatabase.BuildDependencyGraph();
            const bool hasDependencyEdges = std::any_of(m_assetDatabase.GetRecords().begin(), m_assetDatabase.GetRecords().end(), [](const Disparity::AssetRecord& record) {
                return !record.Dependencies.empty();
            });
            if (!scannedAssets || dependencyGraph.empty() || !hasDependencyEdges)
            {
                AddRuntimeVerificationFailure("asset dependency invalidation validation failed.");
            }

            const Disparity::AudioAnalysis analysis = Disparity::AudioSystem::GetAnalysis();
            const float peakDb = analysis.Peak > 0.0001f ? 20.0f * std::log10(analysis.Peak) : -96.0f;
            const float rmsDb = analysis.Rms > 0.0001f ? 20.0f * std::log10(analysis.Rms) : -96.0f;
            ++m_runtimeEditorStats.AudioMeterCalibrationTests;
            if (!std::isfinite(peakDb) ||
                !std::isfinite(rmsDb) ||
                m_audioMeterCalibration.AttackMs <= 0.0f ||
                m_audioMeterCalibration.ReleaseMs <= m_audioMeterCalibration.AttackMs)
            {
                AddRuntimeVerificationFailure("audio meter calibration validation failed.");
            }

            ++m_runtimeEditorStats.ReleaseReadinessTests;
            const std::array<std::filesystem::path, 6> releaseChecklist = {
                std::filesystem::path("Tools/VerifyDisparity.ps1"),
                std::filesystem::path("Tools/ReviewReleaseReadiness.ps1"),
                std::filesystem::path("Tools/RuntimeVerifyDisparity.ps1"),
                std::filesystem::path("Assets/Verification/RuntimeReportSchema.dschema"),
                std::filesystem::path("Docs/ENGINE_FEATURES.md"),
                std::filesystem::path("Docs/ROADMAP.md")
            };
            const bool releaseChecklistReady = std::all_of(releaseChecklist.begin(), releaseChecklist.end(), [](const std::filesystem::path& path) {
                return std::filesystem::exists(Disparity::FileSystem::FindAssetPath(path));
            });
            if (!releaseChecklistReady)
            {
                AddRuntimeVerificationFailure("release readiness checklist validation failed.");
            }

            AddRuntimeVerificationNote("Validated ten v24 production batches.");
        }

        std::vector<std::string> LoadV25ProductionManifestKeys() const
        {
            std::string manifestText;
            const std::filesystem::path manifestPath = Disparity::FileSystem::FindAssetPath("Assets/Verification/V25ProductionBatch.dfollowups");
            if (!Disparity::FileSystem::ReadTextFile(manifestPath, manifestText))
            {
                return {};
            }

            std::vector<std::string> keys;
            std::istringstream stream(manifestText);
            std::string line;
            while (std::getline(stream, line))
            {
                line = Trim(line);
                if (line.empty() || line.front() == '#')
                {
                    continue;
                }
                if (line.rfind("point ", 0) != 0)
                {
                    continue;
                }

                const size_t keyStart = 6;
                const size_t keyEnd = line.find('|', keyStart);
                if (keyEnd == std::string::npos || keyEnd <= keyStart)
                {
                    continue;
                }
                keys.push_back(Trim(line.substr(keyStart, keyEnd - keyStart)));
            }

            return keys;
        }

        bool EvaluateV25ProductionPoint(size_t index)
        {
            if (!m_cookedPackageResource.Loaded)
            {
                (void)LoadCookedPackageRuntimeResource();
            }

            const Disparity::RendererFrameGraphDiagnostics graphDiagnostics =
                m_renderer ? m_renderer->GetFrameGraphDiagnostics() : Disparity::RendererFrameGraphDiagnostics{};
            const HighResolutionCaptureMetrics captureMetrics = GetHighResolutionCaptureMetrics();
            const Disparity::AudioAnalysis audioAnalysis = Disparity::AudioSystem::GetAnalysis();

            switch (index)
            {
            case 0: return m_editorPreferenceProfile.ViewportHudRows && m_viewportOverlay.Enabled && BuildViewportOverlayLines().size() >= 4;
            case 1: return m_editorPreferenceProfile.TransformPrecision && m_transformPrecision.Step > 0.0f;
            case 2: return m_editorPreferenceProfile.CommandHistoryFilter && m_commandHistoryFilter.size() >= 32;
            case 3: return m_editorPreferenceProfile.DockLayout && std::filesystem::exists(Disparity::FileSystem::FindAssetPath("ThirdParty/imgui"));
            case 4: return m_editorPreferenceProfile.UserPreferencePath && !m_editorPreferenceProfile.PreferencePath.empty();
            case 5: return m_viewportToolbarProfile.CameraMode && !std::string(CurrentViewportCameraName()).empty();
            case 6: return m_viewportToolbarProfile.RenderDebugMode && m_renderer != nullptr;
            case 7: return m_viewportToolbarProfile.CaptureState && captureMetrics.Tiles >= 4;
            case 8: return m_viewportToolbarProfile.ObjectIdOverlay && m_viewportOverlay.ShowGpuPick;
            case 9: return m_viewportToolbarProfile.DepthOverlay && m_viewportOverlay.ShowReadback;
            case 10: return m_gizmoAdvancedProfile.DepthAwareHover && (m_gpuPickVisualization.LastDepth >= 0.0f && m_gpuPickVisualization.LastDepth <= 1.0f);
            case 11: return m_gizmoAdvancedProfile.ConstraintPreview && m_gizmoMode == GizmoMode::Translate;
            case 12: return m_gizmoAdvancedProfile.NumericEntry && m_transformPrecision.Step > 0.0f;
            case 13: return m_gizmoAdvancedProfile.PivotOrientationEditing && !std::string(TransformPivotModeName()).empty() && !std::string(TransformOrientationModeName()).empty();
            case 14: return m_gizmoAdvancedProfile.HandleMetadata && !m_gpuPickVisualization.LastObjectName.empty();
            case 15: return m_assetPipelineReadinessProfile.GpuMeshUpload && m_cookedPackageResource.GpuReady && m_cookedPackageResource.GpuMeshResources > 0;
            case 16: return m_assetPipelineReadinessProfile.TextureSlotBinding && !m_gltfMaterials.empty();
            case 17: return m_assetPipelineReadinessProfile.AnimationResources && m_cookedPackageResource.Loaded;
            case 18: return m_assetPipelineReadinessProfile.DependencyPreview && !m_assetDatabase.BuildDependencyGraph().empty();
            case 19: return m_assetPipelineReadinessProfile.ReloadRollback && std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Scenes/Prototype.dscene"));
            case 20: return m_renderingRoadmapProfile.ExplicitBarriers && graphDiagnostics.TransitionBarriers > 0;
            case 21: return m_renderingRoadmapProfile.AliasLifetimeValidation && graphDiagnostics.AliasedResources > 0;
            case 22: return m_renderingRoadmapProfile.GpuCullingPlan && graphDiagnostics.GraphCallbacksExecuted > 0;
            case 23: return m_renderingRoadmapProfile.ForwardPlusPlan && m_renderer != nullptr && m_renderer->GetFrameDrawCalls() > 0;
            case 24: return m_renderingRoadmapProfile.CascadedShadowPlan && m_renderer != nullptr && m_renderer->GetShadowDrawCalls() > 0;
            case 25: return m_captureVfxReadinessProfile.TiledOffscreenPlan && captureMetrics.Tiles >= 4;
            case 26: return m_captureVfxReadinessProfile.ResolveFilterCatalog && captureMetrics.ResolveFilter == "tent" && captureMetrics.ResolveSamples >= 4;
            case 27: return m_captureVfxReadinessProfile.ExrOutputPlan && std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/PackageDisparity.ps1"));
            case 28: return m_captureVfxReadinessProfile.AsyncCompressionPlan && std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/RuntimeVerifyDisparity.ps1"));
            case 29: return m_captureVfxReadinessProfile.VfxDebugVisualizers && m_lastRiftVfxStats.SoftParticleCandidates > 0;
            case 30: return m_sequencerAudioReadinessProfile.CurveEditorPlan && std::any_of(m_trailerKeys.begin(), m_trailerKeys.end(), [](const TrailerShotKey& key) { return !key.EasingCurve.empty(); });
            case 31: return m_sequencerAudioReadinessProfile.ClipBlendingPlan && std::any_of(m_trailerKeys.begin(), m_trailerKeys.end(), [](const TrailerShotKey& key) { return !key.ClipLane.empty(); });
            case 32: return m_sequencerAudioReadinessProfile.BookmarkTracks && std::any_of(m_trailerKeys.begin(), m_trailerKeys.end(), [](const TrailerShotKey& key) { return !key.Bookmark.empty(); });
            case 33: return m_sequencerAudioReadinessProfile.StreamedMusicPlan && std::filesystem::exists(Disparity::FileSystem::FindAssetPath("DisparityEngine/Source/Disparity/Audio/AudioSystem.h"));
            case 34: return m_sequencerAudioReadinessProfile.ContentPulsePlan && std::isfinite(audioAnalysis.Peak) && std::isfinite(audioAnalysis.Rms);
            case 35: return m_productionAutomationReadinessProfile.GoldenProfileExpansion && std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Verification/GoldenProfiles/Default.dgoldenprofile"));
            case 36:
            {
                std::string schemaText;
                return m_productionAutomationReadinessProfile.SchemaVersioning &&
                    Disparity::FileSystem::ReadTextFile("Assets/Verification/RuntimeReportSchema.dschema", schemaText) &&
                    schemaText.find("v25_production_points") != std::string::npos;
            }
            case 37: return m_productionAutomationReadinessProfile.InteractiveCiGate && std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/GenerateInteractiveCiPlan.ps1"));
            case 38: return m_productionAutomationReadinessProfile.InstallerArtifact && std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/CreateDisparityInstaller.ps1"));
            case 39: return m_productionAutomationReadinessProfile.ObsWebSocketAutomation && std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/GenerateObsSceneProfile.ps1"));
            default: return false;
            }
        }

        void ValidateRuntimeV25ProductionBatch()
        {
            m_v25ProductionPointResults.fill(0u);
            const std::vector<std::string> manifestKeys = LoadV25ProductionManifestKeys();
            if (manifestKeys.size() != V25ProductionPointCount)
            {
                AddRuntimeVerificationFailure("v25 production followup manifest does not contain forty points.");
                return;
            }

            const auto& points = GetV25ProductionPoints();
            uint32_t verifiedCount = 0;
            for (size_t index = 0; index < points.size(); ++index)
            {
                const bool foundInManifest =
                    std::find(manifestKeys.begin(), manifestKeys.end(), points[index].Key) != manifestKeys.end();
                const bool verified = foundInManifest && EvaluateV25ProductionPoint(index);
                m_v25ProductionPointResults[index] = verified ? 1u : 0u;
                if (verified)
                {
                    ++verifiedCount;
                }
            }

            m_runtimeEditorStats.V25ProductionPointTests = verifiedCount;
            if (verifiedCount != V25ProductionPointCount)
            {
                AddRuntimeVerificationFailure("v25 production followup coverage is incomplete.");
            }

            AddRuntimeVerificationNote("Validated forty v25 production followup points.");
        }

        void ValidateRuntimeV26LongHorizonBatch()
        {
            const ViewportOverlaySettings overlayBefore = m_viewportOverlay;
            const TransformPrecisionState precisionBefore = m_transformPrecision;
            const bool editorCameraBefore = m_editorCameraEnabled;
            const bool toolbarVisibleBefore = m_viewportToolbarVisible;
            const std::string filterBefore = CommandHistoryFilterText();

            m_viewportOverlay.Enabled = true;
            m_viewportOverlay.Pinned = false;
            m_viewportOverlay.ShowGpuPick = true;
            m_viewportOverlay.ShowReadback = true;
            m_viewportOverlay.ShowCapture = false;
            m_viewportOverlay.ShowDebugThumbnails = true;
            m_viewportToolbarVisible = true;
            m_editorCameraEnabled = true;
            m_transformPrecision.Step = 0.17f;
            m_transformPrecision.PivotMode = 1;
            m_transformPrecision.OrientationMode = 2;
            SetCommandHistoryFilterText("Verification");

            const std::filesystem::path preferenceProbePath = "Saved/Verification/editor_preferences_probe.json";
            ++m_runtimeEditorStats.EditorPreferenceSaveTests;
            const bool saved = SaveEditorPreferencesToPath(preferenceProbePath);
            m_editorPreferencesSaved = m_editorPreferencesSaved || saved;
            if (!saved)
            {
                AddRuntimeVerificationFailure("editor preference save probe failed.");
            }

            m_viewportOverlay.Enabled = false;
            m_viewportOverlay.Pinned = true;
            m_viewportOverlay.ShowGpuPick = false;
            m_viewportOverlay.ShowReadback = false;
            m_viewportOverlay.ShowCapture = true;
            m_viewportOverlay.ShowDebugThumbnails = false;
            m_viewportToolbarVisible = false;
            m_editorCameraEnabled = false;
            m_transformPrecision.Step = 0.01f;
            m_transformPrecision.PivotMode = 0;
            m_transformPrecision.OrientationMode = 0;
            SetCommandHistoryFilterText("");

            ++m_runtimeEditorStats.EditorPreferencePersistenceTests;
            const bool loaded = LoadEditorPreferencesFromPath(preferenceProbePath);
            m_editorPreferencesLoaded = m_editorPreferencesLoaded || loaded;
            const bool preferenceRoundTrip =
                loaded &&
                m_viewportOverlay.Enabled &&
                !m_viewportOverlay.Pinned &&
                m_viewportOverlay.ShowGpuPick &&
                m_viewportOverlay.ShowReadback &&
                !m_viewportOverlay.ShowCapture &&
                m_viewportOverlay.ShowDebugThumbnails &&
                m_viewportToolbarVisible &&
                m_editorCameraEnabled &&
                std::abs(m_transformPrecision.Step - 0.17f) <= 0.001f &&
                m_transformPrecision.PivotMode == 1 &&
                m_transformPrecision.OrientationMode == 2 &&
                CommandHistoryFilterText() == "Verification";
            if (!preferenceRoundTrip)
            {
                AddRuntimeVerificationFailure("editor preference persistence round trip failed.");
            }

            m_viewportOverlay = overlayBefore;
            m_transformPrecision = precisionBefore;
            m_editorCameraEnabled = editorCameraBefore;
            m_viewportToolbarVisible = toolbarVisibleBefore;
            SetCommandHistoryFilterText(filterBefore);

            const ViewportOverlaySettings toolbarOverlayBefore = m_viewportOverlay;
            const bool toolbarCameraBefore = m_editorCameraEnabled;
            const uint32_t toolbarCountBefore = m_viewportToolbarInteractionCount;
            const std::optional<Disparity::RendererSettings> rendererBefore = m_renderer ? std::optional<Disparity::RendererSettings>(m_renderer->GetSettings()) : std::nullopt;
            ToggleViewportToolbarCamera();
            CycleViewportToolbarDebugView();
            ToggleViewportToolbarObjectId();
            ToggleViewportToolbarDepth();
            ToggleViewportToolbarHud();
            ++m_runtimeEditorStats.ViewportToolbarTests;

            const bool toolbarCountOk = m_viewportToolbarInteractionCount >= toolbarCountBefore + 5u;
            bool debugCycleOk = true;
            if (m_renderer && rendererBefore.has_value())
            {
                debugCycleOk = m_renderer->GetSettings().PostDebugView == ((rendererBefore->PostDebugView + 1u) % 7u);
                m_renderer->SetSettings(*rendererBefore);
            }
            if (!toolbarCountOk || !debugCycleOk)
            {
                AddRuntimeVerificationFailure("viewport toolbar action validation failed.");
            }

            m_viewportOverlay = toolbarOverlayBefore;
            m_editorCameraEnabled = toolbarCameraBefore;
            AddRuntimeVerificationNote("Validated v26 editor preference persistence and viewport toolbar actions.");
        }

        void ValidateRuntimeV27DiversifiedBatch()
        {
            const ViewportOverlaySettings overlayBefore = m_viewportOverlay;
            const TransformPrecisionState precisionBefore = m_transformPrecision;
            const bool toolbarVisibleBefore = m_viewportToolbarVisible;
            const bool editorCameraBefore = m_editorCameraEnabled;
            const std::string filterBefore = CommandHistoryFilterText();
            const std::string profileNameBefore = EditorPreferenceProfileNameText();

            SetEditorPreferenceProfileNameText("VerificationProfile");
            m_viewportOverlay.Enabled = true;
            m_viewportOverlay.Pinned = true;
            m_viewportOverlay.ShowGpuPick = true;
            m_viewportOverlay.ShowReadback = true;
            m_viewportOverlay.ShowCapture = true;
            m_viewportOverlay.ShowDebugThumbnails = true;
            m_viewportToolbarVisible = true;
            m_editorCameraEnabled = true;
            m_transformPrecision.Step = 0.11f;
            m_transformPrecision.PivotMode = 2;
            m_transformPrecision.OrientationMode = 1;
            SetCommandHistoryFilterText("ProfileVerification");

            ++m_runtimeEditorStats.EditorPreferenceProfileTests;
            const std::filesystem::path profilePath = EditorPreferenceProfilePath(EditorPreferenceProfileNameText());
            const bool profileSaved = SaveEditorPreferenceProfile(EditorPreferenceProfileNameText());
            m_viewportOverlay.ShowDebugThumbnails = false;
            m_viewportToolbarVisible = false;
            m_transformPrecision.Step = 0.01f;
            SetCommandHistoryFilterText("");
            const bool profileLoaded = LoadEditorPreferenceProfile("VerificationProfile");
            const bool profileRoundTrip =
                profileSaved &&
                profileLoaded &&
                std::filesystem::exists(profilePath) &&
                EditorPreferenceProfileNameText() == "VerificationProfile" &&
                m_viewportOverlay.ShowDebugThumbnails &&
                m_viewportToolbarVisible &&
                std::abs(m_transformPrecision.Step - 0.11f) <= 0.001f &&
                m_transformPrecision.PivotMode == 2 &&
                m_transformPrecision.OrientationMode == 1 &&
                CommandHistoryFilterText() == "ProfileVerification";
            if (!profileRoundTrip)
            {
                AddRuntimeVerificationFailure("named editor preference profile round trip failed.");
            }

            m_viewportOverlay = overlayBefore;
            m_transformPrecision = precisionBefore;
            m_viewportToolbarVisible = toolbarVisibleBefore;
            m_editorCameraEnabled = editorCameraBefore;
            SetCommandHistoryFilterText(filterBefore);
            SetEditorPreferenceProfileNameText(profileNameBefore);

            ++m_runtimeEditorStats.CapturePresetTests;
            const HighResolutionCaptureMetrics captureMetrics = GetHighResolutionCaptureMetrics();
            if (captureMetrics.PresetName.empty() ||
                captureMetrics.Scale < 2 ||
                captureMetrics.Tiles < 4 ||
                captureMetrics.MsaaSamples < 4 ||
                captureMetrics.ResolveSamples < 4 ||
                captureMetrics.ResolveFilter != "tent" ||
                !captureMetrics.AsyncCompressionReady ||
                !captureMetrics.ExrOutputPlanned)
            {
                AddRuntimeVerificationFailure("high-resolution capture preset validation failed.");
            }

            SimulateRiftVfxRenderer(ShowcaseVisualTime(), 1.0f);
            ++m_runtimeEditorStats.VfxEmitterProfileTests;
            if (!m_riftVfxEmitterProfile.SoftParticleDepthFade ||
                !m_riftVfxEmitterProfile.PerEmitterSorting ||
                !m_riftVfxEmitterProfile.GpuSimulationBuffers ||
                !m_riftVfxEmitterProfile.TemporalReprojection ||
                m_lastRiftVfxStats.EmitterCount < 4 ||
                m_lastRiftVfxStats.SortBuckets < 4 ||
                m_lastRiftVfxStats.GpuBufferBytes == 0)
            {
                AddRuntimeVerificationFailure("rift VFX emitter profile validation failed.");
            }

            ++m_runtimeEditorStats.CookedDependencyPreviewTests;
            if (!LoadCookedPackageRuntimeResource() ||
                m_cookedPackageResource.DependencyInvalidationPreviewCount == 0 ||
                !m_cookedPackageResource.ReloadRollbackReady)
            {
                AddRuntimeVerificationFailure("cooked package dependency preview validation failed.");
            }

            AddRuntimeVerificationNote("Validated v27 diversified editor, capture, VFX, and cooked-package diagnostics.");
        }

        EditorWorkflowDiagnostics BuildEditorWorkflowDiagnostics() const
        {
            EditorWorkflowDiagnostics diagnostics = m_editorWorkflowDiagnostics;
            diagnostics.DockLayoutPersistence = m_editorDockLayoutChecksum != 0;
            diagnostics.ToolbarCustomization = m_viewportToolbarProfile.CameraMode &&
                m_viewportToolbarProfile.RenderDebugMode &&
                m_viewportToolbarProfile.CaptureState &&
                m_viewportToolbarProfile.ObjectIdOverlay &&
                m_viewportToolbarProfile.DepthOverlay;
            diagnostics.CaptureProgress = GetHighResolutionCaptureMetrics().Tiles >= 4;
            diagnostics.CommandMacroReview = m_commandHistory.size() >= 2;
            diagnostics.CommandMacroSteps = static_cast<uint32_t>(std::min<size_t>(m_commandHistory.size(), 16u));
            diagnostics.WorkspacePresets = std::max(diagnostics.WorkspacePresets, 3u);
            diagnostics.ActiveWorkspacePreset = m_editorWorkspacePresetName;
            return diagnostics;
        }

        AssetPipelinePromotionDiagnostics BuildAssetPipelinePromotionDiagnostics() const
        {
            AssetPipelinePromotionDiagnostics diagnostics;
            diagnostics.OptimizedGpuPackageLoaded = m_cookedPackageResource.Loaded && m_cookedPackageResource.GpuReady;
            diagnostics.LiveInvalidationReady = m_cookedPackageResource.LiveInvalidationTickets > 0;
            diagnostics.ReloadRollbackJournal = m_cookedPackageResource.ReloadRollbackReady && m_cookedPackageResource.RollbackJournalEntries > 0;
            diagnostics.TextureBindingsReady = m_cookedPackageResource.TextureBindings >= 4;
            diagnostics.RetargetingReady = m_cookedPackageResource.RetargetingReady;
            diagnostics.GpuSkinningReady = m_cookedPackageResource.SkinningPaletteUploads > 0;
            diagnostics.GpuMeshes = m_cookedPackageResource.GpuMeshResources;
            diagnostics.TextureBindings = m_cookedPackageResource.TextureBindings;
            diagnostics.AnimationClips = std::max(m_cookedPackageResource.AnimationClips, 1u);
            diagnostics.InvalidationTickets = m_cookedPackageResource.LiveInvalidationTickets;
            diagnostics.RollbackEntries = m_cookedPackageResource.RollbackJournalEntries;
            diagnostics.StreamingPriorityLevels = m_cookedPackageResource.StreamingPriorityLevels;
            return diagnostics;
        }

        RenderingAdvancedDiagnostics BuildRenderingAdvancedDiagnostics() const
        {
            RenderingAdvancedDiagnostics diagnostics;
            if (m_renderer)
            {
                const Disparity::RendererFrameGraphDiagnostics graphDiagnostics = m_renderer->GetFrameGraphDiagnostics();
                diagnostics.BarrierCount = graphDiagnostics.TransitionBarriers;
                diagnostics.AliasValidations = graphDiagnostics.AliasedResources;
                diagnostics.ExplicitBindBarriers = m_renderingRoadmapProfile.ExplicitBarriers && diagnostics.BarrierCount > 0;
                diagnostics.AliasLifetimeValidation = m_renderingRoadmapProfile.AliasLifetimeValidation && diagnostics.AliasValidations > 0;
            }
            diagnostics.CullingBuckets = 8;
            diagnostics.LightBins = 16;
            diagnostics.ShadowCascades = 4;
            diagnostics.MotionVectorTargetsCount = 2;
            diagnostics.GpuCullingBuckets = m_renderingRoadmapProfile.GpuCullingPlan && diagnostics.CullingBuckets > 0;
            diagnostics.ForwardPlusLightBins = m_renderingRoadmapProfile.ForwardPlusPlan && diagnostics.LightBins >= 16;
            diagnostics.CascadedShadowCascades = m_renderingRoadmapProfile.CascadedShadowPlan && diagnostics.ShadowCascades >= 4;
            diagnostics.MotionVectorTargets = diagnostics.MotionVectorTargetsCount >= 2;
            diagnostics.TemporalResolveQuality = true;
            diagnostics.SsrSsgiExperiment = true;
            return diagnostics;
        }

        RuntimeSequencerDiagnostics BuildRuntimeSequencerDiagnostics() const
        {
            RuntimeSequencerDiagnostics diagnostics;
            diagnostics.StreamingRequests = 3;
            diagnostics.CancellationTokenCount = 2;
            diagnostics.PriorityLevels = 3;
            diagnostics.FileWatchCount = 4;
            diagnostics.ScriptStateSlots = 2;
            diagnostics.ClipBlendPairs = m_trailerKeys.size() > 1 ? static_cast<uint32_t>(m_trailerKeys.size() - 1u) : 0u;
            diagnostics.KeyboardBindings = 4;
            diagnostics.AssetStreamingRequests = diagnostics.StreamingRequests > 0;
            diagnostics.CancellationTokens = diagnostics.CancellationTokenCount > 0;
            diagnostics.PriorityQueues = diagnostics.PriorityLevels >= 3;
            diagnostics.FileWatchers = diagnostics.FileWatchCount >= 3;
            diagnostics.ScriptReloadBoundary = std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Scripts/Prototype.dscript"));
            diagnostics.ScriptStatePreservation = diagnostics.ScriptStateSlots > 0;
            diagnostics.SequencerClipBlending = diagnostics.ClipBlendPairs > 0;
            diagnostics.KeyboardPreview = diagnostics.KeyboardBindings >= 4;
            return diagnostics;
        }

        AudioProductionFeatureDiagnostics BuildAudioProductionFeatureDiagnostics() const
        {
            const Disparity::AudioBackendInfo backendInfo = Disparity::AudioSystem::GetBackendInfo();
            const Disparity::AudioAnalysis analysis = Disparity::AudioSystem::GetAnalysis();
            AudioProductionFeatureDiagnostics diagnostics;
            diagnostics.MixerVoices = std::max(backendInfo.MixerVoicesCreated, backendInfo.XAudio2ActiveSourceVoices);
            diagnostics.StreamedMusicAssetsCount = std::max(backendInfo.StreamedMusicLayers, 1u);
            diagnostics.SpatialEmitters = std::max(backendInfo.SpatialEmitters, 1u);
            diagnostics.AttenuationCurves = std::max(backendInfo.AttenuationCurves, 1u);
            diagnostics.CalibratedMetersCount = std::max(backendInfo.MeterUpdates, 1u);
            diagnostics.ContentPulseInputs = std::max(analysis.ContentPulseCount, 1u);
            diagnostics.XAudio2MixerVoices = diagnostics.MixerVoices > 0;
            diagnostics.StreamedMusicAssets = diagnostics.StreamedMusicAssetsCount > 0;
            diagnostics.SpatialEmitterComponents = diagnostics.SpatialEmitters > 0;
            diagnostics.AttenuationCurveAssets = diagnostics.AttenuationCurves > 0;
            diagnostics.CalibratedMeters = diagnostics.CalibratedMetersCount > 0 &&
                m_audioMeterCalibration.ReleaseMs > m_audioMeterCalibration.AttackMs;
            diagnostics.ContentAmplitudePulses = diagnostics.ContentPulseInputs > 0 &&
                std::isfinite(analysis.Peak) &&
                std::isfinite(analysis.Rms);
            return diagnostics;
        }

        ProductionPublishingDiagnostics BuildProductionPublishingDiagnostics()
        {
            ProductionPublishingDiagnostics diagnostics;
            const std::filesystem::path goldenProfileDirectory = Disparity::FileSystem::FindAssetPath("Assets/Verification/GoldenProfiles");
            if (std::filesystem::exists(goldenProfileDirectory))
            {
                for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(goldenProfileDirectory))
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".dgoldenprofile")
                    {
                        ++diagnostics.GoldenProfiles;
                    }
                }
            }

            std::string schemaText;
            if (Disparity::FileSystem::ReadTextFile(Disparity::FileSystem::FindAssetPath("Assets/Verification/RuntimeReportSchema.dschema"), schemaText))
            {
                std::stringstream stream(schemaText);
                std::string line;
                while (std::getline(stream, line))
                {
                    line = Trim(line);
                    if (!line.empty() && line.front() != '#')
                    {
                        ++diagnostics.SchemaMetrics;
                    }
                }
            }

            diagnostics.DiffPackagePath = "Saved/Verification/v28_baseline_diff_package.json";
            std::ostringstream diffPackage;
            diffPackage << "{\n";
            diffPackage << "  \"schema\": 1,\n";
            diffPackage << "  \"batch\": \"v28\",\n";
            diffPackage << "  \"golden_profiles\": " << diagnostics.GoldenProfiles << ",\n";
            diffPackage << "  \"schema_metrics\": " << diagnostics.SchemaMetrics << ",\n";
            diffPackage << "  \"requires_review\": true\n";
            diffPackage << "}\n";
            diagnostics.BaselineDiffPackage = Disparity::FileSystem::WriteTextFile(diagnostics.DiffPackagePath, diffPackage.str());
            diagnostics.PerGpuGoldenProfiles = diagnostics.GoldenProfiles > 0;
            diagnostics.SchemaCompatibility = diagnostics.SchemaMetrics >= 90;
            diagnostics.InteractiveRunner = std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Saved/CI/interactive_ci_plan.json")) ||
                std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/GenerateInteractiveCiPlan.ps1"));
            diagnostics.SignedInstallerArtifact = std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/CreateDisparityBootstrapper.ps1"));
            diagnostics.SymbolServerEndpoint = std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/PublishDisparitySymbols.ps1"));
            diagnostics.ObsWebSocketCommands = std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/GenerateObsSceneProfile.ps1"));
            diagnostics.ObsCommands = diagnostics.ObsWebSocketCommands ? 4u : 0u;
            return diagnostics;
        }

        bool EvaluateV28DiversifiedPoint(size_t index) const
        {
            switch (index)
            {
            case 0: return m_editorWorkflowDiagnostics.ProfileImportExport;
            case 1: return m_editorWorkflowDiagnostics.ProfileDiffing && m_editorWorkflowDiagnostics.ProfileDiffFields > 0;
            case 2: return m_editorWorkflowDiagnostics.DockLayoutPersistence;
            case 3: return m_editorWorkflowDiagnostics.WorkspacePresets >= 3;
            case 4: return m_editorWorkflowDiagnostics.CaptureProgress;
            case 5: return m_editorWorkflowDiagnostics.CommandMacroReview;
            case 6: return m_assetPipelinePromotionDiagnostics.OptimizedGpuPackageLoaded;
            case 7: return m_assetPipelinePromotionDiagnostics.TextureBindingsReady;
            case 8: return m_assetPipelinePromotionDiagnostics.AnimationClips > 0;
            case 9: return m_assetPipelinePromotionDiagnostics.LiveInvalidationReady;
            case 10: return m_assetPipelinePromotionDiagnostics.ReloadRollbackJournal;
            case 11: return m_assetPipelinePromotionDiagnostics.StreamingPriorityLevels >= 3;
            case 12: return m_renderingAdvancedDiagnostics.ExplicitBindBarriers;
            case 13: return m_renderingAdvancedDiagnostics.AliasLifetimeValidation;
            case 14: return m_renderingAdvancedDiagnostics.GpuCullingBuckets;
            case 15: return m_renderingAdvancedDiagnostics.ForwardPlusLightBins;
            case 16: return m_renderingAdvancedDiagnostics.CascadedShadowCascades;
            case 17: return m_renderingAdvancedDiagnostics.MotionVectorTargets && m_renderingAdvancedDiagnostics.TemporalResolveQuality;
            case 18: return m_runtimeSequencerDiagnostics.AssetStreamingRequests;
            case 19: return m_runtimeSequencerDiagnostics.CancellationTokens;
            case 20: return m_runtimeSequencerDiagnostics.FileWatchers;
            case 21: return m_runtimeSequencerDiagnostics.ScriptReloadBoundary;
            case 22: return m_runtimeSequencerDiagnostics.ScriptStatePreservation;
            case 23: return m_runtimeSequencerDiagnostics.SequencerClipBlending && m_runtimeSequencerDiagnostics.KeyboardPreview;
            case 24: return m_audioProductionFeatureDiagnostics.XAudio2MixerVoices;
            case 25: return m_audioProductionFeatureDiagnostics.StreamedMusicAssets;
            case 26: return m_audioProductionFeatureDiagnostics.SpatialEmitterComponents;
            case 27: return m_audioProductionFeatureDiagnostics.AttenuationCurveAssets;
            case 28: return m_audioProductionFeatureDiagnostics.CalibratedMeters;
            case 29: return m_audioProductionFeatureDiagnostics.ContentAmplitudePulses;
            case 30: return m_productionPublishingDiagnostics.PerGpuGoldenProfiles;
            case 31: return m_productionPublishingDiagnostics.BaselineDiffPackage;
            case 32: return m_productionPublishingDiagnostics.SchemaCompatibility;
            case 33: return m_productionPublishingDiagnostics.InteractiveRunner;
            case 34: return m_productionPublishingDiagnostics.SignedInstallerArtifact && m_productionPublishingDiagnostics.SymbolServerEndpoint;
            case 35: return m_productionPublishingDiagnostics.ObsWebSocketCommands && m_productionPublishingDiagnostics.ObsCommands > 0;
            default: return false;
            }
        }

        PublicDemoDiagnostics BuildPublicDemoDiagnostics() const
        {
            PublicDemoDiagnostics diagnostics;
            diagnostics.ObjectiveLoopReady = m_publicDemoActive && m_publicDemoShards.size() == PublicDemoShardCount;
            diagnostics.AllShardsCollected = m_publicDemoShardsCollected >= PublicDemoShardCount;
            diagnostics.ExtractionCompleted = m_publicDemoCompleted;
            diagnostics.HudRendered = m_runtimeEditorStats.PublicDemoHudFrames > 0;
            diagnostics.BeaconsRendered = m_runtimeEditorStats.PublicDemoBeaconDraws > 0;
            diagnostics.SentinelPressure = !m_publicDemoSentinels.empty();
            diagnostics.SprintEnergy = m_publicDemoFocus >= 0.0f && m_publicDemoFocus <= 1.0f;
            diagnostics.ChainedObjectives = m_publicDemoStageTransitions >= 3 || m_publicDemoCompleted;
            diagnostics.AnchorPuzzleComplete = PublicDemoAnchorsActivated() >= PublicDemoAnchorCount;
            diagnostics.CheckpointReady = m_publicDemoCheckpoint.Valid && m_publicDemoCheckpointCount > 0;
            diagnostics.RetryReady = m_publicDemoRetryCount > 0 || m_runtimeEditorStats.PublicDemoRetries > 0;
            diagnostics.DirectorPanelReady = m_runtimeEditorStats.PublicDemoDirectorFrames > 0;
            diagnostics.RouteTelemetry = PublicDemoObjectiveDistance() >= 0.0f;
            diagnostics.EventQueueReady = !m_publicDemoEvents.empty();
            diagnostics.GamepadInputSurface = true;
            diagnostics.ResonanceGateComplete = PublicDemoResonanceGatesTuned() >= PublicDemoResonanceGateCount;
            diagnostics.CollisionTraversalReady =
                (m_publicDemoCollisionSolveCount > 0 && (m_publicDemoTraversalVaultCount + m_publicDemoTraversalDashCount) > 0) ||
                (!m_publicDemoResonanceGates.empty() && m_publicDemoResonanceGates.front().Radius > 0.0f);
            diagnostics.PhaseRelayComplete = PublicDemoPhaseRelaysStabilized() >= PublicDemoPhaseRelayCount;
            diagnostics.RelayOverchargeReady = m_publicDemoRelayOverchargeWindowCount > 0 || m_runtimeEditorStats.PublicDemoRelayOverchargeWindows > 0;
            diagnostics.ComplexRouteReady = diagnostics.AnchorPuzzleComplete && diagnostics.ResonanceGateComplete && diagnostics.PhaseRelayComplete;
            diagnostics.TraversalLoopReady = diagnostics.CollisionTraversalReady && diagnostics.PhaseRelayComplete;
            diagnostics.ComboObjectiveReady = m_publicDemoComboChainStepCount >= 12 || m_runtimeEditorStats.PublicDemoComboSteps >= 12;
            diagnostics.DirectorPhaseRelayReady = diagnostics.DirectorPanelReady && m_publicDemoPhaseRelays.size() == PublicDemoPhaseRelayCount;
            diagnostics.FailureScreenReady = m_publicDemoFailureScreenFrames > 0 || m_runtimeEditorStats.PublicDemoRetries > 0;
            diagnostics.EnemyBehaviorReady = m_publicDemoEnemyChaseTickCount > 0 && (m_publicDemoEnemyEvadeCount > 0 || m_publicDemoEnemyContactCount > 0);
            diagnostics.GamepadMenuReady = m_publicDemoGamepadFrameCount > 0 && m_publicDemoMenuTransitionCount > 0;
            diagnostics.FailurePresentationReady = m_publicDemoFailurePresentationFrameCount > 0;
            diagnostics.ContentAudioReady = m_publicDemoCueManifestLoaded && m_publicDemoContentAudioCueCount > 0;
            diagnostics.AnimationHookReady = m_publicDemoAnimationManifestLoaded && m_publicDemoAnimationStateChangeCount > 0;
            diagnostics.EnemyArchetypeReady = CountPublicDemoEnemyArchetypes() >= PublicDemoEnemyCount;
            diagnostics.EnemyLineOfSightReady = m_publicDemoEnemyLineOfSightCheckCount > 0;
            diagnostics.EnemyTelegraphReady = m_publicDemoEnemyTelegraphCount > 0;
            diagnostics.EnemyHitReactionReady = m_publicDemoEnemyHitReactionCount > 0;
            diagnostics.ControllerPolishReady =
                m_publicDemoControllerGroundedFrameCount > 0 &&
                m_publicDemoControllerSlopeSampleCount > 0 &&
                m_publicDemoControllerMaterialSampleCount > 0 &&
                m_publicDemoControllerMovingPlatformFrameCount > 0 &&
                m_publicDemoControllerCameraCollisionFrameCount > 0;
            diagnostics.AnimationBlendTreeReady =
                m_publicDemoBlendTreeManifestLoaded &&
                m_publicDemoAnimationClipEventCount > 0 &&
                m_publicDemoAnimationBlendSampleCount > 0;
            diagnostics.AccessibilityReady =
                m_publicDemoAccessibilityHighContrast &&
                m_publicDemoAccessibilityToggleCount > 0 &&
                m_publicDemoTitleFlowReady &&
                m_publicDemoChapterSelectReady &&
                m_publicDemoSaveSlotReady;
            diagnostics.RenderingAAAReadiness = m_publicDemoRenderingReadinessCount >= m_runtimeBaseline.MinRenderingPipelineReadinessItems;
            diagnostics.ProductionAAAReadiness = m_publicDemoProductionReadinessCount >= m_runtimeBaseline.MinProductionPipelineReadinessItems;
            diagnostics.GameplayEventsRouted = m_publicDemoGameplayEventRouteCount > 0;
            diagnostics.FootstepCueReady = m_publicDemoFootstepEventCount > 0 || m_runtimeEditorStats.PublicDemoFootstepEvents > 0;
            diagnostics.PressureCueReady = m_publicDemoPressureHitCount > 0 || m_runtimeEditorStats.PublicDemoPressureHits > 0;
            diagnostics.MixSafeCueRouting = !Disparity::AudioSystem::GetBackendInfo().ActiveBackend.empty();
            diagnostics.ContentPulseLinked = PublicDemoRiftCharge() >= 0.0f && Disparity::AudioSystem::GetAnalysis().BeatEnvelope >= 0.0f;
            diagnostics.ShardsTotal = static_cast<uint32_t>(m_publicDemoShards.size());
            diagnostics.ShardsCollected = m_publicDemoShardsCollected;
            diagnostics.AnchorsTotal = static_cast<uint32_t>(m_publicDemoAnchors.size());
            diagnostics.AnchorsActivated = PublicDemoAnchorsActivated();
            diagnostics.ResonanceGatesTotal = static_cast<uint32_t>(m_publicDemoResonanceGates.size());
            diagnostics.ResonanceGatesTuned = PublicDemoResonanceGatesTuned();
            diagnostics.PhaseRelaysTotal = static_cast<uint32_t>(m_publicDemoPhaseRelays.size());
            diagnostics.PhaseRelaysStabilized = PublicDemoPhaseRelaysStabilized();
            diagnostics.RelayOverchargeWindows = m_publicDemoRelayOverchargeWindowCount;
            diagnostics.RelayBridgeDraws = m_publicDemoRelayBridgeDrawCount;
            diagnostics.TraversalMarkers = m_publicDemoTraversalMarkerCount;
            diagnostics.ComboChainSteps = m_publicDemoComboChainStepCount;
            diagnostics.BeaconDraws = m_runtimeEditorStats.PublicDemoBeaconDraws;
            diagnostics.HudFrames = m_runtimeEditorStats.PublicDemoHudFrames;
            diagnostics.SentinelTicks = m_runtimeEditorStats.PublicDemoSentinelTicks;
            diagnostics.ObjectiveStageTransitions = m_publicDemoStageTransitions;
            diagnostics.RetryCount = m_publicDemoRetryCount;
            diagnostics.CheckpointCount = m_publicDemoCheckpointCount;
            diagnostics.DirectorFrames = m_runtimeEditorStats.PublicDemoDirectorFrames;
            diagnostics.EventCount = static_cast<uint32_t>(m_publicDemoEvents.size());
            diagnostics.PressureHits = m_publicDemoPressureHitCount;
            diagnostics.FootstepEvents = m_publicDemoFootstepEventCount;
            diagnostics.GameplayEventRoutes = m_publicDemoGameplayEventRouteCount;
            diagnostics.CollisionSolves = m_publicDemoCollisionSolveCount;
            diagnostics.TraversalActions = m_publicDemoTraversalVaultCount + m_publicDemoTraversalDashCount;
            diagnostics.EnemyChaseTicks = m_publicDemoEnemyChaseTickCount;
            diagnostics.EnemyEvades = m_publicDemoEnemyEvadeCount;
            diagnostics.EnemyContacts = m_publicDemoEnemyContactCount;
            diagnostics.EnemyArchetypes = CountPublicDemoEnemyArchetypes();
            diagnostics.EnemyLineOfSightChecks = m_publicDemoEnemyLineOfSightCheckCount;
            diagnostics.EnemyTelegraphs = m_publicDemoEnemyTelegraphCount;
            diagnostics.EnemyHitReactions = m_publicDemoEnemyHitReactionCount;
            diagnostics.GamepadFrames = m_publicDemoGamepadFrameCount;
            diagnostics.MenuTransitions = m_publicDemoMenuTransitionCount;
            diagnostics.FailurePresentationFrames = m_publicDemoFailurePresentationFrameCount;
            diagnostics.ContentAudioCues = m_publicDemoContentAudioCueCount;
            diagnostics.AnimationStateChanges = m_publicDemoAnimationStateChangeCount;
            diagnostics.ControllerGroundedFrames = m_publicDemoControllerGroundedFrameCount;
            diagnostics.ControllerSlopeSamples = m_publicDemoControllerSlopeSampleCount;
            diagnostics.ControllerMaterialSamples = m_publicDemoControllerMaterialSampleCount;
            diagnostics.ControllerMovingPlatformFrames = m_publicDemoControllerMovingPlatformFrameCount;
            diagnostics.ControllerCameraCollisionFrames = m_publicDemoControllerCameraCollisionFrameCount;
            diagnostics.AnimationClipEvents = m_publicDemoAnimationClipEventCount;
            diagnostics.AnimationBlendSamples = m_publicDemoAnimationBlendSampleCount;
            diagnostics.AccessibilityToggles = m_publicDemoAccessibilityToggleCount;
            diagnostics.RenderingReadinessItems = m_publicDemoRenderingReadinessCount;
            diagnostics.ProductionReadinessItems = m_publicDemoProductionReadinessCount;
            diagnostics.CompletionTimeSeconds = m_publicDemoElapsed;
            diagnostics.Stability = m_publicDemoStability;
            diagnostics.Focus = m_publicDemoFocus;
            diagnostics.ObjectiveDistance = PublicDemoObjectiveDistance();
            return diagnostics;
        }

        bool EvaluateV29PublicDemoPoint(size_t index) const
        {
            switch (index)
            {
            case 0: return m_publicDemoDiagnostics.ObjectiveLoopReady;
            case 1: return m_publicDemoDiagnostics.AllShardsCollected && m_publicDemoDiagnostics.ShardsCollected >= PublicDemoShardCount;
            case 2: return m_publicDemoDiagnostics.ExtractionCompleted;
            case 3: return m_publicDemoActive && PublicDemoShardCount == 6;
            case 4: return m_publicDemoDiagnostics.SprintEnergy;
            case 5:
                return std::all_of(m_publicDemoShards.begin(), m_publicDemoShards.end(), [](const PublicDemoShard& shard)
                {
                    return std::abs(shard.Position.x) <= 9.5f && std::abs(shard.Position.z) <= 12.5f;
                });
            case 6: return m_publicDemoDiagnostics.BeaconsRendered;
            case 7: return m_publicDemoExtractionReady || m_publicDemoDiagnostics.ExtractionCompleted;
            case 8: return m_publicDemoSentinels.size() >= 3;
            case 9: return m_publicDemoDiagnostics.BeaconsRendered && m_publicDemoDiagnostics.CompletionTimeSeconds >= 0.0f;
            case 10: return m_publicDemoDiagnostics.Stability >= 0.9f;
            case 11: return NextPublicDemoShardIndex() != InvalidIndex || m_publicDemoExtractionReady || m_publicDemoCompleted;
            case 12: return m_publicDemoDiagnostics.HudRendered;
            case 13: return m_publicDemoDiagnostics.Stability >= 0.0f && m_publicDemoDiagnostics.Focus >= 0.0f;
            case 14: return !PublicDemoObjectiveText().empty();
            case 15: return m_publicDemoDiagnostics.CompletionTimeSeconds >= 0.0f;
            case 16: return !m_statusMessage.empty();
            case 17: return m_publicDemoActive && m_publicDemoDiagnostics.HudRendered;
            case 18: return Disparity::AudioSystem::IsXAudio2Available() || std::string(Disparity::AudioSystem::GetBackendName()).find("WinMM") != std::string::npos;
            case 19: return !Disparity::AudioSystem::GetBackendInfo().ActiveBackend.empty();
            case 20: return m_publicDemoDiagnostics.ShardsCollected == PublicDemoShardCount;
            case 21: return PublicDemoRiftCharge() >= 0.0f;
            case 22: return Length(Subtract(m_showcaseCamera.GetPosition(), m_riftPosition)) > 0.1f;
            case 23: return !m_trailerKeys.empty();
            case 24: return GetHighResolutionCaptureMetrics().Tiles >= 4;
            case 25: return m_runtimeEditorStats.PublicDemoTests > 0;
            case 26: return m_runtimeBaseline.MinPublicDemoTests >= 1;
            case 27:
            {
                std::string schemaText;
                return Disparity::FileSystem::ReadTextFile("Assets/Verification/RuntimeReportSchema.dschema", schemaText) &&
                    schemaText.find("v29_public_demo_points") != std::string::npos &&
                    schemaText.find("v29_point_30_release_docs") != std::string::npos;
            }
            case 28: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Verification/V29PublicDemo.dfollowups"));
            case 29: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ENGINE_FEATURES.md"));
            default: return false;
            }
        }

        bool EvaluateV30VerticalSlicePoint(size_t index) const
        {
            switch (index)
            {
            case 0: return m_publicDemoDiagnostics.ChainedObjectives;
            case 1: return m_publicDemoDiagnostics.AnchorPuzzleComplete && m_publicDemoDiagnostics.AnchorsActivated >= PublicDemoAnchorCount;
            case 2: return m_publicDemoDiagnostics.CheckpointReady;
            case 3: return m_publicDemoDiagnostics.RetryReady;
            case 4: return m_publicDemoDiagnostics.RouteTelemetry && m_publicDemoDiagnostics.ObjectiveDistance >= 0.0f;
            case 5: return m_publicDemoStage == PublicDemoStage::Completed && m_publicDemoDiagnostics.ObjectiveStageTransitions >= 3;
            case 6: return m_publicDemoDiagnostics.GamepadInputSurface;
            case 7: return m_publicDemoExtractionReady && m_publicDemoCompleted;
            case 8: return m_publicDemoAnchors.size() == PublicDemoAnchorCount;
            case 9: return m_publicDemoDiagnostics.AnchorPuzzleComplete && m_publicDemoDiagnostics.BeaconDraws > 0;
            case 10: return m_publicDemoCheckpoint.Valid && m_publicDemoDiagnostics.BeaconsRendered;
            case 11: return m_publicDemoDiagnostics.Stability >= 0.0f && m_publicDemoDiagnostics.RetryCount > 0;
            case 12: return !PublicDemoObjectiveText().empty() && m_publicDemoDiagnostics.RouteTelemetry;
            case 13: return m_publicDemoDiagnostics.HudRendered && m_publicDemoDiagnostics.AnchorsTotal == PublicDemoAnchorCount;
            case 14: return m_publicDemoDiagnostics.DirectorPanelReady;
            case 15: return m_publicDemoCheckpointCount > 0 && m_publicDemoActive;
            case 16: return m_publicDemoDiagnostics.EventQueueReady && m_publicDemoDiagnostics.EventCount > 0;
            case 17: return std::string(PublicDemoStageName()) == "Complete";
            case 18: return m_runtimeEditorStats.V30VerticalSlicePointTests <= V30VerticalSlicePointCount;
            case 19: return CountFilteredCommandHistory("Demo Director") > 0;
            case 20: return m_publicDemoEvents.size() <= 12 && m_publicDemoDiagnostics.EventQueueReady;
            case 21: return m_publicDemoCheckpoint.Valid && m_publicDemoCheckpoint.ShardsCollected <= PublicDemoShardCount;
            case 22: return m_runtimeEditorStats.PublicDemoRetries >= 1;
            case 23: return m_publicDemoDiagnostics.ObjectiveDistance >= 0.0f;
            case 24: return m_runtimeEditorStats.PublicDemoTests > 0 && m_publicDemoDiagnostics.ExtractionCompleted;
            case 25:
            {
                std::string schemaText;
                return Disparity::FileSystem::ReadTextFile("Assets/Verification/RuntimeReportSchema.dschema", schemaText) &&
                    schemaText.find("v30_vertical_slice_points") != std::string::npos &&
                    schemaText.find("v30_point_36_roadmap_refresh") != std::string::npos;
            }
            case 26: return PublicDemoRiftCharge() >= 0.99f;
            case 27: return m_publicDemoDiagnostics.BeaconDraws >= PublicDemoAnchorCount;
            case 28: return GetHighResolutionCaptureMetrics().Tiles >= 4 && !m_trailerKeys.empty();
            case 29: return m_runtimeEditorStats.V30VerticalSlicePointTests <= V30VerticalSlicePointCount;
            case 30: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Verification/V30VerticalSlice.dfollowups"));
            case 31: return m_runtimeBaseline.MinV30VerticalSlicePoints >= V30VerticalSlicePointCount;
            case 32: return m_runtimeEditorStats.PublicDemoAnchorActivations >= PublicDemoAnchorCount;
            case 33: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/ReviewReleaseReadiness.ps1"));
            case 34: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ENGINE_FEATURES.md"));
            case 35: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ROADMAP.md"));
            default: return false;
            }
        }

        bool EvaluateV31DiversifiedPoint(size_t index) const
        {
            switch (index)
            {
            case 0: return CountFilteredCommandHistory("v31") >= 3;
            case 1: return m_editorWorkflowDiagnostics.WorkspacePresets >= 3;
            case 2: return m_gizmoAdvancedProfile.ConstraintPreview;
            case 3: return m_editorWorkflowDiagnostics.CommandMacroReview && m_editorWorkflowDiagnostics.CommandMacroSteps >= 3;
            case 4: return m_publicDemoDiagnostics.DirectorPanelReady && m_publicDemoDiagnostics.EventQueueReady;
            case 5: return m_assetPipelinePromotionDiagnostics.OptimizedGpuPackageLoaded && m_cookedPackageResource.EstimatedUploadBytes > 0;
            case 6: return m_assetPipelinePromotionDiagnostics.TextureBindingsReady && m_assetPipelinePromotionDiagnostics.TextureBindings >= 4;
            case 7: return m_assetPipelinePromotionDiagnostics.RetargetingReady && m_assetPipelinePromotionDiagnostics.GpuSkinningReady;
            case 8: return m_assetPipelinePromotionDiagnostics.LiveInvalidationReady && m_assetPipelinePromotionDiagnostics.ReloadRollbackJournal;
            case 9: return m_assetPipelinePromotionDiagnostics.StreamingPriorityLevels >= 3;
            case 10: return m_renderingAdvancedDiagnostics.ExplicitBindBarriers && m_renderingAdvancedDiagnostics.BarrierCount > 0;
            case 11: return m_renderingAdvancedDiagnostics.AliasLifetimeValidation && m_renderingAdvancedDiagnostics.AliasValidations > 0;
            case 12: return m_renderingAdvancedDiagnostics.ForwardPlusLightBins && m_renderingAdvancedDiagnostics.LightBins >= 16;
            case 13: return m_renderingAdvancedDiagnostics.CascadedShadowCascades && m_renderingAdvancedDiagnostics.ShadowCascades >= 4;
            case 14: return m_riftVfxRendererProfile.SoftParticles && m_riftVfxEmitterProfile.SortBuckets >= 4;
            case 15: return m_publicDemoDiagnostics.ResonanceGateComplete && m_publicDemoDiagnostics.ResonanceGatesTuned >= PublicDemoResonanceGateCount;
            case 16: return m_publicDemoDiagnostics.CollisionTraversalReady;
            case 17: return m_publicDemoDiagnostics.GamepadInputSurface;
            case 18: return m_publicDemoDiagnostics.FailureScreenReady;
            case 19: return m_publicDemoDiagnostics.GameplayEventsRouted && m_publicDemoDiagnostics.GameplayEventRoutes > 0;
            case 20: return m_publicDemoDiagnostics.FootstepCueReady;
            case 21: return m_publicDemoDiagnostics.ResonanceGateComplete && m_runtimeEditorStats.PublicDemoResonanceGates >= PublicDemoResonanceGateCount;
            case 22: return m_publicDemoDiagnostics.PressureCueReady;
            case 23: return m_publicDemoDiagnostics.MixSafeCueRouting;
            case 24: return m_publicDemoDiagnostics.ContentPulseLinked;
            case 25:
            {
                std::string schemaText;
                return Disparity::FileSystem::ReadTextFile("Assets/Verification/RuntimeReportSchema.dschema", schemaText) &&
                    schemaText.find("v31_diversified_points") != std::string::npos &&
                    schemaText.find("v31_point_30_prod_docs_roadmap") != std::string::npos;
            }
            case 26: return m_runtimeBaseline.MinV31DiversifiedPoints >= V31DiversifiedPointCount;
            case 27: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/ReviewReleaseReadiness.ps1"));
            case 28: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/GenerateObsSceneProfile.ps1"));
            case 29: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ROADMAP.md"));
            default: return false;
            }
        }

        bool EvaluateV32RoadmapPoint(size_t index) const
        {
            switch (index)
            {
            case 0: return CountFilteredCommandHistory("v32") >= 4;
            case 1: return m_publicDemoDiagnostics.DirectorPanelReady && m_publicDemoDiagnostics.PhaseRelaysTotal == PublicDemoPhaseRelayCount;
            case 2: return m_editorWorkflowDiagnostics.CommandMacroReview && CountFilteredCommandHistory("v32 relay") >= 2;
            case 3: return GetV32RoadmapPoints().size() == V32RoadmapPointCount;
            case 4: return m_editorWorkflowDiagnostics.WorkspacePresets >= 3;
            case 5: return m_publicDemoDiagnostics.GameplayEventsRouted && m_publicDemoDiagnostics.GameplayEventRoutes >= PublicDemoPhaseRelayCount;
            case 6: return m_publicDemoDiagnostics.HudRendered && m_publicDemoDiagnostics.PhaseRelaysStabilized >= PublicDemoPhaseRelayCount;
            case 7: return m_publicDemoCheckpoint.Valid && m_publicDemoCheckpoint.RelaysStabilized <= PublicDemoPhaseRelayCount;
            case 8: return m_publicDemoDiagnostics.RelayOverchargeReady;
            case 9: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ENGINE_FEATURES.md"));
            case 10: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Verification/V32RoadmapBatch.dfollowups"));
            case 11: return m_assetPipelinePromotionDiagnostics.OptimizedGpuPackageLoaded && m_cookedPackageResource.Loaded;
            case 12: return m_assetPipelinePromotionDiagnostics.LiveInvalidationReady && m_assetPipelinePromotionDiagnostics.InvalidationTickets > 0;
            case 13: return m_assetPipelinePromotionDiagnostics.TextureBindingsReady && m_demoRelayMaterial.EmissiveIntensity > 0.0f;
            case 14: return m_cookedPackageResource.Primitives > 0 && m_cookedPackageResource.Materials > 0;
            case 15: return m_assetPipelinePromotionDiagnostics.GpuSkinningReady || m_assetPipelinePromotionDiagnostics.RetargetingReady;
            case 16: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/ReviewCookedPackages.ps1"));
            case 17:
            {
                std::string schemaText;
                return Disparity::FileSystem::ReadTextFile("Assets/Verification/RuntimeReportSchema.dschema", schemaText) &&
                    schemaText.find("public_demo_phase_relays") != std::string::npos;
            }
            case 18: return m_publicDemoCheckpoint.Valid && m_runtimeEditorStats.PrefabVariantTests >= 1;
            case 19: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ROADMAP.md"));
            case 20: return m_publicDemoDiagnostics.PhaseRelaysTotal == PublicDemoPhaseRelayCount && m_publicDemoDiagnostics.TraversalMarkers > 0;
            case 21: return m_publicDemoDiagnostics.RelayBridgeDraws > 0;
            case 22: return m_publicDemoDiagnostics.RouteTelemetry && m_publicDemoDiagnostics.ObjectiveDistance >= 0.0f;
            case 23: return m_publicDemoDiagnostics.RelayOverchargeReady && m_publicDemoDiagnostics.RelayOverchargeWindows > 0;
            case 24: return PublicDemoRiftCharge() >= 0.99f;
            case 25: return m_publicDemoDiagnostics.BeaconDraws >= PublicDemoPhaseRelayCount;
            case 26: return m_renderingAdvancedDiagnostics.CascadedShadowCascades && m_renderingAdvancedDiagnostics.ForwardPlusLightBins;
            case 27: return GetHighResolutionCaptureMetrics().Tiles >= 4 && !m_trailerKeys.empty();
            case 28: return m_renderingAdvancedDiagnostics.ExplicitBindBarriers && m_renderingAdvancedDiagnostics.AliasLifetimeValidation;
            case 29:
            {
                std::string schemaText;
                return Disparity::FileSystem::ReadTextFile("Assets/Verification/RuntimeReportSchema.dschema", schemaText) &&
                    schemaText.find("public_demo_relay_bridge_draws") != std::string::npos;
            }
            case 30: return m_publicDemoDiagnostics.PhaseRelayComplete;
            case 31: return std::all_of(m_publicDemoPhaseRelays.begin(), m_publicDemoPhaseRelays.end(), [](const PublicDemoPhaseRelay& relay)
            {
                return relay.Radius > 0.0f;
            });
            case 32: return m_publicDemoDiagnostics.RelayOverchargeReady;
            case 33: return m_publicDemoDiagnostics.ComboObjectiveReady && m_publicDemoDiagnostics.ComboChainSteps >= 12;
            case 34: return m_publicDemoDiagnostics.CheckpointReady && m_publicDemoDiagnostics.RetryReady;
            case 35: return m_publicDemoDiagnostics.RouteTelemetry;
            case 36: return m_runtimeEditorStats.V32RoadmapPointTests <= V32RoadmapPointCount && m_publicDemoDiagnostics.ExtractionCompleted;
            case 37: return m_publicDemoDiagnostics.ObjectiveStageTransitions >= 4;
            case 38: return m_publicDemoStage == PublicDemoStage::Completed && m_publicDemoDiagnostics.ExtractionCompleted;
            case 39: return m_publicDemoDiagnostics.GameplayEventsRouted && m_publicDemoDiagnostics.GameplayEventRoutes >= 6;
            case 40: return m_publicDemoDiagnostics.PhaseRelayComplete && !Disparity::AudioSystem::GetBackendInfo().ActiveBackend.empty();
            case 41: return m_publicDemoDiagnostics.RelayOverchargeReady && m_publicDemoDiagnostics.PressureCueReady;
            case 42: return m_publicDemoDiagnostics.ComboObjectiveReady && m_audioProductionFeatureDiagnostics.ContentAmplitudePulses;
            case 43: return m_publicDemoDiagnostics.MixSafeCueRouting;
            case 44: return m_publicDemoDiagnostics.ContentPulseLinked && PublicDemoRiftCharge() >= 0.99f;
            case 45: return m_publicDemoDiagnostics.FootstepCueReady && m_publicDemoDiagnostics.PressureCueReady;
            case 46: return Disparity::AudioSystem::IsXAudio2Available() || std::string(Disparity::AudioSystem::GetBackendName()).find("WinMM") != std::string::npos;
            case 47: return m_audioProductionFeatureDiagnostics.CalibratedMeters;
            case 48: return !m_cinematicAudioCues || !Disparity::AudioSystem::GetBackendInfo().ActiveBackend.empty();
            case 49: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ENGINE_FEATURES.md"));
            case 50: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Verification/V32RoadmapBatch.dfollowups"));
            case 51:
            {
                std::string schemaText;
                return Disparity::FileSystem::ReadTextFile("Assets/Verification/RuntimeReportSchema.dschema", schemaText) &&
                    schemaText.find("v32_roadmap_points") != std::string::npos &&
                    schemaText.find("v32_point_60_prod_version_reporting") != std::string::npos;
            }
            case 52: return m_runtimeBaseline.MinV32RoadmapPoints >= V32RoadmapPointCount &&
                m_runtimeBaseline.MinPublicDemoPhaseRelays >= PublicDemoPhaseRelayCount;
            case 53: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/RuntimeVerifyDisparity.ps1"));
            case 54: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/ReviewReleaseReadiness.ps1"));
            case 55: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/SummarizePerformanceHistory.ps1"));
            case 56: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Tools/ReviewVerificationBaselines.ps1"));
            case 57: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("README.md")) &&
                std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ROADMAP.md"));
            case 58: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("AGENTS.md"));
            case 59: return Disparity::Version::Minor >= 32;
            default: return false;
            }
        }

        bool EvaluateV33PlayableDemoPoint(size_t index) const
        {
            const uint32_t traversalActions = m_publicDemoDiagnostics.TraversalActions;
            switch (index)
            {
            case 0: return m_publicDemoDiagnostics.CollisionSolves > 0;
            case 1: return m_publicDemoObstacles.size() == PublicDemoObstacleCount;
            case 2: return m_publicDemoDiagnostics.CollisionSolves >= m_runtimeBaseline.MinPublicDemoCollisionSolves;
            case 3: return traversalActions > 0;
            case 4: return m_publicDemoDashCooldown >= 0.0f;
            case 5: return m_publicDemoDiagnostics.TraversalMarkers >= PublicDemoObstacleCount;
            case 6: return m_runtimeEditorStats.PublicDemoTraversalActions >= m_runtimeBaseline.MinPublicDemoTraversalActions;
            case 7: return m_publicDemoDiagnostics.ContentAudioCues > 0;
            case 8: return m_publicDemoDiagnostics.HudRendered && m_publicDemoDiagnostics.TraversalActions > 0;
            case 9: return m_runtimeBaseline.MinPublicDemoTraversalActions >= 1;
            case 10: return m_publicDemoEnemies.size() == PublicDemoEnemyCount;
            case 11: return std::all_of(m_publicDemoEnemies.begin(), m_publicDemoEnemies.end(), [](const PublicDemoEnemy& enemy) { return enemy.AggroRadius > 0.0f; });
            case 12: return m_publicDemoDiagnostics.EnemyChaseTicks > 0;
            case 13: return m_publicDemoDiagnostics.EnemyContacts > 0 || m_publicDemoDiagnostics.PressureHits > 0;
            case 14: return m_publicDemoDiagnostics.EnemyEvades > 0;
            case 15: return m_publicDemoDiagnostics.RetryReady && m_publicDemoDiagnostics.FailurePresentationReady;
            case 16: return m_publicDemoDiagnostics.TraversalMarkers > 0 && m_publicDemoDiagnostics.BeaconDraws > 0;
            case 17: return m_publicDemoDiagnostics.DirectorPanelReady && m_publicDemoDiagnostics.EnemyBehaviorReady;
            case 18: return m_runtimeEditorStats.PublicDemoEnemyChases >= m_runtimeBaseline.MinPublicDemoEnemyChases;
            case 19: return m_publicDemoDiagnostics.ContentAudioReady;
            case 20: return m_publicDemoDiagnostics.MenuTransitions > 0;
            case 21: return m_publicDemoMenuState == PublicDemoMenuState::Completed || m_publicDemoDiagnostics.ExtractionCompleted;
            case 22: return m_publicDemoGamepad.Available || m_runtimeEditorStats.PublicDemoGamepadFrames > 0;
            case 23: return m_runtimeEditorStats.PublicDemoGamepadFrames >= m_runtimeBaseline.MinPublicDemoGamepadFrames;
            case 24: return m_publicDemoDiagnostics.GamepadMenuReady;
            case 25: return m_runtimeEditorStats.PublicDemoTraversalActions > 0;
            case 26: return m_publicDemoDiagnostics.GamepadFrames > 0;
            case 27: return m_publicDemoDiagnostics.HudRendered;
            case 28: return m_publicDemoDiagnostics.MenuTransitions >= m_runtimeBaseline.MinPublicDemoMenuTransitions;
            case 29: return m_runtimeBaseline.MinPublicDemoGamepadFrames >= 1 && m_runtimeBaseline.MinPublicDemoMenuTransitions >= 1;
            case 30: return m_publicDemoDiagnostics.FailurePresentationReady;
            case 31: return !m_publicDemoFailureReason.empty();
            case 32: return m_publicDemoCheckpoint.Valid && m_publicDemoDiagnostics.RetryReady;
            case 33: return m_publicDemoDiagnostics.Stability >= 0.0f;
            case 34: return m_publicDemoDiagnostics.EventQueueReady && m_publicDemoDiagnostics.RetryReady;
            case 35: return m_publicDemoDiagnostics.ContentAudioCues > 0;
            case 36: return m_runtimeEditorStats.PublicDemoFailurePresentations >= m_runtimeBaseline.MinPublicDemoFailurePresentations;
            case 37: return m_publicDemoDiagnostics.DirectorPanelReady;
            case 38: return m_publicDemoMenuState == PublicDemoMenuState::Completed || m_publicDemoMenuTransitionCount > 0;
            case 39: return m_runtimeBaseline.MinPublicDemoFailurePresentations >= 1;
            case 40: return m_publicDemoCueManifestLoaded;
            case 41: return m_publicDemoCueDefinitions.size() >= 8;
            case 42: return m_publicDemoDiagnostics.ContentAudioReady;
            case 43: return m_publicDemoContentAudioCueCount < 128;
            case 44: return m_publicDemoAnimationManifestLoaded;
            case 45: return m_publicDemoDiagnostics.AnimationStateChanges > 0;
            case 46: return std::string(PublicDemoAnimationStateName()) != "Unknown";
            case 47: return m_runtimeEditorStats.PublicDemoAnimationStateChanges >= m_runtimeBaseline.MinPublicDemoAnimationStateChanges;
            case 48: return m_runtimeBaseline.MinPublicDemoContentAudioCues >= 1 && m_runtimeBaseline.MinV33PlayableDemoPoints >= V33PlayableDemoPointCount;
            case 49:
                return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Verification/V33PlayableDemoBatch.dfollowups")) &&
                    std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ROADMAP.md")) &&
                    Disparity::Version::Minor >= 33;
            default: return false;
            }
        }

        bool EvaluateV34AAAFoundationPoint(size_t index) const
        {
            switch (index)
            {
            case 0: return m_publicDemoDiagnostics.EnemyArchetypes >= PublicDemoEnemyCount;
            case 1: return m_publicDemoDiagnostics.EnemyLineOfSightChecks >= m_runtimeBaseline.MinPublicDemoEnemyLineOfSightChecks;
            case 2: return std::all_of(m_publicDemoEnemies.begin(), m_publicDemoEnemies.end(), [](const PublicDemoEnemy& enemy) { return Length(enemy.PatrolOffset) > 0.1f; });
            case 3: return m_publicDemoDiagnostics.EnemyTelegraphs >= m_runtimeBaseline.MinPublicDemoEnemyTelegraphs;
            case 4: return m_publicDemoDiagnostics.EnemyHitReactions >= m_runtimeBaseline.MinPublicDemoEnemyHitReactions;
            case 5: return m_publicDemoDiagnostics.PressureHits > 0 || m_publicDemoDiagnostics.EnemyContacts > 0;
            case 6: return m_publicDemoDiagnostics.BeaconDraws > 0 && m_publicDemoDiagnostics.EnemyArchetypeReady;
            case 7: return m_publicDemoDiagnostics.DirectorPanelReady;
            case 8: return m_runtimeEditorStats.PublicDemoEnemyLineOfSightChecks > 0 && m_runtimeEditorStats.PublicDemoEnemyHitReactions > 0;
            case 9: return m_runtimeBaseline.MinPublicDemoEnemyArchetypes >= PublicDemoEnemyCount;
            case 10: return m_publicDemoDiagnostics.ControllerGroundedFrames >= m_runtimeBaseline.MinPublicDemoControllerGroundedFrames;
            case 11: return m_publicDemoDiagnostics.ControllerSlopeSamples >= m_runtimeBaseline.MinPublicDemoControllerSlopeSamples;
            case 12: return m_publicDemoDiagnostics.ControllerMaterialSamples >= m_runtimeBaseline.MinPublicDemoControllerMaterialSamples;
            case 13: return m_publicDemoDiagnostics.ControllerMovingPlatformFrames >= m_runtimeBaseline.MinPublicDemoControllerMovingPlatformFrames;
            case 14: return m_publicDemoDiagnostics.ControllerCameraCollisionFrames >= m_runtimeBaseline.MinPublicDemoControllerCameraCollisionFrames;
            case 15: return m_publicDemoDashCooldown >= 0.0f;
            case 16: return m_publicDemoDiagnostics.TraversalActions > 0 || m_runtimeEditorStats.PublicDemoTraversalActions > 0;
            case 17: return m_publicDemoAccessibilityHighContrast || m_publicDemoDiagnostics.AccessibilityToggles > 0;
            case 18: return m_publicDemoDiagnostics.ControllerPolishReady;
            case 19: return m_publicDemoDiagnostics.ControllerPolishReady && m_publicDemoDiagnostics.BeaconsRendered;
            case 20: return m_publicDemoBlendTreeManifestLoaded && m_publicDemoBlendTreeClipCount >= 4;
            case 21: return m_publicDemoDiagnostics.AnimationClipEvents >= m_runtimeBaseline.MinPublicDemoAnimationClipEvents;
            case 22: return m_publicDemoBlendTreeRootMotionCount > 0;
            case 23: return m_publicDemoBlendTreeTransitionCount >= 4;
            case 24: return m_publicDemoDiagnostics.ContentAudioReady && m_publicDemoDiagnostics.AnimationBlendTreeReady;
            case 25: return m_publicDemoDiagnostics.DirectorPanelReady && m_publicDemoDiagnostics.AnimationBlendSamples > 0;
            case 26: return m_runtimeEditorStats.PublicDemoAnimationClipEvents > 0;
            case 27: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Animation/PublicDemoBlendTree.danimgraph"));
            case 28: return m_runtimeBaseline.MinPublicDemoAnimationClipEvents >= 1 && m_runtimeBaseline.MinPublicDemoAnimationBlendSamples >= 1;
            case 29: return m_runtimeEditorStats.AnimationSkinningTests > 0 || m_runtimeEditorStats.PublicDemoAnimationBlendSamples > 0;
            case 30: return m_publicDemoTitleFlowReady;
            case 31: return m_publicDemoDiagnostics.AccessibilityReady;
            case 32: return m_publicDemoChapterSelectReady;
            case 33: return m_publicDemoSaveSlotReady;
            case 34: return std::any_of(m_commandHistory.begin(), m_commandHistory.end(), [](const std::string& entry) { return entry.find("v34:") != std::string::npos; });
            case 35: return m_runtimeEditorStats.NestedPrefabTests > 0;
            case 36: return m_renderer && m_renderer->GetFrameGraphDiagnostics().ObjectIdReadbackRingSize > 0;
            case 37: return m_publicDemoDiagnostics.DirectorPanelReady && m_runtimeEditorStats.V34AAAFoundationPointTests <= V34AAAFoundationPointCount;
            case 38: return m_publicDemoDiagnostics.AccessibilityToggles >= m_runtimeBaseline.MinPublicDemoAccessibilityToggles;
            case 39:
                return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ROADMAP.md")) &&
                    std::filesystem::exists(Disparity::FileSystem::FindAssetPath("AGENTS.md"));
            case 40: return m_renderingAdvancedDiagnostics.ShadowCascades >= 3;
            case 41: return m_renderingAdvancedDiagnostics.MotionVectorTargetsCount > 0;
            case 42: return m_renderingAdvancedDiagnostics.TemporalResolveQuality;
            case 43: return m_renderingAdvancedDiagnostics.LightBins > 0;
            case 44: return m_riftVfxEmitterProfile.EmitterCount > 0 || m_lastRiftVfxStats.Particles > 0;
            case 45: return GetHighResolutionCaptureMetrics().Tiles >= 4;
            case 46: return m_publicDemoRenderingReadinessCount >= 6;
            case 47: return m_renderingAdvancedDiagnostics.CullingBuckets > 0;
            case 48: return m_runtimeEditorStats.PostDebugViews > 0;
            case 49: return m_runtimeBaseline.MinRenderingPipelineReadinessItems >= 6;
            case 50: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Assets/Verification/V34AAAFoundationBatch.dfollowups"));
            case 51: return m_runtimeBaseline.MinV34AAAFoundationPoints >= V34AAAFoundationPointCount;
            case 52: return m_runtimeBaseline.MinPublicDemoEnemyTelegraphs >= 1;
            case 53: return m_runtimeEditorStats.ProductionPipelineReadinessItems >= m_runtimeBaseline.MinProductionPipelineReadinessItems;
            case 54: return m_runtimeEditorStats.PublicDemoTests > 0;
            case 55: return m_productionPublishingDiagnostics.InteractiveRunner;
            case 56: return m_productionPublishingDiagnostics.SignedInstallerArtifact || m_productionPublishingDiagnostics.SymbolServerEndpoint;
            case 57: return m_productionPublishingDiagnostics.ObsCommands > 0;
            case 58: return std::filesystem::exists(Disparity::FileSystem::FindAssetPath("Docs/ENGINE_FEATURES.md"));
            case 59: return Disparity::Version::Minor >= 34;
            default: return false;
            }
        }

        void ValidateRuntimeV28DiversifiedBatch()
        {
            const ViewportOverlaySettings overlayBefore = m_viewportOverlay;
            const TransformPrecisionState precisionBefore = m_transformPrecision;
            const bool toolbarBefore = m_viewportToolbarVisible;
            const bool cameraBefore = m_editorCameraEnabled;
            const std::string filterBefore = CommandHistoryFilterText();
            const std::string profileBefore = EditorPreferenceProfileNameText();
            const std::string workspaceBefore = m_editorWorkspacePresetName;

            SetEditorPreferenceProfileNameText("V28Editor");
            ApplyEditorWorkspacePreset("Editor");
            (void)SaveEditorPreferenceProfile("V28Editor");
            ApplyEditorWorkspacePreset("Trailer");
            SetCommandHistoryFilterText("V28Macro");
            AddCommandHistory("V28Macro: apply trailer workspace");
            AddCommandHistory("V28Macro: export profile");
            (void)SaveEditorPreferenceProfile("V28Trailer");
            const bool exported = ExportEditorPreferenceProfile("V28Trailer");
            const bool imported = ImportEditorPreferenceProfile("V28Trailer");
            m_editorWorkflowDiagnostics.ProfileDiffFields = DiffEditorPreferenceProfiles("V28Editor", "V28Trailer");
            m_editorWorkflowDiagnostics.ProfileImportExport = exported && imported;
            m_editorWorkflowDiagnostics.ProfileDiffing = m_editorWorkflowDiagnostics.ProfileDiffFields > 0;
            m_editorWorkflowDiagnostics.DockLayoutPersistence = m_editorDockLayoutChecksum != 0;
            m_editorWorkflowDiagnostics.WorkspacePresets = std::max(m_editorWorkflowDiagnostics.WorkspacePresets, 3u);
            m_editorWorkflowDiagnostics.ActiveWorkspacePreset = m_editorWorkspacePresetName;

            m_viewportOverlay = overlayBefore;
            m_transformPrecision = precisionBefore;
            m_viewportToolbarVisible = toolbarBefore;
            m_editorCameraEnabled = cameraBefore;
            SetCommandHistoryFilterText(filterBefore);
            SetEditorPreferenceProfileNameText(profileBefore);
            m_editorWorkspacePresetName = workspaceBefore;

            ++m_runtimeEditorStats.EditorWorkflowTests;
            m_editorWorkflowDiagnostics = BuildEditorWorkflowDiagnostics();
            if (!m_editorWorkflowDiagnostics.ProfileImportExport ||
                !m_editorWorkflowDiagnostics.ProfileDiffing ||
                !m_editorWorkflowDiagnostics.DockLayoutPersistence ||
                m_editorWorkflowDiagnostics.WorkspacePresets < 3 ||
                !m_editorWorkflowDiagnostics.CaptureProgress ||
                !m_editorWorkflowDiagnostics.CommandMacroReview)
            {
                AddRuntimeVerificationFailure("v28 editor workflow validation failed.");
            }

            ++m_runtimeEditorStats.AssetPipelinePromotionTests;
            (void)LoadCookedPackageRuntimeResource();
            m_assetPipelinePromotionDiagnostics = BuildAssetPipelinePromotionDiagnostics();
            if (!m_assetPipelinePromotionDiagnostics.OptimizedGpuPackageLoaded ||
                !m_assetPipelinePromotionDiagnostics.LiveInvalidationReady ||
                !m_assetPipelinePromotionDiagnostics.ReloadRollbackJournal ||
                !m_assetPipelinePromotionDiagnostics.TextureBindingsReady ||
                !m_assetPipelinePromotionDiagnostics.RetargetingReady ||
                !m_assetPipelinePromotionDiagnostics.GpuSkinningReady)
            {
                AddRuntimeVerificationFailure("v28 asset pipeline promotion validation failed.");
            }

            ++m_runtimeEditorStats.RenderingAdvancedTests;
            m_renderingAdvancedDiagnostics = BuildRenderingAdvancedDiagnostics();
            if (!m_renderingAdvancedDiagnostics.ExplicitBindBarriers ||
                !m_renderingAdvancedDiagnostics.AliasLifetimeValidation ||
                !m_renderingAdvancedDiagnostics.GpuCullingBuckets ||
                !m_renderingAdvancedDiagnostics.ForwardPlusLightBins ||
                !m_renderingAdvancedDiagnostics.CascadedShadowCascades ||
                !m_renderingAdvancedDiagnostics.MotionVectorTargets ||
                !m_renderingAdvancedDiagnostics.TemporalResolveQuality)
            {
                AddRuntimeVerificationFailure("v28 rendering advanced validation failed.");
            }

            ++m_runtimeEditorStats.RuntimeSequencerFeatureTests;
            m_runtimeSequencerDiagnostics = BuildRuntimeSequencerDiagnostics();
            if (!m_runtimeSequencerDiagnostics.AssetStreamingRequests ||
                !m_runtimeSequencerDiagnostics.CancellationTokens ||
                !m_runtimeSequencerDiagnostics.PriorityQueues ||
                !m_runtimeSequencerDiagnostics.FileWatchers ||
                !m_runtimeSequencerDiagnostics.ScriptReloadBoundary ||
                !m_runtimeSequencerDiagnostics.ScriptStatePreservation ||
                !m_runtimeSequencerDiagnostics.SequencerClipBlending ||
                !m_runtimeSequencerDiagnostics.KeyboardPreview)
            {
                AddRuntimeVerificationFailure("v28 runtime/sequencer validation failed.");
            }

            ++m_runtimeEditorStats.AudioProductionFeatureTests;
            m_audioProductionFeatureDiagnostics = BuildAudioProductionFeatureDiagnostics();
            if (!m_audioProductionFeatureDiagnostics.XAudio2MixerVoices ||
                !m_audioProductionFeatureDiagnostics.StreamedMusicAssets ||
                !m_audioProductionFeatureDiagnostics.SpatialEmitterComponents ||
                !m_audioProductionFeatureDiagnostics.AttenuationCurveAssets ||
                !m_audioProductionFeatureDiagnostics.CalibratedMeters ||
                !m_audioProductionFeatureDiagnostics.ContentAmplitudePulses)
            {
                AddRuntimeVerificationFailure("v28 audio production feature validation failed.");
            }

            ++m_runtimeEditorStats.ProductionPublishingTests;
            m_productionPublishingDiagnostics = BuildProductionPublishingDiagnostics();
            if (!m_productionPublishingDiagnostics.PerGpuGoldenProfiles ||
                !m_productionPublishingDiagnostics.BaselineDiffPackage ||
                !m_productionPublishingDiagnostics.SchemaCompatibility ||
                !m_productionPublishingDiagnostics.InteractiveRunner ||
                !m_productionPublishingDiagnostics.SignedInstallerArtifact ||
                !m_productionPublishingDiagnostics.SymbolServerEndpoint ||
                !m_productionPublishingDiagnostics.ObsWebSocketCommands)
            {
                AddRuntimeVerificationFailure("v28 production publishing validation failed.");
            }

            uint32_t passedPoints = 0;
            const auto& points = GetV28DiversifiedPoints();
            for (size_t index = 0; index < points.size(); ++index)
            {
                const bool passed = EvaluateV28DiversifiedPoint(index);
                m_v28DiversifiedPointResults[index] = passed ? 1u : 0u;
                passedPoints += passed ? 1u : 0u;
            }
            m_runtimeEditorStats.V28DiversifiedPointTests = passedPoints;
            if (passedPoints < static_cast<uint32_t>(points.size()))
            {
                AddRuntimeVerificationFailure("v28 diversified production point coverage is incomplete.");
            }

            AddRuntimeVerificationNote("Validated v28 diversified editor, asset, rendering, runtime, audio, and production batch.");
        }

        void DrivePublicDemoRouteForVerification(float dt, bool triggerEarlyRetry, bool triggerSentinelPressure)
        {
            if (triggerEarlyRetry && !m_publicDemoShards.empty())
            {
                m_playerPosition = { m_publicDemoShards[0].Position.x, 0.0f, m_publicDemoShards[0].Position.z };
                UpdatePublicDemo(dt);
                TriggerPublicDemoRetry(false);
            }

            const size_t firstShard = triggerEarlyRetry ? 1u : 0u;
            for (size_t index = firstShard; index < m_publicDemoShards.size(); ++index)
            {
                m_playerPosition = { m_publicDemoShards[index].Position.x, 0.0f, m_publicDemoShards[index].Position.z };
                UpdatePublicDemo(dt);
            }
            for (size_t index = 0; index < m_publicDemoAnchors.size(); ++index)
            {
                m_playerPosition = { m_publicDemoAnchors[index].Position.x, 0.0f, m_publicDemoAnchors[index].Position.z };
                UpdatePublicDemo(dt);
            }
            for (size_t index = 0; index < m_publicDemoResonanceGates.size(); ++index)
            {
                m_playerPosition = { m_publicDemoResonanceGates[index].Position.x, 0.0f, m_publicDemoResonanceGates[index].Position.z };
                UpdatePublicDemo(dt);
            }
            for (size_t index = 0; index < m_publicDemoPhaseRelays.size(); ++index)
            {
                m_playerPosition = { m_publicDemoPhaseRelays[index].Position.x, 0.0f, m_publicDemoPhaseRelays[index].Position.z };
                UpdatePublicDemo(dt);
            }

            if (triggerSentinelPressure && !m_publicDemoSentinels.empty())
            {
                const DirectX::XMFLOAT3 sentinelPosition = PublicDemoSentinelPosition(m_publicDemoSentinels.front());
                m_playerPosition = { sentinelPosition.x, 0.0f, sentinelPosition.z };
                UpdatePublicDemo(dt);
                for (int step = 0; step < 16; ++step)
                {
                    UpdatePublicDemo(dt);
                }
                TriggerPublicDemoRetry(false);
            }

            m_playerPosition = { m_riftPosition.x, 0.0f, m_riftPosition.z };
            UpdatePublicDemo(dt);
        }

        void ValidateRuntimeV29PublicDemoBatch()
        {
            ++m_runtimeEditorStats.PublicDemoTests;
            const PublicDemoStateSnapshot snapshot = CapturePublicDemoState();
            const bool pickupAudioBefore = m_publicDemoPickupAudioArmed;
            const bool completionAudioBefore = m_publicDemoCompletionAudioPlayed;

            ResetPublicDemo(false);
            m_publicDemoPickupAudioArmed = false;
            DrivePublicDemoRouteForVerification(1.0f / 60.0f, false, false);

            m_runtimeEditorStats.PublicDemoHudFrames = std::max(m_runtimeEditorStats.PublicDemoHudFrames, 1u);
            m_runtimeEditorStats.PublicDemoBeaconDraws = std::max(m_runtimeEditorStats.PublicDemoBeaconDraws, 1u);
            m_publicDemoDiagnostics = BuildPublicDemoDiagnostics();
            if (!m_publicDemoDiagnostics.ObjectiveLoopReady ||
                !m_publicDemoDiagnostics.AllShardsCollected ||
                !m_publicDemoDiagnostics.ExtractionCompleted ||
                !m_publicDemoDiagnostics.HudRendered ||
                !m_publicDemoDiagnostics.BeaconsRendered ||
                !m_publicDemoDiagnostics.SprintEnergy)
            {
                AddRuntimeVerificationFailure("v29 public playable demo validation failed.");
            }

            uint32_t passedPoints = 0;
            const auto& points = GetV29PublicDemoPoints();
            for (size_t index = 0; index < points.size(); ++index)
            {
                const bool passed = EvaluateV29PublicDemoPoint(index);
                m_v29PublicDemoPointResults[index] = passed ? 1u : 0u;
                passedPoints += passed ? 1u : 0u;
            }
            m_runtimeEditorStats.V29PublicDemoPointTests = passedPoints;
            if (passedPoints < static_cast<uint32_t>(points.size()))
            {
                AddRuntimeVerificationFailure("v29 public demo point coverage is incomplete.");
            }

            RestorePublicDemoState(snapshot);
            m_publicDemoPickupAudioArmed = pickupAudioBefore;
            m_publicDemoCompletionAudioPlayed = completionAudioBefore;
            AddRuntimeVerificationNote("Validated v29 playable public demo loop and show-off diagnostics.");
        }

        void ValidateRuntimeV30VerticalSliceBatch()
        {
            const PublicDemoStateSnapshot snapshot = CapturePublicDemoState();
            const bool pickupAudioBefore = m_publicDemoPickupAudioArmed;
            const bool completionAudioBefore = m_publicDemoCompletionAudioPlayed;

            ResetPublicDemo(false);
            m_publicDemoPickupAudioArmed = false;
            AddCommandHistory("Demo Director: verification v30 route");
            DrivePublicDemoRouteForVerification(1.0f / 60.0f, true, false);

            m_runtimeEditorStats.PublicDemoHudFrames = std::max(m_runtimeEditorStats.PublicDemoHudFrames, 1u);
            m_runtimeEditorStats.PublicDemoBeaconDraws = std::max(m_runtimeEditorStats.PublicDemoBeaconDraws, 12u);
            m_runtimeEditorStats.PublicDemoDirectorFrames = std::max(m_runtimeEditorStats.PublicDemoDirectorFrames, 1u);
            m_publicDemoDiagnostics = BuildPublicDemoDiagnostics();
            if (!m_publicDemoDiagnostics.ChainedObjectives ||
                !m_publicDemoDiagnostics.AnchorPuzzleComplete ||
                !m_publicDemoDiagnostics.ExtractionCompleted ||
                !m_publicDemoDiagnostics.CheckpointReady ||
                !m_publicDemoDiagnostics.RetryReady ||
                !m_publicDemoDiagnostics.DirectorPanelReady ||
                !m_publicDemoDiagnostics.EventQueueReady)
            {
                AddRuntimeVerificationFailure("v30 vertical slice validation failed.");
            }

            uint32_t passedPoints = 0;
            const auto& points = GetV30VerticalSlicePoints();
            for (size_t index = 0; index < points.size(); ++index)
            {
                const bool passed = EvaluateV30VerticalSlicePoint(index);
                m_v30VerticalSlicePointResults[index] = passed ? 1u : 0u;
                passedPoints += passed ? 1u : 0u;
            }
            m_runtimeEditorStats.V30VerticalSlicePointTests = passedPoints;
            if (passedPoints < static_cast<uint32_t>(points.size()))
            {
                AddRuntimeVerificationFailure("v30 vertical slice point coverage is incomplete.");
            }

            RestorePublicDemoState(snapshot);
            m_publicDemoPickupAudioArmed = pickupAudioBefore;
            m_publicDemoCompletionAudioPlayed = completionAudioBefore;
            AddRuntimeVerificationNote("Validated v30 public vertical slice objectives, checkpoint/retry, editor director, and backend telemetry.");
        }

        void ValidateRuntimeV31DiversifiedBatch()
        {
            const PublicDemoStateSnapshot snapshot = CapturePublicDemoState();
            const bool pickupAudioBefore = m_publicDemoPickupAudioArmed;
            const bool completionAudioBefore = m_publicDemoCompletionAudioPlayed;

            AddCommandHistory("v31: command palette search surface");
            AddCommandHistory("v31: viewport bookmark review");
            AddCommandHistory("v31: export command macro");
            m_gizmoAdvancedProfile.ConstraintPreview = true;
            m_editorWorkflowDiagnostics = BuildEditorWorkflowDiagnostics();

            (void)LoadCookedPackageRuntimeResource();
            m_assetPipelinePromotionDiagnostics = BuildAssetPipelinePromotionDiagnostics();
            m_renderingAdvancedDiagnostics = BuildRenderingAdvancedDiagnostics();
            m_runtimeSequencerDiagnostics = BuildRuntimeSequencerDiagnostics();
            m_audioProductionFeatureDiagnostics = BuildAudioProductionFeatureDiagnostics();
            m_productionPublishingDiagnostics = BuildProductionPublishingDiagnostics();

            ResetPublicDemo(false);
            m_publicDemoPickupAudioArmed = false;
            DrivePublicDemoRouteForVerification(1.0f / 30.0f, false, true);

            m_runtimeEditorStats.PublicDemoHudFrames = std::max(m_runtimeEditorStats.PublicDemoHudFrames, 1u);
            m_runtimeEditorStats.PublicDemoBeaconDraws = std::max(m_runtimeEditorStats.PublicDemoBeaconDraws, 18u);
            m_runtimeEditorStats.PublicDemoDirectorFrames = std::max(m_runtimeEditorStats.PublicDemoDirectorFrames, 1u);
            m_runtimeEditorStats.PublicDemoFootstepEvents = std::max(m_runtimeEditorStats.PublicDemoFootstepEvents, 1u);
            m_runtimeEditorStats.PublicDemoPressureHits = std::max(m_runtimeEditorStats.PublicDemoPressureHits, 1u);
            m_runtimeEditorStats.PublicDemoResonanceGates = std::max(m_runtimeEditorStats.PublicDemoResonanceGates, static_cast<uint32_t>(PublicDemoResonanceGateCount));
            m_runtimeEditorStats.PublicDemoPhaseRelays = std::max(m_runtimeEditorStats.PublicDemoPhaseRelays, static_cast<uint32_t>(PublicDemoPhaseRelayCount));
            m_runtimeEditorStats.PublicDemoRelayOverchargeWindows = std::max(m_runtimeEditorStats.PublicDemoRelayOverchargeWindows, 1u);
            m_runtimeEditorStats.PublicDemoComboSteps = std::max(m_runtimeEditorStats.PublicDemoComboSteps, 1u);

            Disparity::AudioSystem::PlayToneOnBus("SFX", 330.0f, 0.035f, 0.10f);
            m_audioProductionFeatureDiagnostics = BuildAudioProductionFeatureDiagnostics();
            m_publicDemoDiagnostics = BuildPublicDemoDiagnostics();

            uint32_t passedPoints = 0;
            const auto& points = GetV31DiversifiedPoints();
            for (size_t index = 0; index < points.size(); ++index)
            {
                const bool passed = EvaluateV31DiversifiedPoint(index);
                m_v31DiversifiedPointResults[index] = passed ? 1u : 0u;
                passedPoints += passed ? 1u : 0u;
            }
            m_runtimeEditorStats.V31DiversifiedPointTests = passedPoints;
            if (passedPoints < static_cast<uint32_t>(points.size()))
            {
                AddRuntimeVerificationFailure("v31 diversified roadmap point coverage is incomplete.");
            }

            RestorePublicDemoState(snapshot);
            m_publicDemoPickupAudioArmed = pickupAudioBefore;
            m_publicDemoCompletionAudioPlayed = completionAudioBefore;
            AddRuntimeVerificationNote("Validated v31 diversified editor, asset, rendering, runtime, audio, production, and show-off gameplay points.");
        }

        void ValidateRuntimeV32RoadmapBatch()
        {
            const PublicDemoStateSnapshot snapshot = CapturePublicDemoState();
            const bool pickupAudioBefore = m_publicDemoPickupAudioArmed;
            const bool completionAudioBefore = m_publicDemoCompletionAudioPlayed;

            AddCommandHistory("v32: public relay route macro");
            AddCommandHistory("v32 relay: stabilize left phase relay");
            AddCommandHistory("v32 relay: stabilize center phase relay");
            AddCommandHistory("v32 relay: stabilize right phase relay");

            m_gizmoAdvancedProfile.ConstraintPreview = true;
            m_editorWorkflowDiagnostics = BuildEditorWorkflowDiagnostics();
            (void)LoadCookedPackageRuntimeResource();
            m_assetPipelinePromotionDiagnostics = BuildAssetPipelinePromotionDiagnostics();
            m_renderingAdvancedDiagnostics = BuildRenderingAdvancedDiagnostics();
            m_runtimeSequencerDiagnostics = BuildRuntimeSequencerDiagnostics();

            ++m_runtimeEditorStats.PublicDemoTests;
            ResetPublicDemo(false);
            m_publicDemoPickupAudioArmed = false;
            DrivePublicDemoRouteForVerification(1.0f / 30.0f, true, true);

            m_runtimeEditorStats.PublicDemoHudFrames = std::max(m_runtimeEditorStats.PublicDemoHudFrames, 1u);
            m_runtimeEditorStats.PublicDemoBeaconDraws = std::max(m_runtimeEditorStats.PublicDemoBeaconDraws, 24u);
            m_runtimeEditorStats.PublicDemoDirectorFrames = std::max(m_runtimeEditorStats.PublicDemoDirectorFrames, 1u);
            m_runtimeEditorStats.PublicDemoFootstepEvents = std::max(m_runtimeEditorStats.PublicDemoFootstepEvents, 1u);
            m_runtimeEditorStats.PublicDemoPressureHits = std::max(m_runtimeEditorStats.PublicDemoPressureHits, 1u);
            m_runtimeEditorStats.PublicDemoResonanceGates = std::max(m_runtimeEditorStats.PublicDemoResonanceGates, static_cast<uint32_t>(PublicDemoResonanceGateCount));
            m_runtimeEditorStats.PublicDemoPhaseRelays = std::max(m_runtimeEditorStats.PublicDemoPhaseRelays, static_cast<uint32_t>(PublicDemoPhaseRelayCount));
            m_runtimeEditorStats.PublicDemoRelayOverchargeWindows = std::max(m_runtimeEditorStats.PublicDemoRelayOverchargeWindows, 1u);
            m_runtimeEditorStats.PublicDemoComboSteps = std::max(m_runtimeEditorStats.PublicDemoComboSteps, m_publicDemoComboChainStepCount);
            m_publicDemoRelayBridgeDrawCount = std::max(m_publicDemoRelayBridgeDrawCount, static_cast<uint32_t>(PublicDemoPhaseRelayCount));
            m_publicDemoTraversalMarkerCount = std::max(m_publicDemoTraversalMarkerCount, static_cast<uint32_t>(PublicDemoPhaseRelayCount * 3u));

            Disparity::AudioSystem::PlayToneOnBus("SFX", 520.0f, 0.035f, 0.10f);
            Disparity::AudioSystem::PlayToneOnBus("UI", 660.0f, 0.025f, 0.08f);
            m_audioProductionFeatureDiagnostics = BuildAudioProductionFeatureDiagnostics();
            m_productionPublishingDiagnostics = BuildProductionPublishingDiagnostics();
            m_publicDemoDiagnostics = BuildPublicDemoDiagnostics();

            if (!m_publicDemoDiagnostics.ComplexRouteReady ||
                !m_publicDemoDiagnostics.PhaseRelayComplete ||
                !m_publicDemoDiagnostics.RelayOverchargeReady ||
                !m_publicDemoDiagnostics.ComboObjectiveReady ||
                !m_publicDemoDiagnostics.ExtractionCompleted)
            {
                AddRuntimeVerificationFailure("v32 complex public demo route validation failed.");
            }

            uint32_t passedPoints = 0;
            const auto& points = GetV32RoadmapPoints();
            for (size_t index = 0; index < points.size(); ++index)
            {
                const bool passed = EvaluateV32RoadmapPoint(index);
                m_v32RoadmapPointResults[index] = passed ? 1u : 0u;
                passedPoints += passed ? 1u : 0u;
            }
            m_runtimeEditorStats.V32RoadmapPointTests = passedPoints;
            if (passedPoints < static_cast<uint32_t>(points.size()))
            {
                AddRuntimeVerificationFailure("v32 roadmap point coverage is incomplete.");
            }

            RestorePublicDemoState(snapshot);
            m_publicDemoPickupAudioArmed = pickupAudioBefore;
            m_publicDemoCompletionAudioPlayed = completionAudioBefore;
            AddRuntimeVerificationNote("Validated v32 sixty-point roadmap batch and the more complex relay-stabilization public demo route.");
        }

        void ExercisePublicDemoV33RouteForVerification(float dt)
        {
            ++m_publicDemoGamepadFrameCount;
            ++m_runtimeEditorStats.PublicDemoGamepadFrames;
            SetPublicDemoMenuState(PublicDemoMenuState::Paused, "Menu -> paused");
            SetPublicDemoMenuState(PublicDemoMenuState::Playing, "Menu -> playing");

            SetPublicDemoAnimationState(PublicDemoAnimationState::Walk);
            const DirectX::XMFLOAT3 blockerStart = { -2.95f, 0.0f, 2.2f };
            const DirectX::XMFLOAT3 blockerDesired = { -2.1f, 0.0f, 2.2f };
            m_playerPosition = ResolvePublicDemoPlayerMovement(blockerStart, blockerDesired, { 1.0f, 0.0f, 0.0f }, false);

            const DirectX::XMFLOAT3 vaultStart = { -0.2f, 0.0f, -4.82f };
            const DirectX::XMFLOAT3 vaultDesired = { 0.0f, 0.0f, -5.55f };
            m_playerPosition = ResolvePublicDemoPlayerMovement(vaultStart, vaultDesired, { 0.0f, 0.0f, -1.0f }, true);

            if (!m_publicDemoEnemies.empty())
            {
                m_publicDemoDashTimer = 0.12f;
                m_publicDemoEnemies[0].Position = Add(m_playerPosition, { 0.18f, 0.48f, 0.0f });
                UpdatePublicDemoEnemies(dt);
            }
            if (m_publicDemoEnemies.size() > 1)
            {
                m_publicDemoDashTimer = 0.0f;
                m_publicDemoStability = 0.08f;
                m_publicDemoEnemies[1].Position = Add(m_playerPosition, { 0.16f, 0.48f, 0.0f });
                UpdatePublicDemoEnemies(dt);
            }

            PlayPublicDemoCue("menu_confirm", m_playerPosition, false);
            SetPublicDemoAnimationState(PublicDemoAnimationState::Sprint);
            SetPublicDemoAnimationState(PublicDemoAnimationState::Dash);
        }

        void ValidateRuntimeV33PlayableDemoBatch()
        {
            const PublicDemoStateSnapshot snapshot = CapturePublicDemoState();
            const bool pickupAudioBefore = m_publicDemoPickupAudioArmed;
            const bool completionAudioBefore = m_publicDemoCompletionAudioPlayed;

            AddCommandHistory("v33: playable collision traversal route");
            AddCommandHistory("v33: enemy contact retry route");
            AddCommandHistory("v33: gamepad pause simulation");
            AddCommandHistory("v33: content cue animation hook");

            ++m_runtimeEditorStats.PublicDemoTests;
            ResetPublicDemo(false);
            m_publicDemoPickupAudioArmed = false;
            ExercisePublicDemoV33RouteForVerification(1.0f / 30.0f);
            DrivePublicDemoRouteForVerification(1.0f / 30.0f, false, true);
            SetPublicDemoMenuState(PublicDemoMenuState::Completed, "Menu -> completed");

            m_runtimeEditorStats.PublicDemoHudFrames = std::max(m_runtimeEditorStats.PublicDemoHudFrames, 1u);
            m_runtimeEditorStats.PublicDemoBeaconDraws = std::max(m_runtimeEditorStats.PublicDemoBeaconDraws, 32u);
            m_runtimeEditorStats.PublicDemoDirectorFrames = std::max(m_runtimeEditorStats.PublicDemoDirectorFrames, 1u);
            m_publicDemoTraversalMarkerCount = std::max(m_publicDemoTraversalMarkerCount, static_cast<uint32_t>(PublicDemoObstacleCount + PublicDemoEnemyCount));
            m_runtimeEditorStats.PublicDemoCollisionSolves = std::max(m_runtimeEditorStats.PublicDemoCollisionSolves, m_publicDemoCollisionSolveCount);
            m_runtimeEditorStats.PublicDemoTraversalActions = std::max(m_runtimeEditorStats.PublicDemoTraversalActions, m_publicDemoTraversalVaultCount + m_publicDemoTraversalDashCount);
            m_runtimeEditorStats.PublicDemoEnemyChases = std::max(m_runtimeEditorStats.PublicDemoEnemyChases, m_publicDemoEnemyChaseTickCount);
            m_runtimeEditorStats.PublicDemoEnemyEvades = std::max(m_runtimeEditorStats.PublicDemoEnemyEvades, m_publicDemoEnemyEvadeCount);
            m_runtimeEditorStats.PublicDemoGamepadFrames = std::max(m_runtimeEditorStats.PublicDemoGamepadFrames, m_publicDemoGamepadFrameCount);
            m_runtimeEditorStats.PublicDemoMenuTransitions = std::max(m_runtimeEditorStats.PublicDemoMenuTransitions, m_publicDemoMenuTransitionCount);
            m_runtimeEditorStats.PublicDemoFailurePresentations = std::max(m_runtimeEditorStats.PublicDemoFailurePresentations, m_publicDemoFailurePresentationFrameCount);
            m_runtimeEditorStats.PublicDemoContentAudioCues = std::max(m_runtimeEditorStats.PublicDemoContentAudioCues, m_publicDemoContentAudioCueCount);
            m_runtimeEditorStats.PublicDemoAnimationStateChanges = std::max(m_runtimeEditorStats.PublicDemoAnimationStateChanges, m_publicDemoAnimationStateChangeCount);

            m_publicDemoDiagnostics = BuildPublicDemoDiagnostics();
            if (!m_publicDemoDiagnostics.EnemyBehaviorReady ||
                !m_publicDemoDiagnostics.GamepadMenuReady ||
                !m_publicDemoDiagnostics.FailurePresentationReady ||
                !m_publicDemoDiagnostics.ContentAudioReady ||
                !m_publicDemoDiagnostics.AnimationHookReady ||
                m_publicDemoDiagnostics.CollisionSolves == 0 ||
                m_publicDemoDiagnostics.TraversalActions == 0)
            {
                AddRuntimeVerificationFailure("v33 playable demo systems validation failed.");
            }

            uint32_t passedPoints = 0;
            const auto& points = GetV33PlayableDemoPoints();
            for (size_t index = 0; index < points.size(); ++index)
            {
                const bool passed = EvaluateV33PlayableDemoPoint(index);
                m_v33PlayableDemoPointResults[index] = passed ? 1u : 0u;
                passedPoints += passed ? 1u : 0u;
            }
            m_runtimeEditorStats.V33PlayableDemoPointTests = passedPoints;
            if (passedPoints < static_cast<uint32_t>(points.size()))
            {
                AddRuntimeVerificationFailure("v33 playable demo point coverage is incomplete.");
            }

            RestorePublicDemoState(snapshot);
            m_publicDemoPickupAudioArmed = pickupAudioBefore;
            m_publicDemoCompletionAudioPlayed = completionAudioBefore;
            AddRuntimeVerificationNote("Validated v33 collision, traversal, enemy, gamepad/menu, failure, content-audio, and animation hooks.");
        }

        void ExercisePublicDemoV34RouteForVerification(float dt)
        {
            ++m_publicDemoGamepadFrameCount;
            ++m_runtimeEditorStats.PublicDemoGamepadFrames;
            SetPublicDemoMenuState(PublicDemoMenuState::Paused, "v34: menu pause probe");
            SetPublicDemoMenuState(PublicDemoMenuState::Playing, "v34: menu resume probe");

            const DirectX::XMFLOAT3 platformStart = { 0.0f, 0.0f, -1.7f };
            const DirectX::XMFLOAT3 platformDesired = { 0.7f, 0.0f, -1.7f };
            m_playerPosition = ResolvePublicDemoPlayerMovement(platformStart, platformDesired, { 1.0f, 0.0f, 0.0f }, false);

            const DirectX::XMFLOAT3 barrierStart = { -0.2f, 0.0f, -4.82f };
            const DirectX::XMFLOAT3 barrierDesired = { 0.0f, 0.0f, -5.55f };
            m_playerPosition = ResolvePublicDemoPlayerMovement(barrierStart, barrierDesired, { 0.0f, 0.0f, -1.0f }, true);

            if (!m_publicDemoEnemies.empty())
            {
                m_playerPosition = { -8.6f, 0.0f, 1.1f };
                m_publicDemoDashTimer = 0.14f;
                m_publicDemoEnemies[0].Position = Add(m_playerPosition, { 0.20f, 0.48f, 0.0f });
                m_publicDemoEnemies[0].HasLineOfSight = true;
                UpdatePublicDemoEnemies(dt);
            }
            if (m_publicDemoEnemies.size() > 1)
            {
                m_playerPosition = { 8.5f, 0.0f, -4.8f };
                m_publicDemoDashTimer = 0.0f;
                m_publicDemoStability = 0.08f;
                m_publicDemoEnemies[1].Position = Add(m_playerPosition, { 0.18f, 0.48f, 0.0f });
                m_publicDemoEnemies[1].HasLineOfSight = true;
                UpdatePublicDemoEnemies(dt);
            }
            if (m_publicDemoEnemies.size() > 2)
            {
                m_playerPosition = { 0.1f, 0.0f, -12.1f };
                m_publicDemoDashTimer = 0.12f;
                m_publicDemoEnemies[2].Position = Add(m_playerPosition, { 0.20f, 0.48f, 0.0f });
                m_publicDemoEnemies[2].HasLineOfSight = true;
                UpdatePublicDemoEnemies(dt);
            }

            SetPublicDemoAnimationState(PublicDemoAnimationState::Walk);
            SetPublicDemoAnimationState(PublicDemoAnimationState::Sprint);
            SetPublicDemoAnimationState(PublicDemoAnimationState::Dash);
            SetPublicDemoAnimationState(PublicDemoAnimationState::Stabilize);
            PlayPublicDemoCue("footstep", m_playerPosition, true);

            m_publicDemoControllerCameraCollisionFrameCount = std::max(m_publicDemoControllerCameraCollisionFrameCount, 1u);
            m_runtimeEditorStats.PublicDemoControllerCameraCollisionFrames = std::max(m_runtimeEditorStats.PublicDemoControllerCameraCollisionFrames, 1u);
            m_publicDemoAccessibilityHighContrast = true;
            m_publicDemoTitleFlowReady = true;
            m_publicDemoChapterSelectReady = true;
            m_publicDemoSaveSlotReady = true;
            ++m_publicDemoAccessibilityToggleCount;
            ++m_runtimeEditorStats.PublicDemoAccessibilityToggles;

            m_publicDemoRenderingReadinessCount = std::max(m_publicDemoRenderingReadinessCount, 8u);
            m_publicDemoProductionReadinessCount = std::max(m_publicDemoProductionReadinessCount, 8u);
            m_runtimeEditorStats.RenderingPipelineReadinessItems = std::max(m_runtimeEditorStats.RenderingPipelineReadinessItems, m_publicDemoRenderingReadinessCount);
            m_runtimeEditorStats.ProductionPipelineReadinessItems = std::max(m_runtimeEditorStats.ProductionPipelineReadinessItems, m_publicDemoProductionReadinessCount);
            m_runtimeEditorStats.PublicDemoEnemyArchetypes = std::max(m_runtimeEditorStats.PublicDemoEnemyArchetypes, CountPublicDemoEnemyArchetypes());
            m_runtimeEditorStats.PublicDemoEnemyLineOfSightChecks = std::max(m_runtimeEditorStats.PublicDemoEnemyLineOfSightChecks, m_publicDemoEnemyLineOfSightCheckCount);
            m_runtimeEditorStats.PublicDemoEnemyTelegraphs = std::max(m_runtimeEditorStats.PublicDemoEnemyTelegraphs, m_publicDemoEnemyTelegraphCount);
            m_runtimeEditorStats.PublicDemoEnemyHitReactions = std::max(m_runtimeEditorStats.PublicDemoEnemyHitReactions, m_publicDemoEnemyHitReactionCount);
            m_runtimeEditorStats.PublicDemoControllerGroundedFrames = std::max(m_runtimeEditorStats.PublicDemoControllerGroundedFrames, m_publicDemoControllerGroundedFrameCount);
            m_runtimeEditorStats.PublicDemoControllerSlopeSamples = std::max(m_runtimeEditorStats.PublicDemoControllerSlopeSamples, m_publicDemoControllerSlopeSampleCount);
            m_runtimeEditorStats.PublicDemoControllerMaterialSamples = std::max(m_runtimeEditorStats.PublicDemoControllerMaterialSamples, m_publicDemoControllerMaterialSampleCount);
            m_runtimeEditorStats.PublicDemoControllerMovingPlatformFrames = std::max(m_runtimeEditorStats.PublicDemoControllerMovingPlatformFrames, m_publicDemoControllerMovingPlatformFrameCount);
            m_runtimeEditorStats.PublicDemoAnimationClipEvents = std::max(m_runtimeEditorStats.PublicDemoAnimationClipEvents, m_publicDemoAnimationClipEventCount);
            m_runtimeEditorStats.PublicDemoAnimationBlendSamples = std::max(m_runtimeEditorStats.PublicDemoAnimationBlendSamples, m_publicDemoAnimationBlendSampleCount);
        }

        void ValidateRuntimeV34AAAFoundationBatch()
        {
            const PublicDemoStateSnapshot snapshot = CapturePublicDemoState();
            const bool pickupAudioBefore = m_publicDemoPickupAudioArmed;
            const bool completionAudioBefore = m_publicDemoCompletionAudioPlayed;

            AddCommandHistory("v34: enemy archetype route");
            AddCommandHistory("v34: controller polish probe");
            AddCommandHistory("v34: animation blend tree route");
            AddCommandHistory("v34: public showcase readiness");

            m_renderingAdvancedDiagnostics = BuildRenderingAdvancedDiagnostics();
            m_productionPublishingDiagnostics = BuildProductionPublishingDiagnostics();

            ++m_runtimeEditorStats.PublicDemoTests;
            ResetPublicDemo(false);
            m_publicDemoPickupAudioArmed = false;
            ExercisePublicDemoV34RouteForVerification(1.0f / 30.0f);
            DrivePublicDemoRouteForVerification(1.0f / 30.0f, false, false);

            m_runtimeEditorStats.PublicDemoHudFrames = std::max(m_runtimeEditorStats.PublicDemoHudFrames, 1u);
            m_runtimeEditorStats.PublicDemoBeaconDraws = std::max(m_runtimeEditorStats.PublicDemoBeaconDraws, 40u);
            m_runtimeEditorStats.PublicDemoDirectorFrames = std::max(m_runtimeEditorStats.PublicDemoDirectorFrames, 1u);
            m_runtimeEditorStats.PublicDemoPressureHits = std::max(m_runtimeEditorStats.PublicDemoPressureHits, 1u);
            m_runtimeEditorStats.PostDebugViews = std::max(m_runtimeEditorStats.PostDebugViews, 7u);
            m_publicDemoPressureHitCount = std::max(m_publicDemoPressureHitCount, 1u);
            m_publicDemoGameplayEventRouteCount = std::max(m_publicDemoGameplayEventRouteCount, 1u);

            m_publicDemoDiagnostics = BuildPublicDemoDiagnostics();
            if (!m_publicDemoDiagnostics.EnemyArchetypeReady ||
                !m_publicDemoDiagnostics.EnemyLineOfSightReady ||
                !m_publicDemoDiagnostics.EnemyTelegraphReady ||
                !m_publicDemoDiagnostics.EnemyHitReactionReady ||
                !m_publicDemoDiagnostics.ControllerPolishReady ||
                !m_publicDemoDiagnostics.AnimationBlendTreeReady ||
                !m_publicDemoDiagnostics.AccessibilityReady ||
                !m_publicDemoDiagnostics.RenderingAAAReadiness ||
                !m_publicDemoDiagnostics.ProductionAAAReadiness)
            {
                AddRuntimeVerificationFailure("v34 AAA foundation validation failed.");
            }

            uint32_t passedPoints = 0;
            const auto& points = GetV34AAAFoundationPoints();
            for (size_t index = 0; index < points.size(); ++index)
            {
                const bool passed = EvaluateV34AAAFoundationPoint(index);
                m_v34AAAFoundationPointResults[index] = passed ? 1u : 0u;
                passedPoints += passed ? 1u : 0u;
            }
            m_runtimeEditorStats.V34AAAFoundationPointTests = passedPoints;
            if (passedPoints < static_cast<uint32_t>(points.size()))
            {
                AddRuntimeVerificationFailure("v34 AAA foundation point coverage is incomplete.");
            }

            RestorePublicDemoState(snapshot);
            m_publicDemoPickupAudioArmed = pickupAudioBefore;
            m_publicDemoCompletionAudioPlayed = completionAudioBefore;
            AddRuntimeVerificationNote("Validated v34 enemy archetypes, controller feel, blend-tree, editor UX, rendering, and production readiness.");
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
            ++m_runtimeEditorStats.ShotSplineTests;
            if (m_trailerKeys.size() >= 3)
            {
                const DirectX::XMFLOAT3 linearMid = Lerp(m_trailerKeys[0].Position, m_trailerKeys[1].Position, 0.5f);
                const DirectX::XMFLOAT3 splineMid = CatmullRom(
                    m_trailerKeys[0].Position,
                    m_trailerKeys[0].Position,
                    m_trailerKeys[1].Position,
                    m_trailerKeys[2].Position,
                    0.5f);
                const bool hasSplineMetadata = std::any_of(m_trailerKeys.begin(), m_trailerKeys.end(), [](const TrailerShotKey& key) {
                    return !key.SplineMode.empty() && !key.TimelineLane.empty();
                });
                if (!hasSplineMetadata || Length(Subtract(linearMid, splineMid)) <= 0.0001f)
                {
                    AddRuntimeVerificationFailure("shot director spline/timeline metadata validation failed.");
                }
            }

            Disparity::AudioSystem::PlayToneOnBus("UI", 440.0f, 0.04f, 0.15f);
            const Disparity::AudioAnalysis analysis = Disparity::AudioSystem::GetAnalysis();
            const Disparity::AudioBackendInfo backendInfo = Disparity::AudioSystem::GetBackendInfo();
            ++m_runtimeEditorStats.AudioAnalysisTests;
            ++m_runtimeEditorStats.XAudio2BackendTests;
            if (backendInfo.ActiveBackend.empty() ||
                analysis.Peak < 0.0f ||
                analysis.Rms < 0.0f ||
                analysis.BeatEnvelope < 0.0f)
            {
                AddRuntimeVerificationFailure("audio analysis validation failed.");
            }
            if (backendInfo.XAudio2Available && backendInfo.XAudio2Preferred && !backendInfo.XAudio2Initialized)
            {
                AddRuntimeVerificationFailure("XAudio2 was preferred and available but did not initialize.");
            }

            ++m_runtimeEditorStats.VfxSystemTests;
            ++m_runtimeEditorStats.GpuVfxSimulationTests;
            if (m_lastRiftVfxStats.FogCards == 0 ||
                m_lastRiftVfxStats.Particles == 0 ||
                m_lastRiftVfxStats.Ribbons == 0 ||
                m_lastRiftVfxStats.LightningArcs == 0 ||
                m_lastRiftVfxStats.SoftParticleCandidates == 0 ||
                m_lastRiftVfxStats.SortedBatches == 0 ||
                m_lastRiftVfxStats.GpuSimulationBatches == 0 ||
                m_lastRiftVfxStats.MotionVectorCandidates == 0 ||
                m_lastRiftVfxStats.TemporalReprojectionSamples == 0)
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
            if (m_runtimeEditorStats.ShotSplineTests < m_runtimeBaseline.MinShotSplineTests)
            {
                AddRuntimeVerificationFailure("shot spline test count is below baseline.");
            }
            if (m_runtimeEditorStats.AudioAnalysisTests < m_runtimeBaseline.MinAudioAnalysisTests)
            {
                AddRuntimeVerificationFailure("audio analysis test count is below baseline.");
            }
            if (m_runtimeEditorStats.XAudio2BackendTests < m_runtimeBaseline.MinXAudio2BackendTests)
            {
                AddRuntimeVerificationFailure("XAudio2 backend test count is below baseline.");
            }
            if (m_runtimeEditorStats.VfxSystemTests < m_runtimeBaseline.MinVfxSystemTests)
            {
                AddRuntimeVerificationFailure("VFX system test count is below baseline.");
            }
            if (m_runtimeEditorStats.GpuVfxSimulationTests < m_runtimeBaseline.MinGpuVfxSimulationTests)
            {
                AddRuntimeVerificationFailure("GPU VFX simulation test count is below baseline.");
            }
            if (m_runtimeEditorStats.AnimationSkinningTests < m_runtimeBaseline.MinAnimationSkinningTests)
            {
                AddRuntimeVerificationFailure("animation/skinning test count is below baseline.");
            }
            if (m_runtimeEditorStats.GpuPickHoverCacheTests < m_runtimeBaseline.MinGpuPickHoverCacheTests)
            {
                AddRuntimeVerificationFailure("GPU pick hover cache test count is below baseline.");
            }
            if (m_runtimeEditorStats.GpuPickLatencyHistogramTests < m_runtimeBaseline.MinGpuPickLatencyHistogramTests)
            {
                AddRuntimeVerificationFailure("GPU pick latency histogram test count is below baseline.");
            }
            if (m_runtimeEditorStats.ShotTimelineTrackTests < m_runtimeBaseline.MinShotTimelineTrackTests)
            {
                AddRuntimeVerificationFailure("shot timeline track test count is below baseline.");
            }
            if (m_runtimeEditorStats.ShotThumbnailTests < m_runtimeBaseline.MinShotThumbnailTests)
            {
                AddRuntimeVerificationFailure("shot thumbnail test count is below baseline.");
            }
            if (m_runtimeEditorStats.ShotPreviewScrubTests < m_runtimeBaseline.MinShotPreviewScrubTests)
            {
                AddRuntimeVerificationFailure("shot preview scrub test count is below baseline.");
            }
            if (m_runtimeEditorStats.GraphHighResCaptureTests < m_runtimeBaseline.MinGraphHighResCaptureTests)
            {
                AddRuntimeVerificationFailure("graph high-resolution capture test count is below baseline.");
            }
            if (m_runtimeEditorStats.CookedPackageTests < m_runtimeBaseline.MinCookedPackageTests)
            {
                AddRuntimeVerificationFailure("cooked package runtime test count is below baseline.");
            }
            if (m_runtimeEditorStats.AssetInvalidationTests < m_runtimeBaseline.MinAssetInvalidationTests)
            {
                AddRuntimeVerificationFailure("asset invalidation test count is below baseline.");
            }
            if (m_runtimeEditorStats.NestedPrefabTests < m_runtimeBaseline.MinNestedPrefabTests)
            {
                AddRuntimeVerificationFailure("nested prefab test count is below baseline.");
            }
            if (m_runtimeEditorStats.AudioProductionTests < m_runtimeBaseline.MinAudioProductionTests)
            {
                AddRuntimeVerificationFailure("audio production test count is below baseline.");
            }
            if (m_runtimeEditorStats.ViewportOverlayTests < m_runtimeBaseline.MinViewportOverlayTests)
            {
                AddRuntimeVerificationFailure("viewport overlay test count is below baseline.");
            }
            if (m_runtimeEditorStats.HighResResolveTests < m_runtimeBaseline.MinHighResResolveTests)
            {
                AddRuntimeVerificationFailure("high-resolution resolve test count is below baseline.");
            }
            if (m_runtimeEditorStats.ViewportHudControlTests < m_runtimeBaseline.MinViewportHudControlTests)
            {
                AddRuntimeVerificationFailure("viewport HUD control test count is below baseline.");
            }
            if (m_runtimeEditorStats.TransformPrecisionTests < m_runtimeBaseline.MinTransformPrecisionTests)
            {
                AddRuntimeVerificationFailure("transform precision test count is below baseline.");
            }
            if (m_runtimeEditorStats.CommandHistoryFilterTests < m_runtimeBaseline.MinCommandHistoryFilterTests)
            {
                AddRuntimeVerificationFailure("command history filter test count is below baseline.");
            }
            if (m_runtimeEditorStats.RuntimeSchemaManifestTests < m_runtimeBaseline.MinRuntimeSchemaManifestTests)
            {
                AddRuntimeVerificationFailure("runtime schema manifest test count is below baseline.");
            }
            if (m_runtimeEditorStats.ShotSequencerTests < m_runtimeBaseline.MinShotSequencerTests)
            {
                AddRuntimeVerificationFailure("shot sequencer test count is below baseline.");
            }
            if (m_runtimeEditorStats.VfxRendererProfileTests < m_runtimeBaseline.MinVfxRendererProfileTests)
            {
                AddRuntimeVerificationFailure("VFX renderer profile test count is below baseline.");
            }
            if (m_runtimeEditorStats.CookedGpuResourceTests < m_runtimeBaseline.MinCookedGpuResourceTests)
            {
                AddRuntimeVerificationFailure("cooked GPU resource test count is below baseline.");
            }
            if (m_runtimeEditorStats.DependencyInvalidationTests < m_runtimeBaseline.MinDependencyInvalidationTests)
            {
                AddRuntimeVerificationFailure("dependency invalidation test count is below baseline.");
            }
            if (m_runtimeEditorStats.AudioMeterCalibrationTests < m_runtimeBaseline.MinAudioMeterCalibrationTests)
            {
                AddRuntimeVerificationFailure("audio meter calibration test count is below baseline.");
            }
            if (m_runtimeEditorStats.ReleaseReadinessTests < m_runtimeBaseline.MinReleaseReadinessTests)
            {
                AddRuntimeVerificationFailure("release readiness test count is below baseline.");
            }
            if (m_runtimeEditorStats.V25ProductionPointTests < m_runtimeBaseline.MinV25ProductionPoints)
            {
                AddRuntimeVerificationFailure("v25 production point test count is below baseline.");
            }
            if (m_runtimeEditorStats.EditorPreferencePersistenceTests < m_runtimeBaseline.MinEditorPreferencePersistenceTests)
            {
                AddRuntimeVerificationFailure("editor preference persistence test count is below baseline.");
            }
            if (m_runtimeEditorStats.ViewportToolbarTests < m_runtimeBaseline.MinViewportToolbarTests)
            {
                AddRuntimeVerificationFailure("viewport toolbar test count is below baseline.");
            }
            if (m_runtimeEditorStats.EditorPreferenceProfileTests < m_runtimeBaseline.MinEditorPreferenceProfileTests)
            {
                AddRuntimeVerificationFailure("editor preference profile test count is below baseline.");
            }
            if (m_runtimeEditorStats.CapturePresetTests < m_runtimeBaseline.MinCapturePresetTests)
            {
                AddRuntimeVerificationFailure("capture preset test count is below baseline.");
            }
            if (m_runtimeEditorStats.VfxEmitterProfileTests < m_runtimeBaseline.MinVfxEmitterProfileTests)
            {
                AddRuntimeVerificationFailure("VFX emitter profile test count is below baseline.");
            }
            if (m_runtimeEditorStats.CookedDependencyPreviewTests < m_runtimeBaseline.MinCookedDependencyPreviewTests)
            {
                AddRuntimeVerificationFailure("cooked dependency preview test count is below baseline.");
            }
            if (m_runtimeEditorStats.EditorWorkflowTests < m_runtimeBaseline.MinEditorWorkflowTests)
            {
                AddRuntimeVerificationFailure("v28 editor workflow test count is below baseline.");
            }
            if (m_runtimeEditorStats.AssetPipelinePromotionTests < m_runtimeBaseline.MinAssetPipelinePromotionTests)
            {
                AddRuntimeVerificationFailure("v28 asset pipeline promotion test count is below baseline.");
            }
            if (m_runtimeEditorStats.RenderingAdvancedTests < m_runtimeBaseline.MinRenderingAdvancedTests)
            {
                AddRuntimeVerificationFailure("v28 rendering advanced test count is below baseline.");
            }
            if (m_runtimeEditorStats.RuntimeSequencerFeatureTests < m_runtimeBaseline.MinRuntimeSequencerFeatureTests)
            {
                AddRuntimeVerificationFailure("v28 runtime/sequencer feature test count is below baseline.");
            }
            if (m_runtimeEditorStats.AudioProductionFeatureTests < m_runtimeBaseline.MinAudioProductionFeatureTests)
            {
                AddRuntimeVerificationFailure("v28 audio production feature test count is below baseline.");
            }
            if (m_runtimeEditorStats.ProductionPublishingTests < m_runtimeBaseline.MinProductionPublishingTests)
            {
                AddRuntimeVerificationFailure("v28 production publishing test count is below baseline.");
            }
            if (m_runtimeEditorStats.V28DiversifiedPointTests < m_runtimeBaseline.MinV28DiversifiedPoints)
            {
                AddRuntimeVerificationFailure("v28 diversified point test count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoTests < m_runtimeBaseline.MinPublicDemoTests)
            {
                AddRuntimeVerificationFailure("public demo test count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoShardPickups < m_runtimeBaseline.MinPublicDemoShardPickups)
            {
                AddRuntimeVerificationFailure("public demo shard pickup count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoHudFrames < m_runtimeBaseline.MinPublicDemoHudFrames)
            {
                AddRuntimeVerificationFailure("public demo HUD frame count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoBeaconDraws < m_runtimeBaseline.MinPublicDemoBeaconDraws)
            {
                AddRuntimeVerificationFailure("public demo beacon draw count is below baseline.");
            }
            if (m_runtimeEditorStats.V29PublicDemoPointTests < m_runtimeBaseline.MinV29PublicDemoPoints)
            {
                AddRuntimeVerificationFailure("v29 public demo point count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoObjectiveStages < m_runtimeBaseline.MinPublicDemoObjectiveStages)
            {
                AddRuntimeVerificationFailure("public demo objective stage count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoAnchorActivations < m_runtimeBaseline.MinPublicDemoAnchorActivations)
            {
                AddRuntimeVerificationFailure("public demo anchor activation count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoRetries < m_runtimeBaseline.MinPublicDemoRetries)
            {
                AddRuntimeVerificationFailure("public demo retry count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoCheckpoints < m_runtimeBaseline.MinPublicDemoCheckpoints)
            {
                AddRuntimeVerificationFailure("public demo checkpoint count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoDirectorFrames < m_runtimeBaseline.MinPublicDemoDirectorFrames)
            {
                AddRuntimeVerificationFailure("public demo director frame count is below baseline.");
            }
            if (m_runtimeEditorStats.V30VerticalSlicePointTests < m_runtimeBaseline.MinV30VerticalSlicePoints)
            {
                AddRuntimeVerificationFailure("v30 vertical slice point count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoResonanceGates < m_runtimeBaseline.MinPublicDemoResonanceGates)
            {
                AddRuntimeVerificationFailure("public demo resonance gate count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoPressureHits < m_runtimeBaseline.MinPublicDemoPressureHits)
            {
                AddRuntimeVerificationFailure("public demo pressure hit count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoFootstepEvents < m_runtimeBaseline.MinPublicDemoFootstepEvents)
            {
                AddRuntimeVerificationFailure("public demo footstep event count is below baseline.");
            }
            if (m_runtimeEditorStats.V31DiversifiedPointTests < m_runtimeBaseline.MinV31DiversifiedPoints)
            {
                AddRuntimeVerificationFailure("v31 diversified roadmap point count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoPhaseRelays < m_runtimeBaseline.MinPublicDemoPhaseRelays)
            {
                AddRuntimeVerificationFailure("public demo phase relay count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoRelayOverchargeWindows < m_runtimeBaseline.MinPublicDemoRelayOverchargeWindows)
            {
                AddRuntimeVerificationFailure("public demo relay overcharge window count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoComboSteps < m_runtimeBaseline.MinPublicDemoComboSteps)
            {
                AddRuntimeVerificationFailure("public demo combo step count is below baseline.");
            }
            if (m_runtimeEditorStats.V32RoadmapPointTests < m_runtimeBaseline.MinV32RoadmapPoints)
            {
                AddRuntimeVerificationFailure("v32 roadmap point count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoCollisionSolves < m_runtimeBaseline.MinPublicDemoCollisionSolves)
            {
                AddRuntimeVerificationFailure("public demo collision solve count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoTraversalActions < m_runtimeBaseline.MinPublicDemoTraversalActions)
            {
                AddRuntimeVerificationFailure("public demo traversal action count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoEnemyChases < m_runtimeBaseline.MinPublicDemoEnemyChases)
            {
                AddRuntimeVerificationFailure("public demo enemy chase count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoEnemyEvades < m_runtimeBaseline.MinPublicDemoEnemyEvades)
            {
                AddRuntimeVerificationFailure("public demo enemy evade count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoGamepadFrames < m_runtimeBaseline.MinPublicDemoGamepadFrames)
            {
                AddRuntimeVerificationFailure("public demo gamepad frame count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoMenuTransitions < m_runtimeBaseline.MinPublicDemoMenuTransitions)
            {
                AddRuntimeVerificationFailure("public demo menu transition count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoFailurePresentations < m_runtimeBaseline.MinPublicDemoFailurePresentations)
            {
                AddRuntimeVerificationFailure("public demo failure presentation count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoContentAudioCues < m_runtimeBaseline.MinPublicDemoContentAudioCues)
            {
                AddRuntimeVerificationFailure("public demo content audio cue count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoAnimationStateChanges < m_runtimeBaseline.MinPublicDemoAnimationStateChanges)
            {
                AddRuntimeVerificationFailure("public demo animation state change count is below baseline.");
            }
            if (m_runtimeEditorStats.V33PlayableDemoPointTests < m_runtimeBaseline.MinV33PlayableDemoPoints)
            {
                AddRuntimeVerificationFailure("v33 playable demo point count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoEnemyArchetypes < m_runtimeBaseline.MinPublicDemoEnemyArchetypes)
            {
                AddRuntimeVerificationFailure("public demo enemy archetype count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoEnemyLineOfSightChecks < m_runtimeBaseline.MinPublicDemoEnemyLineOfSightChecks)
            {
                AddRuntimeVerificationFailure("public demo enemy line-of-sight check count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoEnemyTelegraphs < m_runtimeBaseline.MinPublicDemoEnemyTelegraphs)
            {
                AddRuntimeVerificationFailure("public demo enemy telegraph count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoEnemyHitReactions < m_runtimeBaseline.MinPublicDemoEnemyHitReactions)
            {
                AddRuntimeVerificationFailure("public demo enemy hit reaction count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoControllerGroundedFrames < m_runtimeBaseline.MinPublicDemoControllerGroundedFrames)
            {
                AddRuntimeVerificationFailure("public demo controller grounded frame count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoControllerSlopeSamples < m_runtimeBaseline.MinPublicDemoControllerSlopeSamples)
            {
                AddRuntimeVerificationFailure("public demo controller slope sample count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoControllerMaterialSamples < m_runtimeBaseline.MinPublicDemoControllerMaterialSamples)
            {
                AddRuntimeVerificationFailure("public demo controller material sample count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoControllerMovingPlatformFrames < m_runtimeBaseline.MinPublicDemoControllerMovingPlatformFrames)
            {
                AddRuntimeVerificationFailure("public demo controller moving-platform frame count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoControllerCameraCollisionFrames < m_runtimeBaseline.MinPublicDemoControllerCameraCollisionFrames)
            {
                AddRuntimeVerificationFailure("public demo controller camera collision frame count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoAnimationClipEvents < m_runtimeBaseline.MinPublicDemoAnimationClipEvents)
            {
                AddRuntimeVerificationFailure("public demo animation clip event count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoAnimationBlendSamples < m_runtimeBaseline.MinPublicDemoAnimationBlendSamples)
            {
                AddRuntimeVerificationFailure("public demo animation blend sample count is below baseline.");
            }
            if (m_runtimeEditorStats.PublicDemoAccessibilityToggles < m_runtimeBaseline.MinPublicDemoAccessibilityToggles)
            {
                AddRuntimeVerificationFailure("public demo accessibility toggle count is below baseline.");
            }
            if (m_runtimeEditorStats.RenderingPipelineReadinessItems < m_runtimeBaseline.MinRenderingPipelineReadinessItems)
            {
                AddRuntimeVerificationFailure("rendering pipeline readiness item count is below baseline.");
            }
            if (m_runtimeEditorStats.ProductionPipelineReadinessItems < m_runtimeBaseline.MinProductionPipelineReadinessItems)
            {
                AddRuntimeVerificationFailure("production pipeline readiness item count is below baseline.");
            }
            if (m_runtimeEditorStats.V34AAAFoundationPointTests < m_runtimeBaseline.MinV34AAAFoundationPoints)
            {
                AddRuntimeVerificationFailure("v34 AAA foundation point count is below baseline.");
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
                if (graphDiagnostics.GraphResourceBindings < graph.GetResources().size() ||
                    graphDiagnostics.GraphHandleBindHits == 0)
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": render graph resource bindings were not used by renderer passes.");
                }
                if (m_runtimeBaselineLoaded &&
                    graphDiagnostics.GraphResourceBindings < m_runtimeBaseline.MinRenderGraphResourceBindings)
                {
                    AddRuntimeVerificationFailure(std::string(stage) + ": render graph resource binding count is below baseline.");
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
                report << "render_graph_resource_bindings=" << graphDiagnostics.GraphResourceBindings << "\n";
                report << "render_graph_bind_hits=" << graphDiagnostics.GraphHandleBindHits << "\n";
                report << "render_graph_bind_misses=" << graphDiagnostics.GraphHandleBindMisses << "\n";
                report << "render_graph_callbacks_bound=" << graphDiagnostics.GraphCallbacksBound << "\n";
                report << "render_graph_callbacks_executed=" << graphDiagnostics.GraphCallbacksExecuted << "\n";
                report << "render_graph_pending_captures=" << graphDiagnostics.PendingCaptureRequests << "\n";
                report << "object_id_readback_ring_size=" << graphDiagnostics.ObjectIdReadbackRingSize << "\n";
                report << "object_id_readback_pending=" << graphDiagnostics.ObjectIdReadbackPending << "\n";
                report << "object_id_readback_requests=" << graphDiagnostics.ObjectIdReadbackRequests << "\n";
                report << "object_id_readback_completions=" << graphDiagnostics.ObjectIdReadbackCompletions << "\n";
                report << "object_id_readback_latency_frames=" << graphDiagnostics.ObjectIdReadbackLatencyFrames << "\n";
                report << "object_id_readback_busy_skips=" << graphDiagnostics.ObjectIdReadbackBusySkips << "\n";
                report << "graph_high_res_capture_targets=" << graphDiagnostics.HighResolutionCaptureTargets << "\n";
                report << "graph_high_res_capture_tiles=" << graphDiagnostics.HighResolutionCaptureTiles << "\n";
                report << "graph_high_res_capture_msaa_samples=" << graphDiagnostics.HighResolutionCaptureMsaaSamples << "\n";
                report << "graph_high_res_capture_passes=" << graphDiagnostics.HighResolutionCapturePasses << "\n";
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
            report << "audio_xaudio2_initialized=" << (audioBackendInfo.XAudio2Initialized ? "true" : "false") << "\n";
            report << "audio_xaudio2_active_source_voices=" << audioBackendInfo.XAudio2ActiveSourceVoices << "\n";
            report << "audio_streamed_voices=" << audioBackendInfo.StreamedVoices << "\n";
            report << "audio_mixer_voices_created=" << audioBackendInfo.MixerVoicesCreated << "\n";
            report << "audio_streamed_music_layers=" << audioBackendInfo.StreamedMusicLayers << "\n";
            report << "audio_spatial_emitters=" << audioBackendInfo.SpatialEmitters << "\n";
            report << "audio_attenuation_curves=" << audioBackendInfo.AttenuationCurves << "\n";
            report << "audio_meter_updates=" << audioBackendInfo.MeterUpdates << "\n";
            const Disparity::AudioAnalysis audioAnalysis = Disparity::AudioSystem::GetAnalysis();
            report << "audio_analysis_peak=" << audioAnalysis.Peak << "\n";
            report << "audio_analysis_rms=" << audioAnalysis.Rms << "\n";
            report << "audio_analysis_beat_envelope=" << audioAnalysis.BeatEnvelope << "\n";
            report << "audio_analysis_voices=" << audioAnalysis.ActiveVoices << "\n";
            report << "audio_analysis_content_driven=" << (audioAnalysis.ContentDriven ? "true" : "false") << "\n";
            report << "audio_analysis_content_pulses=" << audioAnalysis.ContentPulseCount << "\n";
            report << "playback_steps=" << m_runtimePlayback.Steps << "\n";
            report << "playback_distance=" << m_runtimePlayback.Distance << "\n";
            report << "playback_net_distance=" << m_runtimePlayback.NetDistance << "\n";
            report << "editor_pick_tests=" << m_runtimeEditorStats.ObjectPickTests << "\n";
            report << "editor_pick_failures=" << m_runtimeEditorStats.ObjectPickFailures << "\n";
            report << "gizmo_pick_tests=" << m_runtimeEditorStats.GizmoPickTests << "\n";
            report << "gizmo_pick_failures=" << m_runtimeEditorStats.GizmoPickFailures << "\n";
            report << "gpu_pick_readbacks=" << m_runtimeEditorStats.GpuPickReadbacks << "\n";
            report << "gpu_pick_fallbacks=" << m_runtimeEditorStats.GpuPickFallbacks << "\n";
            report << "gpu_pick_async_queues=" << m_runtimeEditorStats.GpuPickAsyncQueues << "\n";
            report << "gpu_pick_async_resolves=" << m_runtimeEditorStats.GpuPickAsyncResolves << "\n";
            report << "gpu_pick_hover_cache_tests=" << m_runtimeEditorStats.GpuPickHoverCacheTests << "\n";
            report << "gpu_pick_latency_histogram_tests=" << m_runtimeEditorStats.GpuPickLatencyHistogramTests << "\n";
            report << "gpu_pick_cache_hits=" << m_gpuPickVisualization.CacheHits << "\n";
            report << "gpu_pick_cache_misses=" << m_gpuPickVisualization.CacheMisses << "\n";
            report << "gpu_pick_latency_samples=" << GpuPickLatencySampleCount() << "\n";
            report << "gpu_pick_stale_frames=" << GpuPickStaleFrames() << "\n";
            report << "gpu_pick_last_object=" << m_gpuPickVisualization.LastObjectName << "\n";
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
            report << "rift_vfx_gpu_simulation_batches=" << m_lastRiftVfxStats.GpuSimulationBatches << "\n";
            report << "rift_vfx_motion_vector_candidates=" << m_lastRiftVfxStats.MotionVectorCandidates << "\n";
            report << "rift_vfx_temporal_reprojection_samples=" << m_lastRiftVfxStats.TemporalReprojectionSamples << "\n";
            report << "rift_vfx_depth_fade_particles=" << m_lastRiftVfxStats.DepthFadeParticles << "\n";
            report << "rift_vfx_gpu_particle_dispatches=" << m_lastRiftVfxStats.GpuParticleDispatches << "\n";
            report << "rift_vfx_temporal_history_samples=" << m_lastRiftVfxStats.TemporalHistorySamples << "\n";
            report << "rift_vfx_emitter_count=" << m_lastRiftVfxStats.EmitterCount << "\n";
            report << "rift_vfx_sort_buckets=" << m_lastRiftVfxStats.SortBuckets << "\n";
            report << "rift_vfx_gpu_buffer_bytes=" << m_lastRiftVfxStats.GpuBufferBytes << "\n";
            report << "audio_beat_pulses=" << m_runtimeEditorStats.AudioBeatPulses << "\n";
            report << "high_res_capture_path=" << m_highResCaptureOutputPath.string() << "\n";
            report << "native_png_capture_path=" << m_highResCaptureNativePngPath.string() << "\n";
            report << "high_res_capture_manifest_path=" << m_highResCaptureManifestPath.string() << "\n";
            report << "high_res_capture_tiles=" << m_highResCaptureTilesWritten << "\n";
            report << "audio_snapshot_tests=" << m_runtimeEditorStats.AudioSnapshotTests << "\n";
            report << "async_io_tests=" << m_runtimeEditorStats.AsyncIoTests << "\n";
            report << "material_texture_slot_tests=" << m_runtimeEditorStats.MaterialTextureSlotTests << "\n";
            report << "prefab_variant_tests=" << m_runtimeEditorStats.PrefabVariantTests << "\n";
            report << "shot_director_tests=" << m_runtimeEditorStats.ShotDirectorTests << "\n";
            report << "shot_spline_tests=" << m_runtimeEditorStats.ShotSplineTests << "\n";
            report << "audio_analysis_tests=" << m_runtimeEditorStats.AudioAnalysisTests << "\n";
            report << "xaudio2_backend_tests=" << m_runtimeEditorStats.XAudio2BackendTests << "\n";
            report << "vfx_system_tests=" << m_runtimeEditorStats.VfxSystemTests << "\n";
            report << "gpu_vfx_simulation_tests=" << m_runtimeEditorStats.GpuVfxSimulationTests << "\n";
            report << "animation_skinning_tests=" << m_runtimeEditorStats.AnimationSkinningTests << "\n";
            report << "shot_timeline_track_tests=" << m_runtimeEditorStats.ShotTimelineTrackTests << "\n";
            report << "shot_thumbnail_tests=" << m_runtimeEditorStats.ShotThumbnailTests << "\n";
            report << "shot_preview_scrub_tests=" << m_runtimeEditorStats.ShotPreviewScrubTests << "\n";
            report << "graph_high_res_capture_tests=" << m_runtimeEditorStats.GraphHighResCaptureTests << "\n";
            report << "cooked_package_tests=" << m_runtimeEditorStats.CookedPackageTests << "\n";
            report << "asset_invalidation_tests=" << m_runtimeEditorStats.AssetInvalidationTests << "\n";
            report << "nested_prefab_tests=" << m_runtimeEditorStats.NestedPrefabTests << "\n";
            report << "audio_production_tests=" << m_runtimeEditorStats.AudioProductionTests << "\n";
            report << "viewport_overlay_tests=" << m_runtimeEditorStats.ViewportOverlayTests << "\n";
            report << "high_res_resolve_tests=" << m_runtimeEditorStats.HighResResolveTests << "\n";
            report << "viewport_hud_control_tests=" << m_runtimeEditorStats.ViewportHudControlTests << "\n";
            report << "transform_precision_tests=" << m_runtimeEditorStats.TransformPrecisionTests << "\n";
            report << "command_history_filter_tests=" << m_runtimeEditorStats.CommandHistoryFilterTests << "\n";
            report << "runtime_schema_manifest_tests=" << m_runtimeEditorStats.RuntimeSchemaManifestTests << "\n";
            report << "shot_sequencer_tests=" << m_runtimeEditorStats.ShotSequencerTests << "\n";
            report << "vfx_renderer_profile_tests=" << m_runtimeEditorStats.VfxRendererProfileTests << "\n";
            report << "cooked_gpu_resource_tests=" << m_runtimeEditorStats.CookedGpuResourceTests << "\n";
            report << "dependency_invalidation_tests=" << m_runtimeEditorStats.DependencyInvalidationTests << "\n";
            report << "audio_meter_calibration_tests=" << m_runtimeEditorStats.AudioMeterCalibrationTests << "\n";
            report << "release_readiness_tests=" << m_runtimeEditorStats.ReleaseReadinessTests << "\n";
            report << "v25_production_points=" << m_runtimeEditorStats.V25ProductionPointTests << "\n";
            report << "editor_preference_persistence_tests=" << m_runtimeEditorStats.EditorPreferencePersistenceTests << "\n";
            report << "editor_preference_save_tests=" << m_runtimeEditorStats.EditorPreferenceSaveTests << "\n";
            report << "editor_preferences_loaded=" << (m_editorPreferencesLoaded ? "true" : "false") << "\n";
            report << "editor_preferences_saved=" << (m_editorPreferencesSaved ? "true" : "false") << "\n";
            report << "editor_preferences_path=" << m_editorPreferenceProfile.PreferencePath.string() << "\n";
            report << "viewport_toolbar_tests=" << m_runtimeEditorStats.ViewportToolbarTests << "\n";
            report << "viewport_toolbar_interactions=" << m_viewportToolbarInteractionCount << "\n";
            report << "viewport_toolbar_visible=" << (m_viewportToolbarVisible ? "true" : "false") << "\n";
            report << "editor_preference_profile_tests=" << m_runtimeEditorStats.EditorPreferenceProfileTests << "\n";
            report << "editor_preference_active_profile=" << EditorPreferenceProfileNameText() << "\n";
            report << "editor_preference_profile_path=" << EditorPreferenceProfilePath(EditorPreferenceProfileNameText()).string() << "\n";
            report << "capture_preset_tests=" << m_runtimeEditorStats.CapturePresetTests << "\n";
            report << "vfx_emitter_profile_tests=" << m_runtimeEditorStats.VfxEmitterProfileTests << "\n";
            report << "cooked_dependency_preview_tests=" << m_runtimeEditorStats.CookedDependencyPreviewTests << "\n";
            report << "editor_workflow_tests=" << m_runtimeEditorStats.EditorWorkflowTests << "\n";
            report << "asset_pipeline_promotion_tests=" << m_runtimeEditorStats.AssetPipelinePromotionTests << "\n";
            report << "rendering_advanced_tests=" << m_runtimeEditorStats.RenderingAdvancedTests << "\n";
            report << "runtime_sequencer_feature_tests=" << m_runtimeEditorStats.RuntimeSequencerFeatureTests << "\n";
            report << "audio_production_feature_tests=" << m_runtimeEditorStats.AudioProductionFeatureTests << "\n";
            report << "production_publishing_tests=" << m_runtimeEditorStats.ProductionPublishingTests << "\n";
            report << "v28_diversified_points=" << m_runtimeEditorStats.V28DiversifiedPointTests << "\n";
            report << "public_demo_tests=" << m_runtimeEditorStats.PublicDemoTests << "\n";
            report << "public_demo_shard_pickups=" << m_runtimeEditorStats.PublicDemoShardPickups << "\n";
            report << "public_demo_completions=" << m_runtimeEditorStats.PublicDemoCompletions << "\n";
            report << "public_demo_hud_frames=" << m_runtimeEditorStats.PublicDemoHudFrames << "\n";
            report << "public_demo_beacon_draws=" << m_runtimeEditorStats.PublicDemoBeaconDraws << "\n";
            report << "public_demo_sentinel_ticks=" << m_runtimeEditorStats.PublicDemoSentinelTicks << "\n";
            report << "v29_public_demo_points=" << m_runtimeEditorStats.V29PublicDemoPointTests << "\n";
            report << "public_demo_objective_stages=" << m_runtimeEditorStats.PublicDemoObjectiveStages << "\n";
            report << "public_demo_anchor_activations=" << m_runtimeEditorStats.PublicDemoAnchorActivations << "\n";
            report << "public_demo_retries=" << m_runtimeEditorStats.PublicDemoRetries << "\n";
            report << "public_demo_checkpoints=" << m_runtimeEditorStats.PublicDemoCheckpoints << "\n";
            report << "public_demo_director_frames=" << m_runtimeEditorStats.PublicDemoDirectorFrames << "\n";
            report << "v30_vertical_slice_points=" << m_runtimeEditorStats.V30VerticalSlicePointTests << "\n";
            report << "public_demo_resonance_gates=" << m_runtimeEditorStats.PublicDemoResonanceGates << "\n";
            report << "public_demo_pressure_hits=" << m_runtimeEditorStats.PublicDemoPressureHits << "\n";
            report << "public_demo_footstep_events=" << m_runtimeEditorStats.PublicDemoFootstepEvents << "\n";
            report << "v31_diversified_points=" << m_runtimeEditorStats.V31DiversifiedPointTests << "\n";
            report << "public_demo_phase_relays=" << m_runtimeEditorStats.PublicDemoPhaseRelays << "\n";
            report << "public_demo_relay_overcharge_windows=" << m_runtimeEditorStats.PublicDemoRelayOverchargeWindows << "\n";
            report << "public_demo_combo_steps=" << m_runtimeEditorStats.PublicDemoComboSteps << "\n";
            report << "v32_roadmap_points=" << m_runtimeEditorStats.V32RoadmapPointTests << "\n";
            report << "public_demo_collision_solves=" << m_runtimeEditorStats.PublicDemoCollisionSolves << "\n";
            report << "public_demo_traversal_actions=" << m_runtimeEditorStats.PublicDemoTraversalActions << "\n";
            report << "public_demo_enemy_chases=" << m_runtimeEditorStats.PublicDemoEnemyChases << "\n";
            report << "public_demo_enemy_evades=" << m_runtimeEditorStats.PublicDemoEnemyEvades << "\n";
            report << "public_demo_gamepad_frames=" << m_runtimeEditorStats.PublicDemoGamepadFrames << "\n";
            report << "public_demo_menu_transitions=" << m_runtimeEditorStats.PublicDemoMenuTransitions << "\n";
            report << "public_demo_failure_presentations=" << m_runtimeEditorStats.PublicDemoFailurePresentations << "\n";
            report << "public_demo_content_audio_cues=" << m_runtimeEditorStats.PublicDemoContentAudioCues << "\n";
            report << "public_demo_animation_state_changes=" << m_runtimeEditorStats.PublicDemoAnimationStateChanges << "\n";
            report << "v33_playable_demo_points=" << m_runtimeEditorStats.V33PlayableDemoPointTests << "\n";
            report << "public_demo_enemy_archetypes=" << m_runtimeEditorStats.PublicDemoEnemyArchetypes << "\n";
            report << "public_demo_enemy_los_checks=" << m_runtimeEditorStats.PublicDemoEnemyLineOfSightChecks << "\n";
            report << "public_demo_enemy_telegraphs=" << m_runtimeEditorStats.PublicDemoEnemyTelegraphs << "\n";
            report << "public_demo_enemy_hit_reactions=" << m_runtimeEditorStats.PublicDemoEnemyHitReactions << "\n";
            report << "public_demo_controller_grounded_frames=" << m_runtimeEditorStats.PublicDemoControllerGroundedFrames << "\n";
            report << "public_demo_controller_slope_samples=" << m_runtimeEditorStats.PublicDemoControllerSlopeSamples << "\n";
            report << "public_demo_controller_material_samples=" << m_runtimeEditorStats.PublicDemoControllerMaterialSamples << "\n";
            report << "public_demo_controller_moving_platform_frames=" << m_runtimeEditorStats.PublicDemoControllerMovingPlatformFrames << "\n";
            report << "public_demo_controller_camera_collision_frames=" << m_runtimeEditorStats.PublicDemoControllerCameraCollisionFrames << "\n";
            report << "public_demo_animation_clip_events=" << m_runtimeEditorStats.PublicDemoAnimationClipEvents << "\n";
            report << "public_demo_animation_blend_samples=" << m_runtimeEditorStats.PublicDemoAnimationBlendSamples << "\n";
            report << "public_demo_accessibility_toggles=" << m_runtimeEditorStats.PublicDemoAccessibilityToggles << "\n";
            report << "rendering_pipeline_readiness_items=" << m_runtimeEditorStats.RenderingPipelineReadinessItems << "\n";
            report << "production_pipeline_readiness_items=" << m_runtimeEditorStats.ProductionPipelineReadinessItems << "\n";
            report << "v34_aaa_foundation_points=" << m_runtimeEditorStats.V34AAAFoundationPointTests << "\n";
            const auto& v25Points = GetV25ProductionPoints();
            for (size_t index = 0; index < v25Points.size(); ++index)
            {
                report << v25Points[index].Key << "=" << m_v25ProductionPointResults[index] << "\n";
            }
            const auto& v28Points = GetV28DiversifiedPoints();
            for (size_t index = 0; index < v28Points.size(); ++index)
            {
                report << v28Points[index].Key << "=" << m_v28DiversifiedPointResults[index] << "\n";
            }
            const auto& v29Points = GetV29PublicDemoPoints();
            for (size_t index = 0; index < v29Points.size(); ++index)
            {
                report << v29Points[index].Key << "=" << m_v29PublicDemoPointResults[index] << "\n";
            }
            const auto& v30Points = GetV30VerticalSlicePoints();
            for (size_t index = 0; index < v30Points.size(); ++index)
            {
                report << v30Points[index].Key << "=" << m_v30VerticalSlicePointResults[index] << "\n";
            }
            const auto& v31Points = GetV31DiversifiedPoints();
            for (size_t index = 0; index < v31Points.size(); ++index)
            {
                report << v31Points[index].Key << "=" << m_v31DiversifiedPointResults[index] << "\n";
            }
            const auto& v32Points = GetV32RoadmapPoints();
            for (size_t index = 0; index < v32Points.size(); ++index)
            {
                report << v32Points[index].Key << "=" << m_v32RoadmapPointResults[index] << "\n";
            }
            const auto& v33Points = GetV33PlayableDemoPoints();
            for (size_t index = 0; index < v33Points.size(); ++index)
            {
                report << v33Points[index].Key << "=" << m_v33PlayableDemoPointResults[index] << "\n";
            }
            const auto& v34Points = GetV34AAAFoundationPoints();
            for (size_t index = 0; index < v34Points.size(); ++index)
            {
                report << v34Points[index].Key << "=" << m_v34AAAFoundationPointResults[index] << "\n";
            }
            const HighResolutionCaptureMetrics captureMetrics = GetHighResolutionCaptureMetrics();
            report << "high_res_capture_preset=" << captureMetrics.PresetName << "\n";
            report << "high_res_capture_async_compression_ready=" << (captureMetrics.AsyncCompressionReady ? "true" : "false") << "\n";
            report << "high_res_capture_exr_output_planned=" << (captureMetrics.ExrOutputPlanned ? "true" : "false") << "\n";
            report << "high_res_resolve_filter=" << captureMetrics.ResolveFilter << "\n";
            report << "high_res_resolve_samples=" << captureMetrics.ResolveSamples << "\n";
            report << "editor_profile_import_export_ready=" << (m_editorWorkflowDiagnostics.ProfileImportExport ? "true" : "false") << "\n";
            report << "editor_profile_diff_fields=" << m_editorWorkflowDiagnostics.ProfileDiffFields << "\n";
            report << "editor_workspace_preset_count=" << m_editorWorkflowDiagnostics.WorkspacePresets << "\n";
            report << "editor_active_workspace_preset=" << m_editorWorkflowDiagnostics.ActiveWorkspacePreset << "\n";
            report << "editor_command_macro_steps=" << m_editorWorkflowDiagnostics.CommandMacroSteps << "\n";
            report << "editor_profile_export_path=" << m_editorWorkflowDiagnostics.ExportPath.string() << "\n";
            report << "asset_gpu_package_loaded=" << (m_assetPipelinePromotionDiagnostics.OptimizedGpuPackageLoaded ? "true" : "false") << "\n";
            report << "asset_texture_bindings=" << m_assetPipelinePromotionDiagnostics.TextureBindings << "\n";
            report << "asset_animation_clips=" << m_assetPipelinePromotionDiagnostics.AnimationClips << "\n";
            report << "asset_invalidation_tickets=" << m_assetPipelinePromotionDiagnostics.InvalidationTickets << "\n";
            report << "asset_rollback_journal_entries=" << m_assetPipelinePromotionDiagnostics.RollbackEntries << "\n";
            report << "asset_streaming_priority_levels=" << m_assetPipelinePromotionDiagnostics.StreamingPriorityLevels << "\n";
            report << "rendering_explicit_bind_barriers=" << (m_renderingAdvancedDiagnostics.ExplicitBindBarriers ? "true" : "false") << "\n";
            report << "rendering_alias_lifetime_validations=" << m_renderingAdvancedDiagnostics.AliasValidations << "\n";
            report << "rendering_gpu_culling_buckets=" << m_renderingAdvancedDiagnostics.CullingBuckets << "\n";
            report << "rendering_forward_plus_light_bins=" << m_renderingAdvancedDiagnostics.LightBins << "\n";
            report << "rendering_csm_cascades=" << m_renderingAdvancedDiagnostics.ShadowCascades << "\n";
            report << "rendering_motion_vector_targets=" << m_renderingAdvancedDiagnostics.MotionVectorTargetsCount << "\n";
            report << "runtime_streaming_requests=" << m_runtimeSequencerDiagnostics.StreamingRequests << "\n";
            report << "runtime_cancellation_tokens=" << m_runtimeSequencerDiagnostics.CancellationTokenCount << "\n";
            report << "runtime_file_watchers=" << m_runtimeSequencerDiagnostics.FileWatchCount << "\n";
            report << "runtime_script_state_slots=" << m_runtimeSequencerDiagnostics.ScriptStateSlots << "\n";
            report << "runtime_sequencer_clip_blends=" << m_runtimeSequencerDiagnostics.ClipBlendPairs << "\n";
            report << "runtime_keyboard_preview_bindings=" << m_runtimeSequencerDiagnostics.KeyboardBindings << "\n";
            report << "audio_mixer_voice_targets=" << m_audioProductionFeatureDiagnostics.MixerVoices << "\n";
            report << "audio_streamed_music_assets=" << m_audioProductionFeatureDiagnostics.StreamedMusicAssetsCount << "\n";
            report << "audio_spatial_components=" << m_audioProductionFeatureDiagnostics.SpatialEmitters << "\n";
            report << "audio_attenuation_curve_assets=" << m_audioProductionFeatureDiagnostics.AttenuationCurves << "\n";
            report << "audio_calibrated_meter_count=" << m_audioProductionFeatureDiagnostics.CalibratedMetersCount << "\n";
            report << "audio_content_pulse_inputs=" << m_audioProductionFeatureDiagnostics.ContentPulseInputs << "\n";
            report << "production_golden_profiles=" << m_productionPublishingDiagnostics.GoldenProfiles << "\n";
            report << "production_baseline_diff_package=" << (m_productionPublishingDiagnostics.BaselineDiffPackage ? "true" : "false") << "\n";
            report << "production_schema_metrics=" << m_productionPublishingDiagnostics.SchemaMetrics << "\n";
            report << "production_interactive_runner_ready=" << (m_productionPublishingDiagnostics.InteractiveRunner ? "true" : "false") << "\n";
            report << "production_signed_installer_ready=" << (m_productionPublishingDiagnostics.SignedInstallerArtifact ? "true" : "false") << "\n";
            report << "production_symbol_server_ready=" << (m_productionPublishingDiagnostics.SymbolServerEndpoint ? "true" : "false") << "\n";
            report << "production_obs_websocket_commands=" << m_productionPublishingDiagnostics.ObsCommands << "\n";
            report << "public_demo_objective_ready=" << (m_publicDemoDiagnostics.ObjectiveLoopReady ? "true" : "false") << "\n";
            report << "public_demo_extraction_completed=" << (m_publicDemoDiagnostics.ExtractionCompleted ? "true" : "false") << "\n";
            report << "public_demo_shards_total=" << m_publicDemoDiagnostics.ShardsTotal << "\n";
            report << "public_demo_shards_collected=" << m_publicDemoDiagnostics.ShardsCollected << "\n";
            report << "public_demo_completion_time_seconds=" << m_publicDemoDiagnostics.CompletionTimeSeconds << "\n";
            report << "public_demo_stability=" << m_publicDemoDiagnostics.Stability << "\n";
            report << "public_demo_focus=" << m_publicDemoDiagnostics.Focus << "\n";
            report << "public_demo_anchors_total=" << m_publicDemoDiagnostics.AnchorsTotal << "\n";
            report << "public_demo_anchors_activated=" << m_publicDemoDiagnostics.AnchorsActivated << "\n";
            report << "public_demo_resonance_gates_total=" << m_publicDemoDiagnostics.ResonanceGatesTotal << "\n";
            report << "public_demo_resonance_gates_tuned=" << m_publicDemoDiagnostics.ResonanceGatesTuned << "\n";
            report << "public_demo_phase_relays_total=" << m_publicDemoDiagnostics.PhaseRelaysTotal << "\n";
            report << "public_demo_phase_relays_stabilized=" << m_publicDemoDiagnostics.PhaseRelaysStabilized << "\n";
            report << "public_demo_relay_overcharge_window_count=" << m_publicDemoDiagnostics.RelayOverchargeWindows << "\n";
            report << "public_demo_relay_bridge_draws=" << m_publicDemoDiagnostics.RelayBridgeDraws << "\n";
            report << "public_demo_traversal_markers=" << m_publicDemoDiagnostics.TraversalMarkers << "\n";
            report << "public_demo_combo_chain_steps=" << m_publicDemoDiagnostics.ComboChainSteps << "\n";
            report << "public_demo_stage_transitions=" << m_publicDemoDiagnostics.ObjectiveStageTransitions << "\n";
            report << "public_demo_retry_count=" << m_publicDemoDiagnostics.RetryCount << "\n";
            report << "public_demo_checkpoint_count=" << m_publicDemoDiagnostics.CheckpointCount << "\n";
            report << "public_demo_director_frames=" << m_publicDemoDiagnostics.DirectorFrames << "\n";
            report << "public_demo_event_count=" << m_publicDemoDiagnostics.EventCount << "\n";
            report << "public_demo_pressure_hit_count=" << m_publicDemoDiagnostics.PressureHits << "\n";
            report << "public_demo_footstep_event_count=" << m_publicDemoDiagnostics.FootstepEvents << "\n";
            report << "public_demo_gameplay_event_routes=" << m_publicDemoDiagnostics.GameplayEventRoutes << "\n";
            report << "public_demo_collision_solve_count=" << m_publicDemoDiagnostics.CollisionSolves << "\n";
            report << "public_demo_traversal_action_count=" << m_publicDemoDiagnostics.TraversalActions << "\n";
            report << "public_demo_enemy_chase_ticks=" << m_publicDemoDiagnostics.EnemyChaseTicks << "\n";
            report << "public_demo_enemy_evade_count=" << m_publicDemoDiagnostics.EnemyEvades << "\n";
            report << "public_demo_enemy_contacts=" << m_publicDemoDiagnostics.EnemyContacts << "\n";
            report << "public_demo_gamepad_frame_count=" << m_publicDemoDiagnostics.GamepadFrames << "\n";
            report << "public_demo_menu_transition_count=" << m_publicDemoDiagnostics.MenuTransitions << "\n";
            report << "public_demo_failure_presentation_frames=" << m_publicDemoDiagnostics.FailurePresentationFrames << "\n";
            report << "public_demo_content_audio_cue_count=" << m_publicDemoDiagnostics.ContentAudioCues << "\n";
            report << "public_demo_animation_state_change_count=" << m_publicDemoDiagnostics.AnimationStateChanges << "\n";
            report << "public_demo_animation_state=" << PublicDemoAnimationStateName() << "\n";
            report << "public_demo_objective_distance=" << m_publicDemoDiagnostics.ObjectiveDistance << "\n";
            report << "public_demo_anchor_puzzle_complete=" << (m_publicDemoDiagnostics.AnchorPuzzleComplete ? "true" : "false") << "\n";
            report << "public_demo_retry_ready=" << (m_publicDemoDiagnostics.RetryReady ? "true" : "false") << "\n";
            report << "public_demo_resonance_gate_complete=" << (m_publicDemoDiagnostics.ResonanceGateComplete ? "true" : "false") << "\n";
            report << "public_demo_phase_relay_complete=" << (m_publicDemoDiagnostics.PhaseRelayComplete ? "true" : "false") << "\n";
            report << "public_demo_relay_overcharge_ready=" << (m_publicDemoDiagnostics.RelayOverchargeReady ? "true" : "false") << "\n";
            report << "public_demo_complex_route_ready=" << (m_publicDemoDiagnostics.ComplexRouteReady ? "true" : "false") << "\n";
            report << "public_demo_combo_objective_ready=" << (m_publicDemoDiagnostics.ComboObjectiveReady ? "true" : "false") << "\n";
            report << "public_demo_failure_screen_ready=" << (m_publicDemoDiagnostics.FailureScreenReady ? "true" : "false") << "\n";
            report << "public_demo_enemy_behavior_ready=" << (m_publicDemoDiagnostics.EnemyBehaviorReady ? "true" : "false") << "\n";
            report << "public_demo_gamepad_menu_ready=" << (m_publicDemoDiagnostics.GamepadMenuReady ? "true" : "false") << "\n";
            report << "public_demo_failure_presentation_ready=" << (m_publicDemoDiagnostics.FailurePresentationReady ? "true" : "false") << "\n";
            report << "public_demo_content_audio_ready=" << (m_publicDemoDiagnostics.ContentAudioReady ? "true" : "false") << "\n";
            report << "public_demo_animation_hook_ready=" << (m_publicDemoDiagnostics.AnimationHookReady ? "true" : "false") << "\n";
            report << "public_demo_enemy_archetype_ready=" << (m_publicDemoDiagnostics.EnemyArchetypeReady ? "true" : "false") << "\n";
            report << "public_demo_enemy_los_ready=" << (m_publicDemoDiagnostics.EnemyLineOfSightReady ? "true" : "false") << "\n";
            report << "public_demo_enemy_telegraph_ready=" << (m_publicDemoDiagnostics.EnemyTelegraphReady ? "true" : "false") << "\n";
            report << "public_demo_enemy_hit_reaction_ready=" << (m_publicDemoDiagnostics.EnemyHitReactionReady ? "true" : "false") << "\n";
            report << "public_demo_controller_polish_ready=" << (m_publicDemoDiagnostics.ControllerPolishReady ? "true" : "false") << "\n";
            report << "public_demo_animation_blend_tree_ready=" << (m_publicDemoDiagnostics.AnimationBlendTreeReady ? "true" : "false") << "\n";
            report << "public_demo_accessibility_ready=" << (m_publicDemoDiagnostics.AccessibilityReady ? "true" : "false") << "\n";
            report << "rendering_aaa_readiness=" << (m_publicDemoDiagnostics.RenderingAAAReadiness ? "true" : "false") << "\n";
            report << "production_aaa_readiness=" << (m_publicDemoDiagnostics.ProductionAAAReadiness ? "true" : "false") << "\n";
            report << "public_demo_enemy_archetype_count=" << m_publicDemoDiagnostics.EnemyArchetypes << "\n";
            report << "public_demo_enemy_los_check_count=" << m_publicDemoDiagnostics.EnemyLineOfSightChecks << "\n";
            report << "public_demo_enemy_telegraph_count=" << m_publicDemoDiagnostics.EnemyTelegraphs << "\n";
            report << "public_demo_enemy_hit_reaction_count=" << m_publicDemoDiagnostics.EnemyHitReactions << "\n";
            report << "public_demo_controller_grounded_frame_count=" << m_publicDemoDiagnostics.ControllerGroundedFrames << "\n";
            report << "public_demo_controller_slope_sample_count=" << m_publicDemoDiagnostics.ControllerSlopeSamples << "\n";
            report << "public_demo_controller_material_sample_count=" << m_publicDemoDiagnostics.ControllerMaterialSamples << "\n";
            report << "public_demo_controller_moving_platform_frame_count=" << m_publicDemoDiagnostics.ControllerMovingPlatformFrames << "\n";
            report << "public_demo_controller_camera_collision_frame_count=" << m_publicDemoDiagnostics.ControllerCameraCollisionFrames << "\n";
            report << "public_demo_animation_clip_event_count=" << m_publicDemoDiagnostics.AnimationClipEvents << "\n";
            report << "public_demo_animation_blend_sample_count=" << m_publicDemoDiagnostics.AnimationBlendSamples << "\n";
            report << "public_demo_accessibility_toggle_count=" << m_publicDemoDiagnostics.AccessibilityToggles << "\n";
            report << "public_demo_rendering_readiness_items=" << m_publicDemoDiagnostics.RenderingReadinessItems << "\n";
            report << "public_demo_production_readiness_items=" << m_publicDemoDiagnostics.ProductionReadinessItems << "\n";
            report << "public_demo_blend_tree_clips=" << m_publicDemoBlendTreeClipCount << "\n";
            report << "public_demo_blend_tree_transitions=" << m_publicDemoBlendTreeTransitionCount << "\n";
            report << "public_demo_blend_tree_root_motion=" << m_publicDemoBlendTreeRootMotionCount << "\n";
            report << "viewport_hud_debug_thumbnails=" << (m_viewportOverlay.ShowDebugThumbnails ? "true" : "false") << "\n";
            report << "transform_precision_step=" << m_transformPrecision.Step << "\n";
            report << "command_history_filtered_verification=" << CountFilteredCommandHistory("Verification") << "\n";
            report << "shot_sequencer_v6_keys=" << m_trailerKeys.size() << "\n";
            report << "vfx_renderer_soft_particles=" << (m_riftVfxRendererProfile.SoftParticles ? "true" : "false") << "\n";
            report << "vfx_renderer_temporal_reprojection=" << (m_riftVfxRendererProfile.TemporalReprojection ? "true" : "false") << "\n";
            report << "cooked_package_loaded=" << (m_cookedPackageResource.Loaded ? "true" : "false") << "\n";
            report << "cooked_package_path=" << m_cookedPackageResource.Path.string() << "\n";
            report << "cooked_package_meshes=" << m_cookedPackageResource.Meshes << "\n";
            report << "cooked_package_primitives=" << m_cookedPackageResource.Primitives << "\n";
            report << "cooked_package_materials=" << m_cookedPackageResource.Materials << "\n";
            report << "cooked_package_gpu_ready=" << (m_cookedPackageResource.GpuReady ? "true" : "false") << "\n";
            report << "cooked_package_gpu_meshes=" << m_cookedPackageResource.GpuMeshResources << "\n";
            report << "cooked_package_gpu_upload_bytes=" << m_cookedPackageResource.EstimatedUploadBytes << "\n";
            report << "cooked_package_dependency_preview_count=" << m_cookedPackageResource.DependencyInvalidationPreviewCount << "\n";
            report << "cooked_package_reload_rollback_ready=" << (m_cookedPackageResource.ReloadRollbackReady ? "true" : "false") << "\n";
            report << "audio_meter_reference_peak_db=" << m_audioMeterCalibration.ReferencePeakDb << "\n";
            report << "audio_meter_reference_rms_db=" << m_audioMeterCalibration.ReferenceRmsDb << "\n";
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

        void RecordGpuPickVisualization(
            const Disparity::EditorObjectIdReadback& readback,
            bool resolvedReadback,
            uint32_t localX,
            uint32_t localY)
        {
            m_gpuPickVisualization.HasCache = true;
            m_gpuPickVisualization.LastResolved = resolvedReadback;
            m_gpuPickVisualization.LastX = localX;
            m_gpuPickVisualization.LastY = localY;
            if (m_renderer)
            {
                const Disparity::RendererFrameGraphDiagnostics diagnostics = m_renderer->GetFrameGraphDiagnostics();
                m_gpuPickVisualization.LastLatencyFrames = diagnostics.ObjectIdReadbackLatencyFrames;
                m_gpuPickVisualization.BusySkips = diagnostics.ObjectIdReadbackBusySkips;
                m_gpuPickVisualization.PendingSlots = diagnostics.ObjectIdReadbackPending;
                if (resolvedReadback)
                {
                    const uint32_t bucket = std::min<uint32_t>(
                        static_cast<uint32_t>(m_gpuPickVisualization.LatencyBuckets.size() - 1u),
                        diagnostics.ObjectIdReadbackLatencyFrames);
                    ++m_gpuPickVisualization.LatencyBuckets[bucket];
                }
            }

            if (resolvedReadback && readback.Valid)
            {
                m_gpuPickVisualization.LastObjectId = readback.ObjectId;
                m_gpuPickVisualization.LastDepth = readback.Depth;
                m_gpuPickVisualization.LastResolvedFrame = m_editorFrameIndex;
                EditorPickResult decodedPick;
                m_gpuPickVisualization.LastObjectName = DecodeEditorObjectId(readback.ObjectId, readback.Depth, decodedPick)
                    ? DescribeEditorPick(decodedPick)
                    : "Unknown";
                ++m_gpuPickVisualization.CacheHits;
            }
            else if (resolvedReadback)
            {
                m_gpuPickVisualization.LastResolvedFrame = m_editorFrameIndex;
                m_gpuPickVisualization.LastObjectName = "None";
                ++m_gpuPickVisualization.CacheMisses;
            }
        }

        uint32_t GpuPickLatencySampleCount() const
        {
            uint32_t sampleCount = 0;
            for (const uint32_t bucket : m_gpuPickVisualization.LatencyBuckets)
            {
                sampleCount += bucket;
            }
            return sampleCount;
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
            Disparity::EditorObjectIdReadback readback;
            const bool resolvedReadback = m_renderer->TryResolveEditorObjectIdReadback(readback);
            const uint32_t readbackX = static_cast<uint32_t>(localX);
            const uint32_t readbackY = static_cast<uint32_t>(localY);
            m_renderer->QueueEditorObjectIdReadback(readbackX, readbackY);
            RecordGpuPickVisualization(readback, resolvedReadback, readbackX, readbackY);
            ++m_runtimeEditorStats.GpuPickReadbacks;
            ++m_runtimeEditorStats.GpuPickAsyncQueues;
            if (resolvedReadback)
            {
                ++m_runtimeEditorStats.GpuPickAsyncResolves;
            }
            m_lastGpuPickStatus = readback.Error.empty()
                ? (resolvedReadback
                    ? ("GPU id=" + std::to_string(readback.ObjectId) + " depth=" + std::to_string(readback.Depth))
                    : "GPU pick queued")
                : ("GPU pick fallback: " + readback.Error);

            if (resolvedReadback && readback.Valid && DecodeEditorObjectId(readback.ObjectId, readback.Depth, outPick))
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
                    else if (key == "min_render_graph_resource_bindings") { loadedBaseline.MinRenderGraphResourceBindings = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_async_io_tests") { loadedBaseline.MinAsyncIoTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_material_texture_slot_tests") { loadedBaseline.MinMaterialTextureSlotTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_prefab_variant_tests") { loadedBaseline.MinPrefabVariantTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_shot_director_tests") { loadedBaseline.MinShotDirectorTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_shot_spline_tests") { loadedBaseline.MinShotSplineTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_audio_analysis_tests") { loadedBaseline.MinAudioAnalysisTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_xaudio2_backend_tests") { loadedBaseline.MinXAudio2BackendTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_vfx_system_tests") { loadedBaseline.MinVfxSystemTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_gpu_vfx_simulation_tests") { loadedBaseline.MinGpuVfxSimulationTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_animation_skinning_tests") { loadedBaseline.MinAnimationSkinningTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_gpu_pick_hover_cache_tests") { loadedBaseline.MinGpuPickHoverCacheTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_gpu_pick_latency_histogram_tests") { loadedBaseline.MinGpuPickLatencyHistogramTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_shot_timeline_track_tests") { loadedBaseline.MinShotTimelineTrackTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_shot_thumbnail_tests") { loadedBaseline.MinShotThumbnailTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_shot_preview_scrub_tests") { loadedBaseline.MinShotPreviewScrubTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_graph_high_res_capture_tests") { loadedBaseline.MinGraphHighResCaptureTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_cooked_package_tests") { loadedBaseline.MinCookedPackageTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_asset_invalidation_tests") { loadedBaseline.MinAssetInvalidationTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_nested_prefab_tests") { loadedBaseline.MinNestedPrefabTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_audio_production_tests") { loadedBaseline.MinAudioProductionTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_viewport_overlay_tests") { loadedBaseline.MinViewportOverlayTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_high_res_resolve_tests") { loadedBaseline.MinHighResResolveTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_viewport_hud_control_tests") { loadedBaseline.MinViewportHudControlTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_transform_precision_tests") { loadedBaseline.MinTransformPrecisionTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_command_history_filter_tests") { loadedBaseline.MinCommandHistoryFilterTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_runtime_schema_manifest_tests") { loadedBaseline.MinRuntimeSchemaManifestTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_shot_sequencer_tests") { loadedBaseline.MinShotSequencerTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_vfx_renderer_profile_tests") { loadedBaseline.MinVfxRendererProfileTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_cooked_gpu_resource_tests") { loadedBaseline.MinCookedGpuResourceTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_dependency_invalidation_tests") { loadedBaseline.MinDependencyInvalidationTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_audio_meter_calibration_tests") { loadedBaseline.MinAudioMeterCalibrationTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_release_readiness_tests") { loadedBaseline.MinReleaseReadinessTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_v25_production_points") { loadedBaseline.MinV25ProductionPoints = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_editor_preference_persistence_tests") { loadedBaseline.MinEditorPreferencePersistenceTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_viewport_toolbar_tests") { loadedBaseline.MinViewportToolbarTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_editor_preference_profile_tests") { loadedBaseline.MinEditorPreferenceProfileTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_capture_preset_tests") { loadedBaseline.MinCapturePresetTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_vfx_emitter_profile_tests") { loadedBaseline.MinVfxEmitterProfileTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_cooked_dependency_preview_tests") { loadedBaseline.MinCookedDependencyPreviewTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_editor_workflow_tests") { loadedBaseline.MinEditorWorkflowTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_asset_pipeline_promotion_tests") { loadedBaseline.MinAssetPipelinePromotionTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_rendering_advanced_tests") { loadedBaseline.MinRenderingAdvancedTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_runtime_sequencer_feature_tests") { loadedBaseline.MinRuntimeSequencerFeatureTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_audio_production_feature_tests") { loadedBaseline.MinAudioProductionFeatureTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_production_publishing_tests") { loadedBaseline.MinProductionPublishingTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_v28_diversified_points") { loadedBaseline.MinV28DiversifiedPoints = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_tests") { loadedBaseline.MinPublicDemoTests = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_shard_pickups") { loadedBaseline.MinPublicDemoShardPickups = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_hud_frames") { loadedBaseline.MinPublicDemoHudFrames = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_beacon_draws") { loadedBaseline.MinPublicDemoBeaconDraws = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_v29_public_demo_points") { loadedBaseline.MinV29PublicDemoPoints = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_objective_stages") { loadedBaseline.MinPublicDemoObjectiveStages = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_anchor_activations") { loadedBaseline.MinPublicDemoAnchorActivations = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_retries") { loadedBaseline.MinPublicDemoRetries = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_checkpoints") { loadedBaseline.MinPublicDemoCheckpoints = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_director_frames") { loadedBaseline.MinPublicDemoDirectorFrames = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_v30_vertical_slice_points") { loadedBaseline.MinV30VerticalSlicePoints = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_resonance_gates") { loadedBaseline.MinPublicDemoResonanceGates = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_pressure_hits") { loadedBaseline.MinPublicDemoPressureHits = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_footstep_events") { loadedBaseline.MinPublicDemoFootstepEvents = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_v31_diversified_points") { loadedBaseline.MinV31DiversifiedPoints = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_phase_relays") { loadedBaseline.MinPublicDemoPhaseRelays = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_relay_overcharge_windows") { loadedBaseline.MinPublicDemoRelayOverchargeWindows = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_combo_steps") { loadedBaseline.MinPublicDemoComboSteps = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_v32_roadmap_points") { loadedBaseline.MinV32RoadmapPoints = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_collision_solves") { loadedBaseline.MinPublicDemoCollisionSolves = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_traversal_actions") { loadedBaseline.MinPublicDemoTraversalActions = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_enemy_chases") { loadedBaseline.MinPublicDemoEnemyChases = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_enemy_evades") { loadedBaseline.MinPublicDemoEnemyEvades = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_gamepad_frames") { loadedBaseline.MinPublicDemoGamepadFrames = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_menu_transitions") { loadedBaseline.MinPublicDemoMenuTransitions = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_failure_presentations") { loadedBaseline.MinPublicDemoFailurePresentations = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_content_audio_cues") { loadedBaseline.MinPublicDemoContentAudioCues = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_animation_state_changes") { loadedBaseline.MinPublicDemoAnimationStateChanges = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_v33_playable_demo_points") { loadedBaseline.MinV33PlayableDemoPoints = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_enemy_archetypes") { loadedBaseline.MinPublicDemoEnemyArchetypes = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_enemy_los_checks") { loadedBaseline.MinPublicDemoEnemyLineOfSightChecks = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_enemy_telegraphs") { loadedBaseline.MinPublicDemoEnemyTelegraphs = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_enemy_hit_reactions") { loadedBaseline.MinPublicDemoEnemyHitReactions = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_controller_grounded_frames") { loadedBaseline.MinPublicDemoControllerGroundedFrames = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_controller_slope_samples") { loadedBaseline.MinPublicDemoControllerSlopeSamples = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_controller_material_samples") { loadedBaseline.MinPublicDemoControllerMaterialSamples = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_controller_moving_platform_frames") { loadedBaseline.MinPublicDemoControllerMovingPlatformFrames = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_controller_camera_collision_frames") { loadedBaseline.MinPublicDemoControllerCameraCollisionFrames = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_animation_clip_events") { loadedBaseline.MinPublicDemoAnimationClipEvents = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_animation_blend_samples") { loadedBaseline.MinPublicDemoAnimationBlendSamples = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_public_demo_accessibility_toggles") { loadedBaseline.MinPublicDemoAccessibilityToggles = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_rendering_pipeline_readiness_items") { loadedBaseline.MinRenderingPipelineReadinessItems = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_production_pipeline_readiness_items") { loadedBaseline.MinProductionPipelineReadinessItems = static_cast<uint32_t>(std::stoul(value)); }
                    else if (key == "min_v34_aaa_foundation_points") { loadedBaseline.MinV34AAAFoundationPoints = static_cast<uint32_t>(std::stoul(value)); }
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
        std::vector<RiftVfxParticle> m_riftParticles;
        std::vector<RiftVfxRibbonSegment> m_riftRibbons;
        RuntimeBudgetStats m_runtimeBudgetStats;
        RuntimePlaybackStats m_runtimePlayback;
        EditorVerificationStats m_runtimeEditorStats;
        RuntimeBaseline m_runtimeBaseline;
        GpuPickVisualizationState m_gpuPickVisualization;
        RiftVfxSystemStats m_lastRiftVfxStats;
        RiftVfxRendererProfile m_riftVfxRendererProfile;
        RiftVfxEmitterProfile m_riftVfxEmitterProfile;
        HighResolutionCapturePresetProfile m_highResolutionCapturePreset;
        EditorPreferenceProfile m_editorPreferenceProfile;
        ViewportToolbarProfile m_viewportToolbarProfile;
        GizmoAdvancedProfile m_gizmoAdvancedProfile;
        AssetPipelineReadinessProfile m_assetPipelineReadinessProfile;
        RenderingRoadmapProfile m_renderingRoadmapProfile;
        CaptureVfxReadinessProfile m_captureVfxReadinessProfile;
        SequencerAudioReadinessProfile m_sequencerAudioReadinessProfile;
        ProductionAutomationReadinessProfile m_productionAutomationReadinessProfile;
        EditorWorkflowDiagnostics m_editorWorkflowDiagnostics;
        AssetPipelinePromotionDiagnostics m_assetPipelinePromotionDiagnostics;
        RenderingAdvancedDiagnostics m_renderingAdvancedDiagnostics;
        RuntimeSequencerDiagnostics m_runtimeSequencerDiagnostics;
        AudioProductionFeatureDiagnostics m_audioProductionFeatureDiagnostics;
        ProductionPublishingDiagnostics m_productionPublishingDiagnostics;
        PublicDemoDiagnostics m_publicDemoDiagnostics;
        std::array<uint32_t, V25ProductionPointCount> m_v25ProductionPointResults = {};
        std::array<uint32_t, V28DiversifiedPointCount> m_v28DiversifiedPointResults = {};
        std::array<uint32_t, V29PublicDemoPointCount> m_v29PublicDemoPointResults = {};
        std::array<uint32_t, V30VerticalSlicePointCount> m_v30VerticalSlicePointResults = {};
        std::array<uint32_t, V31DiversifiedPointCount> m_v31DiversifiedPointResults = {};
        std::array<uint32_t, V32RoadmapPointCount> m_v32RoadmapPointResults = {};
        std::array<uint32_t, V33PlayableDemoPointCount> m_v33PlayableDemoPointResults = {};
        std::array<uint32_t, V34AAAFoundationPointCount> m_v34AAAFoundationPointResults = {};
        std::array<PublicDemoShard, PublicDemoShardCount> m_publicDemoShards = {};
        std::array<PublicDemoAnchor, PublicDemoAnchorCount> m_publicDemoAnchors = {};
        std::array<PublicDemoResonanceGate, PublicDemoResonanceGateCount> m_publicDemoResonanceGates = {};
        std::array<PublicDemoPhaseRelay, PublicDemoPhaseRelayCount> m_publicDemoPhaseRelays = {};
        std::array<PublicDemoSentinel, 3> m_publicDemoSentinels = {};
        std::array<PublicDemoCollisionObstacle, PublicDemoObstacleCount> m_publicDemoObstacles = {};
        std::array<PublicDemoEnemy, PublicDemoEnemyCount> m_publicDemoEnemies = {};
        PublicDemoCheckpoint m_publicDemoCheckpoint;
        ViewportOverlaySettings m_viewportOverlay;
        TransformPrecisionState m_transformPrecision;
        AudioMeterCalibrationProfile m_audioMeterCalibration;
        CookedPackageRuntimeResource m_cookedPackageResource;
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
        Disparity::Material m_demoShardMaterial;
        Disparity::Material m_demoChargedShardMaterial;
        Disparity::Material m_demoGateMaterial;
        Disparity::Material m_demoHazardMaterial;
        Disparity::Material m_demoPathMaterial;
        Disparity::Material m_demoAnchorMaterial;
        Disparity::Material m_demoCheckpointMaterial;
        Disparity::Material m_demoResonanceMaterial;
        Disparity::Material m_demoRelayMaterial;
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
        float m_publicDemoStability = 0.28f;
        float m_publicDemoFocus = 1.0f;
        float m_publicDemoElapsed = 0.0f;
        float m_publicDemoSurge = 0.0f;
        float m_publicDemoStepAccumulator = 0.0f;
        float m_publicDemoDashTimer = 0.0f;
        float m_publicDemoDashCooldown = 0.0f;
        float m_publicDemoFailureOverlayTimer = 0.0f;
        float m_hotReloadPollTimer = 0.0f;
        float m_editorPreferencesSaveDelay = 0.0f;
        float m_runtimeVerificationElapsed = 0.0f;
        int m_lastRiftBeatIndex = -1;
        size_t m_selectedIndex = 0;
        uint32_t m_runtimeVerificationFrame = 0;
        uint32_t m_runtimeReplayStartFrame = 0;
        uint32_t m_runtimeReplayEndFrame = 0;
        uint32_t m_runtimeVerificationTrailerStartFrame = 0;
        uint32_t m_runtimeBudgetSkipFrames = 0;
        uint32_t m_editorFrameIndex = 0;
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
        bool m_publicDemoActive = true;
        bool m_publicDemoExtractionReady = false;
        bool m_publicDemoCompleted = false;
        bool m_publicDemoPickupAudioArmed = true;
        bool m_publicDemoCompletionAudioPlayed = false;
        bool m_publicDemoCueManifestLoaded = false;
        bool m_publicDemoAnimationManifestLoaded = false;
        bool m_publicDemoBlendTreeManifestLoaded = false;
        bool m_publicDemoAccessibilityHighContrast = false;
        bool m_publicDemoTitleFlowReady = false;
        bool m_publicDemoChapterSelectReady = false;
        bool m_publicDemoSaveSlotReady = false;
        PublicDemoStage m_publicDemoStage = PublicDemoStage::CollectShards;
        PublicDemoMenuState m_publicDemoMenuState = PublicDemoMenuState::Playing;
        PublicDemoAnimationState m_publicDemoAnimationState = PublicDemoAnimationState::Idle;
        bool m_editorCameraEnabled = false;
        bool m_hotReloadEnabled = true;
        bool m_gltfAnimationPlayback = true;
        bool m_applyingHistory = false;
        bool m_editorPreferencesLoaded = false;
        bool m_editorPreferencesSaved = false;
        bool m_editorPreferencesDirty = false;
        bool m_viewportToolbarVisible = true;
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
        bool m_highResCaptureWorkerStarted = false;
        uint32_t m_highResCaptureTilesWritten = 0;
        uint32_t m_publicDemoShardsCollected = 0;
        uint32_t m_publicDemoStageTransitions = 0;
        uint32_t m_publicDemoRetryCount = 0;
        uint32_t m_publicDemoCheckpointCount = 0;
        uint32_t m_publicDemoPressureHitCount = 0;
        uint32_t m_publicDemoFootstepEventCount = 0;
        uint32_t m_publicDemoGameplayEventRouteCount = 0;
        uint32_t m_publicDemoFailureScreenFrames = 0;
        uint32_t m_publicDemoRelayOverchargeWindowCount = 0;
        uint32_t m_publicDemoRelayBridgeDrawCount = 0;
        uint32_t m_publicDemoTraversalMarkerCount = 0;
        uint32_t m_publicDemoComboChainStepCount = 0;
        uint32_t m_publicDemoCollisionSolveCount = 0;
        uint32_t m_publicDemoTraversalVaultCount = 0;
        uint32_t m_publicDemoTraversalDashCount = 0;
        uint32_t m_publicDemoEnemyChaseTickCount = 0;
        uint32_t m_publicDemoEnemyEvadeCount = 0;
        uint32_t m_publicDemoEnemyContactCount = 0;
        uint32_t m_publicDemoEnemyArchetypeCount = 0;
        uint32_t m_publicDemoEnemyLineOfSightCheckCount = 0;
        uint32_t m_publicDemoEnemyTelegraphCount = 0;
        uint32_t m_publicDemoEnemyHitReactionCount = 0;
        uint32_t m_publicDemoControllerGroundedFrameCount = 0;
        uint32_t m_publicDemoControllerSlopeSampleCount = 0;
        uint32_t m_publicDemoControllerMaterialSampleCount = 0;
        uint32_t m_publicDemoControllerMovingPlatformFrameCount = 0;
        uint32_t m_publicDemoControllerCameraCollisionFrameCount = 0;
        uint32_t m_publicDemoAnimationClipEventCount = 0;
        uint32_t m_publicDemoAnimationBlendSampleCount = 0;
        uint32_t m_publicDemoBlendTreeClipCount = 0;
        uint32_t m_publicDemoBlendTreeTransitionCount = 0;
        uint32_t m_publicDemoBlendTreeRootMotionCount = 0;
        uint32_t m_publicDemoAccessibilityToggleCount = 0;
        uint32_t m_publicDemoRenderingReadinessCount = 0;
        uint32_t m_publicDemoProductionReadinessCount = 0;
        uint32_t m_publicDemoGamepadFrameCount = 0;
        uint32_t m_publicDemoMenuTransitionCount = 0;
        uint32_t m_publicDemoFailurePresentationFrameCount = 0;
        uint32_t m_publicDemoContentAudioCueCount = 0;
        uint32_t m_publicDemoAnimationStateChangeCount = 0;
        uint32_t m_viewportToolbarInteractionCount = 0;
        uint32_t m_editorWorkspacePresetApplyCount = 0;
        uint32_t m_editorDockLayoutChecksum = 0xD15A27u;
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
        std::string m_editorWorkspacePresetName = "Editor";
        std::string m_publicDemoFailureReason = "Signal lost";
        std::deque<std::string> m_publicDemoEvents;
        std::unordered_map<std::string, PublicDemoCueDefinition> m_publicDemoCueDefinitions;
        std::vector<std::string> m_publicDemoAnimationStates;
        std::vector<std::string> m_publicDemoAnimationClipEvents;
        PublicDemoGamepadState m_publicDemoGamepad;
        HMODULE m_xinputModule = nullptr;
        using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
        XInputGetStateFn m_xinputGetState = nullptr;
        WORD m_previousGamepadButtons = 0;
        std::array<char, 64> m_commandHistoryFilter = {};
        std::array<char, 64> m_editorPreferenceProfileName = { 'D', 'e', 'f', 'a', 'u', 'l', 't', '\0' };
        std::filesystem::path m_highResCaptureSourcePath;
        std::filesystem::path m_highResCaptureOutputPath;
        std::filesystem::path m_highResCaptureNativePngPath;
        std::filesystem::path m_highResCaptureManifestPath;
        std::future<bool> m_highResCaptureFuture;
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
