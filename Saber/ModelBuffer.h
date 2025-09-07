#ifndef MODEL_BUFFER_H
#define MODEL_BUFFER_H

#ifdef __cplusplus

#include <limits>

#endif  // __cplusplus

#include "HlslCppTypesRedefine.h"

struct ModelBuffer
{
    matrix modelMatrix;
    matrix normalMatrix;
    float4 bbmin;
    float4 bbmax;
    uint4 materialId;

#ifdef __cplusplus
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
    ModelBuffer(const DirectX::XMMATRIX& modelMatrix, size_t materialId = 0) {
        UpdateMatrices(modelMatrix);
        SetMaterial(materialId);
    }

    void UpdateMatrices(const DirectX::XMMATRIX& newModelMatrix) {
        modelMatrix = newModelMatrix;
        normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, modelMatrix));
    }

    void SetMaterial(size_t newMaterialId) {
        materialId.x = newMaterialId;
    }
#endif
};
#endif  // MODEL_BUFFER_H
