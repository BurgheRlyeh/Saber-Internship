/**
 * @file MeshRenderObject.h
 * @brief Mesh-based render objects with indirect-command filling and concrete scene factories.
 *
 * Provides:
 *  - @ref MeshRenderObject<ModelBuffer> — template base combining a @ref Mesh,
 *    a @ref ModelBuffer, and @ref RenderObject infrastructure.  Implements all
 *    @c FillIndirectCommand overloads and exposes a bounding-box vertex handler.
 *  - @ref TestRenderObject — intermediate class that defines the SoA input layout.
 *  - @ref TestTextureRenderObject — factory helpers for a textured cube and a
 *    GLTF model with the standard G-buffer PSO.
 *  - @ref TestAlphaRenderObject — factory helper for alpha-kill GLTF objects.
 */
#pragma once

#include <GLTFSDK/GLTF.h>

#include "Headers.h"

#include <initializer_list>
#include <limits>

#include "Atlas.h"
#include "Buffer.h"
#include "IndirectCommand.h"
#include "MaterialManager.h"
#include "Mesh.h"
#include "ModelBuffer.h"
#include "RenderObject.h"
#include "Texture.h"
#include "Vertices.h"

/**
 * @brief Render object backed by a @ref Mesh and a per-object @ref ModelBuffer.
 *
 * @tparam ModelBuffer CPU-side per-object constant buffer type.
 *
 * The mesh is loaded into the @ref Atlas<Mesh> on first use via @ref InitMesh.
 * Indirect command structs are filled with the current mesh views and the
 * object's model-buffer slot index.
 */
template <typename ModelBuffer>
class MeshRenderObject : public RenderObject {
protected:
    std::wstring m_name{};

    std::shared_ptr<Mesh> m_pMesh{};

    ModelBuffer m_modelBuffer{};                           /**< @brief CPU-side model buffer data. */
    std::shared_ptr<Buffer<ModelBuffer>> m_pModelCb{};     /**< @brief GPU model constant buffer (unused when subsystem manages it). */
    size_t m_modelBufferId{ static_cast<size_t>(-1) };    /**< @brief Index into the subsystem's model-buffer array; -1 means standalone. */

public:
    /**
     * @brief Constructs the object with an optional initial model buffer.
     * @param name         Debug name.
     * @param pDevice      D3D12 device wrapper (reserved for future per-object CBV).
     * @param pModelBuffer Optional initial model buffer; copied if non-null.
     */
    MeshRenderObject(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        ModelBuffer* pModelBuffer = nullptr
    ) : m_name(name) {
        if (pModelBuffer) {
            m_modelBuffer = *pModelBuffer;
        }
		/*m_pModelCb = std::make_shared<Buffer<ModelBuffer>>(
			m_name + L"/ModelCb",
			pDevice,
			sizeof(ModelBuffer),
			&m_modelBuffer
		);*/
    }

    /** @brief Aggregates mesh initialisation data for @ref InitMesh. */
    struct MeshInitData {
        Mesh::MeshData meshData; /**< @brief Variant holding raw-data or GLTF descriptor. */
    };

    /**
     * @brief Loads (or retrieves from cache) the mesh and stores a shared pointer to it.
     * @param pDeviceContext Device context providing the @ref Atlas<Mesh>.
     * @param pCommandList   Command list for GPU upload commands.
     * @param meshInitData   Mesh construction data.
     */
    void InitMesh(
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const MeshInitData& meshInitData
    ) {
        m_pMesh = pDeviceContext->GetMeshAtlas()->Assign(
            m_name + L"/Mesh",
            pDeviceContext,
            pCommandList,
            meshInitData.meshData
        );
    }

    /**
     * @brief Sets the object's slot index in the subsystem's model-buffer array.
     * @param id Zero-based model-buffer index.
     */
    void SetModelBufferId(size_t id) {
        m_modelBufferId = id;
    }

    /** @brief Returns a reference to the CPU-side model buffer. */
    ModelBuffer& GetModelBuffer() {
        return m_modelBuffer;
    }

