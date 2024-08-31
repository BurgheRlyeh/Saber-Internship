#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

#include "Atlas.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"

class PostProcessing : protected RenderObject {


public:
    using RenderObject::InitMaterial;

protected:
    virtual void RenderJob(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect) const override {
        
    }

    virtual void InnerRootParametersSetter(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        UINT& rootParamId
    ) const override {

    }

    UINT GetIndexCountPerInstance() const override {
        return 3;
    }

    UINT GetInstanceCount() const override {
        return 1;
    }
};
