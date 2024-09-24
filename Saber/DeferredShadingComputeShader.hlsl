#include "BlinnPhongLighting.hlsli"
#include "Math.hlsli"
#include "MaterialCB.h"

#define LIGHTS_MAX_COUNT 10

struct SceneBuffer
{
    matrix vpMatrix;
    matrix invViewProjMatrix;
    float4 cameraPosition;
    float4 nearFar;
};

ConstantBuffer<SceneBuffer> SceneCB : register(b0);

struct LightBuffer
{
    float4 ambientColorAndPower;
    uint4 lightCount;
    Light lights[LIGHTS_MAX_COUNT];
};

ConstantBuffer<LightBuffer> LightCB : register(b1);

Texture2D<float4> uvMaterialId : register(t0);
Texture2D<float4> tbn : register(t1);

Texture2D<float> depthBuffer : register(t2);

RWTexture2D<float4> output : register(u0);

ConstantBuffer<MaterialCB> Materials : register(b2);
Texture2D<float4> MaterialsTextures[] : register(t3);

SamplerState s1 : register(s0);

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

float3 WorldPositionFromDepth(float2 uv, float depth)
{
    uv = float2(2.f, -2.f) * uv - float2(1.f, -1.f);
    float4 worldPos = mul(SceneCB.invViewProjMatrix, float4(uv, depth, 1.f));
    return worldPos.xyz / worldPos.w;
}

#define BLOCK_SIZE 8
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    uint w = 0;
    uint h = 0;
    uvMaterialId.GetDimensions(w, h);
    
    if (!(IN.DispatchThreadID.x < w && IN.DispatchThreadID.y < h))
    {
        return;
    }
    
    uint2 pixel = IN.DispatchThreadID.xy;
    
    float3 uvm = NonUniformResourceIndex(uvMaterialId.Load(IN.DispatchThreadID).xyz);
    
    uint materialId = uvm.z;
    if (materialId == 0) {
        output[pixel] = float4(.4f, .6f, .9f, 1.f);
        return;
    }
    
    uint4 material = Materials.materials[uvm.z];
    
    // normal
    float3 nmValue = MaterialsTextures[material.y].Sample(s1, uvm.xy).xyz;
    float3 localNorm = normalize(2.f * nmValue - 1.f); // normalize to avoid unnormalized texture
    float4 tbnQuat = tbn.Load(IN.DispatchThreadID);
    matrix tbnMatrix = quaternion_to_matrix(tbnQuat);
    float3 norm = mul(tbnMatrix, float4(localNorm, 0.f)).xyz;
    
    // world position
    float2 uv = float2(pixel) / float2(w, h);
    float depth = depthBuffer.Load(IN.DispatchThreadID);
    float3 worldPos = WorldPositionFromDepth(uv, depth);
    
    float3 lightColor = LightCB.ambientColorAndPower.xyz * LightCB.ambientColorAndPower.w;
    for (uint i = 0; i < LightCB.lightCount.x; ++i)
    {
        Lighting lighting = GetPointLight(
            LightCB.lights[i],
            worldPos,
            worldPos - SceneCB.cameraPosition.xyz,
            norm,
            1.f
        );
        
        lightColor += lighting.diffuse;
        lightColor += lighting.specular;
    }
    
    float3 albedo = MaterialsTextures[material.x].Sample(s1, uvm.xy).xyz;
    float3 finalColor = albedo * lightColor;
    
    output[pixel] = float4(finalColor, 1.f);
}