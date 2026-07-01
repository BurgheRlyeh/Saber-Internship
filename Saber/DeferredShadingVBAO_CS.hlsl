#include "BlinnPhongLighting.hlsli"
#include "Math.hlsli"
#include "MaterialCB.h"
#include "SceneBuffer.h"

ConstantBuffer<SceneBuffer> SceneCB : register(b0);
ConstantBuffer<LightBuffer> LightCB : register(b1);

Texture2D<float4> uvMaterialId : register(t0);
Texture2D<float4> tbn : register(t1);

Texture2D<float> depthBuffer : register(t2);

RWTexture2D<float4> output : register(u0);
RWTexture2D<float> output_vbao : register(u1);

ConstantBuffer<MaterialCB> Materials : register(b2);
Texture2D<float4> MaterialsTextures[] : register(t3);

SamplerState s1 : register(s0);

static const float pi = 3.14159265359;
static const float twoPi = 2.0 * pi;
static const float halfPi = 0.5 * pi;




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

#define USE_VBAO
#ifdef USE_VBAO
// VBAO Utility funcs
// https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
float randf(int x, int y)
{
    return fmod(52.9829189 * fmod(0.06711056 * float(x) + 0.00583715 * float(y), 1.0), 1.0);
}

// https://graphics.stanford.edu/%7Eseander/bithacks.html
uint bitCount(uint value)
{
    value = value - ((value >> 1u) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2u) & 0x33333333u);
    return ((value + (value >> 4u) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
}

// https://cdrinmatane.github.io/posts/ssaovb-code/
static const uint sectorCount = 32;
uint updateSectors(float minHorizon, float maxHorizon, uint outBitfield)
{
    uint startBit = uint(minHorizon * float(sectorCount));
    uint horizonAngle = uint(round((maxHorizon - minHorizon) * float(sectorCount)));
    uint angleBit = horizonAngle > 0u ? uint(0xFFFFFFFFu >> (sectorCount - horizonAngle)) : 0u;
    uint currentBitfield = angleBit << startBit;
    return outBitfield | currentBitfield;
}



float ACosPoly(float x)
{
#if ACOS_QUALITY_MODE == 1
    // GTAOFastAcos
    return 1.5707963267948966 - 0.1565827644218014 * x;
#else    
    // higher quality version of GTAOFastAcos (for the cost of one additional mad)
    // minimizes max abs(ACos_Approx(cos(x)) - x)
    return 1.5707963267948966 + (-0.20491203466059038 + 0.04832927023878897 * x) * x;
#endif    
}

float ACos_Approx(float x)
{
    float u = ACosPoly(abs(x)) * sqrt(1.0 - abs(x));
			
    return x >= 0.0 ? u : pi - u;
}

float ACos01_Approx(float x)// x: [0,1]
{
    return ACosPoly(x) * sqrt(1.0 - x);
}

float ACos(float x)
{
    return ACos_Approx(clamp(x, -1.0, 1.0));
}
// end of VBAO Utility funcs

static const int sampleCount = 5;
static const float sampleRadius = 20.0;
static const float sliceCount = 4.0;
static const float hitThickness = 0.1;

float3 CalculateSliceOcclusion(float2 direction, int3 pixel, uint w, uint h, float3 worldPos, float3 worldNormal)
{
    uint occlusion = 0; 
    int3 samplePixel = pixel;
    float3 Horizon = float3(0, 0, 0);
    float3 camera = normalize(SceneCB.cameraPosition.xyz - worldPos);
    float2 TettaSign = float2(-1, -1);
    float2 Tetta = float2(0, 0);
    float test = 0.5;
    float3 dirWorld = mul(SceneCB.viewProjMatrix, float4(direction, 0.0f, 0.f)).xyz;
    float3 orthoAxis = (cross(camera, dirWorld));
            //projectedNormal = normalize(worldNormal - orthoAxis * dot(orthoAxis, worldNormal));
           // Horizon = cross(projectedNormal, orthoAxis);
        Horizon = normalize(cross(worldNormal, orthoAxis));
    float3 projectedNormal = normalize(cross(orthoAxis, Horizon));
    
        float d = sign(dot(Horizon, camera));
            //if ( d < 0)
        Horizon = select(d < 0, -Horizon, Horizon);
    
    d = sign(dot(worldNormal, projectedNormal));

    float step = sampleRadius / sampleCount;
    float minTetta = 0;
    float maxTetta = pi;

    for (int i = -sampleCount + 1; i < sampleCount; i++)
    {
        if (i == 0)
        {
            continue;
        }
        samplePixel.xy = float2(pixel.xy) + direction * (i) * step;
        
        if (samplePixel.x >= w || samplePixel.x < 0 || samplePixel.y >= h || samplePixel.y < 0)
        {
            continue;
        }
        
        float4 uvmi = NonUniformResourceIndex(uvMaterialId.Load(samplePixel));
        float2 uv = uvmi.xy;
        uint materialId = uvmi.z;
    
        if (materialId == 0)
        {
            continue;
        }
        
        float depth = depthBuffer.Load(samplePixel);
            // world position
        float2 uvGlobal = float2(samplePixel.xy) / float2(w, h);
        float3 sampleWorldPos = WorldPositionFromDepth(uvGlobal, depth) ;
        float3 horizFront = (sampleWorldPos - worldPos);
        float lengthV = length(horizFront);
        horizFront /= lengthV;
        float3 horizBack = normalize(horizFront - (camera * hitThickness));
        

        TettaSign = float2(dot(projectedNormal, horizFront), dot(projectedNormal, horizBack));
        if (TettaSign.x < 0.)
        {
            continue;
        }

        
        Tetta = acos(TettaSign);
        TettaSign = sign(float2(dot(Horizon, horizFront), dot(Horizon, horizBack)));
        Tetta = Tetta * TettaSign + halfPi;
        
        if (TettaSign.x < 0.)
        {
            minTetta = max(Tetta.x, minTetta);

        }
        else
        {
            maxTetta = min(Tetta.x, maxTetta);
        }
        
        
        //Tetta = Tetta / pi;
        //Tetta = clamp(Tetta, 0.0, 1.0);
        //occlusion = updateSectors(Tetta.x, Tetta.y, occlusion);
    
    }   
    
    float res = (maxTetta - minTetta) / pi;
   // d = dot(Horizon, camera);
    Horizon = Horizon * 0.5 + 0.5;
    float3 visibility = float3(res, (minTetta) / pi, (maxTetta) / pi); //    mul(SceneCB.viewProjMatrix, float4(Horizon, 1.f)).xyz; //float2(0, 0);
    //float3 visibility =  float(bitCount(occlusion)) / float(sectorCount);
    return visibility;

}

#endif

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
    
    if (materialId == 0)
    {
        output[pixel.xy] = float4(.4f, .6f, .9f, 1.f);
        output_vbao[pixel.xy] = 0.;
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
    
#ifdef USE_VBAO
    
    float3 ao = CalculateSliceOcclusion(float2(0, -1), pixel, w, h, worldPos, normalize(norm));
    //ao = CalculateSliceOcclusion(float2(0, 1), pixel, w, h, worldPos, norm);
    //ao += CalculateSliceOcclusion(normalize(float2(1, 1)), pixel, w, h, worldPos, norm);
    //ao += CalculateSliceOcclusion(normalize(float2(-1, 1)), pixel, w, h, worldPos, norm);
    //ao = ao / 4;
    
#endif    
    float3 lightColor = LightCB.ambientColorAndPower.xyz * LightCB.ambientColorAndPower.w;
    for (uint i = 0; i < LightCB.lightsCount.x; ++i)
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
    
    float3 albedo = MaterialsTextures[NonUniformResourceIndex(material.x)].SampleGrad(s1, uv, uvDdx, uvDdy).rgb;
    
    float3 finalColor = worldPos;// albedo * lightColor;
    #ifdef USE_VBAO
    //output[pixel.xy] = float4(finalColor, 1.f);
    output[pixel.xy] = float4(ao, 1.f);
    output_vbao[pixel.xy] = ao;
    #else
    output[pixel.xy] = float4(finalColor, 1.f);
    output_vbao[pixel.xy] = 0.25f;
#endif

}