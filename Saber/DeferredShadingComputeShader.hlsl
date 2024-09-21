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

Texture2D<float4> position : register(t0);
Texture2D<float4> normal : register(t1);
Texture2D<float4> albedo : register(t2);

RWTexture2D<float4> output : register(u0);

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

#define BLOCK_SIZE 8
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    uint w = 0;
    uint h = 0;
    position.GetDimensions(w, h);
    
    if (IN.DispatchThreadID.x < w && IN.DispatchThreadID.y < h)
    {
        
        uint2 pixel = IN.DispatchThreadID.xy;
    
        float3 lightColor = LightCB.ambientColorAndPower.xyz * LightCB.ambientColorAndPower.w;
        for (uint i = 0; i < LightCB.lightCount.x; ++i)
        {
            Lighting lighting = GetPointLight(
            LightCB.lights[i],
            position.Load(IN.DispatchThreadID).xyz,
            position.Load(IN.DispatchThreadID).xyz - SceneCB.cameraPosition.xyz,
            normal.Load(IN.DispatchThreadID).xyz,
            10.f
        );
        
            lightColor += lighting.diffuse;
            lightColor += lighting.specular;
        }
    
        float3 finalColor = albedo.Load(IN.DispatchThreadID).xyz * lightColor;
    
        output[pixel] = float4(finalColor, 1.f);
    }
}