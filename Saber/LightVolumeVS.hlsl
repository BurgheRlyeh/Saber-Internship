#include "ModelBuffer.h"
#include "CameraBuffer.h"

ConstantBuffer<CameraBuffer> CameraCB : register(b0);

cbuffer ModelBuffer : register(b1) {
    float4x4 invLightViewProj;
}

Texture2D<float4> ShadowMap : register(t0);
SamplerState s1 : register(s0);

struct VSOutput
{
    float3 worldPos : POSITION;
    float depthSm : TEXCOORD;
    float viewDepth : TEXCOORD1;
    float4 position : SV_Position;
};

VSOutput main(
    float3 position : POSITION,
    float3 norm : NORMAL,
    float4 tang : TANGENT,
    float2 uv : TEXCOORD
)
{
    VSOutput vtxOut;

    float depthSm = ShadowMap.SampleLevel(s1, uv, 0).r;

    float4 lightNdc = float4(uv.x * 2.f - 1.f, 1.f - uv.y * 2.f, depthSm, 1.f);

    float4 worldPos = mul(invLightViewProj, lightNdc);
    worldPos /= worldPos.w;

    vtxOut.worldPos = worldPos.xyz;
    vtxOut.depthSm = depthSm;

    vtxOut.position = mul(CameraCB.viewProjMatrix, worldPos);
    vtxOut.viewDepth = vtxOut.position.w;

    return vtxOut;
}
