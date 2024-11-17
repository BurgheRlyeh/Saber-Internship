#pragma once

#include "Headers.h"

struct ModelBuffer {
    DirectX::XMMATRIX m_modelMatrix{ DirectX::XMMatrixIdentity() };
    DirectX::XMMATRIX m_normalMatrix{ DirectX::XMMatrixIdentity() };
    DirectX::XMUINT4 m_materialId{};

    ModelBuffer() = default;
    ModelBuffer(const DirectX::XMMATRIX& modelMatrix, size_t materialId = 0) {
        UpdateMatrices(modelMatrix);
        SetMaterial(materialId);
    }

    void UpdateMatrices(const DirectX::XMMATRIX& modelMatrix) {
        m_modelMatrix = modelMatrix;
        m_normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, m_modelMatrix));
    }

    void SetMaterial(size_t materialId) {
        m_materialId.x = materialId;
    }
};
