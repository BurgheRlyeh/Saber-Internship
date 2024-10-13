#pragma once

#include "Headers.h"

#include "Atlas.h"
#include "ComputeObject.h"
#include "ConstantBuffer.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"
#include "Texture.h"

#define FFX_CPU
#include "FidelityFX/gpu/ffx_core.h"
#include "FidelityFX/gpu/spd/ffx_spd.h"

class SinglePassDownsampler : public ComputeObject {
    struct SpdGlobalAtomicBuffer {
        FfxUInt32 counter[6];
    };
    std::shared_ptr<Texture> m_pSpdCounterBuffer{};
    std::shared_ptr<DescHeapRange> m_pSpdCounterBufferRange{};

    struct SPDConstantBuffer {
        FfxUInt32       mips{};
        FfxUInt32       numWorkGroups{};
        FfxUInt32x2     workGroupOffset{};
        FfxFloat32x2    invInputSize{};     // Only used for linear sampling mode
        FfxFloat32x2    padding{};
    } m_spdConstantBuffer{};
    std::shared_ptr<ConstantBuffer> m_pSpdConstantBuffer{};
    std::shared_ptr<DescHeapRange> m_pSpdConstantBufferRange{};

    uint32_t m_dispatchX{};
    uint32_t m_dispatchY{};

public:
    SinglePassDownsampler(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
        UINT64 width,
        UINT height
    );

    void Resize(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        UINT64 width,
        UINT height
    );

    void Dispatch(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListCompute,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescHeap,
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE midMipUavHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE mipsUavsHandle
    );

protected:
    virtual void InnerRootParametersSetter(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        UINT& rootParamId
    ) const override;

private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    ) {
        size_t rp{};
        CD3DX12_ROOT_PARAMETER1 rootParameters[5]{};

        // SPD ConstantBuffer
        CD3DX12_DESCRIPTOR_RANGE1 rangeCbvSpd[1]{};
        rangeCbvSpd[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
        rootParameters[rp++].InitAsDescriptorTable(_countof(rangeCbvSpd), rangeCbvSpd);

        // input texture
        CD3DX12_DESCRIPTOR_RANGE1 rangeSrvInput[1]{};
        rangeSrvInput[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        rootParameters[rp++].InitAsDescriptorTable(_countof(rangeSrvInput), rangeSrvInput);

        // global atomic counter
        CD3DX12_DESCRIPTOR_RANGE1 rangeUavCounter[1]{};
        rangeUavCounter[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        rootParameters[rp++].InitAsDescriptorTable(_countof(rangeUavCounter), rangeUavCounter);

        // mid mipmap
        CD3DX12_DESCRIPTOR_RANGE1 rangeUavMidMipmap[1]{};
        rangeUavMidMipmap[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
        rootParameters[rp++].InitAsDescriptorTable(_countof(rangeUavMidMipmap), rangeUavMidMipmap);

        // mips
        CD3DX12_DESCRIPTOR_RANGE1 rangeUavMips[1]{};
        rangeUavMips[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, -1, 2);
        rootParameters[rp++].InitAsDescriptorTable(_countof(rangeUavMips), rangeUavMips);

        D3D12_STATIC_SAMPLER_DESC sampler{
            .Filter{ D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT },
            .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
            .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
            .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
            .MipLODBias{},
            .MaxAnisotropy{ 1 },
            .ComparisonFunc{ D3D12_COMPARISON_FUNC_NEVER },
            .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
            .MinLOD{},
            .MaxLOD{ D3D12_FLOAT32_MAX },
            .ShaderRegister{},
            .RegisterSpace{},
            .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
        };

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler);

        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
        if (FAILED(pDevice->CheckFeatureSupport(
            D3D12_FEATURE_ROOT_SIGNATURE,
            &featureData,
            sizeof(featureData)
        ))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
        HRESULT hr{ D3DX12SerializeVersionedRootSignature(
            &rootSignatureDescription,
            featureData.HighestVersion,
            &rootSignatureBlob,
            &errorBlob
        ) };
        if (FAILED(hr) && errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        ThrowIfFailed(hr);

        return rootSignatureBlob;
    }
};