    /**
     * @brief Replaces the model buffer and triggers a GPU upload.
     * @param modelBuffer New model buffer value.
     */
    void SetModelBuffer(const ModelBuffer& modelBuffer) {
        m_modelBuffer = modelBuffer;
        UpdateModelBuffer();
    }

    /** @brief Uploads the current CPU model buffer to the GPU (per-object CBV path). */
    void UpdateModelBuffer() {
        m_pModelCb->Update(&m_modelBuffer);
    }

    /** @brief Fills a @c CbMeshIndirectCommand with this object's CBV, IBV, VBV, and draw args. */
    void FillIndirectCommand(CbMeshIndirectCommand& indirectCommand) override {
        indirectCommand = CbMeshIndirectCommand{
            .constantBufferView{
                m_pModelCb->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
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

    /** @brief Fills a @c CbMesh4IndirectCommand with CBV, IBV, 4 VBVs, and draw args. */
    void FillIndirectCommand(CbMesh4IndirectCommand& indirectCommand) override {
        indirectCommand = CbMesh4IndirectCommand{
            .constantBufferView{
                m_pModelCb->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
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

    /** @brief Fills a @c ConstMesh4IndirectCommand using a root-constant model-buffer index. */
    void FillIndirectCommand(ConstMesh4IndirectCommand& indirectCommand) override {
        indirectCommand = ConstMesh4IndirectCommand{
            .rootConstant{ DirectX::XMUINT4{ static_cast<uint32_t>(m_modelBufferId), 0, 0, 0 } },
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

    /** @brief Fills a @c CbConstMesh4IndirectCommand with both a CBV and a root-constant index. */
    void FillIndirectCommand(CbConstMesh4IndirectCommand& indirectCommand) override {
        indirectCommand = CbConstMesh4IndirectCommand{
            .constantBufferView{
                m_pModelCb->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
            },
            .rootConstant{ DirectX::XMUINT4{ static_cast<uint32_t>(m_modelBufferId), 0, 0, 0 } },
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

    /**
     * @brief Returns a vertex-data handler that accumulates a per-object AABB from positions.
     *
     * Pass the returned callable as the @c handler field of a @ref Mesh::VertexData or
     * @ref Mesh::Attribute to automatically compute @c bbmin / @c bbmax in @ref m_modelBuffer.
     *
     * @return @c std::function<void(void*, size_t)> that updates @c m_modelBuffer.bbmin/bbmax.
     */
    std::function<void(void*, size_t)> VerticesPositionsBBHandler() {
        return [&](void* data, size_t size) {
            const DirectX::XMFLOAT3* positions{ static_cast<DirectX::XMFLOAT3*>(data) };
            for (size_t i{}; i < size; ++i) {
                m_modelBuffer.bbmin.x = std::min(m_modelBuffer.bbmin.x, positions[i].x);
                m_modelBuffer.bbmin.y = std::min(m_modelBuffer.bbmin.y, positions[i].y);
                m_modelBuffer.bbmin.z = std::min(m_modelBuffer.bbmin.z, positions[i].z);
                m_modelBuffer.bbmax.x = std::max(m_modelBuffer.bbmax.x, positions[i].x);
                m_modelBuffer.bbmax.y = std::max(m_modelBuffer.bbmax.y, positions[i].y);
                m_modelBuffer.bbmax.z = std::max(m_modelBuffer.bbmax.z, positions[i].z);
            }
        };
    }

protected:
    /** @brief Binds all vertex buffers and the index buffer from the mesh. */
    void RenderJob(
        std::shared_ptr<CommandList> pCommandListDirect
    ) const override {
        assert(m_pMesh);
        auto pD3D12CommandList{ pCommandListDirect->GetD3D12CommandList() };
        if (m_pMesh) {
            pD3D12CommandList->IASetVertexBuffers(
                0,
                static_cast<UINT>(m_pMesh->GetVertexBuffersCount()),
                m_pMesh->GetVertexBufferViews()
            );
            pD3D12CommandList->IASetIndexBuffer(m_pMesh->GetIndexBufferView());
        }
    }

    /**
     * @brief Binds either a per-object CBV or a root-constant model-buffer index.
     *
     * If @ref m_modelBufferId is valid (not @c -1) a 32-bit root constant is used;
     * otherwise a per-object @c SetGraphicsRootConstantBufferView is issued.
     */
    void InnerRootParametersSetter(
        std::shared_ptr<CommandList> pCommandListDirect,
        UINT& rootParamId
    ) const override {
        auto pD3D12CommandList{ pCommandListDirect->GetD3D12CommandList() };
        if (m_modelBufferId == static_cast<size_t>(-1)) {
            pD3D12CommandList->SetGraphicsRootConstantBufferView(
                rootParamId++,
                m_pModelCb->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
            );
        }
        else {
            pD3D12CommandList->SetGraphicsRoot32BitConstant(rootParamId++, m_modelBufferId, 0);
        }
    }

    /** @brief Issues @c DrawIndexedInstanced for the mesh's full index range. */
    void DrawCall(
        std::shared_ptr<CommandList> pCommandList
    ) const override {
        pCommandList->GetD3D12CommandList()->DrawIndexedInstanced(
            static_cast<UINT>(m_pMesh->GetIndicesCount()),
            1,
            0, 0, 0
        );
    }
};

/**
 * @brief Intermediate base that defines the four-stream SoA D3D12 input layout.
 *
 * Streams: POSITION (float3), NORMAL (float3), TANGENT (float3), TEXCOORD (float2).
 */
class TestRenderObject : protected MeshRenderObject<ModelBuffer> {
protected:
    static inline D3D12_INPUT_ELEMENT_DESC m_inputLayoutSoA[4]{
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        {   "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        {  "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0,    DXGI_FORMAT_R32G32_FLOAT, 3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
};

/**
 * @brief Factory for standard G-buffer objects: a textured cube and a GLTF model.
 *
 * Both factories create a @ref MeshRenderObject<ModelBuffer> initialised with
 * @c SimpleVS.cso / @c SimplePS.cso and the G-buffer root signature.
 */
class TestTextureRenderObject : protected TestRenderObject {
public:
    /**
     * @brief Creates a hard-coded unit cube with brick textures.
     * @param pDeviceContext Device context.
     * @param pCommandList   Command list for mesh upload.
     * @param pGBuffer       G-buffer (provides the RTV format array).
     * @param modelMatrix    Model-to-world transform (default identity).
     * @return Shared pointer to the created @ref MeshRenderObject<ModelBuffer>.
     */
    static std::shared_ptr<MeshRenderObject<ModelBuffer>> CreateTextureCube(
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        std::shared_ptr<Texture> pGBuffer,
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

        std::shared_ptr<MeshRenderObject<ModelBuffer>> pObj{
            std::make_shared<MeshRenderObject<ModelBuffer>>(L"SimpleTextureCube", pDeviceContext->GetDevice())
        };

        Mesh::MeshDataIndicesVertices meshData{
            // indices data
            .indices{ indices },
            .indicesCnt{ _countof(indices) },
            .indexSize{ sizeof(*indices) },
            .indexFormat{ DXGI_FORMAT_R32_UINT },
            // vertices data
            .verticesCnt{ 24 },
            .verticesData{
                {
                    .data{ positions },
                    .size{ sizeof(*positions) },
                    .handler{ pObj->VerticesPositionsBBHandler() }
                },
                {.data{ normals }, .size{ sizeof(*normals) } },
                {.data{ tangents }, .size{ sizeof(*tangents) } },
                {.data{ uvs }, .size{ sizeof(*uvs) } }
            }
        };
        pObj->InitMesh(pDeviceContext, pCommandList, MeshInitData(meshData));
        pObj->InitMaterial(
            pDeviceContext,
            RootSignatureData{
                CreateRootSignatureBlob(),
                L"GLTFRootSignature"
            },
            ShaderData{
                L"SimpleVS.cso",
                L"SimplePS.cso"
            },
            PipelineStateData{
                CreatePipelineStateDesc(m_inputLayoutSoA, _countof(m_inputLayoutSoA), pGBuffer->GetRtFormatArray())
            }
        );

        pObj->GetModelBuffer().UpdateMatrices(modelMatrix);
        pObj->GetModelBuffer().SetMaterial(pDeviceContext->GetMaterialManager()->GetCreateMaterial(
            pDeviceContext,
            pCommandList,
            L"Brick.dds",
            L"BrickNM.dds"
        ));

        return pObj;
    }

    /**
     * @brief Creates a GLTF model with the standard G-buffer PSO (barbarian textures).
     * @param pDeviceContext Device context.
     * @param pCommandList   Command list for mesh upload.
     * @param filepath       Path to the .gltf / .glb file.
     * @param pGBuffer       G-buffer (provides the RTV format array).
     * @param modelMatrix    Model-to-world transform (default identity).
     * @return Shared pointer to the created @ref MeshRenderObject<ModelBuffer>.
     */
    static std::shared_ptr<MeshRenderObject<ModelBuffer>> CreateModelFromGLTF(
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        std::filesystem::path& filepath,
        std::shared_ptr<Texture> pGBuffer,
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
    ) {
        std::shared_ptr<MeshRenderObject<ModelBuffer>> pObj{
            std::make_shared<MeshRenderObject<ModelBuffer>>(L"MeshGLTF", pDeviceContext->GetDevice())
        };

        Mesh::MeshDataGLTF data{
            .filepath{ filepath },
            .attributes{
                Mesh::Attribute{
                    .name{ Microsoft::glTF::ACCESSOR_POSITION },
                    .size{ sizeof(DirectX::XMFLOAT3) },
                    .handler{ pObj->VerticesPositionsBBHandler() }
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

        pObj->InitMesh(pDeviceContext, pCommandList, MeshInitData(data));
        pObj->InitMaterial(
            pDeviceContext,
            RootSignatureData{
                CreateRootSignatureBlob(),
                L"GLTFRootSignature"
            },
            ShaderData{
                L"SimpleVS.cso",
                L"SimplePS.cso"
            },
            PipelineStateData{
                CreatePipelineStateDesc(m_inputLayoutSoA, _countof(m_inputLayoutSoA), pGBuffer->GetRtFormatArray())
            }
        );

        pObj->GetModelBuffer().UpdateMatrices(modelMatrix);
        pObj->GetModelBuffer().SetMaterial(pDeviceContext->GetMaterialManager()->GetCreateMaterial(
            pDeviceContext,
            pCommandList,
            L"barbarian_diffuse.dds",
            L"barb2_n.dds"
        ));

        return pObj;
    }

private:
    /** @brief Creates the root signature blob for the standard G-buffer pass (scene CB, model constant, model SRV). */
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob() {
        // Allow input layout and deny unnecessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
        };

        size_t rp{}, spCbv{}, srSrv{};
        CD3DX12_ROOT_PARAMETER1 rootParameters[3]{};
        rootParameters[rp++].InitAsConstantBufferView(spCbv++); // scene CB
        rootParameters[rp++].InitAsConstants(4, spCbv++);       // model CB ID
        rootParameters[rp++].InitAsShaderResourceView(srSrv++); // model CBs

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
            &rootSignatureDescription,
            D3D_ROOT_SIGNATURE_VERSION_1_1,
            &rootSignatureBlob,
            &errorBlob
        ));

        return rootSignatureBlob;
    }

    /** @brief Creates the graphics PSO descriptor for the standard G-buffer pass. */
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

/**
 * @brief Factory for alpha-kill G-buffer objects loaded from GLTF files.
 *
 * Uses @c AlphaKillVS.cso / @c AlphaKillPS.cso and a root signature that
 * additionally exposes the material CBV and bindless texture SRV table for
 * alpha testing in the pixel shader.  Back-face culling is disabled.
 */
class TestAlphaRenderObject : protected TestRenderObject {
public:
    /**
     * @brief Creates an alpha-kill GLTF object (e.g. grass foliage).
     * @param pDeviceContext Device context.
     * @param pCommandList   Command list for mesh upload.
     * @param filepath       Path to the .gltf / .glb file.
     * @param pGBuffer       G-buffer (provides the RTV format array).
     * @param modelMatrix    Model-to-world transform (default identity).
     * @return Shared pointer to the created @ref MeshRenderObject<ModelBuffer>.
     */
    static std::shared_ptr<MeshRenderObject<ModelBuffer>> CreateAlphaModelFromGLTF(
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        std::filesystem::path& filepath,
        std::shared_ptr<Texture> pGBuffer,
        const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
    ) {
        std::shared_ptr<MeshRenderObject<ModelBuffer>> pObj{
            std::make_shared<MeshRenderObject<ModelBuffer>>(L"AlphaGrassGLTF", pDeviceContext->GetDevice())
        };

        Mesh::MeshDataGLTF data{
            .filepath{ filepath },
            .attributes{
                Mesh::Attribute{
                    .name{ Microsoft::glTF::ACCESSOR_POSITION },
                    .size{ sizeof(DirectX::XMFLOAT3) },
                    .handler{ pObj->VerticesPositionsBBHandler() }
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

        pObj->InitMesh(pDeviceContext, pCommandList, MeshInitData(data));
        pObj->InitMaterial(
            pDeviceContext,
            RootSignatureData{
                CreateRootSignatureBlob(),
                L"AlphaGrassGLTFRootSignature"
            },
            ShaderData{
                L"AlphaKillVS.cso",
                L"AlphaKillPS.cso"
            },
            PipelineStateData{
                CreateAlphaPipelineStateDesc(m_inputLayoutSoA, _countof(m_inputLayoutSoA), pGBuffer->GetRtFormatArray())
            }
        );

        pObj->GetModelBuffer().UpdateMatrices(modelMatrix);
        pObj->GetModelBuffer().SetMaterial(pDeviceContext->GetMaterialManager()->GetCreateMaterial(
            pDeviceContext,
            pCommandList,
            L"grassAlbedo.dds",
            L"grassNormal.dds"
        ));

        return pObj;
    }

protected:
    /** @brief Creates the root signature blob for the alpha-kill pass (adds material CBV and bindless SRV table). */
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob() {
        // Allow input layout and deny unnecessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
        };

        size_t rp{}, srCbv{}, srSrv{};
        CD3DX12_ROOT_PARAMETER1 rootParameters[5]{};
        rootParameters[rp++].InitAsConstantBufferView(srCbv++); // scene CB
        rootParameters[rp++].InitAsConstants(4, srCbv++);       // model CB id
        rootParameters[rp++].InitAsShaderResourceView(srSrv++); // model CBs id

        CD3DX12_DESCRIPTOR_RANGE1 rangeCbvsMaterials[1]{};
        rangeCbvsMaterials[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, srCbv++);
        rootParameters[rp++].InitAsDescriptorTable(_countof(rangeCbvsMaterials), rangeCbvsMaterials/*, D3D12_SHADER_VISIBILITY_PIXEL*/);

        CD3DX12_DESCRIPTOR_RANGE1 rangeSrvsMaterial[1]{};
        rangeSrvsMaterial[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, srSrv++, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
        rootParameters[rp++].InitAsDescriptorTable(_countof(rangeSrvsMaterial), rangeSrvsMaterial, D3D12_SHADER_VISIBILITY_PIXEL);

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
            D3D_ROOT_SIGNATURE_VERSION_1_1,
            &rootSignatureBlob,
            &errorBlob
        ));

        return rootSignatureBlob;
    }

    /** @brief Creates the graphics PSO descriptor for the alpha-kill pass (no back-face culling). */
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
