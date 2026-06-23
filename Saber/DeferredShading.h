#pragma once

#include "Headers.h"

#include "ComputeObject.h"

class DeviceContext;

class DeferredShading : public ComputeObject {
public:
    static std::shared_ptr<ComputeObject> CreateDefferedShadingComputeObject(
        std::shared_ptr<DeviceContext> pDeviceContext,
        bool DoSSAO
    );

private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(bool DoSSAO);
};