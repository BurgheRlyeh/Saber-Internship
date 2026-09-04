#ifndef CAMERA_BUFFER_H
#define CAMERA_BUFFER_H

#ifdef __cplusplus
#include "Camera.h"
#endif

#include "HlslTypesDef.h"

struct CameraBuffer {
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

        const Camera::Settings& cameraSettings{ pCamera->GetSettings() };
        nearFar = { cameraSettings.nearPlane, cameraSettings.farPlane, 0.f, 0.f };

        pCamera->BuildViewFrustumPlanes(viewFrustumPlanes, &viewProjMatrix);
    }
#endif
};

#include "HlslTypesUndef.h"

#endif  // CAMERA_BUFFER_H
