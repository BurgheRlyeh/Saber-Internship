#include "Math.hlsli"
#include "ModelBuffer.h"
#include "SceneBuffer.h"

ConstantBuffer<SceneBuffer> SceneCB : register(b0);

ConstantBuffer<ModelBuffer> ModelCB : register(b1);

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
    output.uvMaterialId = float4(input.uv, ModelCB.materialId.x, 0.f);
    output.tbn = tbnQuat;
    
    return output;
}
