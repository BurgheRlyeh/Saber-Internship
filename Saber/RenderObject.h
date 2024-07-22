#pragma once

#include "Headers.h"

#include "Atlas.h"
#include "CommandQueue.h"
#include "Mesh.h"
#include "PSOLibrary.h"
#include "Resources.h"
#include "Vertices.h"
#include "Texture.h"

class RenderObject {
    std::shared_ptr<Mesh> m_pMesh{};
    std::shared_ptr<RootSignatureResource> m_pRootSignatureResource{};
    std::shared_ptr<ShaderResource> m_pVertexShaderResource{};
    std::shared_ptr<ShaderResource> m_pPixelShaderResource{};
    //std::shared_ptr<PipelineStateResource> m_pPipelineStateResource{};
    std::shared_ptr<Texture> m_pTexture{};

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
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        std::shared_ptr<Texture> pTexture = nullptr
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
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
    ) {
        VertexPositionColor vertices[8]{
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

    static RenderObject createTextureCube(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::shared_ptr<CommandQueue> const& pCommandQueueDirect,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
    ) {
        VertexPositionUV vertices[24]{
            // Bottom face
            { { -1.f, -1.f,  1.f, 1.f }, { 0.f, 1.f } },
            { {  1.f, -1.f,  1.f, 1.f }, { 1.f, 1.f } },
            { {  1.f, -1.f, -1.f, 1.f }, { 1.f, 0.f } },
            { { -1.f, -1.f, -1.f, 1.f }, { 0.f, 0.f } },
            // Front face 
            { { -1.f,  1.f, -1.f, 1.f }, { 0.f, 1.f } },
            { {  1.f,  1.f, -1.f, 1.f }, { 1.f, 1.f } },
            { {  1.f,  1.f,  1.f, 1.f }, { 1.f, 0.f } },
            { { -1.f,  1.f,  1.f, 1.f }, { 0.f, 0.f } },
            // Top face
            { {  1.f, -1.f, -1.f, 1.f }, { 0.f, 1.f } },
            { {  1.f, -1.f,  1.f, 1.f }, { 1.f, 1.f } },
            { {  1.f,  1.f,  1.f, 1.f }, { 1.f, 0.f } },
            { {  1.f,  1.f, -1.f, 1.f }, { 0.f, 0.f } },
            // Back face
            { { -1.f, -1.f,  1.f, 1.f }, { 0.f, 1.f } },
            { { -1.f, -1.f, -1.f, 1.f }, { 1.f, 1.f } },
            { { -1.f,  1.f, -1.f, 1.f }, { 1.f, 0.f } },
            { { -1.f,  1.f,  1.f, 1.f }, { 0.f, 0.f } },
            // Right face
            { {  1.f, -1.f,  1.f, 1.f }, { 0.f, 1.f } },
            { { -1.f, -1.f,  1.f, 1.f }, { 1.f, 1.f } },
            { { -1.f,  1.f,  1.f, 1.f }, { 1.f, 0.f } },
            { {  1.f,  1.f,  1.f, 1.f }, { 0.f, 0.f } },
            // Left face
            { { -1.f, -1.f, -1.f, 1.f }, { 0.f, 1.f } },
            { {  1.f, -1.f, -1.f, 1.f }, { 1.f, 1.f } },
            { {  1.f,  1.f, -1.f, 1.f }, { 1.f, 0.f } },
            { { -1.f,  1.f, -1.f, 1.f }, { 0.f, 0.f } }
        };

        uint32_t indices[36]{
             0,  1,	 2,  0,  2,  3,
             4,  5,	 6,  4,  6,  7,
             8,  9,	10,  8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23
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

        //DirectX::GetMetadataFromDDSFile()
        //DirectX::LoadFromDDSFile()
        //DirectX::ScratchImage image{};
        //DirectX::LoadFromDDSFile(L"Kitty.dds", DirectX::DDS_FLAGS_NONE, nullptr, image);

        Microsoft::WRL::ComPtr<ID3D12Resource> res{};

        RenderObject obj{ modelMatrix };
        obj.InitMesh(pDevice, pCommandQueueCopy, pMeshAtlas, meshData, L"SimpleTextureCube");
        obj.InitMaterial(
            pDevice,
            pShaderAtlas,
            L"SimpleUVVertexShader.cso",
            L"SimpleUVPixelShader.cso",
            pRootSignatureAtlas,
            CreateTextureCubeRootSignatureBlob(pDevice),
            L"SimpleTextureRootSignature",
            pPSOLibrary,
            std::make_shared<Texture>(pDevice, pCommandQueueCopy, pCommandQueueDirect, nullptr, L"Kitty.dds")
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

    static Microsoft::WRL::ComPtr<ID3DBlob> CreateTextureCubeRootSignatureBlob(
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
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
        };

        CD3DX12_DESCRIPTOR_RANGE1 descriptorTableRange(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            1,
            0,
            0
        );

        // A single 32-bit constant root parameter that is used by the vertex shader.
        CD3DX12_ROOT_PARAMETER1 rootParameters[3]{};
        rootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // ViewProj matrix
        rootParameters[1].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX); // Model matrix
        rootParameters[2].InitAsDescriptorTable(1, &descriptorTableRange, D3D12_SHADER_VISIBILITY_PIXEL);

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