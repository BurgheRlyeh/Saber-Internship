cbuffer SpdCB : register(b0)
{
    uint mips;
    uint numWorkGroups;
    uint2 workGroupOffset;
    float2 invInputSize;
    float2 padding;
}

Texture2D<float> InputTexture : register(t0);

struct SpdGlobalAtomicBuffer
{
    uint counter[6];
};
RWStructuredBuffer<SpdGlobalAtomicBuffer> SpdGlobalAtomicCounterBuffer : register(u0);

globallycoherent RWTexture2D<float> TextureMip6 : register(u1);
RWTexture2D<float> TextureMips[] : register(u2);

SamplerState s_LinearClamp : register(s0);

groupshared uint spdCounter;
groupshared float4 spdIntermediate[16][16];

// Either samples or loads from the input texture for a given UV and slice.
float4 SpdLoadSourceImage(float2 uv, uint slice)
{
    return float4(InputTexture.Load(int3(uv, slice)), 0.f, 0.f, 0.f);
}

// Loads from separately bound MIP level 6 resource with the [globallycoherent] tag
// for a given UV and slice. This function is only used by the last active thread group.
float4 SpdLoad(int2 uv, uint slice)
{
    return float4(TextureMip6.Load(int3(uv, slice)), 0, 0, 0);
}

// Stores the output to the MIP levels. If mip value is 5, the output needs to be
// stored to the separately bound MIP level 6 resource with the [globallycoherent] tag.
void SpdStore(int2 uv, float4 value, uint mip, uint slice)
{
    if (mip == 5)
        TextureMip6[uv] = value.x;
    TextureMips[mip][uv] = value.x;
}

// Increments the global atomic counter. We have one counter per slice.
// The atomic add returns the previous value, which needs to be stored
// to shared memory, since only thread 0 calls the atomic add. This way,
// the previous value is accessible by all threads in the thread group and can
// be used by each thread to determine whether they should terminate or continue.
void SpdIncreaseAtomicCounter(uint slice)
{
    InterlockedAdd(SpdGlobalAtomicCounterBuffer[0].counter[slice], 1, spdCounter);
}

// Reads the value of the global atomic counter that was stored in shared memory.
uint SpdGetAtomicCounter()
{
    return spdCounter;
}

// Resets the global atomic counter for the given slice back to 0.
void SpdResetAtomicCounter(uint slice)
{
    SpdGlobalAtomicCounterBuffer[0].counter[slice] = 0;
}

// The 2×2 -> 1 reduction kernel that is user specified.
// Common reduction functions are min, max or the average.
float4 SpdReduce4(float4 v0, float4 v1, float4 v2, float4 v3)
{
    return max(max(v0, v1), max(v2, v3));
}

// Loads from the group shared buffer.
float4 SpdLoadIntermediate(uint x, uint y)
{
    return spdIntermediate[x][y];
}

// Stores to the group shared buffer.
void SpdStoreIntermediate(uint x, uint y, float4 value)
{
    spdIntermediate[x][y] = value;
}

#define FFX_GPU
#define FFX_HLSL
#include "ffx_core.h"
#include "spd/ffx_spd.h"

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};
[numthreads(256, 1, 1)]
void main(ComputeShaderInput IN)
{
    SpdDownsample(IN.GroupID.xy, IN.GroupIndex, mips, numWorkGroups, IN.GroupID.z, workGroupOffset);
}