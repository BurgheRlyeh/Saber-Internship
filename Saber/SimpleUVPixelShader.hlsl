#include "BlinnPhongLighting.hlsli"

#define LIGHTS_MAX_COUNT 10

struct SceneBuffer
{
    matrix vpMatrix;
    float4 cameraPosition;
};

ConstantBuffer<SceneBuffer> SceneCB : register(b0);

struct LightBuffer
{
    float4 ambientColorAndPower;
    uint4 lightCount;
    Light lights[LIGHTS_MAX_COUNT];
};

ConstantBuffer<LightBuffer> LightCB : register(b1);

struct ModelBuffer
{
    matrix modelMatrix;
    matrix normalMatrix;
};

ConstantBuffer<ModelBuffer> ModelCB : register(b2);

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
    float4 normal : SV_Target1;
    float4 color : SV_Target2;
};

PSOutput main(PSInput input)
{
    float3 lightColor = LightCB.ambientColorAndPower.xyz * LightCB.ambientColorAndPower.w;
    float3 t = normalize(input.tang);
    float3 n = normalize(input.norm);
    float3 binorm = (cross(n, t)) * input.tang.w; // no need to normalize
    float3 nn = texs[1].Sample(s1, input.uv).xyz;
    float3 localNorm = normalize(2.f * nn - float3(1.f, 1.f, 1.f)); // normalize to avoid unnormalized texture
    float3 norm = localNorm.x * t + localNorm.y * binorm + localNorm.z * n;

    //for (uint i = 0; i < LightCB.lightCount.x; ++i)
    //{
    //    Lighting lighting = GetPointLight(
    //        LightCB.lights[i],
    //        input.worldPos,
    //        input.worldPos - SceneCB.cameraPosition.xyz,
    //        norm,
    //        10.f
    //    );

    //    lightColor += lighting.diffuse;
    //    lightColor += lighting.specular;
    //}
    
    float4 texColor = texs[0].Sample(s1, input.uv);
    //float3 finalColor = texColor.xyz * lightColor;
    
    PSOutput output;
    output.position = float4(input.worldPos, 1.f);
    output.normal = float4(norm, 0.f);
    output.color = texColor;
    
    return output;
}
