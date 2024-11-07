#define threadBlockSize 128

cbuffer UpdateCountBuffer : register(b0)
{
    uint countToUpdate;
}

struct DrawIndexedArguments
{
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int BaseVertexLocation;
    uint StartInstanceLocation;
};
struct IndirectCommand
{
    uint2 cbvAddress;
    uint4 indexBufferView;
    uint4 vertexBufferView;
    DrawIndexedArguments drawArgs;
};

StructuredBuffer<uint> updateBufferIds : register(t0);
StructuredBuffer<IndirectCommand> updateBuffer : register(t1);
RWStructuredBuffer<IndirectCommand> indirectCommandBuffer : register(u0);

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};
[numthreads(threadBlockSize, 1, 1)]
void main(ComputeShaderInput IN)
{
    uint index = (IN.GroupID.x * threadBlockSize) + IN.GroupIndex;
    if (index < countToUpdate)
    {
        uint instId = updateBufferIds[index];
        IndirectCommand indirectCommand = updateBuffer[index];
        indirectCommandBuffer[instId] = indirectCommand;
    }
}