#include "MeshRenderObject.h"

MeshRenderObject::MeshRenderObject(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    const DirectX::XMMATRIX& modelMatrix
) {
    m_modelBuffer.modelMatrix = modelMatrix;
    m_modelBuffer.normalMatrix = DirectX::XMMatrixTranspose(
        DirectX::XMMatrixInverse(nullptr, m_modelBuffer.modelMatrix)
    );
    m_pModelCB = std::make_shared<ConstantBuffer>(
        pAllocator,
        sizeof(ModelBuffer),
        static_cast<void*>(&m_modelBuffer)
    );
}

void MeshRenderObject::InitMesh(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
    const Mesh::MeshData& meshData,
    const std::wstring& meshFilename
) {
    m_pMesh = pMeshAtlas->Assign(meshFilename, pDevice, pAllocator, pCommandQueueCopy, meshData);
}

void MeshRenderObject::SetMaterialId(size_t id) {
    m_modelBuffer.materialId.x = id;
    m_pModelCB->Update(&m_modelBuffer);
}

void MeshRenderObject::Update() {

}

D3D12_GPU_VIRTUAL_ADDRESS MeshRenderObject::GetMeshCbGpuVirtualAdress()
{
    return m_pModelCB->GetResource()->GetGPUVirtualAddress();
}

SimpleIndirectCommand&& MeshRenderObject::IndirectCommandParameters()
{
    //SimpleIndirectCommand command{ m_pModelCB->GetResource()->GetGPUVirtualAddress(), { m_pMesh->GetIndicesCount(), 1, 0, 0, 0},  m_pMesh->GetIndexBufferView()->BufferLocation, m_pMesh->GetVertexBufferView()->BufferLocation};
    return std::move(SimpleIndirectCommand{ m_pModelCB->GetResource()->GetGPUVirtualAddress(), D3D12_DRAW_INDEXED_ARGUMENTS{ static_cast<unsigned int>(m_pMesh->GetIndicesCount()), 1, 0, 0, 0},  *m_pMesh->GetIndexBufferView(), *m_pMesh->GetVertexBufferView() });
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

void MeshRenderObject::InnerRootParametersSetter(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    UINT& rootParamId
) const {
    pCommandListDirect->SetGraphicsRootConstantBufferView(rootParamId++, m_pModelCB->GetResource()->GetGPUVirtualAddress());
}

UINT MeshRenderObject::GetIndexCountPerInstance() const {
    return static_cast<UINT>(m_pMesh->GetIndicesCount());
}

UINT MeshRenderObject::GetInstanceCount() const {
    return 1;
}
