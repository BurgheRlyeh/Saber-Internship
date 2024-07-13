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
    RenderObject() = default;
    RenderObject(const DirectX::XMMATRIX& modelMatrix)
        : m_modelMatrix(modelMatrix)
    {}

    void InitMesh(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        const MeshData& meshData,
        const std::wstring& meshFilename = L""
    );

    void InitMaterial(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        const LPCWSTR& vertexShaderFilepath,
        const LPCWSTR& pixelShaderFilepath,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob,
        const std::wstring& rootSignatureFilename,
        std::shared_ptr<PSOLibrary> pPSOLibrary
    );

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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineStateDesc(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<RootSignatureResource> pRootSignatureResource,
        std::shared_ptr<ShaderResource> pVertexShaderResource,
        std::shared_ptr<ShaderResource> pPixelShaderResource
    );
};

class TestRenderObject : RenderObject {
public:
    static RenderObject createTriangle(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
    ) {
        VertexPositionColor vertices[]{
            { { -1.0f, -1.0f, 0.f, 1.f }, { 1.0f, 0.0f, 0.0f, 0.f } },
            { {  0.0f,  1.0f, 0.f, 1.f }, { 0.0f, 1.0f, 0.0f, 0.f } },
            { {  1.0f, -1.0f, 0.f, 1.f }, { 0.0f, 0.0f, 1.0f, 0.f } }
        };
        uint32_t indices[]{ 0, 1, 2, 2, 1, 0 };

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

        RenderObject obj{ modelMatrix };
        obj.InitMesh(pDevice, pCommandQueueCopy, pMeshAtlas, meshData, L"SimpleTriangle");
        obj.InitMaterial(
            pDevice,
            pShaderAtlas,
            L"SimpleVertexShader.cso",
            L"SimplePixelShader.cso",
            pRootSignatureAtlas,
            CreateRootSignatureBlob(pDevice),
            L"SimpleRootSignature",
            //pPipelineStateAtlas
            pPSOLibrary
        );

        return obj;
    }

    static RenderObject createCube(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity(),
        DirectX::XMFLOAT4 color = { 0.5f, 0.5f, 0.5f, 0.5f }
    ) {
        VertexPositionColor vertices[8]{
            { { -1.0f, -1.0f, -1.0f, 1.0f }, color },
            { { -1.0f,  1.0f, -1.0f, 1.0f }, color },
            { {  1.0f,  1.0f, -1.0f, 1.0f }, color },
            { {  1.0f, -1.0f, -1.0f, 1.0f }, color },
            { { -1.0f, -1.0f,  1.0f, 1.0f }, color },
            { { -1.0f,  1.0f,  1.0f, 1.0f }, color },
            { {  1.0f,  1.0f,  1.0f, 1.0f }, color },
            { {  1.0f, -1.0f,  1.0f, 1.0f }, color }
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

        RenderObject obj{ modelMatrix };
        obj.InitMesh(pDevice, pCommandQueueCopy, pMeshAtlas, meshData, L"SimpleCube");
        obj.InitMaterial(
            pDevice,
            pShaderAtlas,
            L"SimpleVertexShader.cso",
            L"SimplePixelShader.cso",
            pRootSignatureAtlas,
            CreateRootSignatureBlob(pDevice),
            L"SimpleRootSignature",
            //pPipelineStateAtlas
            pPSOLibrary
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
        CD3DX12_ROOT_PARAMETER1 rootParameters[2]{};
        rootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // ViewProj matrix
        rootParameters[1].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX); // Model matrix

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