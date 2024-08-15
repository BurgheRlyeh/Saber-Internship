struct PSInput
{
    float3 norm : NORMAL;
    float4 color : COLOR;
};

float4 main(PSInput pixelIn) : SV_TARGET
{
    return pixelIn.color;
}
