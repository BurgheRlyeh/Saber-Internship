#include "Math.hlsli"
#include "MaterialCB.h"
#include "ModelBuffer.h"
#include "CameraBuffer.h"

ConstantBuffer<CameraBuffer> CameraCB : register(b0);

cbuffer RootConstants : register(b1)
{
    uint modelCbId;
}
StructuredBuffer<ModelBuffer> ModelCBs : register(t0);

ConstantBuffer<MaterialCB> Materials : register(b2);
Texture2D<float4> MaterialsTextures[] : register(t1);

SamplerState s1 : register(s0);

struct PSInput
{
    float2 uv : TEXCOORD;
};

void main(PSInput input)
{
    uint materialId = ModelCBs[modelCbId].materialId.x;
    if (MaterialsTextures[Materials.materials[materialId].x].Sample(s1, input.uv).w == 0.f)
    {
        discard;
    }
}
