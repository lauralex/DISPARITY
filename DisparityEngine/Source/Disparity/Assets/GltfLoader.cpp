#include "Disparity/Assets/GltfLoader.h"

#include "Disparity/Assets/SimpleJson.h"
#include "Disparity/Core/FileSystem.h"
#include "Disparity/Core/Log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace Disparity
{
    namespace
    {
        struct BufferView
        {
            int Buffer = 0;
            size_t ByteOffset = 0;
            size_t ByteLength = 0;
            size_t ByteStride = 0;
        };

        struct Accessor
        {
            int BufferView = -1;
            size_t ByteOffset = 0;
            int ComponentType = 0;
            size_t Count = 0;
            std::string Type;
            bool Normalized = false;
        };

        size_t ComponentSize(int componentType)
        {
            switch (componentType)
            {
            case 5120:
            case 5121:
                return 1;
            case 5122:
            case 5123:
                return 2;
            case 5125:
            case 5126:
                return 4;
            default:
                return 0;
            }
        }

        size_t ComponentCount(const std::string& type)
        {
            if (type == "SCALAR") return 1;
            if (type == "VEC2") return 2;
            if (type == "VEC3") return 3;
            if (type == "VEC4") return 4;
            return 0;
        }

        template <typename T>
        T ReadValue(const std::vector<unsigned char>& bytes, size_t offset)
        {
            T value = {};
            if (offset + sizeof(T) <= bytes.size())
            {
                std::memcpy(&value, bytes.data() + offset, sizeof(T));
            }
            return value;
        }

        int Base64Index(char value)
        {
            if (value >= 'A' && value <= 'Z') return value - 'A';
            if (value >= 'a' && value <= 'z') return value - 'a' + 26;
            if (value >= '0' && value <= '9') return value - '0' + 52;
            if (value == '+') return 62;
            if (value == '/') return 63;
            return -1;
        }

        bool DecodeBase64(const std::string& text, std::vector<unsigned char>& outBytes)
        {
            outBytes.clear();
            int accumulator = 0;
            int bits = -8;

            for (char value : text)
            {
                if (value == '=')
                {
                    break;
                }

                const int decoded = Base64Index(value);
                if (decoded < 0)
                {
                    continue;
                }

                accumulator = (accumulator << 6) | decoded;
                bits += 6;

                if (bits >= 0)
                {
                    outBytes.push_back(static_cast<unsigned char>((accumulator >> bits) & 0xff));
                    bits -= 8;
                }
            }

            return !outBytes.empty();
        }

        bool LoadBuffer(const std::filesystem::path& gltfPath, const JsonValue& bufferValue, std::vector<unsigned char>& outBytes)
        {
            const JsonValue* uriValue = bufferValue.Find("uri");
            if (!uriValue)
            {
                Log(LogLevel::Warning, "glTF buffers without URI are not supported by the v1 loader.");
                return false;
            }

            const std::string uri = uriValue->AsString();
            const std::string base64Marker = ";base64,";
            const size_t base64Position = uri.find(base64Marker);
            if (uri.rfind("data:", 0) == 0 && base64Position != std::string::npos)
            {
                return DecodeBase64(uri.substr(base64Position + base64Marker.size()), outBytes);
            }

            const std::filesystem::path bufferPath = gltfPath.parent_path() / std::filesystem::path(uri);
            return FileSystem::ReadBinaryFile(bufferPath, outBytes);
        }

        DirectX::XMFLOAT3 ReadFloat3(
            const std::vector<unsigned char>& bytes,
            const std::vector<BufferView>& views,
            const Accessor& accessor,
            size_t index)
        {
            if (accessor.BufferView < 0 || accessor.BufferView >= static_cast<int>(views.size()) || accessor.ComponentType != 5126)
            {
                return {};
            }

            const BufferView& view = views[static_cast<size_t>(accessor.BufferView)];
            const size_t stride = view.ByteStride == 0 ? ComponentSize(accessor.ComponentType) * ComponentCount(accessor.Type) : view.ByteStride;
            const size_t offset = view.ByteOffset + accessor.ByteOffset + index * stride;

            return {
                ReadValue<float>(bytes, offset),
                ReadValue<float>(bytes, offset + sizeof(float)),
                ReadValue<float>(bytes, offset + sizeof(float) * 2u)
            };
        }

        DirectX::XMFLOAT2 ReadFloat2(
            const std::vector<unsigned char>& bytes,
            const std::vector<BufferView>& views,
            const Accessor& accessor,
            size_t index)
        {
            if (accessor.BufferView < 0 || accessor.BufferView >= static_cast<int>(views.size()) || accessor.ComponentType != 5126)
            {
                return {};
            }

            const BufferView& view = views[static_cast<size_t>(accessor.BufferView)];
            const size_t stride = view.ByteStride == 0 ? ComponentSize(accessor.ComponentType) * ComponentCount(accessor.Type) : view.ByteStride;
            const size_t offset = view.ByteOffset + accessor.ByteOffset + index * stride;

            return {
                ReadValue<float>(bytes, offset),
                ReadValue<float>(bytes, offset + sizeof(float))
            };
        }

        DirectX::XMFLOAT4 ReadFloat4(
            const std::vector<unsigned char>& bytes,
            const std::vector<BufferView>& views,
            const Accessor& accessor,
            size_t index)
        {
            if (accessor.BufferView < 0 || accessor.BufferView >= static_cast<int>(views.size()) || accessor.ComponentType != 5126)
            {
                return {};
            }

            const BufferView& view = views[static_cast<size_t>(accessor.BufferView)];
            const size_t stride = view.ByteStride == 0 ? ComponentSize(accessor.ComponentType) * ComponentCount(accessor.Type) : view.ByteStride;
            const size_t offset = view.ByteOffset + accessor.ByteOffset + index * stride;

            return {
                ReadValue<float>(bytes, offset),
                ReadValue<float>(bytes, offset + sizeof(float)),
                ReadValue<float>(bytes, offset + sizeof(float) * 2u),
                ReadValue<float>(bytes, offset + sizeof(float) * 3u)
            };
        }

        float ReadScalarFloat(
            const std::vector<unsigned char>& bytes,
            const std::vector<BufferView>& views,
            const Accessor& accessor,
            size_t index)
        {
            if (accessor.BufferView < 0 || accessor.BufferView >= static_cast<int>(views.size()))
            {
                return 0.0f;
            }

            const BufferView& view = views[static_cast<size_t>(accessor.BufferView)];
            const size_t stride = view.ByteStride == 0 ? ComponentSize(accessor.ComponentType) : view.ByteStride;
            const size_t offset = view.ByteOffset + accessor.ByteOffset + index * stride;

            switch (accessor.ComponentType)
            {
            case 5126: return ReadValue<float>(bytes, offset);
            case 5125: return static_cast<float>(ReadValue<uint32_t>(bytes, offset));
            case 5123: return static_cast<float>(ReadValue<unsigned short>(bytes, offset));
            case 5121: return static_cast<float>(ReadValue<unsigned char>(bytes, offset));
            default: return 0.0f;
            }
        }

        DirectX::XMUINT4 ReadJoint4(
            const std::vector<unsigned char>& bytes,
            const std::vector<BufferView>& views,
            const Accessor& accessor,
            size_t index)
        {
            DirectX::XMUINT4 joints = {};
            if (accessor.BufferView < 0 || accessor.BufferView >= static_cast<int>(views.size()))
            {
                return joints;
            }

            const BufferView& view = views[static_cast<size_t>(accessor.BufferView)];
            const size_t stride = view.ByteStride == 0 ? ComponentSize(accessor.ComponentType) * ComponentCount(accessor.Type) : view.ByteStride;
            const size_t offset = view.ByteOffset + accessor.ByteOffset + index * stride;

            if (accessor.ComponentType == 5121)
            {
                joints.x = ReadValue<unsigned char>(bytes, offset);
                joints.y = ReadValue<unsigned char>(bytes, offset + 1u);
                joints.z = ReadValue<unsigned char>(bytes, offset + 2u);
                joints.w = ReadValue<unsigned char>(bytes, offset + 3u);
            }
            else if (accessor.ComponentType == 5123)
            {
                joints.x = ReadValue<unsigned short>(bytes, offset);
                joints.y = ReadValue<unsigned short>(bytes, offset + sizeof(unsigned short));
                joints.z = ReadValue<unsigned short>(bytes, offset + sizeof(unsigned short) * 2u);
                joints.w = ReadValue<unsigned short>(bytes, offset + sizeof(unsigned short) * 3u);
            }

            return joints;
        }

        DirectX::XMFLOAT4 ReadWeight4(
            const std::vector<unsigned char>& bytes,
            const std::vector<BufferView>& views,
            const Accessor& accessor,
            size_t index)
        {
            DirectX::XMFLOAT4 weights = {};
            if (accessor.BufferView < 0 || accessor.BufferView >= static_cast<int>(views.size()))
            {
                return weights;
            }

            if (accessor.ComponentType == 5126)
            {
                return ReadFloat4(bytes, views, accessor, index);
            }

            const BufferView& view = views[static_cast<size_t>(accessor.BufferView)];
            const size_t stride = view.ByteStride == 0 ? ComponentSize(accessor.ComponentType) * ComponentCount(accessor.Type) : view.ByteStride;
            const size_t offset = view.ByteOffset + accessor.ByteOffset + index * stride;

            if (accessor.ComponentType == 5121)
            {
                weights.x = static_cast<float>(ReadValue<unsigned char>(bytes, offset)) / 255.0f;
                weights.y = static_cast<float>(ReadValue<unsigned char>(bytes, offset + 1u)) / 255.0f;
                weights.z = static_cast<float>(ReadValue<unsigned char>(bytes, offset + 2u)) / 255.0f;
                weights.w = static_cast<float>(ReadValue<unsigned char>(bytes, offset + 3u)) / 255.0f;
            }
            else if (accessor.ComponentType == 5123)
            {
                weights.x = static_cast<float>(ReadValue<unsigned short>(bytes, offset)) / 65535.0f;
                weights.y = static_cast<float>(ReadValue<unsigned short>(bytes, offset + sizeof(unsigned short))) / 65535.0f;
                weights.z = static_cast<float>(ReadValue<unsigned short>(bytes, offset + sizeof(unsigned short) * 2u)) / 65535.0f;
                weights.w = static_cast<float>(ReadValue<unsigned short>(bytes, offset + sizeof(unsigned short) * 3u)) / 65535.0f;
            }

            const float sum = weights.x + weights.y + weights.z + weights.w;
            if (sum > 0.0001f)
            {
                weights.x /= sum;
                weights.y /= sum;
                weights.z /= sum;
                weights.w /= sum;
            }

            return weights;
        }

        uint32_t ReadIndex(const std::vector<unsigned char>& bytes, const std::vector<BufferView>& views, const Accessor& accessor, size_t index)
        {
            if (accessor.BufferView < 0 || accessor.BufferView >= static_cast<int>(views.size()))
            {
                return 0;
            }

            const BufferView& view = views[static_cast<size_t>(accessor.BufferView)];
            const size_t stride = view.ByteStride == 0 ? ComponentSize(accessor.ComponentType) : view.ByteStride;
            const size_t offset = view.ByteOffset + accessor.ByteOffset + index * stride;

            switch (accessor.ComponentType)
            {
            case 5121: return ReadValue<unsigned char>(bytes, offset);
            case 5123: return ReadValue<unsigned short>(bytes, offset);
            case 5125: return ReadValue<uint32_t>(bytes, offset);
            default: return 0;
            }
        }

        void GenerateNormals(MeshData& mesh)
        {
            for (Vertex& vertex : mesh.Vertices)
            {
                vertex.Normal = {};
            }

            for (size_t index = 0; index + 2 < mesh.Indices.size(); index += 3)
            {
                Vertex& a = mesh.Vertices[mesh.Indices[index]];
                Vertex& b = mesh.Vertices[mesh.Indices[index + 1]];
                Vertex& c = mesh.Vertices[mesh.Indices[index + 2]];

                const DirectX::XMVECTOR av = DirectX::XMLoadFloat3(&a.Position);
                const DirectX::XMVECTOR bv = DirectX::XMLoadFloat3(&b.Position);
                const DirectX::XMVECTOR cv = DirectX::XMLoadFloat3(&c.Position);
                const DirectX::XMVECTOR edgeA = DirectX::XMVectorSubtract(bv, av);
                const DirectX::XMVECTOR edgeB = DirectX::XMVectorSubtract(cv, av);
                const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(edgeA, edgeB));

                DirectX::XMFLOAT3 n = {};
                DirectX::XMStoreFloat3(&n, normal);
                a.Normal.x += n.x; a.Normal.y += n.y; a.Normal.z += n.z;
                b.Normal.x += n.x; b.Normal.y += n.y; b.Normal.z += n.z;
                c.Normal.x += n.x; c.Normal.y += n.y; c.Normal.z += n.z;
            }

            for (Vertex& vertex : mesh.Vertices)
            {
                DirectX::XMVECTOR normal = DirectX::XMLoadFloat3(&vertex.Normal);
                normal = DirectX::XMVector3Normalize(normal);
                DirectX::XMStoreFloat3(&vertex.Normal, normal);
            }
        }

        bool LoadAllMeshes(
            const std::filesystem::path& resolvedPath,
            const JsonValue& root,
            std::vector<MeshData>& outMeshes,
            std::vector<GltfMeshPrimitiveInfo>* outPrimitiveInfo)
        {
            const JsonValue* buffersValue = root.Find("buffers");
            const JsonValue* viewsValue = root.Find("bufferViews");
            const JsonValue* accessorsValue = root.Find("accessors");
            const JsonValue* meshesValue = root.Find("meshes");
            if (!buffersValue || !viewsValue || !accessorsValue || !meshesValue)
            {
                return false;
            }

            std::vector<std::vector<unsigned char>> buffers;
            for (const JsonValue& bufferValue : buffersValue->Array)
            {
                std::vector<unsigned char> bytes;
                if (!LoadBuffer(resolvedPath, bufferValue, bytes))
                {
                    return false;
                }
                buffers.push_back(std::move(bytes));
            }

            std::vector<BufferView> views;
            for (const JsonValue& viewValue : viewsValue->Array)
            {
                BufferView view;
                view.Buffer = viewValue.Find("buffer") ? viewValue.Find("buffer")->AsInt() : 0;
                view.ByteOffset = static_cast<size_t>(viewValue.Find("byteOffset") ? viewValue.Find("byteOffset")->AsNumber() : 0.0);
                view.ByteLength = static_cast<size_t>(viewValue.Find("byteLength") ? viewValue.Find("byteLength")->AsNumber() : 0.0);
                view.ByteStride = static_cast<size_t>(viewValue.Find("byteStride") ? viewValue.Find("byteStride")->AsNumber() : 0.0);
                views.push_back(view);
            }

            std::vector<Accessor> accessors;
            for (const JsonValue& accessorValue : accessorsValue->Array)
            {
                Accessor accessor;
                accessor.BufferView = accessorValue.Find("bufferView") ? accessorValue.Find("bufferView")->AsInt(-1) : -1;
                accessor.ByteOffset = static_cast<size_t>(accessorValue.Find("byteOffset") ? accessorValue.Find("byteOffset")->AsNumber() : 0.0);
                accessor.ComponentType = accessorValue.Find("componentType") ? accessorValue.Find("componentType")->AsInt() : 0;
                accessor.Count = static_cast<size_t>(accessorValue.Find("count") ? accessorValue.Find("count")->AsNumber() : 0.0);
                accessor.Type = accessorValue.Find("type") ? accessorValue.Find("type")->AsString() : "";
                accessor.Normalized = accessorValue.Find("normalized") ? accessorValue.Find("normalized")->Bool : false;
                accessors.push_back(accessor);
            }

            outMeshes.clear();
            if (outPrimitiveInfo)
            {
                outPrimitiveInfo->clear();
            }
            size_t meshIndex = 0;
            for (const JsonValue& meshValue : meshesValue->Array)
            {
                const JsonValue* primitives = meshValue.Find("primitives");
                if (!primitives)
                {
                    ++meshIndex;
                    continue;
                }

                size_t primitiveIndex = 0;
                for (const JsonValue& primitiveValue : primitives->Array)
                {
                    const JsonValue* attributes = primitiveValue.Find("attributes");
                    const JsonValue* positionValue = attributes ? attributes->Find("POSITION") : nullptr;
                    if (!positionValue)
                    {
                        continue;
                    }

                    const int positionAccessorIndex = positionValue->AsInt(-1);
                    const int normalAccessorIndex = attributes->Find("NORMAL") ? attributes->Find("NORMAL")->AsInt(-1) : -1;
                    const int texCoordAccessorIndex = attributes->Find("TEXCOORD_0") ? attributes->Find("TEXCOORD_0")->AsInt(-1) : -1;
                    const int jointAccessorIndex = attributes->Find("JOINTS_0") ? attributes->Find("JOINTS_0")->AsInt(-1) : -1;
                    const int weightAccessorIndex = attributes->Find("WEIGHTS_0") ? attributes->Find("WEIGHTS_0")->AsInt(-1) : -1;
                    const int indexAccessorIndex = primitiveValue.Find("indices") ? primitiveValue.Find("indices")->AsInt(-1) : -1;

                    if (positionAccessorIndex < 0 || positionAccessorIndex >= static_cast<int>(accessors.size()))
                    {
                        continue;
                    }

                    const Accessor& positions = accessors[static_cast<size_t>(positionAccessorIndex)];
                    const BufferView& positionView = views[static_cast<size_t>(positions.BufferView)];
                    const std::vector<unsigned char>& vertexBuffer = buffers[static_cast<size_t>(positionView.Buffer)];

                    MeshData mesh;
                    mesh.Vertices.reserve(positions.Count);

                    for (size_t vertexIndex = 0; vertexIndex < positions.Count; ++vertexIndex)
                    {
                        Vertex vertex;
                        vertex.Position = ReadFloat3(vertexBuffer, views, positions, vertexIndex);

                        if (normalAccessorIndex >= 0 && normalAccessorIndex < static_cast<int>(accessors.size()))
                        {
                            const Accessor& normals = accessors[static_cast<size_t>(normalAccessorIndex)];
                            const BufferView& normalView = views[static_cast<size_t>(normals.BufferView)];
                            vertex.Normal = ReadFloat3(buffers[static_cast<size_t>(normalView.Buffer)], views, normals, vertexIndex);
                        }

                        if (texCoordAccessorIndex >= 0 && texCoordAccessorIndex < static_cast<int>(accessors.size()))
                        {
                            const Accessor& texCoords = accessors[static_cast<size_t>(texCoordAccessorIndex)];
                            const BufferView& texCoordView = views[static_cast<size_t>(texCoords.BufferView)];
                            vertex.TexCoord = ReadFloat2(buffers[static_cast<size_t>(texCoordView.Buffer)], views, texCoords, vertexIndex);
                        }

                        if (jointAccessorIndex >= 0 && jointAccessorIndex < static_cast<int>(accessors.size()))
                        {
                            const Accessor& joints = accessors[static_cast<size_t>(jointAccessorIndex)];
                            const BufferView& jointView = views[static_cast<size_t>(joints.BufferView)];
                            vertex.Joints = ReadJoint4(buffers[static_cast<size_t>(jointView.Buffer)], views, joints, vertexIndex);
                        }

                        if (weightAccessorIndex >= 0 && weightAccessorIndex < static_cast<int>(accessors.size()))
                        {
                            const Accessor& weights = accessors[static_cast<size_t>(weightAccessorIndex)];
                            const BufferView& weightView = views[static_cast<size_t>(weights.BufferView)];
                            vertex.Weights = ReadWeight4(buffers[static_cast<size_t>(weightView.Buffer)], views, weights, vertexIndex);
                        }

                        mesh.Vertices.push_back(vertex);
                    }

                    if (indexAccessorIndex >= 0 && indexAccessorIndex < static_cast<int>(accessors.size()))
                    {
                        const Accessor& indices = accessors[static_cast<size_t>(indexAccessorIndex)];
                        const BufferView& indexView = views[static_cast<size_t>(indices.BufferView)];
                        const std::vector<unsigned char>& indexBuffer = buffers[static_cast<size_t>(indexView.Buffer)];
                        mesh.Indices.reserve(indices.Count);

                        for (size_t index = 0; index < indices.Count; ++index)
                        {
                            mesh.Indices.push_back(ReadIndex(indexBuffer, views, indices, index));
                        }
                    }
                    else
                    {
                        for (uint32_t index = 0; index < static_cast<uint32_t>(mesh.Vertices.size()); ++index)
                        {
                            mesh.Indices.push_back(index);
                        }
                    }

                    if (normalAccessorIndex < 0)
                    {
                        GenerateNormals(mesh);
                    }

                    if (!mesh.Vertices.empty() && !mesh.Indices.empty())
                    {
                        if (outPrimitiveInfo)
                        {
                            GltfMeshPrimitiveInfo primitiveInfo;
                            primitiveInfo.Name = "Mesh_" + std::to_string(meshIndex) + "_Primitive_" + std::to_string(primitiveIndex);
                            primitiveInfo.Material = primitiveValue.Find("material") ? primitiveValue.Find("material")->AsInt(-1) : -1;
                            primitiveInfo.HasSkinWeights = jointAccessorIndex >= 0 && weightAccessorIndex >= 0;
                            outPrimitiveInfo->push_back(std::move(primitiveInfo));
                        }
                        outMeshes.push_back(std::move(mesh));
                    }

                    ++primitiveIndex;
                }

                ++meshIndex;
            }

            return !outMeshes.empty();
        }

        DirectX::XMFLOAT3 ReadFloat3Array(const JsonValue* value, DirectX::XMFLOAT3 fallback)
        {
            if (!value || !value->IsArray() || value->Array.size() < 3)
            {
                return fallback;
            }

            return {
                static_cast<float>(value->Array[0].AsNumber(fallback.x)),
                static_cast<float>(value->Array[1].AsNumber(fallback.y)),
                static_cast<float>(value->Array[2].AsNumber(fallback.z))
            };
        }

        DirectX::XMFLOAT4 ReadFloat4Array(const JsonValue* value, DirectX::XMFLOAT4 fallback)
        {
            if (!value || !value->IsArray() || value->Array.size() < 4)
            {
                return fallback;
            }

            return {
                static_cast<float>(value->Array[0].AsNumber(fallback.x)),
                static_cast<float>(value->Array[1].AsNumber(fallback.y)),
                static_cast<float>(value->Array[2].AsNumber(fallback.z)),
                static_cast<float>(value->Array[3].AsNumber(fallback.w))
            };
        }

        std::vector<int> ReadIntArray(const JsonValue* value)
        {
            std::vector<int> result;
            if (!value || !value->IsArray())
            {
                return result;
            }

            result.reserve(value->Array.size());
            for (const JsonValue& item : value->Array)
            {
                result.push_back(item.AsInt());
            }

            return result;
        }

        DirectX::XMFLOAT4X4 ReadFloat4x4(
            const std::vector<unsigned char>& bytes,
            const std::vector<BufferView>& views,
            const Accessor& accessor,
            size_t index)
        {
            DirectX::XMFLOAT4X4 matrix = {};
            if (accessor.BufferView < 0 || accessor.BufferView >= static_cast<int>(views.size()) || accessor.ComponentType != 5126)
            {
                DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixIdentity());
                return matrix;
            }

            const BufferView& view = views[static_cast<size_t>(accessor.BufferView)];
            const size_t stride = view.ByteStride == 0 ? sizeof(float) * 16u : view.ByteStride;
            const size_t offset = view.ByteOffset + accessor.ByteOffset + index * stride;

            if (offset + sizeof(float) * 16u <= bytes.size())
            {
                std::memcpy(&matrix, bytes.data() + offset, sizeof(float) * 16u);
            }
            else
            {
                DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixIdentity());
            }

            return matrix;
        }

        bool LoadAccessorData(
            const std::filesystem::path& resolvedPath,
            const JsonValue& root,
            std::vector<std::vector<unsigned char>>& outBuffers,
            std::vector<BufferView>& outViews,
            std::vector<Accessor>& outAccessors)
        {
            const JsonValue* buffersValue = root.Find("buffers");
            const JsonValue* viewsValue = root.Find("bufferViews");
            const JsonValue* accessorsValue = root.Find("accessors");
            if (!buffersValue || !viewsValue || !accessorsValue)
            {
                return false;
            }

            outBuffers.clear();
            for (const JsonValue& bufferValue : buffersValue->Array)
            {
                std::vector<unsigned char> bytes;
                if (!LoadBuffer(resolvedPath, bufferValue, bytes))
                {
                    return false;
                }
                outBuffers.push_back(std::move(bytes));
            }

            outViews.clear();
            for (const JsonValue& viewValue : viewsValue->Array)
            {
                BufferView view;
                view.Buffer = viewValue.Find("buffer") ? viewValue.Find("buffer")->AsInt() : 0;
                view.ByteOffset = static_cast<size_t>(viewValue.Find("byteOffset") ? viewValue.Find("byteOffset")->AsNumber() : 0.0);
                view.ByteLength = static_cast<size_t>(viewValue.Find("byteLength") ? viewValue.Find("byteLength")->AsNumber() : 0.0);
                view.ByteStride = static_cast<size_t>(viewValue.Find("byteStride") ? viewValue.Find("byteStride")->AsNumber() : 0.0);
                outViews.push_back(view);
            }

            outAccessors.clear();
            for (const JsonValue& accessorValue : accessorsValue->Array)
            {
                Accessor accessor;
                accessor.BufferView = accessorValue.Find("bufferView") ? accessorValue.Find("bufferView")->AsInt(-1) : -1;
                accessor.ByteOffset = static_cast<size_t>(accessorValue.Find("byteOffset") ? accessorValue.Find("byteOffset")->AsNumber() : 0.0);
                accessor.ComponentType = accessorValue.Find("componentType") ? accessorValue.Find("componentType")->AsInt() : 0;
                accessor.Count = static_cast<size_t>(accessorValue.Find("count") ? accessorValue.Find("count")->AsNumber() : 0.0);
                accessor.Type = accessorValue.Find("type") ? accessorValue.Find("type")->AsString() : "";
                accessor.Normalized = accessorValue.Find("normalized") ? accessorValue.Find("normalized")->Bool : false;
                outAccessors.push_back(accessor);
            }

            return true;
        }
    }

    bool GltfLoader::LoadFirstMesh(const std::filesystem::path& path, MeshData& outMesh)
    {
        const std::filesystem::path resolvedPath = FileSystem::FindAssetPath(path);

        std::string text;
        if (!FileSystem::ReadTextFile(resolvedPath, text))
        {
            return false;
        }

        JsonValue root;
        std::string error;
        if (!SimpleJson::Parse(text, root, &error))
        {
            Log(LogLevel::Error, "glTF JSON parse failed: " + error);
            return false;
        }

        const JsonValue* buffersValue = root.Find("buffers");
        const JsonValue* viewsValue = root.Find("bufferViews");
        const JsonValue* accessorsValue = root.Find("accessors");
        const JsonValue* meshesValue = root.Find("meshes");
        if (!buffersValue || !viewsValue || !accessorsValue || !meshesValue)
        {
            Log(LogLevel::Warning, "glTF file is missing buffers, bufferViews, accessors, or meshes.");
            return false;
        }

        std::vector<std::vector<unsigned char>> buffers;
        for (const JsonValue& bufferValue : buffersValue->Array)
        {
            std::vector<unsigned char> bytes;
            if (!LoadBuffer(resolvedPath, bufferValue, bytes))
            {
                return false;
            }
            buffers.push_back(std::move(bytes));
        }

        std::vector<BufferView> views;
        for (const JsonValue& viewValue : viewsValue->Array)
        {
            BufferView view;
            view.Buffer = viewValue.Find("buffer") ? viewValue.Find("buffer")->AsInt() : 0;
            view.ByteOffset = static_cast<size_t>(viewValue.Find("byteOffset") ? viewValue.Find("byteOffset")->AsNumber() : 0.0);
            view.ByteLength = static_cast<size_t>(viewValue.Find("byteLength") ? viewValue.Find("byteLength")->AsNumber() : 0.0);
            view.ByteStride = static_cast<size_t>(viewValue.Find("byteStride") ? viewValue.Find("byteStride")->AsNumber() : 0.0);
            views.push_back(view);
        }

        std::vector<Accessor> accessors;
        for (const JsonValue& accessorValue : accessorsValue->Array)
        {
            Accessor accessor;
            accessor.BufferView = accessorValue.Find("bufferView") ? accessorValue.Find("bufferView")->AsInt(-1) : -1;
            accessor.ByteOffset = static_cast<size_t>(accessorValue.Find("byteOffset") ? accessorValue.Find("byteOffset")->AsNumber() : 0.0);
                accessor.ComponentType = accessorValue.Find("componentType") ? accessorValue.Find("componentType")->AsInt() : 0;
                accessor.Count = static_cast<size_t>(accessorValue.Find("count") ? accessorValue.Find("count")->AsNumber() : 0.0);
                accessor.Type = accessorValue.Find("type") ? accessorValue.Find("type")->AsString() : "";
                accessor.Normalized = accessorValue.Find("normalized") ? accessorValue.Find("normalized")->Bool : false;
                accessors.push_back(accessor);
            }

        const JsonValue* mesh = meshesValue->At(0);
        const JsonValue* primitives = mesh ? mesh->Find("primitives") : nullptr;
        const JsonValue* primitive = primitives ? primitives->At(0) : nullptr;
        const JsonValue* attributes = primitive ? primitive->Find("attributes") : nullptr;
        if (!primitive || !attributes)
        {
            Log(LogLevel::Warning, "glTF file does not contain a first mesh primitive.");
            return false;
        }

        const JsonValue* positionValue = attributes->Find("POSITION");
        if (!positionValue)
        {
            Log(LogLevel::Warning, "glTF first primitive has no POSITION attribute.");
            return false;
        }

        const int positionAccessorIndex = positionValue->AsInt(-1);
        const int normalAccessorIndex = attributes->Find("NORMAL") ? attributes->Find("NORMAL")->AsInt(-1) : -1;
        const int texCoordAccessorIndex = attributes->Find("TEXCOORD_0") ? attributes->Find("TEXCOORD_0")->AsInt(-1) : -1;
        const int jointAccessorIndex = attributes->Find("JOINTS_0") ? attributes->Find("JOINTS_0")->AsInt(-1) : -1;
        const int weightAccessorIndex = attributes->Find("WEIGHTS_0") ? attributes->Find("WEIGHTS_0")->AsInt(-1) : -1;
        const int indexAccessorIndex = primitive->Find("indices") ? primitive->Find("indices")->AsInt(-1) : -1;

        if (positionAccessorIndex < 0 || positionAccessorIndex >= static_cast<int>(accessors.size()))
        {
            return false;
        }

        const Accessor& positions = accessors[static_cast<size_t>(positionAccessorIndex)];
        const BufferView& positionView = views[static_cast<size_t>(positions.BufferView)];
        const std::vector<unsigned char>& vertexBuffer = buffers[static_cast<size_t>(positionView.Buffer)];

        outMesh.Vertices.clear();
        outMesh.Indices.clear();
        outMesh.Vertices.reserve(positions.Count);

        for (size_t vertexIndex = 0; vertexIndex < positions.Count; ++vertexIndex)
        {
            Vertex vertex;
            vertex.Position = ReadFloat3(vertexBuffer, views, positions, vertexIndex);

            if (normalAccessorIndex >= 0 && normalAccessorIndex < static_cast<int>(accessors.size()))
            {
                const Accessor& normals = accessors[static_cast<size_t>(normalAccessorIndex)];
                const BufferView& normalView = views[static_cast<size_t>(normals.BufferView)];
                vertex.Normal = ReadFloat3(buffers[static_cast<size_t>(normalView.Buffer)], views, normals, vertexIndex);
            }

            if (texCoordAccessorIndex >= 0 && texCoordAccessorIndex < static_cast<int>(accessors.size()))
            {
                const Accessor& texCoords = accessors[static_cast<size_t>(texCoordAccessorIndex)];
                const BufferView& texCoordView = views[static_cast<size_t>(texCoords.BufferView)];
                vertex.TexCoord = ReadFloat2(buffers[static_cast<size_t>(texCoordView.Buffer)], views, texCoords, vertexIndex);
            }

            if (jointAccessorIndex >= 0 && jointAccessorIndex < static_cast<int>(accessors.size()))
            {
                const Accessor& joints = accessors[static_cast<size_t>(jointAccessorIndex)];
                const BufferView& jointView = views[static_cast<size_t>(joints.BufferView)];
                vertex.Joints = ReadJoint4(buffers[static_cast<size_t>(jointView.Buffer)], views, joints, vertexIndex);
            }

            if (weightAccessorIndex >= 0 && weightAccessorIndex < static_cast<int>(accessors.size()))
            {
                const Accessor& weights = accessors[static_cast<size_t>(weightAccessorIndex)];
                const BufferView& weightView = views[static_cast<size_t>(weights.BufferView)];
                vertex.Weights = ReadWeight4(buffers[static_cast<size_t>(weightView.Buffer)], views, weights, vertexIndex);
            }

            outMesh.Vertices.push_back(vertex);
        }

        if (indexAccessorIndex >= 0 && indexAccessorIndex < static_cast<int>(accessors.size()))
        {
            const Accessor& indices = accessors[static_cast<size_t>(indexAccessorIndex)];
            const BufferView& indexView = views[static_cast<size_t>(indices.BufferView)];
            const std::vector<unsigned char>& indexBuffer = buffers[static_cast<size_t>(indexView.Buffer)];
            outMesh.Indices.reserve(indices.Count);

            for (size_t index = 0; index < indices.Count; ++index)
            {
                outMesh.Indices.push_back(ReadIndex(indexBuffer, views, indices, index));
            }
        }
        else
        {
            for (uint32_t index = 0; index < static_cast<uint32_t>(outMesh.Vertices.size()); ++index)
            {
                outMesh.Indices.push_back(index);
            }
        }

        if (normalAccessorIndex < 0)
        {
            GenerateNormals(outMesh);
        }

        return !outMesh.Vertices.empty() && !outMesh.Indices.empty();
    }

    bool GltfLoader::LoadScene(const std::filesystem::path& path, GltfSceneAsset& outScene)
    {
        outScene = {};
        const std::filesystem::path resolvedPath = FileSystem::FindAssetPath(path);

        std::string text;
        if (!FileSystem::ReadTextFile(resolvedPath, text))
        {
            return false;
        }

        JsonValue root;
        std::string error;
        if (!SimpleJson::Parse(text, root, &error))
        {
            Log(LogLevel::Error, "glTF scene JSON parse failed: " + error);
            return false;
        }

        (void)LoadAllMeshes(resolvedPath, root, outScene.Meshes, &outScene.MeshPrimitives);

        std::vector<std::vector<unsigned char>> buffers;
        std::vector<BufferView> views;
        std::vector<Accessor> accessors;
        const bool hasAccessorData = LoadAccessorData(resolvedPath, root, buffers, views, accessors);

        std::vector<std::filesystem::path> imagePaths;
        if (const JsonValue* images = root.Find("images"))
        {
            for (const JsonValue& image : images->Array)
            {
                const std::string uri = image.Find("uri") ? image.Find("uri")->AsString() : "";
                if (!uri.empty() && uri.rfind("data:", 0) != 0)
                {
                    imagePaths.push_back(resolvedPath.parent_path() / uri);
                }
                else
                {
                    imagePaths.emplace_back();
                }
            }
        }

        std::vector<int> textureSources;
        if (const JsonValue* textures = root.Find("textures"))
        {
            for (const JsonValue& texture : textures->Array)
            {
                textureSources.push_back(texture.Find("source") ? texture.Find("source")->AsInt(-1) : -1);
            }
        }

        if (const JsonValue* materials = root.Find("materials"))
        {
            for (const JsonValue& materialValue : materials->Array)
            {
                GltfMaterialInfo material;
                material.Name = materialValue.Find("name") ? materialValue.Find("name")->AsString("Material") : "Material";

                if (const JsonValue* pbr = materialValue.Find("pbrMetallicRoughness"))
                {
                    const DirectX::XMFLOAT4 baseColor = ReadFloat4Array(
                        pbr->Find("baseColorFactor"),
                        { material.MaterialData.Albedo.x, material.MaterialData.Albedo.y, material.MaterialData.Albedo.z, material.MaterialData.Alpha });
                    material.MaterialData.Albedo = { baseColor.x, baseColor.y, baseColor.z };
                    material.MaterialData.Alpha = baseColor.w;
                    material.MaterialData.Roughness = static_cast<float>(pbr->Find("roughnessFactor") ? pbr->Find("roughnessFactor")->AsNumber(0.65) : 0.65);
                    material.MaterialData.Metallic = static_cast<float>(pbr->Find("metallicFactor") ? pbr->Find("metallicFactor")->AsNumber(0.0) : 0.0);

                    if (const JsonValue* baseColorTexture = pbr->Find("baseColorTexture"))
                    {
                        const int textureIndex = baseColorTexture->Find("index") ? baseColorTexture->Find("index")->AsInt(-1) : -1;
                        if (textureIndex >= 0 && textureIndex < static_cast<int>(textureSources.size()))
                        {
                            const int source = textureSources[static_cast<size_t>(textureIndex)];
                            if (source >= 0 && source < static_cast<int>(imagePaths.size()))
                            {
                                material.BaseColorTexturePath = imagePaths[static_cast<size_t>(source)];
                            }
                        }
                    }
                }

                outScene.Materials.push_back(std::move(material));
            }
        }

        if (const JsonValue* nodes = root.Find("nodes"))
        {
            for (const JsonValue& nodeValue : nodes->Array)
            {
                GltfNodeInfo node;
                node.Name = nodeValue.Find("name") ? nodeValue.Find("name")->AsString("Node") : "Node";
                node.Mesh = nodeValue.Find("mesh") ? nodeValue.Find("mesh")->AsInt(-1) : -1;
                node.Skin = nodeValue.Find("skin") ? nodeValue.Find("skin")->AsInt(-1) : -1;
                node.Translation = ReadFloat3Array(nodeValue.Find("translation"), node.Translation);
                node.Rotation = ReadFloat4Array(nodeValue.Find("rotation"), node.Rotation);
                node.Scale = ReadFloat3Array(nodeValue.Find("scale"), node.Scale);
                node.Children = ReadIntArray(nodeValue.Find("children"));
                outScene.Nodes.push_back(std::move(node));
            }
        }

        if (const JsonValue* skins = root.Find("skins"))
        {
            for (const JsonValue& skinValue : skins->Array)
            {
                GltfSkinInfo skin;
                skin.Name = skinValue.Find("name") ? skinValue.Find("name")->AsString("Skin") : "Skin";
                skin.InverseBindMatrices = skinValue.Find("inverseBindMatrices") ? skinValue.Find("inverseBindMatrices")->AsInt(-1) : -1;
                skin.Joints = ReadIntArray(skinValue.Find("joints"));
                if (hasAccessorData &&
                    skin.InverseBindMatrices >= 0 &&
                    skin.InverseBindMatrices < static_cast<int>(accessors.size()))
                {
                    const Accessor& matrices = accessors[static_cast<size_t>(skin.InverseBindMatrices)];
                    if (matrices.BufferView >= 0 && matrices.BufferView < static_cast<int>(views.size()))
                    {
                        const BufferView& matrixView = views[static_cast<size_t>(matrices.BufferView)];
                        const std::vector<unsigned char>& matrixBuffer = buffers[static_cast<size_t>(matrixView.Buffer)];
                        skin.InverseBindMatricesData.reserve(matrices.Count);
                        for (size_t matrixIndex = 0; matrixIndex < matrices.Count; ++matrixIndex)
                        {
                            skin.InverseBindMatricesData.push_back(ReadFloat4x4(matrixBuffer, views, matrices, matrixIndex));
                        }
                    }
                }
                outScene.Skins.push_back(std::move(skin));
            }
        }

        if (const JsonValue* animations = root.Find("animations"))
        {
            for (const JsonValue& animationValue : animations->Array)
            {
                GltfAnimationClipInfo clip;
                clip.Name = animationValue.Find("name") ? animationValue.Find("name")->AsString("Animation") : "Animation";
                const JsonValue* samplers = animationValue.Find("samplers");
                std::vector<std::string> samplerTargetPaths(samplers ? samplers->Array.size() : 0u);

                if (const JsonValue* channels = animationValue.Find("channels"))
                {
                    for (const JsonValue& channelValue : channels->Array)
                    {
                        GltfAnimationChannelInfo channel;
                        channel.Sampler = channelValue.Find("sampler") ? channelValue.Find("sampler")->AsInt(-1) : -1;
                        if (const JsonValue* target = channelValue.Find("target"))
                        {
                            channel.TargetNode = target->Find("node") ? target->Find("node")->AsInt(-1) : -1;
                            channel.TargetPath = target->Find("path") ? target->Find("path")->AsString() : "";
                        }
                        if (channel.Sampler >= 0 && channel.Sampler < static_cast<int>(samplerTargetPaths.size()))
                        {
                            samplerTargetPaths[static_cast<size_t>(channel.Sampler)] = channel.TargetPath;
                        }
                        clip.Channels.push_back(std::move(channel));
                    }
                }

                if (samplers)
                {
                    for (size_t samplerIndex = 0; samplerIndex < samplers->Array.size(); ++samplerIndex)
                    {
                        const JsonValue& samplerValue = samplers->Array[samplerIndex];
                        GltfAnimationSamplerInfo sampler;
                        sampler.Interpolation = samplerValue.Find("interpolation") ? samplerValue.Find("interpolation")->AsString("LINEAR") : "LINEAR";

                        const int inputAccessor = samplerValue.Find("input") ? samplerValue.Find("input")->AsInt(-1) : -1;
                        const int outputAccessor = samplerValue.Find("output") ? samplerValue.Find("output")->AsInt(-1) : -1;
                        if (hasAccessorData &&
                            inputAccessor >= 0 &&
                            inputAccessor < static_cast<int>(accessors.size()) &&
                            outputAccessor >= 0 &&
                            outputAccessor < static_cast<int>(accessors.size()))
                        {
                            const Accessor& input = accessors[static_cast<size_t>(inputAccessor)];
                            const Accessor& output = accessors[static_cast<size_t>(outputAccessor)];
                            if (input.BufferView >= 0 &&
                                input.BufferView < static_cast<int>(views.size()) &&
                                output.BufferView >= 0 &&
                                output.BufferView < static_cast<int>(views.size()))
                            {
                                const BufferView& inputView = views[static_cast<size_t>(input.BufferView)];
                                const BufferView& outputView = views[static_cast<size_t>(output.BufferView)];
                                const std::vector<unsigned char>& inputBuffer = buffers[static_cast<size_t>(inputView.Buffer)];
                                const std::vector<unsigned char>& outputBuffer = buffers[static_cast<size_t>(outputView.Buffer)];

                                sampler.InputTimes.reserve(input.Count);
                                for (size_t keyIndex = 0; keyIndex < input.Count; ++keyIndex)
                                {
                                    sampler.InputTimes.push_back(ReadScalarFloat(inputBuffer, views, input, keyIndex));
                                }

                                const std::string targetPath = samplerIndex < samplerTargetPaths.size() ? samplerTargetPaths[samplerIndex] : "";
                                if (targetPath == "rotation")
                                {
                                    sampler.OutputRotations.reserve(output.Count);
                                    for (size_t keyIndex = 0; keyIndex < output.Count; ++keyIndex)
                                    {
                                        sampler.OutputRotations.push_back(ReadFloat4(outputBuffer, views, output, keyIndex));
                                    }
                                }
                                else if (targetPath == "scale")
                                {
                                    sampler.OutputScales.reserve(output.Count);
                                    for (size_t keyIndex = 0; keyIndex < output.Count; ++keyIndex)
                                    {
                                        sampler.OutputScales.push_back(ReadFloat3(outputBuffer, views, output, keyIndex));
                                    }
                                }
                                else
                                {
                                    sampler.OutputTranslations.reserve(output.Count);
                                    for (size_t keyIndex = 0; keyIndex < output.Count; ++keyIndex)
                                    {
                                        sampler.OutputTranslations.push_back(ReadFloat3(outputBuffer, views, output, keyIndex));
                                    }
                                }
                            }
                        }

                        clip.Samplers.push_back(std::move(sampler));
                    }
                }

                outScene.Animations.push_back(std::move(clip));
            }
        }

        return !outScene.Meshes.empty() || !outScene.Nodes.empty() || !outScene.Materials.empty();
    }
}
