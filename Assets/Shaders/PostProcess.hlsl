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

float4 PSMain(VSOutput input) : SV_TARGET
{
    const float2 uv = saturate(input.TexCoord);
    float3 hdrColor = SceneTexture.Sample(LinearSampler, uv).rgb;

    const float2 bloomStep = InvResolution * 3.0f;
    float3 bloom = 0.0f.xxx;
    bloom += max(SceneTexture.Sample(LinearSampler, uv + float2( bloomStep.x, 0.0f)).rgb - 1.0f.xxx, 0.0f.xxx);
    bloom += max(SceneTexture.Sample(LinearSampler, uv + float2(-bloomStep.x, 0.0f)).rgb - 1.0f.xxx, 0.0f.xxx);
    bloom += max(SceneTexture.Sample(LinearSampler, uv + float2(0.0f,  bloomStep.y)).rgb - 1.0f.xxx, 0.0f.xxx);
    bloom += max(SceneTexture.Sample(LinearSampler, uv + float2(0.0f, -bloomStep.y)).rgb - 1.0f.xxx, 0.0f.xxx);
    hdrColor += bloom * 0.25f * BloomStrength;

    const float centerDepth = DepthTexture.Sample(LinearSampler, uv).r;
    const float rightDepth = DepthTexture.Sample(LinearSampler, uv + float2(InvResolution.x * 2.0f, 0.0f)).r;
    const float downDepth = DepthTexture.Sample(LinearSampler, uv + float2(0.0f, InvResolution.y * 2.0f)).r;
    const float depthEdge = saturate((abs(centerDepth - rightDepth) + abs(centerDepth - downDepth)) * 18.0f);
    const float ambientOcclusion = lerp(1.0f, 1.0f - depthEdge * 0.65f, saturate(SsaoStrength));
    hdrColor *= ambientOcclusion;

    const float3 historyColor = HistoryTexture.Sample(LinearSampler, uv).rgb;
    hdrColor = lerp(hdrColor, historyColor, saturate(TaaBlend) * saturate(HistoryValid));
    hdrColor *= max(Exposure, 0.001f);

    const float3 reinhard = hdrColor / (hdrColor + 1.0f.xxx);
    const float3 mapped = lerp(saturate(hdrColor), reinhard, saturate(ToneMapEnabled));
    const float3 gammaCorrected = pow(saturate(mapped), 1.0f / 2.2f);
    return float4(gammaCorrected, 1.0f);
}
