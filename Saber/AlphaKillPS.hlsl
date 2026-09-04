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
    float3 worldPos : POSITION;
    float3 norm : NORMAL;
    float4 tang : TANGENT;
    float2 uv : TEXCOORD;
};

struct PSOutput
{
    float4 uvMaterialId : SV_Target0;
    float4 tbn : SV_Target1;
};

PSOutput main(PSInput input)
{
    uint materialId = ModelCBs[modelCbId].materialId.x;
    if (MaterialsTextures[Materials.materials[materialId].x].Sample(s1, input.uv).w == 0.f)
    {
        discard;
    }
    
    float3 t = normalize(input.tang.xyz);
    float3 n = normalize(input.norm);
    float3 b = (cross(n, t)) * input.tang.w; // no need to normalize
    
    matrix tbnMatrix = transpose(matrix(
        float4(t, 0.f),
        float4(b, 0.f),
        float4(n, 0.f),
        float4(0.f, 0.f, 0.f, 1.f)
    ));
    float4 tbnQuat = matrix_to_quaternion(tbnMatrix);
    
    PSOutput output;
    output.uvMaterialId = float4(input.uv, materialId, 0.f);
    output.tbn = tbnQuat;
    
    return output;
}
