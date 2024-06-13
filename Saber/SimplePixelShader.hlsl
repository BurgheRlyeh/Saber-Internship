struct PSInput
{
    float4 color : COLOR;
};

float4 main(PSInput pixelIn) : SV_TARGET
{
    return pixelIn.color;
}
