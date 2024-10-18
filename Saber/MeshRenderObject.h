#pragma once

#include <GLTFSDK/GLTF.h>

#include "Headers.h"

#include <filesystem>
#include <initializer_list>

#include "Atlas.h"
#include "CommandQueue.h"
#include "ConstantBuffer.h"
#include "GBuffer.h"
#include "MaterialManager.h"
#include "Mesh.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"
#include "Vertices.h"

template <typename ModelBuffer>
class MeshRenderObject : public RenderObject {
protected:
    std::shared_ptr<Mesh> m_pMesh{};

    std::shared_ptr<ModelBuffer> m_pModelBuffer{};
    std::shared_ptr<ConstantBuffer> m_pModelCB{};

public:
    MeshRenderObject(
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<ModelBuffer> pModelBuffer = nullptr
    ) {
        m_pModelCB = std::make_shared<ConstantBuffer>(
            pAllocator,
            sizeof(ModelBuffer),
            nullptr
        );
        m_pModelBuffer = pModelBuffer ? pModelBuffer : std::make_shared<ModelBuffer>();
        UpdateModelBuffer();
    }

    struct MeshData {
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas{};
        Mesh::MeshData meshData{};
        std::wstring meshFilename{};

        MeshData(
            std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
            const Mesh::MeshData& meshData,
            const std::wstring& meshFilename
        ) : pMeshAtlas(pMeshAtlas),
            meshData(meshData),
            meshFilename(meshFilename)
        {}
    };
    void InitMesh(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const MeshData& meshData
    ) {
        m_pMesh = meshData.pMeshAtlas->Assign(meshData.meshFilename, pDevice, pAllocator, pCommandQueueCopy, meshData.meshData);
    }

    std::shared_ptr<ModelBuffer> GetModelBuffer() const {
        return m_pModelBuffer;
    }
    void UpdateModelBuffer() {
        m_pModelCB->Update(m_pModelBuffer.get());
    }

protected:
    virtual void RenderJob(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect) const override {
        if (m_pMesh) {
            pCommandListDirect->IASetVertexBuffers(
                0,
                static_cast<UINT>(m_pMesh->GetVertexBuffersCount()),
                m_pMesh->GetVertexBufferViews()
            );
            pCommandListDirect->IASetIndexBuffer(m_pMesh->GetIndexBufferView());
        }
    }

    virtual void InnerRootParametersSetter(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        UINT& rootParamId
    ) const override {
        pCommandListDirect->SetGraphicsRootConstantBufferView(
            rootParamId++,
            m_pModelCB->GetResource()->GetGPUVirtualAddress()
        );
    }

    UINT GetIndexCountPerInstance() const override {
        return static_cast<UINT>(m_pMesh->GetIndicesCount());
    }

    UINT GetInstanceCount() const override {
        return 1;
    }
};

struct ModelBuffer {
    DirectX::XMMATRIX m_modelMatrix{ DirectX::XMMatrixIdentity() };
    DirectX::XMMATRIX m_normalMatrix{ DirectX::XMMatrixIdentity() };
    DirectX::XMUINT4 m_materialId{};

    ModelBuffer() = default;
    ModelBuffer(const DirectX::XMMATRIX& modelMatrix, size_t materialId = 0) {
        UpdateMatrices(modelMatrix);
        SetMaterial(materialId);
    }

    void UpdateMatrices(const DirectX::XMMATRIX& modelMatrix) {
        m_modelMatrix = modelMatrix;
        m_normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, m_modelMatrix));
    }

    void SetMaterial(size_t materialId) {
        m_materialId.x = materialId;
    }
};

