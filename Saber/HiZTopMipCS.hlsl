cbuffer HiZTopMipCB : register(b0)
{
    uint2 sourceSize;
    uint targetSize;
    uint padding;
}

Texture2D<float> DepthBuffer : register(t0);
RWTexture2D<float> HiZTopMip : register(u0);

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID;
    uint3 GroupThreadID : SV_GroupThreadID;
    uint3 DispatchThreadID : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

#define BLOCK_SIZE 8
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    uint2 target = IN.DispatchThreadID.xy;
    if (target.x >= targetSize || target.y >= targetSize)
    {
        return;
    }

    uint2 source = min(target * sourceSize / targetSize, sourceSize - 1);
    HiZTopMip[target] = DepthBuffer.Load(int3(source, 0));
}
