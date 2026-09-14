#include "IndirectCommand.h"

#define CommandType ConstMesh4IndirectCommand
#include "OcclusionCulling.hlsli"

[numthreads(CULLING_GROUP_SIZE, 1, 1)]
void main(ComputeShaderInput IN)
{
    CullObjects(IN);
}
