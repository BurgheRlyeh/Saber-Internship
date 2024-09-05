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

RWTexture2D<float4> output : register(u0);
Texture2D<float3> position : register(t0);
Texture2D<float3> normal : register(t1);
Texture2D<float4> albedo : register(t2);


SamplerState s1 : register(s0);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint2 pixel = DTid.xy;
    
    float3 lightColor = LightCB.ambientColorAndPower.xyz * LightCB.ambientColorAndPower.w;
    for (uint i = 0; i < LightCB.lightCount.x; ++i)
    {
        Lighting lighting = GetPointLight(
            LightCB.lights[i],
            position[pixel],
            position[pixel] - SceneCB.cameraPosition.xyz,
            normal[pixel],
            10.f
        );

        lightColor += lighting.diffuse;
        lightColor += lighting.specular;
    }
    
    float3 finalColor = albedo[pixel].xyz * lightColor;
    
    //output[pixel] = float4(finalColor, 1.f);
    output[pixel] = float4(1.f, 0.f, 0.f, 1.f);
}