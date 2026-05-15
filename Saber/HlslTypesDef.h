/**
 * @file HlslTypesDef.h
 * @brief Defines HLSL-compatible type aliases for use in C++ code.
 *
 * Include this file before any struct that must be shared between C++ and HLSL.
 * Pair with @ref HlslTypesUndef.h to restore the original names afterwards.
 */
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

#endif	// __cplusplus
