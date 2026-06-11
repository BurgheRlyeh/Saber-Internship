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

struct VSOutput
{
    float2 uv : TEXCOORD;
    float4 position : SV_Position;
};

VSOutput main(
    float3 position : POSITION,
    float2 uv : TEXCOORD
)
{
    VSOutput vtxOut;
    
    vtxOut.uv = uv;

    float4 pos = float4(position.xyz, 1.f);
    pos = mul(ModelCBs[modelCbId].modelMatrix, pos);
    vtxOut.position = mul(CameraCB.viewProjMatrix, pos);
    
    return vtxOut;
}
