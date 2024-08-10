#include "RenderObject.h"

#include <codecvt>

void RenderObject::InitMesh(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
    const MeshData& meshData,
    const std::wstring& meshFilename
) {
    m_pMesh = pMeshAtlas->Assign(meshFilename, pDevice, pAllocator, pCommandQueueCopy, meshData);
}

void RenderObject::InitMeshFromGLTF(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
    std::filesystem::path& filepath,
    const std::wstring& meshFilename
) {
    m_pMesh = pMeshAtlas->Assign(meshFilename, pDevice, pAllocator, pCommandQueueCopy, filepath);
}

void RenderObject::InitMaterial (Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
    const LPCWSTR& vertexShaderFilepath,
    const LPCWSTR& pixelShaderFilepath,
    std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
    Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob,
    const std::wstring& rootSignatureFilename,
    std::shared_ptr<PSOLibrary> pPSOLibrary,
    D3D12_INPUT_ELEMENT_DESC* inputLayout,
    size_t inputLayoutSize,
    std::shared_ptr<Texture> pTexture
) {
    m_pVertexShaderResource = pShaderAtlas->Assign(vertexShaderFilepath);
    m_pPixelShaderResource = pShaderAtlas->Assign(pixelShaderFilepath);

    RootSignatureResource::RootSignatureResourceData rootSignatureResData{
        .pDevice{ pDevice },
        .pRootSignatureBlob{ pRootSignatureBlob }
    };
    m_pRootSignatureResource = pRootSignatureAtlas->Assign(rootSignatureFilename, rootSignatureResData);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
        CreatePipelineStateDesc(m_pRootSignatureResource, inputLayout, inputLayoutSize, m_pVertexShaderResource, m_pPixelShaderResource)
    };

    m_pPipelineState = pPSOLibrary->Assign(pDevice, (std::wstring(vertexShaderFilepath) + std::wstring(pixelShaderFilepath)).c_str(), &desc);

    m_pTexture = pTexture;
}

void RenderObject::Update() {

}

void RenderObject::Render(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
    Microsoft::WRL::ComPtr<ID3D12Resource> pSceneCB
) const {
    assert(pCommandListDirect->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT);

    pCommandListDirect->SetPipelineState(m_pPipelineState.Get());

    pCommandListDirect->SetGraphicsRootSignature(m_pRootSignatureResource->pRootSignature.Get());

    if (m_pTexture) {
        ID3D12DescriptorHeap* descriptorHeaps[] = { m_pTexture->GetTextureDescHeap().Get() };
        pCommandListDirect->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
        pCommandListDirect->SetGraphicsRootDescriptorTable(2, m_pTexture->GetTextureDescHeap()->GetGPUDescriptorHandleForHeapStart());
    }

    pCommandListDirect->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandListDirect->IASetVertexBuffers(0, 1, m_pMesh->GetVertexBufferView());
    pCommandListDirect->IASetIndexBuffer(m_pMesh->GetIndexBufferView());

    pCommandListDirect->RSSetViewports(1, &viewport);
    pCommandListDirect->RSSetScissorRects(1, &scissorRect);

    pCommandListDirect->OMSetRenderTargets(1, &renderTargetView, FALSE, &depthStencilView);

    // Update the MVP matrix
    void* pData{};
    ThrowIfFailed(pSceneCB->Map(0, &CD3DX12_RANGE(), &pData));
    pCommandListDirect->SetGraphicsRoot32BitConstants(0, sizeof(DirectX::XMMATRIX) / 4, pData, 0);
    //pCommandListDirect->SetGraphicsRootConstantBufferView(0, pSceneCB->GetGPUVirtualAddress());
    pCommandListDirect->SetGraphicsRoot32BitConstants(1, sizeof(DirectX::XMMATRIX) / 4, &m_modelMatrix, 0);
    
    pCommandListDirect->DrawIndexedInstanced(static_cast<UINT>(m_pMesh->GetIndicesCount()), 1, 0, 0, 0);
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC RenderObject::CreatePipelineStateDesc(
    std::shared_ptr<RootSignatureResource> pRootSignatureResource,
    D3D12_INPUT_ELEMENT_DESC* inputLayout,
    size_t inputLayoutSize,
    std::shared_ptr<ShaderResource> pVertexShaderResource,
    std::shared_ptr<ShaderResource> pPixelShaderResource
) {
    D3D12_RT_FORMAT_ARRAY rtvFormats{};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc{ D3D12_DEFAULT };
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

    CD3DX12_PIPELINE_STATE_STREAM pipelineStateStream{};
    pipelineStateStream.pRootSignature = pRootSignatureResource->pRootSignature.Get();
    pipelineStateStream.InputLayout = { inputLayout, UINT(inputLayoutSize) };
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderResource->pShaderBlob.Get());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderResource->pShaderBlob.Get());
    pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipelineStateStream.DepthStencilState = depthStencilDesc;
    pipelineStateStream.RTVFormats = rtvFormats;

    return pipelineStateStream.GraphicsDescV0();
}