struct PointLightData
{
    float3 Position;
    float Radius;
    float3 Color;
    float Intensity;
};

cbuffer FrameConstants : register(b0)
{
    row_major float4x4 ViewProjection;
    float3 CameraPosition;
    float ElapsedTime;
    float3 LightDirection;
    float AmbientIntensity;
    float3 LightColor;
    float LightIntensity;
    row_major float4x4 LightViewProjection;
    float ShadowStrength;
    float ShadowMapTexelSize;
    float PointLightCount;
    float ClusteredLighting;
    PointLightData PointLights[8];
};

cbuffer ObjectConstants : register(b1)
{
    row_major float4x4 World;
    float4 AlbedoRoughness;
    float4 Surface;
    float4 EmissiveColor;
};

Texture2D BaseColorTexture : register(t0);
Texture2D ShadowMapTexture : register(t1);
SamplerState LinearSampler : register(s0);

float ComputeShadow(float3 worldPosition)
{
    const float4 lightPosition = mul(float4(worldPosition, 1.0f), LightViewProjection);
    if (lightPosition.w <= 0.0f)
    {
        return 1.0f;
    }

    const float3 projected = lightPosition.xyz / lightPosition.w;
    const float2 uv = float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);

    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || projected.z < 0.0f || projected.z > 1.0f)
    {
        return 1.0f;
    }

    const float bias = 0.0035f;
    float visibility = 0.0f;
    const float2 texel = float2(ShadowMapTexelSize, ShadowMapTexelSize);

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            const float closestDepth = ShadowMapTexture.Sample(LinearSampler, uv + float2(x, y) * texel).r;
            visibility += (projected.z - bias) <= closestDepth ? 1.0f : 1.0f - saturate(ShadowStrength);
        }
    }

    return visibility / 9.0f;
}

float3 ComputePointLights(float3 worldPosition, float3 normal, float3 viewDir, float3 baseColor, float roughness, float metallic)
{
    float3 result = 0.0f.xxx;

    [unroll]
    for (int index = 0; index < 8; ++index)
    {
        if (index >= (int)PointLightCount)
        {
            break;
        }

        const float3 toLight = PointLights[index].Position - worldPosition;
        const float distanceToLight = length(toLight);
        const float radius = max(PointLights[index].Radius, 0.001f);
        const float attenuation = saturate(1.0f - distanceToLight / radius);
        const float3 lightDir = toLight / max(distanceToLight, 0.001f);
        const float diffuse = saturate(dot(normal, lightDir));
        const float3 halfVector = normalize(lightDir + viewDir);
        const float specPower = lerp(96.0f, 8.0f, saturate(roughness));
        const float specular = pow(saturate(dot(normal, halfVector)), specPower) * lerp(0.04f, 0.55f, saturate(metallic));
        result += (baseColor * diffuse + specular.xxx) * PointLights[index].Color * PointLights[index].Intensity * attenuation * attenuation;
    }

    return result * saturate(ClusteredLighting);
}

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    const float4 worldPosition = mul(float4(input.Position, 1.0f), World);
    output.Position = mul(worldPosition, ViewProjection);
    output.WorldPosition = worldPosition.xyz;
    output.Normal = normalize(mul(float4(input.Normal, 0.0f), World).xyz);
    output.TexCoord = input.TexCoord;
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    const float3 normal = normalize(input.Normal);
    const float3 lightDir = normalize(-LightDirection);
    const float3 viewDir = normalize(CameraPosition - input.WorldPosition);
    const float3 halfVector = normalize(lightDir + viewDir);
    const float3 albedo = AlbedoRoughness.rgb;
    const float roughness = AlbedoRoughness.a;
    const float metallic = Surface.x;
    const float alpha = Surface.y;
    const float useTexture = Surface.z;
    const float emissiveIntensity = Surface.w;

    const float3 textureColor = BaseColorTexture.Sample(LinearSampler, input.TexCoord).rgb;
    const float3 baseColor = lerp(albedo, albedo * textureColor, saturate(useTexture));

    const float diffuse = saturate(dot(normal, lightDir));
    const float specPower = lerp(96.0f, 8.0f, saturate(roughness));
    const float specular = pow(saturate(dot(normal, halfVector)), specPower) * lerp(0.08f, 0.75f, saturate(metallic));
    const float3 diffuseColor = baseColor * (1.0f - saturate(metallic) * 0.65f);
    const float shadow = ComputeShadow(input.WorldPosition);
    const float3 direct = diffuseColor * (AmbientIntensity + diffuse * LightColor * LightIntensity * shadow) + specular * LightColor * shadow;
    const float3 clustered = ComputePointLights(input.WorldPosition, normal, viewDir, diffuseColor, roughness, metallic);
    const float3 lit = direct + clustered + EmissiveColor.rgb * max(emissiveIntensity, 0.0f);
    return float4(max(lit, 0.0f.xxx), alpha);
}
