#pragma once

#include "Headers.h"

#include "Atlas.h"
#include "CommandQueue.h"
#include "Mesh.h"
#include "PSOLibrary.h"
#include "Resources.h"
#include "Vertices.h"

class RenderObject {
    std::shared_ptr<Mesh> m_pMesh{};
    std::shared_ptr<RootSignatureResource> m_pRootSignatureResource{};
    std::shared_ptr<ShaderResource> m_pVertexShaderResource{};
    std::shared_ptr<ShaderResource> m_pPixelShaderResource{};
    //std::shared_ptr<PipelineStateResource> m_pPipelineStateResource{};

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState{};

    DirectX::XMMATRIX m_modelMatrix{ DirectX::XMMatrixIdentity() };

public:
    void InitMesh(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        const MeshData& meshData,
        const std::string& meshFilename = ""
    ) {
        m_pMesh = pMeshAtlas->Assign(meshFilename, pDevice, pCommandQueueCopy, meshData);
    }

    void InitMaterial(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        const LPCWSTR& vertexShaderFilepath,
        const LPCWSTR& pixelShaderFilepath,
        const std::string& vertexShaderFilename,
        const std::string& pixelShaderFilename,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob,
        const std::string& rootSignatureFilename,
        //std::shared_ptr<Atlas<PipelineStateResource>> pPipelineStateAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        const std::string& pipelineStateFilename
    ) {
        ShaderResource::ShaderResourceData vertexShaderResData{
            .filepath{ vertexShaderFilepath }
        };
        m_pVertexShaderResource = pShaderAtlas->Assign(vertexShaderFilename, vertexShaderResData);

        ShaderResource::ShaderResourceData pixelShaderResData{
            .filepath{ pixelShaderFilepath }
        };
        m_pPixelShaderResource = pShaderAtlas->Assign(pixelShaderFilename, pixelShaderResData);


        m_pRootSignatureResource = pRootSignatureAtlas->Find(rootSignatureFilename);
        if (!m_pRootSignatureResource) {
            RootSignatureResource::RootSignatureResourceData rootSignatureResData{
                .pDevice{ pDevice },
                .pRootSignatureBlob{ pRootSignatureBlob }
            };
            m_pRootSignatureResource = pRootSignatureAtlas->Assign(rootSignatureFilename, rootSignatureResData);
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
            CreatePipelineState(pDevice, m_pRootSignatureResource, m_pVertexShaderResource, m_pPixelShaderResource) 
        };

        m_pPipelineState = pPSOLibrary->Assign(pDevice, L"PSO", &desc);

        //PipelineStateResource::PipelineStateResourceData pipelineStateResData{
        //    .pDevice{ pDevice },
        //    .pipelineStateStreamDesc{ CreatePipelineStateStreamDesc(pDevice, m_pRootSignatureResource, m_pVertexShaderResource, m_pPixelShaderResource) }
        //};
        //m_pPipelineStateResource = pPipelineStateAtlas->Assign(pipelineStateFilename, pipelineStateResData);
        //Microsoft::WRL::ComPtr<ID3DBlob> pPipelineStateBlob{
        //    CreatePipelineStateBlob(pDevice, m_pRootSignatureResource, m_pVertexShaderResource, m_pPixelShaderResource)
        //};
    }

    void Update();

    void Render(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
        DirectX::XMMATRIX viewProjectionMatrix
    ) const;

private:

    //struct 

