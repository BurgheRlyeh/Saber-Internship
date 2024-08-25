#include "RenderTarget.h"

RenderTarget::RenderTarget(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator, uint8_t numBuffers, const D3D12_RESOURCE_DESC& resDesc, const D3D12_CLEAR_VALUE& clearValue) : m_numBuffers(numBuffers) {
    m_descSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    Resize(pDevice, pAllocator, resDesc, clearValue);
}

void RenderTarget::Resize(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    const D3D12_RESOURCE_DESC& resDesc,
    const D3D12_CLEAR_VALUE& clearValue
) {
    m_pBuffers.clear();
    m_pBuffers.resize(m_numBuffers);

    CreateBuffers(
        pDevice,
        pAllocator,
        resDesc,
        clearValue
    );
    CreateDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_numBuffers);
    CreateRTVs(pDevice);
}

Microsoft::WRL::ComPtr<ID3D12Resource> RenderTarget::GetCurrentBufferResource(size_t bufferId) const {
    return m_pBuffers.at(bufferId)->GetResource();
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTarget::GetCPUDescHandle(int bufferId) const {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_pDescHeap->GetCPUDescriptorHandleForHeapStart(),
        bufferId,
        m_descSize
    );
}

void RenderTarget::CreateDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors) {
    D3D12_DESCRIPTOR_HEAP_DESC desc{
        .Type{ type },
        .NumDescriptors{ numDescriptors }
    };

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescHeap)));
}

void RenderTarget::CreateBuffers(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator, const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE& clearValue) {
    for (auto& pBuffer : m_pBuffers) {
        pBuffer = std::make_shared<GPUResource>();
        pBuffer->CreateResource(
            pAllocator,
            resourceDesc,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clearValue
        );
    }
}

void RenderTarget::CreateRTVs(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pDescHeap->GetCPUDescriptorHandleForHeapStart());

    for (size_t i{}; i < m_numBuffers; ++i) {
        pDevice->CreateRenderTargetView(
            m_pBuffers.at(i)->GetResource().Get(),
            nullptr,
            rtvHandle
        );

        rtvHandle.Offset(m_descSize);
    }
}
