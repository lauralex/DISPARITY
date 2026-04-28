#pragma once

#include "DisparityGame/GameRuntimeTypes.h"

#include <Disparity/Disparity.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace DisparityGame
{
    DirectX::XMFLOAT3 Add(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);
    DirectX::XMFLOAT3 Subtract(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);
    DirectX::XMFLOAT3 Scale(const DirectX::XMFLOAT3& value, float scalar);
    DirectX::XMFLOAT3 Lerp(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t);
    DirectX::XMFLOAT3 CatmullRom(const DirectX::XMFLOAT3& p0, const DirectX::XMFLOAT3& p1, const DirectX::XMFLOAT3& p2, const DirectX::XMFLOAT3& p3, float t);
    float SmoothStep01(float value);
    float Dot(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);
    DirectX::XMFLOAT3 Cross(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);
    float Length(const DirectX::XMFLOAT3& value);
    DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& value);
    DirectX::XMFLOAT3 TransformDirection(const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT3& rotation);
    std::string SafeFileStem(std::string value);
    DirectX::XMFLOAT3 NormalizeFlat(const DirectX::XMFLOAT3& value);
    DirectX::XMFLOAT3 QuaternionToEuler(const DirectX::XMFLOAT4& q);
    Disparity::Transform CombineTransforms(const Disparity::Transform& parent, const Disparity::Transform& child);
    Disparity::Material LoadMaterial(Disparity::Renderer& renderer, const std::filesystem::path& path, const Disparity::Material& fallback);
    std::string Trim(std::string value);
    bool HasArgument(const std::wstring& arguments, const std::wstring& name);
    uint32_t ParseUnsignedArgument(const std::wstring& arguments, const std::wstring& name, uint32_t fallback);
    std::filesystem::path ParsePathArgument(const std::wstring& arguments, const std::wstring& name, const std::filesystem::path& fallback);
    double ParseDoubleArgument(const std::wstring& arguments, const std::wstring& name, double fallback);
}
