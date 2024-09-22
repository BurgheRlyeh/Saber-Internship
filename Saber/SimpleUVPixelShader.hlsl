#include "Math.hlsli"

struct SceneBuffer
{
    matrix vpMatrix;
    float4 cameraPosition;
};

ConstantBuffer<SceneBuffer> SceneCB : register(b0);

struct ModelBuffer
{
    matrix modelMatrix;
    matrix normalMatrix;
    uint4 materialId;
};

ConstantBuffer<ModelBuffer> ModelCB : register(b1);

struct MaterialBuffer
{
    uint4 albedoNormal;
};
ConstantBuffer<MaterialBuffer> MaterialCB[] : register(b2);

//Texture2D t0 : register(t0);
//Texture2D t1 : register(t1);
Texture2D texs[] : register(t0);

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
    float4 position : SV_Target0;
    float4 tbn : SV_Target1;
    float4 color : SV_Target2;
    float4 uvMaterialId : SV_Target3;
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
    output.position = float4(input.worldPos, 1.f);
    output.tbn = tbnQuat;
    output.uvMaterialId = float4(input.uv, ModelCB.materialId.x, 0.f);
    
    return output;
}
