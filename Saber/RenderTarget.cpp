#include "RenderTarget.h"

RenderTarget::RenderTarget(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    uint8_t numBuffers,
    const D3D12_RESOURCE_DESC& resDesc,
    const D3D12_CLEAR_VALUE& clearValue
) : m_numBuffers(numBuffers) {
    m_descSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    m_pRTVDescHeap = CreateDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_numBuffers);
    m_pSRVDescHeap = CreateDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_numBuffers, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    Resize(pDevice, pAllocator, resDesc, clearValue);
}

void RenderTarget::Resize(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    const D3D12_RESOURCE_DESC& resourceDesc,
    const D3D12_CLEAR_VALUE& clearValue
) {
    m_pTextures.clear();
    m_pTextures.resize(m_numBuffers);

    UpdateTextures(
        pDevice,
        pAllocator,
        resourceDesc,
        clearValue
    );
}

Microsoft::WRL::ComPtr<ID3D12Resource> RenderTarget::GetCurrentBufferResource(size_t bufferId) const {
    return m_pTextures.at(bufferId)->GetResource();
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RenderTarget::GetRTVDescHeap() const {
    return m_pRTVDescHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTarget::GetCPURTVDescHandle(int bufferId) const {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_pRTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
        bufferId,
        m_descSize
    );
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTarget::GetGPURTVDescHandle(int bufferId) const {
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_pRTVDescHeap->GetGPUDescriptorHandleForHeapStart(),
        bufferId,
        m_descSize
    );
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RenderTarget::GetSRVDescHeap() const {
    return m_pSRVDescHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTarget::GetCPUSRVDescHandle(int bufferId) const {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_pSRVDescHeap->GetCPUDescriptorHandleForHeapStart(),
        bufferId,
        m_descSize
    );
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTarget::GetGPUSRVDescHandle(int bufferId) const {
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_pSRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
        bufferId,
        m_descSize
    );
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RenderTarget::CreateDescriptorHeap(
    Microsoft::WRL::ComPtr<ID3D12Device2> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32_t numDescriptors,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags
) {
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescriptor{};

    D3D12_DESCRIPTOR_HEAP_DESC desc{
        .Type{ type },
        .NumDescriptors{ numDescriptors },
        .Flags{ flags }
    };

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pDescriptor)));
    return pDescriptor;
}

void RenderTarget::UpdateTextures(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    const D3D12_RESOURCE_DESC& resourceDesc,
    const D3D12_CLEAR_VALUE& clearValue
) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pRTVDescHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_pSRVDescHeap->GetCPUDescriptorHandleForHeapStart());

    for (auto& pTexture : m_pTextures) {
        pTexture = Texture::CreateRenderTarget(
            pDevice,
            pAllocator,
            resourceDesc,
            &rtvHandle,
            &clearValue
        );

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
            .Format{ resourceDesc.Format },
            .ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
            .Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
            .Texture2D{
                .MipLevels{ 1 }
            }
        };
        pTexture->CreateShaderResourceView(
            pDevice,
            srvDesc,
            &srvHandle
        );

        rtvHandle.Offset(m_descSize);
        srvHandle.Offset(m_descSize);
    }
}
