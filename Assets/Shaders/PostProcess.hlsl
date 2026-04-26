Texture2D SceneTexture : register(t0);
Texture2D HistoryTexture : register(t1);
Texture2D DepthTexture : register(t2);
SamplerState LinearSampler : register(s0);

cbuffer PostConstants : register(b0)
{
    float Exposure;
    float ToneMapEnabled;
    float BloomStrength;
    float SsaoStrength;
    float TaaBlend;
    float HistoryValid;
    float2 InvResolution;
    float BloomThreshold;
    float AntiAliasingStrength;
    float ColorSaturation;
    float ColorContrast;
    float PostDebugView;
    float AntiAliasingEnabled;
    float2 Padding;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
    VSOutput output;
    const float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    const float2 uvs[3] = {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };

    output.Position = float4(positions[vertexId], 0.0f, 1.0f);
    output.TexCoord = uvs[vertexId];
    return output;
}

float3 SampleScene(float2 uv)
{
    return SceneTexture.Sample(LinearSampler, saturate(uv)).rgb;
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 ComputeBloom(float2 uv)
{
    const float threshold = max(BloomThreshold, 0.01f);
    float3 bloom = 0.0f.xxx;
    float totalWeight = 0.0f;

    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            const float2 offset = float2((float)x, (float)y) * InvResolution * 3.0f;
            const float distanceWeight = 1.0f / (1.0f + dot(float2((float)x, (float)y), float2((float)x, (float)y)));
            const float3 sampleColor = SampleScene(uv + offset);
            const float brightness = max(Luminance(sampleColor) - threshold, 0.0f);
            const float softKnee = brightness / (brightness + threshold);
            bloom += sampleColor * softKnee * distanceWeight;
            totalWeight += distanceWeight;
        }
    }

    return bloom / max(totalWeight, 0.001f);
}

float ComputeDepthOcclusion(float2 uv)
{
    const float centerDepth = DepthTexture.Sample(LinearSampler, uv).r;
    float occlusion = 0.0f;
    float totalWeight = 0.0f;

    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            if (x == 0 && y == 0)
            {
                continue;
            }

            const float2 offset = float2((float)x, (float)y) * InvResolution * 2.0f;
            const float neighborDepth = DepthTexture.Sample(LinearSampler, saturate(uv + offset)).r;
            const float depthDelta = max(neighborDepth - centerDepth, 0.0f);
            const float weight = 1.0f / (1.0f + abs((float)x) + abs((float)y));
            occlusion += saturate(depthDelta * 70.0f) * weight;
            totalWeight += weight;
        }
    }

    const float rightDepth = DepthTexture.Sample(LinearSampler, saturate(uv + float2(InvResolution.x * 2.0f, 0.0f))).r;
    const float downDepth = DepthTexture.Sample(LinearSampler, saturate(uv + float2(0.0f, InvResolution.y * 2.0f))).r;
    const float contactEdge = saturate((abs(centerDepth - rightDepth) + abs(centerDepth - downDepth)) * 32.0f);
    return saturate((occlusion / max(totalWeight, 0.001f)) + contactEdge * 0.5f);
}

float3 ResolveAntiAliasing(float2 uv, float3 centerColor, out float edgeAmount)
{
    const float3 leftColor = SampleScene(uv - float2(InvResolution.x, 0.0f));
    const float3 rightColor = SampleScene(uv + float2(InvResolution.x, 0.0f));
    const float3 upColor = SampleScene(uv - float2(0.0f, InvResolution.y));
    const float3 downColor = SampleScene(uv + float2(0.0f, InvResolution.y));

    const float centerLuma = Luminance(centerColor);
    const float minLuma = min(centerLuma, min(min(Luminance(leftColor), Luminance(rightColor)), min(Luminance(upColor), Luminance(downColor))));
    const float maxLuma = max(centerLuma, max(max(Luminance(leftColor), Luminance(rightColor)), max(Luminance(upColor), Luminance(downColor))));
    edgeAmount = saturate((maxLuma - minLuma - 0.025f) * 6.0f) * saturate(AntiAliasingStrength) * saturate(AntiAliasingEnabled);

    const float3 crossAverage = (leftColor + rightColor + upColor + downColor) * 0.25f;
    return lerp(centerColor, crossAverage, edgeAmount * 0.6f);
}

float3 ApplyColorGrade(float3 color)
{
    const float luminance = Luminance(color);
    color = lerp(luminance.xxx, color, max(ColorSaturation, 0.0f));
    color = (color - 0.5f.xxx) * max(ColorContrast, 0.0f) + 0.5f.xxx;
    return color;
}

float3 TonemapAndGamma(float3 hdrColor)
{
    hdrColor *= max(Exposure, 0.001f);
    const float3 reinhard = hdrColor / (hdrColor + 1.0f.xxx);
    const float3 mapped = lerp(saturate(hdrColor), reinhard, saturate(ToneMapEnabled));
    return pow(saturate(ApplyColorGrade(mapped)), 1.0f / 2.2f);
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    const float2 uv = saturate(input.TexCoord);
    const int debugView = (int)round(PostDebugView);

    float3 hdrColor = SampleScene(uv);
    float aaEdge = 0.0f;
    hdrColor = ResolveAntiAliasing(uv, hdrColor, aaEdge);

    const float3 bloom = ComputeBloom(uv);
    hdrColor += bloom * BloomStrength;

    const float depthOcclusion = ComputeDepthOcclusion(uv);
    const float ambientOcclusion = lerp(1.0f, 1.0f - depthOcclusion * 0.82f, saturate(SsaoStrength));
    hdrColor *= ambientOcclusion;

    const float3 historyColor = HistoryTexture.Sample(LinearSampler, uv).rgb;
    hdrColor = lerp(hdrColor, historyColor, saturate(TaaBlend) * saturate(HistoryValid));

    if (debugView == 1)
    {
        return float4(TonemapAndGamma(bloom * max(BloomStrength, 0.001f) * 3.0f), 1.0f);
    }
    if (debugView == 2)
    {
        return float4((1.0f - depthOcclusion * saturate(SsaoStrength)).xxx, 1.0f);
    }
    if (debugView == 3)
    {
        return float4(aaEdge.xxx, 1.0f);
    }
    if (debugView == 4)
    {
        const float depth = DepthTexture.Sample(LinearSampler, uv).r;
        return float4(pow(saturate(depth), 24.0f).xxx, 1.0f);
    }

    return float4(TonemapAndGamma(hdrColor), 1.0f);
}
