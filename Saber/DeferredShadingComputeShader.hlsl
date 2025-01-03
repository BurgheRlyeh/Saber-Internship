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

float3 WorldPositionFromDepth(float2 uv, float depth)
{
    uv = float2(2.f, -2.f) * uv - float2(1.f, -1.f);
    float4 worldPos = mul(SceneCB.invViewProjMatrix, float4(uv, depth, 1.f));
    return worldPos.xyz / worldPos.w;
}

#define DDX_DDY_PIXEL_CHECK_CNT 4   // 2 / 4
float2 BestUVDerivative(
    int3 pixel,
    float4 pixelUVMI,
    float pixelDepth,
    int3 pixelDeltas[DDX_DDY_PIXEL_CHECK_CNT]
)
{
    float2 uvBest = float2(1.f, 1.f);
    
    const float allowedDeltaDepth = 0.0025f;
    for (int i = 0; i < DDX_DDY_PIXEL_CHECK_CNT; ++i)
    {
        float deltaDepth = NonUniformResourceIndex(depthBuffer.Load(pixel + pixelDeltas[i])) - pixelDepth;
        if (abs(deltaDepth) > allowedDeltaDepth)
            continue;
        
        float4 uvmiDelta = NonUniformResourceIndex(uvMaterialId.Load(pixel + pixelDeltas[i])) - pixelUVMI;
        if (uvmiDelta.z == 0.f && uvmiDelta.w == 0.f && length(uvmiDelta.xy) < length(uvBest))
            uvBest = uvmiDelta.xy;
    }

    if (uvBest.x == 1.f && uvBest.y == 1.f)
    {
        return float2(0.f, 0.f);
    }
    return uvBest;
}

#define FFX_GPU
#define FFX_HLSL
#include "ffx_core.h"

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
    uint2 GTid = ffxRemapForWaveReduction(IN.GroupIndex);
    
    uint3 pixel = uint3(IN.GroupID.xy * 8 + GTid.xy, 0);
    
    uint w = 0;
    uint h = 0;
    uvMaterialId.GetDimensions(w, h);
    
    if (!(GTid.x < w && GTid.y < h))
    {
        return;
    }
    
    float4 uvmi = NonUniformResourceIndex(uvMaterialId.Load(pixel));
    float2 uv = uvmi.xy;
    uint materialId = uvmi.z;
    
    if (materialId == 0) {
        output[pixel.xy] = float4(.4f, .6f, .9f, 1.f);
        return;
    }
    
    uint4 material = Materials.materials[materialId];
    float depth = depthBuffer.Load(pixel);
    
#if DDX_DDY_PIXEL_CHECK_CNT == 2
    int3 pixelDeltasX[DDX_DDY_PIXEL_CHECK_CNT] = { int3(-1, 0, 0), int3(1, 0, 0) };
    int3 pixelDeltasY[DDX_DDY_PIXEL_CHECK_CNT] = { int3(0, -1, 0), int3(0, 1, 0) };
#elif DDX_DDY_PIXEL_CHECK_CNT == 4
    int3 pixelDeltasX[DDX_DDY_PIXEL_CHECK_CNT] = { int3(-1, 0, 0), int3(0, -1, 0), int3(1, 0, 0), int3(0, 1, 0) };
    int3 pixelDeltasY[DDX_DDY_PIXEL_CHECK_CNT] = { int3(-1, 0, 0), int3(0, -1, 0), int3(1, 0, 0), int3(0, 1, 0) };
#endif
    
    float2 uvDdx = BestUVDerivative(pixel, uvmi, depth, pixelDeltasX);
    float2 uvDdy = BestUVDerivative(pixel, uvmi, depth, pixelDeltasY);
    
    // normal
    float3 nmValue = MaterialsTextures[NonUniformResourceIndex(material.y)].SampleGrad(s1, uv, uvDdx, uvDdy).xyz;
    float3 localNorm = normalize(2.f * nmValue - 1.f); // normalize to avoid unnormalized texture
    float4 tbnQuat = tbn.Load(pixel);
    matrix tbnMatrix = quaternion_to_matrix(tbnQuat);
    float3 norm = mul(tbnMatrix, float4(localNorm, 0.f)).xyz;
    
    // world position
    float2 uvGlobal = float2(pixel.xy) / float2(w, h);
    float3 worldPos = WorldPositionFromDepth(uvGlobal, depth);
    
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
    
    float3 albedo = MaterialsTextures[NonUniformResourceIndex(material.x)].SampleGrad(s1, uv, uvDdx, uvDdy);
    
    float3 finalColor = albedo * lightColor;
    
    output[pixel.xy] = float4(finalColor, 1.f);
}