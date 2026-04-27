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
    float DepthOfFieldStrength;
    float DepthOfFieldFocus;
    float DepthOfFieldRange;
    float LensDirtStrength;
    float VignetteStrength;
    float LetterboxAmount;
    float TitleSafeOpacity;
    float FilmGrainStrength;
    float PresentationPulse;
    float PostPadding;
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

float Hash21(float2 value)
{
    const float3 p = frac(float3(value.xyx) * 0.1031f);
    const float3 mixed = p + dot(p, p.yzx + 33.33f);
    return frac((mixed.x + mixed.y) * mixed.z);
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

float ComputeCircleOfConfusion(float depth)
{
    const float focusRange = max(DepthOfFieldRange, 0.001f);
    return saturate(abs(depth - DepthOfFieldFocus) / focusRange) * saturate(DepthOfFieldStrength);
}

float3 ComputeDepthOfField(float2 uv, float3 centerColor, out float coc)
{
    const float depth = DepthTexture.Sample(LinearSampler, uv).r;
    coc = ComputeCircleOfConfusion(depth);
    if (coc <= 0.001f)
    {
        return centerColor;
    }

    const float2 radius = InvResolution * lerp(1.0f, 9.0f, coc);
    float3 blur = centerColor * 0.24f;
    blur += SampleScene(uv + float2(1.0f, 0.0f) * radius) * 0.10f;
    blur += SampleScene(uv + float2(-1.0f, 0.0f) * radius) * 0.10f;
    blur += SampleScene(uv + float2(0.0f, 1.0f) * radius) * 0.10f;
    blur += SampleScene(uv + float2(0.0f, -1.0f) * radius) * 0.10f;
    blur += SampleScene(uv + float2(0.707f, 0.707f) * radius) * 0.09f;
    blur += SampleScene(uv + float2(-0.707f, 0.707f) * radius) * 0.09f;
    blur += SampleScene(uv + float2(0.707f, -0.707f) * radius) * 0.09f;
    blur += SampleScene(uv + float2(-0.707f, -0.707f) * radius) * 0.09f;
    return lerp(centerColor, blur, coc);
}

float ComputeLensDirt(float2 uv)
{
    const float2 centered = uv * 2.0f - 1.0f;
    const float vignette = saturate(dot(centered, centered));
    const float speckles =
        Hash21(floor(uv * 22.0f)) * 0.34f +
        Hash21(floor(uv * 57.0f + 11.0f)) * 0.22f +
        Hash21(floor(uv * 107.0f + 3.0f)) * 0.12f;
    const float streak = pow(saturate(1.0f - abs(centered.y + centered.x * 0.18f) * 2.2f), 8.0f) * 0.22f;
    return saturate(pow(speckles, 2.2f) * (0.35f + vignette * 0.9f) + streak);
}

float3 ApplyCinematicPresentation(float2 uv, float3 color)
{
    const float2 centered = uv * 2.0f - 1.0f;
    const float vignette = smoothstep(0.2f, 1.45f, dot(centered, centered));
    color *= 1.0f - vignette * saturate(VignetteStrength);

    const float2 pixelUv = uv / max(InvResolution, float2(0.000001f, 0.000001f));
    const float grain = (Hash21(pixelUv + PresentationPulse * 37.0f) - 0.5f) * FilmGrainStrength;
    color += grain.xxx;

    const float letter = step(uv.y, LetterboxAmount) + step(1.0f - LetterboxAmount, uv.y);
    color *= 1.0f - saturate(letter) * 0.92f;

    if (TitleSafeOpacity > 0.001f)
    {
        const float lineWidth = max(InvResolution.x, InvResolution.y) * 1.25f;
        const float titleLeft = 1.0f - smoothstep(0.0f, lineWidth, abs(uv.x - 0.10f));
        const float titleRight = 1.0f - smoothstep(0.0f, lineWidth, abs(uv.x - 0.90f));
        const float titleTop = 1.0f - smoothstep(0.0f, lineWidth, abs(uv.y - 0.10f));
        const float titleBottom = 1.0f - smoothstep(0.0f, lineWidth, abs(uv.y - 0.90f));
        const float actionLeft = 1.0f - smoothstep(0.0f, lineWidth, abs(uv.x - 0.05f));
        const float actionRight = 1.0f - smoothstep(0.0f, lineWidth, abs(uv.x - 0.95f));
        const float actionTop = 1.0f - smoothstep(0.0f, lineWidth, abs(uv.y - 0.05f));
        const float actionBottom = 1.0f - smoothstep(0.0f, lineWidth, abs(uv.y - 0.95f));
        const float guideLine = saturate(titleLeft + titleRight + titleTop + titleBottom + (actionLeft + actionRight + actionTop + actionBottom) * 0.45f);
        color = lerp(color, float3(0.22f, 0.86f, 1.0f), guideLine * TitleSafeOpacity * 0.65f);
    }

    return saturate(color);
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

    float dofCoc = 0.0f;
    hdrColor = ComputeDepthOfField(uv, hdrColor, dofCoc);

    const float3 historyColor = HistoryTexture.Sample(LinearSampler, uv).rgb;
    hdrColor = lerp(hdrColor, historyColor, saturate(TaaBlend) * saturate(HistoryValid));
    const float lensDirt = ComputeLensDirt(uv);
    hdrColor += bloom * lensDirt * LensDirtStrength * (1.0f + PresentationPulse * 1.5f);

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
    if (debugView == 5)
    {
        return float4(dofCoc.xxx, 1.0f);
    }
    if (debugView == 6)
    {
        return float4((ComputeLensDirt(uv) * saturate(LensDirtStrength + 0.25f)).xxx, 1.0f);
    }

    return float4(ApplyCinematicPresentation(uv, TonemapAndGamma(hdrColor)), 1.0f);
}
