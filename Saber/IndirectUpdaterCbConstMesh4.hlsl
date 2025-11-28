#include "IndirectCommand.h"

#define CommandType CbConstMesh4IndirectCommand
#include "IndirectCommandUpdater.hlsli"

[numthreads(threadBlockSize, 1, 1)]
void main(ComputeShaderInput IN)
{
    UpdateIndirectCommands(IN);
}