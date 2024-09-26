#include "Math.hlsli"
#include "MaterialCB.h"

struct SceneBuffer
{
    matrix vpMatrix;
    matrix invViewProjMatrix;
    float4 cameraPosition;
    float4 nearFar;
};

ConstantBuffer<SceneBuffer> SceneCB : register(b0);

struct ModelBuffer
{
    matrix modelMatrix;
    matrix normalMatrix;
    uint4 materialId;
};

ConstantBuffer<ModelBuffer> ModelCB : register(b1);

ConstantBuffer<MaterialCB> Materials : register(b2);
Texture2D<float4> MaterialsTextures[] : register(t0);

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
    if (MaterialsTextures[Materials.materials[ModelCB.materialId.x].x].Sample(s1, input.uv).w == 0.f)
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
    output.uvMaterialId = float4(input.uv, ModelCB.materialId.x, 0.f);
    output.tbn = tbnQuat;
    
    return output;
}
