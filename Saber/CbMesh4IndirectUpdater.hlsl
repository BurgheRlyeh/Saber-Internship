#include "IndirectCommand.h"

#define CommandType CbMesh4IndirectCommand
#include "IndirectCommandUpdater.hlsli"

[numthreads(threadBlockSize, 1, 1)]
void main(ComputeShaderInput IN)
{
    UpdateIndirectCommands(IN);
}