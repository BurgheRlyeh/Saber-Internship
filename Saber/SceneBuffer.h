/**
 * @file SceneBuffer.h
 * @brief Defines the per-frame scene constant buffer shared between C++ and HLSL.
 *
 * Holds camera matrices, position, clipping planes, and view-frustum planes
 * required by vertex and compute shaders.
 */
#ifndef SCENE_BUFFER_H
#define SCENE_BUFFER_H

#ifdef __cplusplus
#include "Camera.h"
#endif

#include "HlslTypesDef.h"

/**
 * @brief Per-frame GPU constant buffer describing the active camera and scene.
 *
 * Updated every frame before the render pass and bound to all shader stages
 * that need camera or frustum information.
 */
struct SceneBuffer {
    matrix viewProjMatrix;          /**< @brief Combined view-projection matrix. */
    matrix invViewProjMatrix;       /**< @brief Inverse of @ref viewProjMatrix, used for reconstruction. */
    float4 cameraPosition;          /**< @brief World-space camera position (xyz); w unused. */
    float4 nearFar;                 /**< @brief x = near plane distance, y = far plane distance. */
    float4 viewFrustumPlanes[6];    /**< @brief Six view-frustum planes for GPU-side culling. */

#ifdef __cplusplus
    /**
     * @brief Populates the buffer from the given camera's current state.
     * @param pCamera Active scene camera to read matrices and planes from.
     */
    void Update(const std::shared_ptr<Camera>& pCamera) {
        viewProjMatrix = pCamera->GetViewProjectionMatrix();
        invViewProjMatrix = DirectX::XMMatrixInverse(nullptr, viewProjMatrix);

        DirectX::XMFLOAT3 pos{ pCamera->GetPosition() };
        cameraPosition = { pos.x, pos.y, pos.z, 0.f };

        nearFar = { pCamera->m_near, pCamera->m_far, 0.f, 0.f };

        pCamera->BuildViewFrustumPlanes(viewFrustumPlanes, &viewProjMatrix);
    }
#endif
};

#include "HlslTypesUndef.h"

#endif  // SCENE_BUFFER_H
