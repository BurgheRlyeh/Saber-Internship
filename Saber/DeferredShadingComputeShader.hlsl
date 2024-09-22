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

Texture2D<float4> position : register(t0); // position
Texture2D<float4> normal : register(t1); // normal
Texture2D<float4> albedo : register(t2); // albedo
Texture2D<float4> uvMaterialId : register(t3); // 

RWTexture2D<float4> output : register(u0);

struct MaterialBuffer
{
    uint4 albedoNormal;
};
ConstantBuffer<MaterialBuffer> MaterialCBs[] : register(b2);
Texture2D<float4> MaterialsTextures[] : register(t4);

SamplerState s1 : register(s0);


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
    
        float3 uvm = uvMaterialId.Load(DTid).xyz;
    
        uint materialId = uvm.z;
        if (materialId == 0) {
            output[pixel] = float4(.4f, .6f, .9f, 1.f);
            return;
        }
    
        MaterialBuffer material = MaterialCBs[uvm.z];
        float3 albedoValue = MaterialsTextures[material.albedoNormal.x].Sample(s1, uvm.xy).xyz;
        float3 normalValue = MaterialsTextures[material.albedoNormal.y].Sample(s1, uvm.xy).xyz;
    
        float3 lightColor = LightCB.ambientColorAndPower.xyz * LightCB.ambientColorAndPower.w;
        for (uint i = 0; i < LightCB.lightCount.x; ++i)
        {
            Lighting lighting = GetPointLight(
                LightCB.lights[i],
                position.Load(DTid).xyz,
                position.Load(DTid).xyz - SceneCB.cameraPosition.xyz,
                normal.Load(DTid).xyz,
                //normalize(normalValue),
                1.f
            );
        
            lightColor += lighting.diffuse;
            lightColor += lighting.specular;
        }
    
        //float3 finalColor = albedo.Load(DTid).xyz * lightColor;
        float3 finalColor = albedoValue * lightColor;
    
        output[pixel] = float4(finalColor, 1.f);
    }
}