class TestRenderObject : protected MeshRenderObject<ModelBuffer> {
protected:
    static D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineStateDesc(
        D3D12_INPUT_ELEMENT_DESC* inputLayout,
        size_t inputLayoutSize,
        const D3D12_RT_FORMAT_ARRAY& rtvFormats
    ) {
        CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc{ D3D12_DEFAULT };
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

        CD3DX12_RASTERIZER_DESC rasterizerDesc{ D3D12_DEFAULT };
        rasterizerDesc.FrontCounterClockwise = true;

        CD3DX12_PIPELINE_STATE_STREAM pipelineStateStream{};
        pipelineStateStream.InputLayout = { inputLayout, static_cast<UINT>(inputLayoutSize) };
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pipelineStateStream.DepthStencilState = depthStencilDesc;
        pipelineStateStream.RasterizerState = rasterizerDesc;
        pipelineStateStream.RTVFormats = rtvFormats;

        return pipelineStateStream.GraphicsDescV0();
    }
};

class TestColorRenderObject : protected TestRenderObject {
    static inline D3D12_INPUT_ELEMENT_DESC m_inputLayout[3]{
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

public:
    static std::shared_ptr<MeshRenderObject<ModelBuffer>> CreateTriangle(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
    ) {
        VertexPosNormCl vertices[]{
            { { -1.0f, -1.0f, 0.f }, {  0.f,  0.f,  1.f }, { 1.0f, 0.0f, 0.0f, 0.f } },
            { {  0.0f,  1.0f, 0.f }, {  0.f,  0.f,  1.f }, { 0.0f, 1.0f, 0.0f, 0.f } },
            { {  1.0f, -1.0f, 0.f }, {  0.f,  0.f,  1.f }, { 0.0f, 0.0f, 1.0f, 0.f } },
            { { -1.0f, -1.0f, 0.f }, {  0.f,  0.f, -1.f }, { 1.0f, 0.0f, 0.0f, 0.f } },
            { {  0.0f,  1.0f, 0.f }, {  0.f,  0.f, -1.f }, { 0.0f, 1.0f, 0.0f, 0.f } },
            { {  1.0f, -1.0f, 0.f }, {  0.f,  0.f, -1.f }, { 0.0f, 0.0f, 1.0f, 0.f } }
        };
        uint32_t indices[]{
            0, 2, 1,
            3, 4, 5
        };

        Mesh::MeshDataVerticesIndices meshData{
            // vertices data
            .vertices{ vertices },
            .verticesCnt{ _countof(vertices) },
            .vertexSize{ sizeof(*vertices) },
            // indices data
            .indices{ indices },
            .indicesCnt{ _countof(indices) },
            .indexSize{ sizeof(*indices) },
            .indexFormat{ DXGI_FORMAT_R32_UINT }
        };

        std::shared_ptr<MeshRenderObject<ModelBuffer>> pObj{
            std::make_shared<MeshRenderObject<ModelBuffer>>(pAllocator, std::make_shared<ModelBuffer>(modelMatrix))
        };
        pObj->InitMesh(
            pDevice,
            pAllocator,
            pCommandQueueCopy,
            MeshData(pMeshAtlas, meshData, L"SimpleTriangle")
        );

        D3D12_RT_FORMAT_ARRAY rtvFormats{ .NumRenderTargets{ 1 } };
        rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pObj->InitMaterial(
            pDevice,
            RootSignatureData(
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"SimpleRootSignature"
            ),
            ShaderData(
                pShaderAtlas,
                L"SimpleVertexShader.cso",
                L"SimplePixelShader.cso"
            ),
            PipelineStateData(
                pPSOLibrary,
                CreatePipelineStateDesc(m_inputLayout, _countof(m_inputLayout), rtvFormats)
            )
        );

        return pObj;
    }
    static std::shared_ptr<MeshRenderObject<ModelBuffer>> CreateCube(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
    ) {
        VertexPosNormCl vertices[24]{
            { { -1.f, -1.f,  1.f }, {  0.f, -1.f,  0.f }, { 1.0f, 0.0f, 0.0f, 0.0f } },
            { {  1.f, -1.f,  1.f }, {  0.f, -1.f,  0.f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
            { {  1.f, -1.f, -1.f }, {  0.f, -1.f,  0.f }, { 0.0f, 1.0f, 1.0f, 0.0f } },
            { { -1.f, -1.f, -1.f }, {  0.f, -1.f,  0.f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { { -1.f,  1.f, -1.f }, {  0.f,  1.f,  0.f }, { 0.0f, 0.0f, 1.0f, 0.0f } },
            { {  1.f,  1.f, -1.f }, {  0.f,  1.f,  0.f }, { 0.0f, 1.0f, 0.0f, 0.0f } },
            { {  1.f,  1.f,  1.f }, {  0.f,  1.f,  0.f }, { 1.0f, 1.0f, 0.0f, 0.0f } },
            { { -1.f,  1.f,  1.f }, {  0.f,  1.f,  0.f }, { 1.0f, 0.0f, 1.0f, 0.0f } },
            { {  1.f, -1.f, -1.f }, {  1.f,  0.f,  0.f }, { 0.0f, 1.0f, 1.0f, 0.0f } },
            { {  1.f, -1.f,  1.f }, {  1.f,  0.f,  0.f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
            { {  1.f,  1.f,  1.f }, {  1.f,  0.f,  0.f }, { 1.0f, 1.0f, 0.0f, 0.0f } },
            { {  1.f,  1.f, -1.f }, {  1.f,  0.f,  0.f }, { 0.0f, 1.0f, 0.0f, 0.0f } },
            { { -1.f, -1.f,  1.f }, { -1.f,  0.f,  0.f }, { 1.0f, 0.0f, 0.0f, 0.0f } },
            { { -1.f, -1.f, -1.f }, { -1.f,  0.f,  0.f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { { -1.f,  1.f, -1.f }, { -1.f,  0.f,  0.f }, { 0.0f, 0.0f, 1.0f, 0.0f } },
            { { -1.f,  1.f,  1.f }, { -1.f,  0.f,  0.f }, { 1.0f, 0.0f, 1.0f, 0.0f } },
            { {  1.f, -1.f,  1.f }, {  0.f,  0.f,  1.f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
            { { -1.f, -1.f,  1.f }, {  0.f,  0.f,  1.f }, { 1.0f, 0.0f, 0.0f, 0.0f } },
            { { -1.f,  1.f,  1.f }, {  0.f,  0.f,  1.f }, { 1.0f, 0.0f, 1.0f, 0.0f } },
            { {  1.f,  1.f,  1.f }, {  0.f,  0.f,  1.f }, { 1.0f, 1.0f, 0.0f, 0.0f } },
            { { -1.f, -1.f, -1.f }, {  0.f,  0.f, -1.f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { {  1.f, -1.f, -1.f }, {  0.f,  0.f, -1.f }, { 0.0f, 1.0f, 1.0f, 0.0f } },
            { {  1.f,  1.f, -1.f }, {  0.f,  0.f, -1.f }, { 0.0f, 1.0f, 0.0f, 0.0f } },
            { { -1.f,  1.f, -1.f }, {  0.f,  0.f, -1.f }, { 0.0f, 0.0f, 1.0f, 0.0f } }
        };
        uint32_t indices[36]{
             0,	 2,  1,  0,  3,  2,
             4,	 6,  5,  4,  7,  6,
             8,	10,  9,  8, 11, 10,
            12, 14, 13, 12, 15, 14,
            16, 18, 17, 16, 19, 18,
            20, 22, 21, 20, 23, 22
        };

        Mesh::MeshDataVerticesIndices meshData{
            // vertices data
            .vertices{ vertices },
            .verticesCnt{ _countof(vertices) },
            .vertexSize{ sizeof(*vertices) },
            // indices data
            .indices{ indices },
            .indicesCnt{ _countof(indices) },
            .indexSize{ sizeof(*indices) },
            .indexFormat{ DXGI_FORMAT_R32_UINT }
        };

        std::shared_ptr<MeshRenderObject<ModelBuffer>> pObj{
            std::make_shared<MeshRenderObject<ModelBuffer>>(pAllocator, std::make_shared<ModelBuffer>(modelMatrix))
        };
        pObj->InitMesh(pDevice, pAllocator, pCommandQueueCopy, MeshData(pMeshAtlas, meshData, L"SimpleCube"));

        D3D12_RT_FORMAT_ARRAY rtvFormats{ .NumRenderTargets{ 1 } };
        rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pObj->InitMaterial(
            pDevice,
            RootSignatureData(
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"SimpleRootSignature"
            ),
            ShaderData(
                pShaderAtlas,
                L"SimpleVertexShader.cso",
                L"SimplePixelShader.cso"
            ),
            PipelineStateData(
                pPSOLibrary,
                CreatePipelineStateDesc(m_inputLayout, _countof(m_inputLayout), rtvFormats)
            )
        );

        return pObj;
    }


private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    ) {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
        if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // Allow input layout and deny unnecessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
        };

        // A single 32-bit constant root parameter that is used by the vertex shader.
        CD3DX12_ROOT_PARAMETER1 rootParameters[2]{};
        rootParameters[0].InitAsConstantBufferView(0);  // scene CB
        rootParameters[1].InitAsConstantBufferView(1);  // model CB

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
            &rootSignatureDescription,
            featureData.HighestVersion,
            &rootSignatureBlob,
            &errorBlob
        ));

        return rootSignatureBlob;
    }
};

class TestTextureRenderObject : protected TestRenderObject {
    static inline D3D12_INPUT_ELEMENT_DESC m_inputLayoutAoS[4]{
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    static inline D3D12_INPUT_ELEMENT_DESC m_inputLayoutSoA[4]{
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

public:
    static std::shared_ptr<MeshRenderObject<ModelBuffer>> CreateTextureCube(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<CommandQueue> const& pCommandQueueDirect,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        std::shared_ptr<GBuffer> pGBuffer,
        std::shared_ptr<MaterialManager> pMaterialManager,
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
    ) {
        VertexPosNormTangUV vertices[24]{
            { { -1.f, -1.f,  1.f }, {  0.f, -1.f,  0.f }, {  1.f,  0.f,  0.f }, { 0.f, 1.f } },
            { {  1.f, -1.f,  1.f }, {  0.f, -1.f,  0.f }, {  1.f,  0.f,  0.f }, { 1.f, 1.f } },
            { {  1.f, -1.f, -1.f }, {  0.f, -1.f,  0.f }, {  1.f,  0.f,  0.f }, { 1.f, 0.f } },
            { { -1.f, -1.f, -1.f }, {  0.f, -1.f,  0.f }, {  1.f,  0.f,  0.f }, { 0.f, 0.f } },
            { { -1.f,  1.f, -1.f }, {  0.f,  1.f,  0.f }, {  1.f,  0.f,  0.f }, { 0.f, 1.f } },
            { {  1.f,  1.f, -1.f }, {  0.f,  1.f,  0.f }, {  1.f,  0.f,  0.f }, { 1.f, 1.f } },
            { {  1.f,  1.f,  1.f }, {  0.f,  1.f,  0.f }, {  1.f,  0.f,  0.f }, { 1.f, 0.f } },
            { { -1.f,  1.f,  1.f }, {  0.f,  1.f,  0.f }, {  1.f,  0.f,  0.f }, { 0.f, 0.f } },
            { {  1.f, -1.f, -1.f }, {  1.f,  0.f,  0.f }, {  0.f,  0.f,  1.f }, { 0.f, 1.f } },
            { {  1.f, -1.f,  1.f }, {  1.f,  0.f,  0.f }, {  0.f,  0.f,  1.f }, { 1.f, 1.f } },
            { {  1.f,  1.f,  1.f }, {  1.f,  0.f,  0.f }, {  0.f,  0.f,  1.f }, { 1.f, 0.f } },
            { {  1.f,  1.f, -1.f }, {  1.f,  0.f,  0.f }, {  0.f,  0.f,  1.f }, { 0.f, 0.f } },
            { { -1.f, -1.f,  1.f }, { -1.f,  0.f,  0.f }, {  0.f,  0.f, -1.f }, { 0.f, 1.f } },
            { { -1.f, -1.f, -1.f }, { -1.f,  0.f,  0.f }, {  0.f,  0.f, -1.f }, { 1.f, 1.f } },
            { { -1.f,  1.f, -1.f }, { -1.f,  0.f,  0.f }, {  0.f,  0.f, -1.f }, { 1.f, 0.f } },
            { { -1.f,  1.f,  1.f }, { -1.f,  0.f,  0.f }, {  0.f,  0.f, -1.f }, { 0.f, 0.f } },
            { {  1.f, -1.f,  1.f }, {  0.f,  0.f,  1.f }, { -1.f,  0.f,  0.f }, { 0.f, 1.f } },
            { { -1.f, -1.f,  1.f }, {  0.f,  0.f,  1.f }, { -1.f,  0.f,  0.f }, { 1.f, 1.f } },
            { { -1.f,  1.f,  1.f }, {  0.f,  0.f,  1.f }, { -1.f,  0.f,  0.f }, { 1.f, 0.f } },
            { {  1.f,  1.f,  1.f }, {  0.f,  0.f,  1.f }, { -1.f,  0.f,  0.f }, { 0.f, 0.f } },
            { { -1.f, -1.f, -1.f }, {  0.f,  0.f, -1.f }, {  1.f,  0.f,  0.f }, { 0.f, 1.f } },
            { {  1.f, -1.f, -1.f }, {  0.f,  0.f, -1.f }, {  1.f,  0.f,  0.f }, { 1.f, 1.f } },
            { {  1.f,  1.f, -1.f }, {  0.f,  0.f, -1.f }, {  1.f,  0.f,  0.f }, { 1.f, 0.f } },
            { { -1.f,  1.f, -1.f }, {  0.f,  0.f, -1.f }, {  1.f,  0.f,  0.f }, { 0.f, 0.f } }
        };
        uint32_t indices[36]{
             0,	 2,  1,  0,  3,  2,
             4,	 6,  5,  4,  7,  6,
             8,	10,  9,  8, 11, 10,
            12, 14, 13, 12, 15, 14,
            16, 18, 17, 16, 19, 18,
            20, 22, 21, 20, 23, 22
        };

        Mesh::MeshDataVerticesIndices meshData{
            // vertices data
            .vertices{ vertices },
            .verticesCnt{ _countof(vertices) },
            .vertexSize{ sizeof(*vertices) },
            // indices data
            .indices{ indices },
            .indicesCnt{ _countof(indices) },
            .indexSize{ sizeof(*indices) },
            .indexFormat{ DXGI_FORMAT_R32_UINT }
        };

        std::shared_ptr<MeshRenderObject<ModelBuffer>> pObj{
            std::make_shared<MeshRenderObject<ModelBuffer>>(pAllocator)
        };
        pObj->InitMesh(pDevice, pAllocator, pCommandQueueCopy, MeshData(pMeshAtlas, meshData, L"SimpleTextureCube"));
        pObj->InitMaterial(
            pDevice,
            RootSignatureData(
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"SimpleTextureRootSignature"
            ),
            ShaderData(
                pShaderAtlas,
                L"SimpleUVVertexShader.cso",
                L"SimpleUVPixelShader.cso"
            ),
            PipelineStateData(
                pPSOLibrary,
                CreatePipelineStateDesc(m_inputLayoutAoS, _countof(m_inputLayoutAoS), pGBuffer->GetRtFormatArray())
            )
        );

        std::shared_ptr<ModelBuffer> pModelBuffer{ pObj->GetModelBuffer() };
        pModelBuffer->UpdateMatrices(modelMatrix);
        pModelBuffer->SetMaterial(pMaterialManager->AddMaterial(
            pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect, L"Brick.dds", L"BrickNM.dds"
        ));
        pObj->UpdateModelBuffer();

        return pObj;
    }

    static std::shared_ptr<MeshRenderObject<ModelBuffer>> CreateModelFromGLTF(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<CommandQueue> const& pCommandQueueDirect,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::filesystem::path& filepath,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        std::shared_ptr<GBuffer> pGBuffer,
        std::shared_ptr<MaterialManager> pMaterialManager,
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
    ) {
        Mesh::MeshDataGLTF data{
            .filepath{ filepath },
            .attributes{
                Mesh::Attribute{
                    .name{ Microsoft::glTF::ACCESSOR_POSITION },
                    .size{ sizeof(DirectX::XMFLOAT3) }
                },
                Mesh::Attribute{
                    .name{ Microsoft::glTF::ACCESSOR_NORMAL },
                    .size{ sizeof(DirectX::XMFLOAT3) }
                },
                Mesh::Attribute{
                    .name{ Microsoft::glTF::ACCESSOR_TANGENT },
                    .size{ sizeof(DirectX::XMFLOAT4) }
                },
                Mesh::Attribute{
                    .name{ Microsoft::glTF::ACCESSOR_TEXCOORD_0 },
                    .size{ sizeof(DirectX::XMFLOAT2) }
                }
            }
        };

        std::shared_ptr<MeshRenderObject<ModelBuffer>> pObj{
            std::make_shared<MeshRenderObject<ModelBuffer>>(pAllocator)
        };
        pObj->InitMesh(pDevice, pAllocator, pCommandQueueCopy, MeshData(pMeshAtlas, data, L"MeshGLTF"));
        pObj->InitMaterial(
            pDevice,
            RootSignatureData(
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"GLTFRootSignature"
            ),
            ShaderData(
                pShaderAtlas,
                L"SoAUVVertexShader.cso",
                L"SimpleUVPixelShader.cso"
            ),
            PipelineStateData(
                pPSOLibrary,
                CreatePipelineStateDesc(m_inputLayoutSoA, _countof(m_inputLayoutSoA), pGBuffer->GetRtFormatArray())
            )
        );

        std::shared_ptr<ModelBuffer> pModelBuffer{ pObj->GetModelBuffer() };
        pModelBuffer->UpdateMatrices(modelMatrix);
        pModelBuffer->SetMaterial(pMaterialManager->AddMaterial(
            pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect, L"barbarian_diffuse.dds", L"barb2_n.dds"
        ));
        pObj->UpdateModelBuffer();

        return pObj;
    }

private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    ) {
        // Allow input layout and deny unnecessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
        };

        CD3DX12_ROOT_PARAMETER1 rootParameters[2]{};
        rootParameters[0].InitAsConstantBufferView(0);  // scene CB
        rootParameters[1].InitAsConstantBufferView(1);  // model CB

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
        if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
            &rootSignatureDescription,
            featureData.HighestVersion,
            &rootSignatureBlob,
            &errorBlob
        ));

        return rootSignatureBlob;
    }
};

class TestAlphaRenderObject : protected TestRenderObject {
    static inline D3D12_INPUT_ELEMENT_DESC m_inputLayoutSoA[4]{
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

public:
    static std::shared_ptr<MeshRenderObject<ModelBuffer>> CreateAlphaModelFromGLTF(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<CommandQueue> const& pCommandQueueDirect,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::filesystem::path& filepath,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        std::shared_ptr<GBuffer> pGBuffer,
        std::shared_ptr<MaterialManager> pMaterialManager,
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
    ) {
        Mesh::MeshDataGLTF data{
            .filepath{ filepath },
            .attributes{
                Mesh::Attribute{
                    .name{ Microsoft::glTF::ACCESSOR_POSITION },
                    .size{ sizeof(DirectX::XMFLOAT3) }
                },
                Mesh::Attribute{
                    .name{ Microsoft::glTF::ACCESSOR_NORMAL },
                    .size{ sizeof(DirectX::XMFLOAT3) }
                },
                Mesh::Attribute{
                    .name{ Microsoft::glTF::ACCESSOR_TANGENT },
                    .size{ sizeof(DirectX::XMFLOAT4) }
                },
                Mesh::Attribute{
                    .name{ Microsoft::glTF::ACCESSOR_TEXCOORD_0 },
                    .size{ sizeof(DirectX::XMFLOAT2) }
                }
            }
        };

        std::shared_ptr<MeshRenderObject<ModelBuffer>> obj{
            std::make_shared<MeshRenderObject<ModelBuffer>>(pAllocator)
        };
        obj->InitMesh(pDevice, pAllocator, pCommandQueueCopy, MeshData(pMeshAtlas, data, L"AlphaGrassGLTF"));
        obj->InitMaterial(
            pDevice,
            RootSignatureData(
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"AlphaGrassGLTFRootSignature"
            ),
            ShaderData(
                pShaderAtlas,
                L"AlphaVS.cso",
                L"AlphaPS.cso"
            ),
            PipelineStateData(
                pPSOLibrary,
                CreateAlphaPipelineStateDesc(m_inputLayoutSoA, _countof(m_inputLayoutSoA), pGBuffer->GetRtFormatArray())
            )
        );

        std::shared_ptr<ModelBuffer> pModelBuffer{ obj->GetModelBuffer() };
        pModelBuffer->UpdateMatrices(modelMatrix);
        pModelBuffer->SetMaterial(pMaterialManager->AddMaterial(
            pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect, L"grassAlbedo.dds", L"grassNormal.dds"
        ));
        obj->UpdateModelBuffer();

        return obj;
    }

private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    ) {
        // Allow input layout and deny unnecessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
        };

        CD3DX12_ROOT_PARAMETER1 rootParameters[4]{};
        rootParameters[0].InitAsConstantBufferView(0);  // scene CB
        rootParameters[1].InitAsConstantBufferView(1);  // model CB

        CD3DX12_DESCRIPTOR_RANGE1 rangeCbvsMaterials[1]{};
        rangeCbvsMaterials[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);
        rootParameters[2].InitAsDescriptorTable(_countof(rangeCbvsMaterials), rangeCbvsMaterials, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_DESCRIPTOR_RANGE1 rangeSrvsMaterial[1]{};
        rangeSrvsMaterial[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 0);
        rootParameters[3].InitAsDescriptorTable(_countof(rangeSrvsMaterial), rangeSrvsMaterial, D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC sampler{
            .Filter{ D3D12_FILTER_MIN_MAG_MIP_POINT },
            .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
            .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
            .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
            .MipLODBias{},
            .MaxAnisotropy{},
            .ComparisonFunc{ D3D12_COMPARISON_FUNC_NEVER },
            .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
            .MinLOD{},
            .MaxLOD{ D3D12_FLOAT32_MAX },
            .ShaderRegister{},
            .RegisterSpace{},
            .ShaderVisibility{ D3D12_SHADER_VISIBILITY_PIXEL }
        };

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
        if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
            &rootSignatureDescription,
            featureData.HighestVersion,
            &rootSignatureBlob,
            &errorBlob
        ));

        return rootSignatureBlob;
    }

    static D3D12_GRAPHICS_PIPELINE_STATE_DESC CreateAlphaPipelineStateDesc(
        D3D12_INPUT_ELEMENT_DESC* inputLayout,
        size_t inputLayoutSize,
        const D3D12_RT_FORMAT_ARRAY& rtvFormats
    ) {
        CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc{ D3D12_DEFAULT };
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

        CD3DX12_RASTERIZER_DESC rasterizerDesc{ D3D12_DEFAULT };
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
        rasterizerDesc.FrontCounterClockwise = true;

        CD3DX12_PIPELINE_STATE_STREAM pipelineStateStream{};
        pipelineStateStream.InputLayout = { inputLayout, static_cast<UINT>(inputLayoutSize) };
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pipelineStateStream.DepthStencilState = depthStencilDesc;
        pipelineStateStream.RasterizerState = rasterizerDesc;
        pipelineStateStream.RTVFormats = rtvFormats;

        return pipelineStateStream.GraphicsDescV0();
    }
};
