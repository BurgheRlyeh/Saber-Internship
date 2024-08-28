struct VSOutput
{
    float2 uv : TEXCOORD;
};

Texture2D t0 : register(t0);
SamplerState s0 : register(s0);

float4 main(VSOutput pixel) : SV_TARGET
{
    return t0.Sample(s0, pixel.uv);
}