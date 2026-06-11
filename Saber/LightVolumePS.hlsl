struct PSInput
{
    float3 worldPos : POSITION;
    float depthSm : TEXCOORD;
    float viewDepth : TEXCOORD1;
    float4 position : SV_Position;
};

float main(PSInput input, bool isFront : SV_IsFrontFace) : SV_TARGET
{
    clip(input.depthSm - 1e-5f);
    float d = input.viewDepth;
	return isFront ? -d : d;
}