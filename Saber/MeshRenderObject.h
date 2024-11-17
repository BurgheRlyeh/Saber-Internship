#pragma once

#include <GLTFSDK/GLTF.h>

#include "Headers.h"

#include <filesystem>
#include <initializer_list>

#include "Atlas.h"
#include "CommandQueue.h"
#include "ConstantBuffer.h"
#include "GBuffer.h"
#include "IndirectCommand.h"
#include "MaterialManager.h"
#include "ModelBuffers.h"
#include "Mesh.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"
#include "Vertices.h"

template <typename ModelBuffer>
class MeshRenderObject : public RenderObject {
protected:
    std::shared_ptr<Mesh> m_pMesh{};

    ModelBuffer m_modelBuffer{};
    std::shared_ptr<ConstantBuffer> m_pModelCb{};

public:
    MeshRenderObject(
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        ModelBuffer* pModelBuffer = nullptr
    ) {
        if (pModelBuffer) {
            m_modelBuffer = *pModelBuffer;
        }
        m_pModelCb = std::make_shared<ConstantBuffer>(
            pAllocator,
            sizeof(ModelBuffer),
            &m_modelBuffer
        );
    }

    struct MeshInitData {
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas;
        Mesh::MeshData meshData;
        std::wstring meshFilename;
    };
    void InitMesh(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const MeshInitData& meshInitData
    ) {
        m_pMesh = meshInitData.pMeshAtlas->Assign(
            meshInitData.meshFilename,
            pDevice,
            pAllocator,
            pCommandQueueCopy,
            meshInitData.meshData
        );
    }

    ModelBuffer& GetModelBuffer() {
        return m_modelBuffer;
    }
    void SetModelBuffer(const ModelBuffer& modelBuffer) {
        m_modelBuffer = modelBuffer;
        UpdateModelBuffer();
    }
    void UpdateModelBuffer() {
        m_pModelCb->Update(&m_modelBuffer);
    }

    void FillIndirectCommand(CbMeshIndirectCommand& indirectCommand) override {
        indirectCommand = CbMeshIndirectCommand{
            .constantBufferView{
                m_pModelCb->GetResource()->GetGPUVirtualAddress()
            },
            .indexBufferView{
                *m_pMesh->GetIndexBufferView()
            },
            .vertexBufferView{
                *m_pMesh->GetVertexBufferView()
            },
            .drawArguments{
                .IndexCountPerInstance{ static_cast<UINT>(m_pMesh->GetIndicesCount()) },
                .InstanceCount{ 1 }
            },
        };
    }

    void FillIndirectCommand(CbMesh4IndirectCommand& indirectCommand) override {
        indirectCommand = CbMesh4IndirectCommand{
            .constantBufferView{
                m_pModelCb->GetResource()->GetGPUVirtualAddress()
            },
            .indexBufferView{
                *m_pMesh->GetIndexBufferView()
            },
            .vertexBufferView{ *m_pMesh->GetVertexBufferView() },
            .vertexBufferView1{ *m_pMesh->GetVertexBufferView(1) },
            .vertexBufferView2{ *m_pMesh->GetVertexBufferView(2) },
            .vertexBufferView3{ *m_pMesh->GetVertexBufferView(3) },
            .drawArguments{
                .IndexCountPerInstance{ static_cast<UINT>(m_pMesh->GetIndicesCount()) },
                .InstanceCount{ 1 }
            },
        };
    }

protected:
    void RenderJob(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect
    ) const override {
        assert(m_pMesh);
        if (m_pMesh) {
            pCommandListDirect->IASetVertexBuffers(
                0,
                static_cast<UINT>(m_pMesh->GetVertexBuffersCount()),
                m_pMesh->GetVertexBufferViews()
            );
            pCommandListDirect->IASetIndexBuffer(m_pMesh->GetIndexBufferView());
        }
    }

    void InnerRootParametersSetter(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        UINT& rootParamId
    ) const override {
        pCommandListDirect->SetGraphicsRootConstantBufferView(
            rootParamId++,
            m_pModelCb->GetResource()->GetGPUVirtualAddress()
        );
    }

    void DrawCall(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList
    ) const override {
        pCommandList->DrawIndexedInstanced(
            static_cast<UINT>(m_pMesh->GetIndicesCount()),
            1,
            0, 0, 0
        );
    }
};

