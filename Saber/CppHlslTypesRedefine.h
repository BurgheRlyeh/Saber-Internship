#ifndef CPP_HLSL_TYPES_REDEFINE_H
#define CPP_HLSL_TYPES_REDEFINE_H

#ifdef __cplusplus

#include "Headers.h"

#else

#define D3D12_GPU_VIRTUAL_ADDRESS	uint2
#define D3D12_INDEX_BUFFER_VIEW		uint4
#define D3D12_VERTEX_BUFFER_VIEW	uint4
struct D3D12_DRAW_INDEXED_ARGUMENTS {
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int BaseVertexLocation;
    uint StartInstanceLocation;
};

#endif

#endif