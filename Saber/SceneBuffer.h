#ifndef SCENE_BUFFER_H
#define SCENE_BUFFER_H

#include "HlslCppTypesRedefine.h"

#ifdef __cplusplus
#include "Camera.h"
#endif

struct SceneBuffer {
    matrix viewProjMatrix;
    matrix invViewProjMatrix;
    float4 cameraPosition;
    float4 nearFar; // x - near, y - far
    float4 viewFrustumPlanes[6];

#ifdef __cplusplus
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

#endif  // SCENE_BUFFER_H