    D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineState(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<RootSignatureResource> pRootSignatureResource,
        std::shared_ptr<ShaderResource> pVertexShaderResource,
        std::shared_ptr<ShaderResource> pPixelShaderResource
    ) {
        // Create the vertex input layout
        static D3D12_INPUT_ELEMENT_DESC inputLayout[]{
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_RT_FORMAT_ARRAY rtvFormats{};
        rtvFormats.NumRenderTargets = 1;
        rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        
        CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc{ D3D12_DEFAULT };
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

        CD3DX12_PIPELINE_STATE_STREAM pipelineStateStream{};
        pipelineStateStream.pRootSignature = pRootSignatureResource->pRootSignature.Get();
        pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout) };
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderResource->pShaderBlob.Get());
        pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderResource->pShaderBlob.Get());
        pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pipelineStateStream.DepthStencilState = depthStencilDesc;
        pipelineStateStream.RTVFormats = rtvFormats;

        return pipelineStateStream.GraphicsDescV0();
        
        //D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc{
        //    .SizeInBytes{ sizeof(CD3DX12_PIPELINE_STATE_STREAM) },
        //    .pPipelineStateSubobjectStream{ &pipelineStateStream }
        //};
        //ThrowIfFailed(pDevice->CreatePipelineState(
        //    &pipelineStateStreamDesc,
        //    IID_PPV_ARGS(&m_pPipelineState)
        //));
    }
};

class TestRenderObject : RenderObject {
public:
    static RenderObject createTriangle(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        //std::shared_ptr<Atlas<PipelineStateResource>> pPipelineStateAtlas
        std::shared_ptr<PSOLibrary> pPSOLibrary
    ) {
        VertexPositionColor vertices[]{
            { { -1.0f, -1.0f, 0.f, 1.f }, { 1.0f, 0.0f, 0.0f, 0.f } },
            { {  0.0f,  1.0f, 0.f, 1.f }, { 0.0f, 1.0f, 0.0f, 0.f } },
            { {  1.0f, -1.0f, 0.f, 1.f }, { 0.0f, 0.0f, 1.0f, 0.f } }
        };
        uint32_t indices[]{ 0, 1, 2 };

        MeshData meshData{
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

        RenderObject obj{};
        obj.InitMesh(pDevice, pCommandQueueCopy, pMeshAtlas, meshData, "SimpleTriangle");
        obj.InitMaterial(
            pDevice,
            pShaderAtlas,
            L"SimpleVertexShader.cso",
            L"SimplePixelShader.cso",
            "SimpleVertexShader",
            "SimplePixelShader",
            pRootSignatureAtlas,
            CreateRootSignatureBlob(pDevice),
            "SimpleRootSignature",
            //pPipelineStateAtlas
            pPSOLibrary,
            "SimplePipelineState"
        );

        return obj;
    }

    static RenderObject createCube(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        //std::shared_ptr<Atlas<PipelineStateResource>> pPipelineStateAtlas
        std::shared_ptr<PSOLibrary> pPSOLibrary
    ) {
        VertexPositionColor vertices[]{
            { { -1.0f, -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { { -1.0f,  1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } },
            { {  1.0f,  1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f } },
            { {  1.0f, -1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, 0.0f } },
            { { -1.0f, -1.0f,  1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f } },
            { { -1.0f,  1.0f,  1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 0.0f } },
            { {  1.0f,  1.0f,  1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 0.0f } },
            { {  1.0f, -1.0f,  1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } }
        };

        uint32_t indices[]{
            0, 2, 1, 0, 3, 2,
            4, 5, 6, 4, 6, 7,
            4, 1, 5, 4, 0, 1,
            3, 6, 2, 3, 7, 6,
            1, 6, 5, 1, 2, 6,
            4, 3, 0, 4, 7, 3
        };

        MeshData meshData{
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

        RenderObject obj{};
        obj.InitMesh(pDevice, pCommandQueueCopy, pMeshAtlas, meshData, "SimpleCube");
        obj.InitMaterial(
            pDevice,
            pShaderAtlas,
            L"SimpleVertexShader.cso",
            L"SimplePixelShader.cso",
            "SimpleVertexShader",
            "SimplePixelShader",
            pRootSignatureAtlas,
            CreateRootSignatureBlob(pDevice),
            "SimpleRootSignature",
            //pPipelineStateAtlas
            pPSOLibrary,
            "SimplePipelineState"
        );

        return obj;
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
        CD3DX12_ROOT_PARAMETER1 rootParameters[1]{};
        rootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

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