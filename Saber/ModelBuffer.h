/**
 * @file ModelBuffer.h
 * @brief Defines the per-object model constant buffer shared between C++ and HLSL.
 *
 * Contains the model and normal matrices, bounding-box extents, and material index.
 */
#ifndef MODEL_BUFFER_H
#define MODEL_BUFFER_H

#ifdef __cplusplus
#include <limits>
#endif  // __cplusplus

#include "HlslTypesDef.h"

/**
 * @brief Per-object GPU constant buffer carrying transformation and material data.
 *
 * Uploaded once when an object is modified; the GPU reads it during the geometry pass.
 */
struct ModelBuffer
{
    matrix modelMatrix;   /**< @brief Object-to-world transformation matrix. */
    matrix normalMatrix;  /**< @brief Transposed inverse of @ref modelMatrix, used for normal transform. */
    float4 bbmin;         /**< @brief Object-space AABB minimum corner (xyz); w unused. */
    float4 bbmax;         /**< @brief Object-space AABB maximum corner (xyz); w unused. */
    uint4  materialId;    /**< @brief Material index in x; yzw unused. */

#ifdef __cplusplus
    /** @brief Constructs a ModelBuffer with identity matrices and infinite-inverse AABB. */
    ModelBuffer() {
        modelMatrix = DirectX::XMMatrixIdentity();
        normalMatrix = DirectX::XMMatrixIdentity();
        bbmin = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            0.f
        };
        bbmax = {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            0.f
        };
        materialId = { 0, 0, 0, 0 };
    }

    /**
     * @brief Constructs a ModelBuffer with the given model matrix and material.
     * @param modelMatrix  Initial object-to-world matrix.
     * @param materialId   Material slot index (default 0).
     */
    ModelBuffer(const DirectX::XMMATRIX& modelMatrix, size_t materialId = 0) {
        UpdateMatrices(modelMatrix);
        SetMaterial(materialId);
    }

    /**
     * @brief Updates @ref modelMatrix and recomputes @ref normalMatrix.
     * @param newModelMatrix New object-to-world matrix.
     */
    void UpdateMatrices(const DirectX::XMMATRIX& newModelMatrix) {
        modelMatrix = newModelMatrix;
        normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, modelMatrix));
    }

    /**
     * @brief Sets the material slot index.
     * @param newMaterialId Index into the global material array.
     */
    void SetMaterial(size_t newMaterialId) {
        materialId.x = newMaterialId;
    }
#endif
};

#include "HlslTypesUndef.h"

#endif  // MODEL_BUFFER_H
