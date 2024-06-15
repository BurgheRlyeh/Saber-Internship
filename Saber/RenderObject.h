#pragma once

#include "Headers.h"

#include "Vertices.h"
#include "CommandQueue.h"
#include "Mesh.h"

struct VertexPositionColor;
class CommandQueue;
struct MeshData;
class Mesh;

class RenderObject {
    std::shared_ptr<Mesh> m_pMesh{};

    DirectX::XMMATRIX m_modelMatrix{ DirectX::XMMatrixIdentity() };

public:
    RenderObject() = delete;
    RenderObject(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const MeshData& meshData
    );

    void Update();

    void Render(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipelineState,
        Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
        DirectX::XMMATRIX viewProjectionMatrix
    ) const;
};

class TestRenderObject : RenderObject {
    TestRenderObject(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const MeshData& meshData
    ) : RenderObject(
        pDevice,
        pCommandQueueCopy,
        meshData
    ) {};

public:
    static RenderObject createTriangle(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy
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

        return TestRenderObject(
            pDevice,
            pCommandQueueCopy,
            meshData
        );
    }

    static RenderObject createCube(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy
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
            0, 1, 2, 0, 2, 3,
            4, 6, 5, 4, 7, 6,
            4, 5, 1, 4, 1, 0,
            3, 2, 6, 3, 6, 7,
            1, 5, 6, 1, 6, 2,
            4, 0, 3, 4, 3, 7
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

        return TestRenderObject(
            pDevice,
            pCommandQueueCopy,
            meshData
        );
    }
};