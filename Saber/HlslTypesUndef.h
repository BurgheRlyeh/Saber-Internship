/**
 * @file HlslTypesUndef.h
 * @brief Undefines the HLSL-compatible type aliases introduced by @ref HlslTypesDef.h.
 *
 * Must be included after the shared struct definition to avoid polluting
 * the rest of the C++ translation unit with the short-form macro names.
 */
#ifdef __cplusplus

#undef int2
#undef int3
#undef int4
#undef uint
#undef uint2
#undef uint3
#undef uint4
#undef float2
#undef float3
#undef float4
#undef matrix

#endif	// __cplusplus
