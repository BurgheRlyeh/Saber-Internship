#ifndef INDIRECT_COMMAND_UPDATER_HLSLI
#define INDIRECT_COMMAND_UPDATER_HLSLI

#define threadBlockSize 128

cbuffer UpdateCountBuffer : register(b0)
{
    uint countToUpdate;
}

StructuredBuffer<uint> updateBufferIds : register(t0);
StructuredBuffer<CommandType> updateBuffer : register(t1);
RWStructuredBuffer<CommandType> indirectCommandBuffer : register(u0);

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

[numthreads(threadBlockSize, 1, 1)]
void UpdateIndirectCommands(ComputeShaderInput IN)
{
    uint index = (IN.GroupID.x * threadBlockSize) + IN.GroupIndex;
    if (index < countToUpdate)
    {
        indirectCommandBuffer[updateBufferIds[index]] = updateBuffer[index];
    }
}

#endif
