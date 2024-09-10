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

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint2 pixel = DTid.xy;
    
    float3 lightColor = LightCB.ambientColorAndPower.xyz * LightCB.ambientColorAndPower.w;
    for (uint i = 0; i < LightCB.lightCount.x; ++i)
    {
        Lighting lighting = GetPointLight(
            LightCB.lights[i],
            position.Load(DTid).xyz,
            position.Load(DTid).xyz - SceneCB.cameraPosition.xyz,
            normal.Load(DTid).xyz,
            10.f
        );
        
        lightColor += lighting.diffuse;
        lightColor += lighting.specular;
    }
    
    float3 finalColor = albedo.Load(DTid).xyz * lightColor;
    
    output[pixel] = float4(finalColor, 1.f);
}