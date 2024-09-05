#include "MeshRenderObject.h"

MeshRenderObject::MeshRenderObject(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator, const DirectX::XMMATRIX& modelMatrix) {
    m_modelBuffer.modelMatrix = modelMatrix;
    m_modelBuffer.normalMatrix = DirectX::XMMatrixTranspose(
        DirectX::XMMatrixInverse(nullptr, m_modelBuffer.modelMatrix)
    );
    m_pModelCB = std::make_shared<ConstantBuffer>(
        pDevice,
        pAllocator,
        CD3DX12_RESOURCE_ALLOCATION_INFO(sizeof(ModelBuffer), 0),
        static_cast<void*>(&m_modelBuffer)
    );
}

void MeshRenderObject::InitMesh(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator, std::shared_ptr<CommandQueue> const& pCommandQueueCopy, std::shared_ptr<Atlas<Mesh>> pMeshAtlas, const Mesh::MeshData& meshData, const std::wstring& meshFilename) {
    m_pMesh = pMeshAtlas->Assign(meshFilename, pDevice, pAllocator, pCommandQueueCopy, meshData);
}

void MeshRenderObject::BindTextures(std::shared_ptr<Textures> pTextures) {
    m_pTextures = pTextures;
}

void MeshRenderObject::Update() {

}

void MeshRenderObject::RenderJob(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect) const {
    if (m_pMesh) {
        pCommandListDirect->IASetVertexBuffers(
            0,
            static_cast<UINT>(m_pMesh->GetVertexBuffersCount()),
            m_pMesh->GetVertexBufferViews()
        );
        pCommandListDirect->IASetIndexBuffer(m_pMesh->GetIndexBufferView());
    }
}

void MeshRenderObject::InnerRootParametersSetter(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect, UINT& rootParamId) const {
    pCommandListDirect->SetGraphicsRootConstantBufferView(rootParamId++, m_pModelCB->GetResource()->GetGPUVirtualAddress());
    if (m_pTextures) {
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pTexDescHeap{ m_pTextures->GetSrvUavDescHeap() };
        pCommandListDirect->SetDescriptorHeaps(1, pTexDescHeap.GetAddressOf());
        pCommandListDirect->SetGraphicsRootDescriptorTable(rootParamId++, pTexDescHeap->GetGPUDescriptorHandleForHeapStart());
    }
}

UINT MeshRenderObject::GetIndexCountPerInstance() const {
    return static_cast<UINT>(m_pMesh->GetIndicesCount());
}

UINT MeshRenderObject::GetInstanceCount() const {
    return 1;
}
