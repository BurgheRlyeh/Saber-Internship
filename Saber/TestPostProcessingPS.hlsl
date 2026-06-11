struct VSOutput
{
    float2 uv : TEXCOORD;
};

cbuffer Params : register(b0) {
    float4 k;
}

Texture2D LightVolume : register(t0);
SamplerState s0 : register(s0);

float4 main(VSOutput pixel) : SV_TARGET
{
    float shadowDepth = LightVolume.Sample(s0, pixel.uv).r;
    float shadow = saturate(1.0f - k.x * shadowDepth);

    return float4(shadow, shadow, shadow, 1.0f);
}
