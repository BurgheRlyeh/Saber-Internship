#pragma once

#include "Headers.h"

#include "Vertices.h"
#include "CommandQueue.h"

struct VertexPositionColor;
class CommandQueue;

class Object {
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pVertexBuffer{};
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_pIndexBuffer{};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};

    size_t m_indicesCount{};

    DirectX::XMMATRIX m_modelMatrix{ DirectX::XMMatrixIdentity() };

public:
    Object() = delete;
    Object(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const void* vertices, size_t verticesCnt, size_t vertexSize,
        const void* indices, size_t indicesCnt, size_t indexSize, DXGI_FORMAT indexFormat
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

private:
    void CreateBufferResource(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
        ID3D12Resource** ppDestinationResource,
        ID3D12Resource** ppIntermediateResource,
        size_t numElements,
        size_t elementSize,
        const void* bufferData,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
    );

};

class TestObject : Object {
    TestObject(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const void* vertices, size_t verticesCnt, size_t vertexSize,
        const void* indices, size_t indicesCnt, size_t indexSize, DXGI_FORMAT indexFormat
    ) : Object(
        pDevice,
        pCommandQueueCopy,
        vertices, verticesCnt, vertexSize,
        indices, indicesCnt, indexSize, indexFormat
    ) {};

public:
    static Object createTriangle(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy
    ) {
        VertexPositionColor vertices[]{
            { { -1.0f, -1.0f, 0.f, 1.f }, { 1.0f, 0.0f, 0.0f, 0.f } },
            { {  0.0f,  1.0f, 0.f, 1.f }, { 0.0f, 1.0f, 0.0f, 0.f } },
            { {  1.0f, -1.0f, 0.f, 1.f }, { 0.0f, 0.0f, 1.0f, 0.f } }
        };
        uint32_t indices[]{ 0, 1, 2 };

        return TestObject(
            pDevice,
            pCommandQueueCopy,
            vertices, _countof(vertices), sizeof(*vertices),
            indices, _countof(indices), sizeof(*indices), DXGI_FORMAT_R32_UINT
        );
    }

    static Object createCube(
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

        return TestObject(
            pDevice,
            pCommandQueueCopy,
            vertices, _countof(vertices), sizeof(*vertices),
            indices, _countof(indices), sizeof(*indices), DXGI_FORMAT_R32_UINT
        );
    }
};