#include <Disparity/Disparity.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <deque>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
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

    DirectX::XMFLOAT3 Scale(const DirectX::XMFLOAT3& value, float scalar)
    {
        return { value.x * scalar, value.y * scalar, value.z * scalar };
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

    class DisparityGameLayer final : public Disparity::Layer
    {
    public:
        bool OnAttach(Disparity::Application& application) override
        {
            m_application = &application;
            m_renderer = &application.GetRenderer();

            m_cubeMesh = m_renderer->CreateMesh(Disparity::PrimitiveFactory::CreateCube());
            m_planeMesh = m_renderer->CreateMesh(Disparity::PrimitiveFactory::CreatePlane(48.0f));
            m_terrainMesh = m_renderer->CreateMesh(Disparity::PrimitiveFactory::CreateTerrainGrid(64, 56.0f, 0.32f));

            if (Disparity::GltfLoader::LoadScene("Assets/Meshes/SampleTriangle.gltf", m_gltfSceneAsset))
            {
                LoadGltfRuntimeAssets();
            }

            if (m_cubeMesh == 0 || m_planeMesh == 0 || m_terrainMesh == 0)
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

            if (!m_scene.Load("Assets/Scenes/Prototype.dscene", m_meshes))
            {
                BuildFallbackScene();
            }
            (void)Disparity::ScriptRunner::RunSceneScript("Assets/Scripts/Prototype.dscript", m_scene, m_meshes);
            AppendImportedGltfSceneObjects();

            BuildRuntimeRegistry();
            WatchAssets();

            UpdateCamera();
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

            const DirectX::XMFLOAT2 mouseDelta = Disparity::Input::GetMouseDelta();
            if (Disparity::Input::IsMouseCaptured() && !editorCapturesMouse)
            {
                m_cameraYaw += mouseDelta.x * 0.0025f;
                m_cameraPitch = std::clamp(m_cameraPitch - mouseDelta.y * 0.0022f, -0.15f, 0.95f);
            }

            PollHotReload(dt);
            UpdatePlayer(editorCapturesKeyboard ? 0.0f : dt);
            AnimateScene(dt);
            UpdateCamera();
            Disparity::AudioSystem::SetListenerPosition(m_camera.GetPosition());
            UpdateStatusTimer(dt);
        }

        void OnRender(Disparity::Renderer& renderer) override
        {
            renderer.SetCamera(m_camera);

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

            renderer.SetCamera(m_camera);
            renderer.SetLighting(light);
            DrawWorld(renderer);
            DrawPlayer(renderer);
        }

        void OnGui() override
        {
            if (!m_editorVisible)
            {
                return;
            }

            DrawDockspace();
            DrawMainMenu();
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

                if (!m_statusMessage.empty() && m_statusTimer > 0.0f)
                {
                    ImGui::TextDisabled("%s", m_statusMessage.c_str());
                }

                ImGui::EndMainMenuBar();
            }
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
            }

            ImGui::SeparatorText("Scene");
            const auto& objects = m_scene.GetObjects();
            for (size_t index = 0; index < objects.size(); ++index)
            {
                const bool selected = !m_selectedPlayer && index == m_selectedIndex;
                if (ImGui::Selectable(objects[index].Name.c_str(), selected))
                {
                    m_selectedPlayer = false;
                    m_selectedIndex = index;
                }
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
                    PushUndoState(before);
                }
            }
            else if (m_scene.Count() > 0 && m_selectedIndex < m_scene.Count())
            {
                Disparity::NamedSceneObject& selected = m_scene.GetObjects()[m_selectedIndex];
                ImGui::Text("Name: %s", selected.Name.c_str());
                ImGui::Text("Mesh: %s", selected.MeshName.c_str());
                const EditState before = CaptureEditState();
                bool changed = false;
                changed |= ImGui::DragFloat3("Position", &selected.Object.TransformData.Position.x, 0.05f);
                changed |= ImGui::DragFloat3("Rotation", &selected.Object.TransformData.Rotation.x, 0.02f);
                changed |= ImGui::DragFloat3("Scale", &selected.Object.TransformData.Scale.x, 0.05f, 0.05f, 50.0f);
                changed |= DrawTransformGizmo(selected.Object.TransformData);
                changed |= DrawMaterialEditor("Material", selected.Object.MaterialData);
                if (changed)
                {
                    PushUndoState(before);
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

            ImGui::SeparatorText("Known Assets");
            ImGui::BulletText("Assets/Scenes/Prototype.dscene");
            ImGui::BulletText("Assets/Scripts/Prototype.dscript");
            ImGui::BulletText("Assets/Prefabs/Beacon.dprefab");
            ImGui::BulletText("Assets/Meshes/SampleTriangle.gltf");
            ImGui::BulletText("Assets/Materials/PlayerBody.dmat");
            ImGui::BulletText("Assets/Materials/PlayerHead.dmat");

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
            changed |= ImGui::Checkbox("Temporal AA", &settings.TemporalAA);
            changed |= ImGui::SliderFloat("Exposure", &settings.Exposure, 0.1f, 4.0f);
            changed |= ImGui::SliderFloat("Shadow strength", &settings.ShadowStrength, 0.0f, 0.95f);
            changed |= ImGui::SliderFloat("Bloom strength", &settings.BloomStrength, 0.0f, 1.25f);
            changed |= ImGui::SliderFloat("SSAO strength", &settings.SsaoStrength, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("TAA blend", &settings.TemporalBlend, 0.0f, 0.35f);

            if (changed)
            {
                PushUndoState(before);
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
            ImGui::Text("FPS: %.1f", snapshot.FramesPerSecond);
            ImGui::Text("Frame: %.2f ms", snapshot.FrameMilliseconds);
            ImGui::Separator();
            for (const Disparity::ProfileRecord& record : snapshot.Records)
            {
                ImGui::Text("%s: %.3f ms", record.Name.c_str(), record.Milliseconds);
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
        }

        void ReloadSceneAndScript()
        {
            const bool sceneLoaded = m_scene.Load("Assets/Scenes/Prototype.dscene", m_meshes);
            const bool scriptLoaded = Disparity::ScriptRunner::RunSceneScript("Assets/Scripts/Prototype.dscript", m_scene, m_meshes);
            AppendImportedGltfSceneObjects();
            BuildRuntimeRegistry();
            SetStatus(sceneLoaded && scriptLoaded ? "Reloaded scene + script" : "Reload attempted; check asset paths");
        }

        void SaveRuntimeScene()
        {
            const bool saved = m_scene.Save("Saved/PrototypeRuntime.dscene");
            SetStatus(saved ? "Saved runtime scene snapshot" : "Scene save failed");
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
            BuildRuntimeRegistry();
        }

        void PushUndoState(const EditState& state)
        {
            if (m_applyingHistory)
            {
                return;
            }

            m_undoStack.push_back(state);
            while (m_undoStack.size() > 64)
            {
                m_undoStack.pop_front();
            }
            m_redoStack.clear();
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
            m_redoStack.push_back(CaptureEditState());
            const EditState state = m_undoStack.back();
            m_undoStack.pop_back();
            ApplyEditState(state);
            m_applyingHistory = false;
            SetStatus("Undo");
        }

        void RedoEdit()
        {
            if (m_redoStack.empty())
            {
                return;
            }

            m_applyingHistory = true;
            m_undoStack.push_back(CaptureEditState());
            const EditState state = m_redoStack.back();
            m_redoStack.pop_back();
            ApplyEditState(state);
            m_applyingHistory = false;
            SetStatus("Redo");
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
            m_hotReloader.Watch("Assets/Scenes/Prototype.dscene");
            m_hotReloader.Watch("Assets/Scripts/Prototype.dscript");
            m_hotReloader.Watch("Assets/Prefabs/Beacon.dprefab");
            m_hotReloader.Watch("Assets/Meshes/SampleTriangle.gltf");
            m_hotReloader.Watch("Assets/Materials/PlayerBody.dmat");
            m_hotReloader.Watch("Assets/Materials/PlayerHead.dmat");
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
            SetStatus("Hot reloaded " + std::to_string(changed.size()) + " asset(s)");
        }

        Disparity::Application* m_application = nullptr;
        Disparity::Renderer* m_renderer = nullptr;
        Disparity::Camera m_camera;
        Disparity::Registry m_registry;
        Disparity::Scene m_scene;
        Disparity::AssetHotReloader m_hotReloader;
        Disparity::GltfSceneAsset m_gltfSceneAsset;
        Disparity::BobAnimation m_playerBob;
        std::unordered_map<std::string, Disparity::MeshHandle> m_meshes;
        std::vector<Disparity::Entity> m_sceneEntities;
        std::vector<Disparity::MeshHandle> m_gltfMeshes;
        std::vector<Disparity::Material> m_gltfMaterials;
        std::vector<size_t> m_gltfNodeSceneIndices;
        std::deque<EditState> m_undoStack;
        std::deque<EditState> m_redoStack;
        Disparity::Entity m_playerEntity = 0;
        Disparity::MeshHandle m_cubeMesh = 0;
        Disparity::MeshHandle m_planeMesh = 0;
        Disparity::MeshHandle m_terrainMesh = 0;
        Disparity::MeshHandle m_gltfMesh = 0;
        Disparity::Material m_playerBodyMaterial;
        Disparity::Material m_playerHeadMaterial;
        Disparity::Material m_shadowMaterial;
        DirectX::XMFLOAT3 m_playerPosition = { 0.0f, 0.0f, 0.0f };
        float m_playerYaw = 0.0f;
        float m_cameraYaw = Pi;
        float m_cameraPitch = 0.42f;
        float m_cameraDistance = 7.0f;
        float m_playerBobOffset = 0.0f;
        float m_sceneAnimationTime = 0.0f;
        float m_statusTimer = 0.0f;
        float m_hotReloadPollTimer = 0.0f;
        size_t m_selectedIndex = 0;
        bool m_selectedPlayer = true;
        bool m_editorVisible = true;
        bool m_hotReloadEnabled = true;
        bool m_gltfAnimationPlayback = true;
        bool m_applyingHistory = false;
        std::string m_statusMessage;
    };
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previousInstance, PWSTR commandLine, int commandShow)
{
    (void)previousInstance;
    (void)commandLine;
    (void)commandShow;

    Disparity::Application app({ instance, L"DISPARITY", 1280, 720 });
    app.SetLayer(std::make_unique<DisparityGameLayer>());
    return app.Run();
}
