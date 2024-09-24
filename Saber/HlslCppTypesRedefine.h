#ifndef HLSL_CPP_TYPES_REDEFINE_H
#define HLSL_CPP_TYPES_REDEFINE_H

#ifdef __cplusplus

#include <DirectXMath.h>

#define int2 DirectX::XMINT2
#define int3 DirectX::XMINT3
#define int4 DirectX::XMINT4

#define uint uint32_t
#define uint2 DirectX::XMUINT2
#define uint3 DirectX::XMUINT3
#define uint4 DirectX::XMUINT4

#define float2 DirectX::XMFLOAT2
#define float3 DirectX::XMFLOAT3
#define float4 DirectX::XMFLOAT4

#define matrix DirectX::XMMATRIX

//#define

#endif	// __cplusplus

#endif	// HLSL_CPP_TYPES_REDEFINE_H