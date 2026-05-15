/**
 * @file MaterialCB.h
 * @brief Defines the material constant buffer structure shared between C++ and HLSL.
 *
 * Each entry stores texture IDs (albedo, normal) packed into a @c uint4.
 */
#ifndef MATERIAL_CB_H
#define MATERIAL_CB_H

#include "HlslTypesDef.h"

/** @brief Maximum number of materials that fit in a single MaterialCB. */
#define MaterialCB_SIZE 1024

/**
 * @brief GPU constant buffer holding texture IDs for up to @ref MaterialCB_SIZE materials.
 *
 * Layout per slot: x = albedo texture index, y = normal map texture index, zw = unused.
 */
struct MaterialCB
{
    /** @brief Per-material texture indices. x = albedoId, y = normalId. */
    uint4 materials[MaterialCB_SIZE];
};

#include "HlslTypesUndef.h"

#endif  // MATERIAL_CB_H