class TestRenderObject : protected MeshRenderObject<ModelBuffer> {
protected:
    static inline D3D12_INPUT_ELEMENT_DESC m_inputLayoutSoA[4]{
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
};

class TestTextureRenderObject : protected TestRenderObject {
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
        DirectX::XMFLOAT3 positions[24]{
            { -1.f, -1.f,  1.f }, {  1.f, -1.f,  1.f }, {  1.f, -1.f, -1.f }, { -1.f, -1.f, -1.f },
            { -1.f,  1.f, -1.f }, {  1.f,  1.f, -1.f }, {  1.f,  1.f,  1.f }, { -1.f,  1.f,  1.f },
            {  1.f, -1.f, -1.f }, {  1.f, -1.f,  1.f }, {  1.f,  1.f,  1.f }, {  1.f,  1.f, -1.f },
            { -1.f, -1.f,  1.f }, { -1.f, -1.f, -1.f }, { -1.f,  1.f, -1.f }, { -1.f,  1.f,  1.f },
            {  1.f, -1.f,  1.f }, { -1.f, -1.f,  1.f }, { -1.f,  1.f,  1.f }, {  1.f,  1.f,  1.f },
            { -1.f, -1.f, -1.f }, {  1.f, -1.f, -1.f }, {  1.f,  1.f, -1.f }, { -1.f,  1.f, -1.f }
        };
        DirectX::XMFLOAT3 normals[24]{
            {  0.f, -1.f,  0.f }, {  0.f, -1.f,  0.f }, {  0.f, -1.f,  0.f }, {  0.f, -1.f,  0.f },
            {  0.f,  1.f,  0.f }, {  0.f,  1.f,  0.f }, {  0.f,  1.f,  0.f }, {  0.f,  1.f,  0.f },
            {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f },
            { -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f },
            {  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f },
            {  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f }
        };
        DirectX::XMFLOAT3 tangents[24]{
            {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f },
            {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f },
            {  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f },
            {  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f },
            { -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f },
            {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }
        };
        DirectX::XMFLOAT2 uvs[24]{
            { 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f },
            { 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f },
            { 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f },
            { 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f },
            { 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f },
            { 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f }
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
            // indices data
            .indices{ indices },
            .indicesCnt{ _countof(indices) },
            .indexSize{ sizeof(*indices) },
            .indexFormat{ DXGI_FORMAT_R32_UINT },
            // vertices data
            .verticesCnt{ 24 },
			.verticesData{
				{ .data{ positions }, .size{ sizeof(*positions) } },
				{ .data{ normals }, .size{ sizeof(*normals) } },
				{ .data{ tangents }, .size{ sizeof(*tangents) } },
				{ .data{ uvs }, .size{ sizeof(*uvs) } }
			}
        };

        std::shared_ptr<MeshRenderObject<ModelBuffer>> pObj{
            std::make_shared<MeshRenderObject<ModelBuffer>>(pAllocator)
        };
        pObj->InitMesh(pDevice, pAllocator, pCommandQueueCopy, MeshInitData(pMeshAtlas, meshData, L"SimpleTextureCube"));
        pObj->InitMaterial(
            pDevice,
            RootSignatureData{
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"GLTFRootSignature"
            },
            ShaderData{
                pShaderAtlas,
                L"SimpleUVVertexShader.cso",
                L"SimpleUVPixelShader.cso"
            },
            PipelineStateData{
                pPSOLibrary,
                CreatePipelineStateDesc(m_inputLayoutSoA, _countof(m_inputLayoutSoA), pGBuffer->GetRtFormatArray())
            }
        );

        pObj->SetModelBuffer(ModelBuffer{
            modelMatrix,
            pMaterialManager->AddMaterial(
                pDevice,
                pAllocator,
                pCommandQueueCopy,
                pCommandQueueDirect,
                L"Brick.dds",
                L"BrickNM.dds"
            )
        });

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
        pObj->InitMesh(pDevice, pAllocator, pCommandQueueCopy, MeshInitData(pMeshAtlas, data, L"MeshGLTF"));
        pObj->InitMaterial(
            pDevice,
            RootSignatureData{
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"GLTFRootSignature"
            },
            ShaderData{
                pShaderAtlas,
                L"SimpleUVVertexShader.cso",
                L"SimpleUVPixelShader.cso"
            },
            PipelineStateData{
                pPSOLibrary,
                CreatePipelineStateDesc(m_inputLayoutSoA, _countof(m_inputLayoutSoA), pGBuffer->GetRtFormatArray())
            }
        );

        pObj->SetModelBuffer(ModelBuffer{
            modelMatrix,
            pMaterialManager->AddMaterial(
                pDevice,
                pAllocator,
                pCommandQueueCopy,
                pCommandQueueDirect,
                L"barbarian_diffuse.dds",
                L"barb2_n.dds"
            )
        });

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

class TestAlphaRenderObject : protected TestRenderObject {
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

        std::shared_ptr<MeshRenderObject<ModelBuffer>> pObj{
            std::make_shared<MeshRenderObject<ModelBuffer>>(pAllocator)
        };
        pObj->InitMesh(pDevice, pAllocator, pCommandQueueCopy, MeshInitData(pMeshAtlas, data, L"AlphaGrassGLTF"));
        pObj->InitMaterial(
            pDevice,
            RootSignatureData{
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"AlphaGrassGLTFRootSignature"
            },
            ShaderData{
                pShaderAtlas,
                L"AlphaVS.cso",
                L"AlphaPS.cso"
            },
            PipelineStateData{
                pPSOLibrary,
                CreateAlphaPipelineStateDesc(m_inputLayoutSoA, _countof(m_inputLayoutSoA), pGBuffer->GetRtFormatArray())
            }
        );

        pObj->SetModelBuffer(ModelBuffer{
            modelMatrix,
            pMaterialManager->AddMaterial(
                pDevice,
                pAllocator,
                pCommandQueueCopy,
                pCommandQueueDirect,
                L"grassAlbedo.dds",
                L"grassNormal.dds"
            )
        });

        return pObj;
    }

protected:
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
