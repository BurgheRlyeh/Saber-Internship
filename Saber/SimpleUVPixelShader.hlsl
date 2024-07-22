Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

struct PSInput
{
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
    return t1.Sample(s1, input.uv);
}
