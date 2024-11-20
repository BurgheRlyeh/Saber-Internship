#ifndef MODEL_BUFFER_H
#define MODEL_BUFFER_H

#include "HlslCppTypesRedefine.h"

struct ModelBuffer
{
    matrix modelMatrix;
    matrix normalMatrix;
    uint4 materialId;

#ifdef __cplusplus
    ModelBuffer() = default;
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
