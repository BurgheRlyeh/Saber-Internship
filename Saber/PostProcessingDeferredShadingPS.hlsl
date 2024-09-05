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

SamplerState s1 : register(s0);

struct VSOutput
{
    float2 uv : TEXCOORD;
};

float4 main(VSOutput pixel) : SV_TARGET
{
    float3 lightColor = LightCB.ambientColorAndPower.xyz * LightCB.ambientColorAndPower.w;
    for (uint i = 0; i < LightCB.lightCount.x; ++i)
    {
        Lighting lighting = GetPointLight(
            LightCB.lights[i],
            position.Sample(s1, pixel.uv).xyz,
            position.Sample(s1, pixel.uv).xyz - SceneCB.cameraPosition.xyz,
            normal.Sample(s1, pixel.uv).xyz,
            10.f
        );

        lightColor += lighting.diffuse;
        lightColor += lighting.specular;
    }
    
    float3 finalColor = albedo.Sample(s1, pixel.uv).xyz * lightColor;
    
    return float4(finalColor, 1.f);
}
