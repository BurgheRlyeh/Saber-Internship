/**
 * @file CppHlslTypesRedefine.h
 * @brief Provides compatibility macros so that HLSL built-in types map to
 *        their C++/DirectX Math equivalents when compiled as C++, and to
 *        native HLSL types when compiled as HLSL shader code.
 *
 * This header is intended to be included in files that are compiled by both
 * the C++ compiler and the HLSL compiler (e.g. shared constant-buffer structs).
 */
#ifndef CPP_HLSL_TYPES_REDEFINE_H
#define CPP_HLSL_TYPES_REDEFINE_H

#ifdef __cplusplus

#include "Headers.h"

#else

#define uint32_t uint
#define DirectX::XMUINT2 uint2
#define DirectX::XMUINT3 uint3
#define DirectX::XMUINT4 uint4

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
