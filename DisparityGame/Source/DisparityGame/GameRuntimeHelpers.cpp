#include "DisparityGame/GameRuntimeHelpers.h"

#include <Disparity/Assets/MaterialAsset.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace DisparityGame
{
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

}
