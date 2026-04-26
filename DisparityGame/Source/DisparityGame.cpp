#include <Disparity/Disparity.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
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
        uint32_t TargetFrames = 90;
        std::filesystem::path ReportPath = "Saved/Verification/runtime_verify.txt";
    };

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

            if (Disparity::GltfLoader::LoadScene("Assets/Meshes/SampleTriangle.gltf", m_gltfSceneAsset))
            {
                LoadGltfRuntimeAssets();
            }

            if (m_cubeMesh == 0 || m_planeMesh == 0 || m_terrainMesh == 0 || m_gizmoPlaneHandleMesh == 0 || m_gizmoRingMesh == 0)
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
            if (m_runtimeVerification.Enabled)
            {
                m_editorVisible = false;
                m_hotReloadEnabled = false;
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
            if (Disparity::Input::IsMouseCaptured() && !editorCapturesMouse && !m_editorCameraEnabled)
            {
                m_cameraYaw += mouseDelta.x * 0.0025f;
                m_cameraPitch = std::clamp(m_cameraPitch - mouseDelta.y * 0.0022f, -0.15f, 0.95f);
            }

            PollHotReload(dt);
            UpdatePlayer((editorCapturesKeyboard || m_editorCameraEnabled) ? 0.0f : dt);
            AnimateScene(dt);
            UpdateCamera();
            UpdateEditorCamera(dt, editorCapturesMouse, editorCapturesKeyboard);
            Disparity::AudioSystem::SetListenerPosition(GetRenderCamera().GetPosition());
            UpdateStatusTimer(dt);
            UpdateRuntimeVerification(dt);
        }

        void OnRender(Disparity::Renderer& renderer) override
        {
            renderer.SetCamera(GetRenderCamera());

            Disparity::DirectionalLight light;
            light.Direction = { -0.35f, -1.0f, 0.25f };
            light.Color = { 1.0f, 0.92f, 0.78f };
            light.Intensity = 1.18f;
            light.AmbientIntensity = 0.20f;
            renderer.SetLighting(light);
            const std::array<Disparity::PointLight, 4> pointLights = {
                Disparity::PointLight{ { -4.0f, 3.2f, 3.0f }, 8.0f, { 0.28f, 0.52f, 1.0f }, 0.82f },
                Disparity::PointLight{ { 4.5f, 2.5f, -2.0f }, 7.0f, { 1.0f, 0.46f, 0.24f }, 0.62f },
                Disparity::PointLight{ { 0.0f, 2.0f, 7.0f }, 5.0f, { 0.9f, 0.82f, 0.38f }, 0.55f },
                Disparity::PointLight{ Add(m_playerPosition, { 0.0f, 1.75f, 0.0f }), 4.5f, { 0.35f, 0.72f, 1.0f }, 0.45f }
            };
            renderer.SetPointLights(pointLights.data(), static_cast<uint32_t>(pointLights.size()));

            const float shadowRadius = renderer.GetSettings().CascadedShadows ? 34.0f : 20.0f;
            renderer.BeginShadowPass(light, m_playerPosition, shadowRadius);
            DrawWorld(renderer);
            DrawPlayer(renderer);
            renderer.EndShadowPass();

            renderer.SetCamera(GetRenderCamera());
            renderer.SetLighting(light);
            DrawWorld(renderer);
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
            DrawAudioPanel();
            DrawProfilerPanel();
        }

        void DrawWorld(Disparity::Renderer& renderer)
        {
            for (const Disparity::RenderableEntity& renderable : m_registry.ViewRenderables())
            {
                if (renderable.Id == m_playerEntity || !renderable.TransformData || !renderable.MeshRenderer)
                {
                    continue;
                }

                renderer.DrawMesh(renderable.MeshRenderer->Mesh, *renderable.TransformData, renderable.MeshRenderer->MaterialData);
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

            m_playerBobOffset = m_playerBob.SampleOffset(dt);

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

        const Disparity::Camera& GetRenderCamera() const
        {
            return m_editorCameraEnabled ? m_editorCamera : m_camera;
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
            renderer.DrawMesh(m_cubeMesh, PlayerBodyTransform(), m_playerBodyMaterial);

            Disparity::Transform head;
            head.Position = Add(m_playerPosition, { 0.0f, 1.85f + m_playerBobOffset, 0.0f });
            head.Rotation = { 0.0f, m_playerYaw, 0.0f };
            head.Scale = { 0.55f, 0.45f, 0.55f };
            renderer.DrawMesh(m_cubeMesh, head, m_playerHeadMaterial);
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
                renderer.DrawMesh(m_cubeMesh, body, outlineMaterial);

                Disparity::Transform head;
                head.Position = Add(m_playerPosition, { 0.0f, 1.85f + m_playerBobOffset, 0.0f });
                head.Rotation = { 0.0f, m_playerYaw, 0.0f };
                head.Scale = { 0.62f, 0.52f, 0.62f };
                renderer.DrawMesh(m_cubeMesh, head, outlineMaterial);
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
                renderer.DrawMesh(selected.Object.Mesh, outlineTransform, outlineMaterial);
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
                    Disparity::Transform ring;
                    ring.Position = pivot;
                    ring.Rotation = GizmoRingRotation(axis);
                    ring.Scale = { handleDistance, handleDistance, handleDistance };
                    renderer.DrawMesh(m_gizmoRingMesh, ring, *material);

                    const DirectX::XMFLOAT3 axisVector = AxisVector(axis);
                    Disparity::Transform marker;
                    marker.Position = Add(pivot, Scale(axisVector, handleDistance));
                    marker.Scale = { handleSize * 0.8f, handleSize * 0.8f, handleSize * 0.8f };
                    renderer.DrawMesh(m_cubeMesh, marker, *material);
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
                    Disparity::Transform planeHandle;
                    planeHandle.Position = GizmoPlaneCenter(pivot, plane);
                    planeHandle.Rotation = GizmoPlaneRotation(plane);
                    const float planeSize = screenScale * 0.34f;
                    planeHandle.Scale = { planeSize, planeSize, planeSize };
                    renderer.DrawMesh(m_gizmoPlaneHandleMesh, planeHandle, *material);
                }
            }

            for (const auto& [axis, material] : handles)
            {
                const DirectX::XMFLOAT3 axisVector = AxisVector(axis);

                Disparity::Transform marker;
                marker.Position = Add(pivot, Scale(axisVector, handleDistance));
                marker.Scale = { handleSize, handleSize, handleSize };
                renderer.DrawMesh(m_cubeMesh, marker, *material);

                Disparity::Transform tick;
                tick.Position = Add(pivot, Scale(axisVector, handleDistance * 0.55f));
                tick.Scale = { handleSize * 0.62f, handleSize * 0.62f, handleSize * 0.62f };
                renderer.DrawMesh(m_cubeMesh, tick, *material);
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
            ImGui::Text("Gizmo: %s", m_gizmoStatus.c_str());
            ImGui::TextDisabled("Tab releases mouse; left-click picks. Right-drag plus WASD/QE flies the editor camera.");
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
                changed |= ImGui::DragFloat3("Position", &m_playerPosition.x, 0.05f);
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
                changed |= ImGui::DragFloat3("Position", &selected.Object.TransformData.Position.x, 0.05f);
                changed |= ImGui::DragFloat3("Rotation", &selected.Object.TransformData.Rotation.x, 0.02f);
                changed |= ImGui::DragFloat3("Scale", &selected.Object.TransformData.Scale.x, 0.05f, 0.05f, 50.0f);
                changed |= DrawTransformGizmo(selected.Object.TransformData);
                changed |= DrawMaterialEditor("Material", selected.Object.MaterialData);
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
            ImGui::Text("Assets: %zu", m_assetDatabase.GetRecords().size());
            ImGui::SameLine();
            ImGui::Text("Dirty: %zu", m_assetDatabase.DirtyCount());
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
            changed |= ImGui::SliderFloat("Exposure", &settings.Exposure, 0.1f, 4.0f);
            changed |= ImGui::SliderFloat("Shadow strength", &settings.ShadowStrength, 0.0f, 0.95f);
            changed |= ImGui::SliderFloat("Bloom strength", &settings.BloomStrength, 0.0f, 1.25f);
            changed |= ImGui::SliderFloat("Bloom threshold", &settings.BloomThreshold, 0.05f, 2.0f);
            changed |= ImGui::SliderFloat("SSAO strength", &settings.SsaoStrength, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("AA strength", &settings.AntiAliasingStrength, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("TAA blend", &settings.TemporalBlend, 0.0f, 0.35f);
            changed |= ImGui::SliderFloat("Saturation", &settings.ColorSaturation, 0.0f, 2.0f);
            changed |= ImGui::SliderFloat("Contrast", &settings.ColorContrast, 0.0f, 2.0f);

            const char* debugViews[] = { "Final", "Bloom", "SSAO mask", "AA edges", "Depth" };
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
            ImGui::Text("Point lights: 4");
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
                ImGui::PopID();
            }

            ImGui::TextDisabled("WinMM bus mixer + spatial tone preview");
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

                ImGui::Text("Resources: %zu", graph.GetResources().size());
                ImGui::Text("Scheduled passes: %zu", graph.GetExecutionOrder().size());
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
                AddRuntimeVerificationNote("Saved runtime scene snapshot.");
                m_runtimeVerificationSavedScene = true;
            }

            if (!m_runtimeVerificationExercisedRenderer && m_runtimeVerificationFrame >= 20 && m_renderer)
            {
                m_runtimeVerificationOriginalRendererSettings = m_renderer->GetSettings();
                Disparity::RendererSettings settings = *m_runtimeVerificationOriginalRendererSettings;
                settings.PostDebugView = 1;
                settings.Bloom = !settings.Bloom;
                settings.SSAO = !settings.SSAO;
                settings.AntiAliasing = !settings.AntiAliasing;
                m_renderer->SetSettings(settings);
                m_renderer->SetSettings(*m_runtimeVerificationOriginalRendererSettings);
                AddRuntimeVerificationNote("Toggled renderer post settings and restored defaults.");
                m_runtimeVerificationExercisedRenderer = true;
            }

            if (!m_runtimeVerificationCycledSelection && m_runtimeVerificationFrame >= 26)
            {
                CycleSelection();
                AddRuntimeVerificationNote("Cycled editor selection.");
                m_runtimeVerificationCycledSelection = true;
            }
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
            if (m_cubeMesh == 0 || m_planeMesh == 0 || m_terrainMesh == 0 || m_gizmoRingMesh == 0 || m_gizmoPlaneHandleMesh == 0)
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
            }
        }

        void CompleteRuntimeVerification()
        {
            ValidateRuntimeVerificationState("final");
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
            report << "draw_calls=" << (m_renderer ? m_renderer->GetFrameDrawCalls() : 0) << "\n";
            report << "scene_draw_calls=" << (m_renderer ? m_renderer->GetSceneDrawCalls() : 0) << "\n";
            report << "shadow_draw_calls=" << (m_renderer ? m_renderer->GetShadowDrawCalls() : 0) << "\n";
            if (m_renderer)
            {
                const Disparity::RenderGraph& graph = m_renderer->GetRenderGraph();
                report << "render_graph_passes=" << graph.GetPasses().size() << "\n";
                report << "render_graph_resources=" << graph.GetResources().size() << "\n";
                report << "render_graph_order=" << graph.GetExecutionOrder().size() << "\n";
            }

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

        float PickRadiusForTransform(const Disparity::Transform& transform) const
        {
            return std::max(0.45f, Length(transform.Scale) * 0.62f);
        }

        bool BuildMouseRay(MouseRay& outRay) const
        {
            if (!m_application)
            {
                return false;
            }

            const DirectX::XMFLOAT2 mouse = Disparity::Input::GetMousePosition();
            const float width = static_cast<float>(std::max(1u, m_application->GetWidth()));
            const float height = static_cast<float>(std::max(1u, m_application->GetHeight()));
            if (mouse.x < 0.0f || mouse.y < 0.0f || mouse.x > width || mouse.y > height)
            {
                return false;
            }

            const DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
            const DirectX::XMMATRIX view = GetRenderCamera().GetViewMatrix();
            const DirectX::XMMATRIX projection = GetRenderCamera().GetProjectionMatrix();
            const DirectX::XMVECTOR nearPoint = DirectX::XMVector3Unproject(
                DirectX::XMVectorSet(mouse.x, mouse.y, 0.0f, 1.0f),
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
                DirectX::XMVectorSet(mouse.x, mouse.y, 1.0f, 1.0f),
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

        DirectX::XMFLOAT2 ProjectWorldToScreen(const DirectX::XMFLOAT3& worldPosition) const
        {
            const float width = m_application ? static_cast<float>(std::max(1u, m_application->GetWidth())) : 1.0f;
            const float height = m_application ? static_cast<float>(std::max(1u, m_application->GetHeight())) : 1.0f;
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
            return { screen.x, screen.y };
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

        bool TryPickGizmoAxis(GizmoAxis& outAxis, GizmoPlane& outPlane) const
        {
            outAxis = GizmoAxis::None;
            outPlane = GizmoPlane::None;
            DirectX::XMFLOAT3 pivot = {};
            MouseRay ray;
            if (!TryGetSelectionPivot(pivot) || !BuildMouseRay(ray))
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

            return outAxis != GizmoAxis::None || outPlane != GizmoPlane::None;
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

        void PickSceneObjectAtMouse()
        {
            MouseRay ray;
            if (!BuildMouseRay(ray))
            {
                return;
            }

            float bestDistance = FLT_MAX;
            bool hitPlayer = false;
            size_t hitIndex = InvalidIndex;
            float distance = 0.0f;
            if (IntersectRaySphere(ray.Origin, ray.Direction, Add(m_playerPosition, { 0.0f, 1.1f, 0.0f }), 1.2f, distance))
            {
                bestDistance = distance;
                hitPlayer = true;
            }

            const auto& objects = m_scene.GetObjects();
            for (size_t index = 0; index < objects.size(); ++index)
            {
                const Disparity::Transform& transform = objects[index].Object.TransformData;
                if (IntersectRaySphere(ray.Origin, ray.Direction, transform.Position, PickRadiusForTransform(transform), distance) && distance < bestDistance)
                {
                    bestDistance = distance;
                    hitPlayer = false;
                    hitIndex = index;
                }
            }

            if (hitPlayer)
            {
                m_selectedPlayer = true;
                m_multiSelection.clear();
                m_lastPickStatus = "Player";
                SetStatus("Picked Player");
                return;
            }

            if (hitIndex != InvalidIndex)
            {
                SelectSceneObject(hitIndex, Disparity::Input::IsKeyDown(VK_CONTROL));
                m_lastPickStatus = objects[hitIndex].Name;
                SetStatus("Picked " + objects[hitIndex].Name);
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
            ReloadGltfAssets();
            ReloadSceneAndScript();
            WatchAssets();
            SetStatus("Hot reloaded " + std::to_string(changed.size()) + " asset(s)");
        }

        Disparity::Application* m_application = nullptr;
        Disparity::Renderer* m_renderer = nullptr;
        Disparity::Camera m_camera;
        Disparity::Camera m_editorCamera;
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
        std::optional<EditState> m_gizmoDragBeforeState;
        std::optional<Disparity::RendererSettings> m_runtimeVerificationOriginalRendererSettings;
        std::vector<GizmoDragObject> m_gizmoDragObjects;
        std::vector<std::string> m_runtimeVerificationNotes;
        std::vector<std::string> m_runtimeVerificationFailures;
        Disparity::Entity m_playerEntity = 0;
        Disparity::MeshHandle m_cubeMesh = 0;
        Disparity::MeshHandle m_planeMesh = 0;
        Disparity::MeshHandle m_terrainMesh = 0;
        Disparity::MeshHandle m_gltfMesh = 0;
        Disparity::MeshHandle m_gizmoPlaneHandleMesh = 0;
        Disparity::MeshHandle m_gizmoRingMesh = 0;
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
        DirectX::XMFLOAT3 m_playerPosition = { 0.0f, 0.0f, 0.0f };
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
        float m_statusTimer = 0.0f;
        float m_hotReloadPollTimer = 0.0f;
        float m_runtimeVerificationElapsed = 0.0f;
        size_t m_selectedIndex = 0;
        uint32_t m_runtimeVerificationFrame = 0;
        DirectX::XMFLOAT3 m_editorCameraTarget = { 0.0f, 1.1f, 0.0f };
        DirectX::XMFLOAT2 m_editorLastMousePosition = {};
        RuntimeVerificationConfig m_runtimeVerification;
        bool m_selectedPlayer = true;
        bool m_editorVisible = true;
        bool m_editorCameraEnabled = false;
        bool m_hotReloadEnabled = true;
        bool m_gltfAnimationPlayback = true;
        bool m_applyingHistory = false;
        bool m_runtimeVerificationFinished = false;
        bool m_runtimeVerificationReloadedScene = false;
        bool m_runtimeVerificationSavedScene = false;
        bool m_runtimeVerificationExercisedRenderer = false;
        bool m_runtimeVerificationCycledSelection = false;
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
        std::string m_gizmoStatus = "Idle";
    };
}

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE previousInstance, _In_ PWSTR commandLine, _In_ int commandShow)
{
    (void)previousInstance;
    (void)commandShow;

    RuntimeVerificationConfig runtimeVerification;
    const std::wstring arguments = commandLine ? commandLine : L"";
    runtimeVerification.Enabled = arguments.find(L"--verify-runtime") != std::wstring::npos;
    runtimeVerification.TargetFrames = ParseUnsignedArgument(arguments, L"--verify-frames=", runtimeVerification.TargetFrames);

    Disparity::Application app({ instance, L"DISPARITY", 1280, 720 });
    app.SetLayer(std::make_unique<DisparityGameLayer>(runtimeVerification));
    return app.Run();
}
