#pragma once

#include "Headers.h"

#include <random>

#include "ConstantBuffer.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DynamicUploadRingBuffer.h"
#include "DynamicUploadRingBuffer.h"
#include "GPUResource.h"
#include "IndirectCommandBuffer.h"
#include "IndirectUpdater.h"
#include "MeshRenderObject.h"
#include "ModelBuffers.h"

template <typename ModelBuffer, size_t InstMaxCount>
class IndirectMeshRenderObject : public RenderObject {
protected:
	std::shared_ptr<Mesh> m_pMesh{};

	std::vector<ModelBuffer> m_modelBuffers{ InstMaxCount };
	std::shared_ptr<ConstantBuffer> m_pModelCB{};
	
	struct IndirectCommand {
		D3D12_GPU_VIRTUAL_ADDRESS cbv{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		D3D12_DRAW_INDEXED_ARGUMENTS drawArguments{};

		static inline D3D12_INDIRECT_ARGUMENT_DESC indirectArgumentDescs[4]{
			{.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW }, .ConstantBufferView{ 1 } },
			{.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW } },
			{.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{} },
			{.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED } },
		};

		static D3D12_COMMAND_SIGNATURE_DESC GetCommandSignatureDesc() {
			return D3D12_COMMAND_SIGNATURE_DESC{
				.ByteStride{ sizeof(IndirectCommand) },
				.NumArgumentDescs{ _countof(indirectArgumentDescs) },
				.pArgumentDescs{ indirectArgumentDescs }
			};
		}
	};

	std::shared_ptr<IndirectCommandBuffer<IndirectCommand>> m_pIndirectCommandBuffer{};

public:
	IndirectMeshRenderObject(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
	) {
		m_pModelCB = std::make_shared<ConstantBuffer>(
			pAllocator,
			InstMaxCount * sizeof(ModelBuffer),
			m_modelBuffers.data()
		);
	}

	struct MeshInitData {
		std::shared_ptr<Atlas<Mesh>> pMeshAtlas{};
		Mesh::MeshData meshData{};
		std::wstring meshFilename{};
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

	void InitIndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& objectName,
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap
	) {
		m_pIndirectCommandBuffer = std::make_shared<
			StaticIndirectCommandBuffer<IndirectCommand>
		>(
			pDevice,
			pAllocator,
			IndirectCommand::GetCommandSignatureDesc(),
			m_pRootSignatureResource->pRootSignature,
			pDescHeapManagerCbvSrvUav,
			objectName,
			1,
			pDynamicUploadHeap
		);
	}

	void UpdateIndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) {
		m_pIndirectCommandBuffer->PerformUpdate(
			pDevice,
			pAllocator,
			pCommandQueueCopy,
			pCommandQueueDirect
		);
	}

	ModelBuffer& GetModelBuffer(size_t id) {
		return m_modelBuffers[id];
	}
	void SetModelBuffer(size_t id, const ModelBuffer& modelBuffer) {
		m_modelBuffers[id] = modelBuffer;
		m_pModelCB->Update(&m_modelBuffers[id], id * sizeof(ModelBuffer), sizeof(ModelBuffer));

		m_pIndirectCommandBuffer->SetUpdateAt(
			id,
			IndirectCommand{
				.cbv{ m_pModelCB->GetResource()->GetGPUVirtualAddress() + id * sizeof(ModelBuffer) },
				.indexBufferView{ *m_pMesh->GetIndexBufferView() },
				.vertexBufferView{ *m_pMesh->GetVertexBufferViews() },
				.drawArguments{ D3D12_DRAW_INDEXED_ARGUMENTS{
					.IndexCountPerInstance{ static_cast<UINT>(m_pMesh->GetIndicesCount()) },
					.InstanceCount{ 1 },
				} },
			}
		);
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

	}

	virtual void DrawCall(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList
	) const override {
		m_pIndirectCommandBuffer->Execute(pCommandList);
	}
};

template <typename ModelBuffer, size_t InstMaxCount>
class DynamicIndirectMeshRenderObject : public IndirectMeshRenderObject<ModelBuffer, InstMaxCount> {
	using IndirectMeshRenderObject::InitIndirectCommandBuffer;

public:
	DynamicIndirectMeshRenderObject(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
	) : IndirectMeshRenderObject<ModelBuffer, InstMaxCount>(pAllocator) {}

	void InitIndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& objectName,
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
		std::shared_ptr<ComputeObject> pIndirectUpdater
	) {
		m_pIndirectCommandBuffer = std::make_shared<
			DynamicIndirectCommandBuffer<IndirectCommand>
		>(
			pDevice,
			pAllocator,
			IndirectCommand::GetCommandSignatureDesc(),
			m_pRootSignatureResource->pRootSignature,
			pDescHeapManagerCbvSrvUav,
			objectName,
			1,
			pDynamicUploadHeap,
			pIndirectUpdater
		);
	}
};

template <size_t InstMaxCount = 1024>
class TestIndirectMeshRenderObject : protected TestAlphaRenderObject {
public:
	static std::shared_ptr<IndirectMeshRenderObject<ModelBuffer, InstMaxCount>> CreateStatic(
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
		std::shared_ptr<DescriptorHeapManager> pDescHeapCbvSrvUav,
		std::shared_ptr<MaterialManager> pMaterialManager,
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
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

		std::shared_ptr<IndirectMeshRenderObject<ModelBuffer, InstMaxCount>> pObj{
			std::make_shared<IndirectMeshRenderObject<ModelBuffer, InstMaxCount>>(pAllocator)
		};
		pObj->InitMaterial(
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
		pObj->InitMesh(pDevice, pAllocator, pCommandQueueCopy, 
			IndirectMeshRenderObject<ModelBuffer, InstMaxCount>::MeshInitData(pMeshAtlas, data, L"AlphaGrassGLTF"));
		pObj->InitIndirectCommandBuffer(pDevice, pAllocator, pDescHeapCbvSrvUav, L"IndirectAlphaGrassGLTF", pDynamicUploadHeap);

		size_t materialId{ pMaterialManager->AddMaterial(
				pDevice,
				pAllocator,
				pCommandQueueCopy,
				pCommandQueueDirect,
				L"grassAlbedo.dds",
				L"grassNormal.dds"
		) };
		pObj->SetModelBuffer(0, ModelBuffer{ modelMatrix, materialId });

		DirectX::XMMATRIX scale{ DirectX::XMMatrixScaling(.025f, .025f, .025f) };
		for (size_t i{ 1 }; i < InstMaxCount; ++i) {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_real_distribution<float> posDist(-10.f, 10.f);
			pObj->SetModelBuffer(i, ModelBuffer{
				scale * DirectX::XMMatrixTranslation(posDist(gen), -1.f, posDist(gen)),
				materialId
				});
		}

		pObj->UpdateIndirectCommandBuffer(pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect);

		return pObj;
	}
	static std::shared_ptr<IndirectMeshRenderObject<ModelBuffer, InstMaxCount>> CreateDynamic(
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
		std::shared_ptr<DescriptorHeapManager> pDescHeapCbvSrvUav,
		std::shared_ptr<MaterialManager> pMaterialManager,
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
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

		std::shared_ptr<DynamicIndirectMeshRenderObject<ModelBuffer, InstMaxCount>> pObj{
			std::make_shared<DynamicIndirectMeshRenderObject<ModelBuffer, InstMaxCount>>(pAllocator)
		};
		pObj->InitMaterial(
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
		pObj->InitMesh(pDevice, pAllocator, pCommandQueueCopy, 
			DynamicIndirectMeshRenderObject<ModelBuffer, InstMaxCount>::MeshInitData(pMeshAtlas, data, L"AlphaGrassGLTF")
		);
		pObj->InitIndirectCommandBuffer(
			pDevice,
			pAllocator,
			pDescHeapCbvSrvUav,
			L"IndirectAlphaGrassGLTF",
			pDynamicUploadHeap,
			IndirectUpdater::CreateCbMeshUpdater(pDevice, pShaderAtlas, pRootSignatureAtlas, pPSOLibrary)
		);

		size_t materialId{ pMaterialManager->AddMaterial(
				pDevice,
				pAllocator,
				pCommandQueueCopy,
				pCommandQueueDirect,
				L"grassAlbedo.dds",
				L"grassNormal.dds"
		) };
		pObj->SetModelBuffer(0, ModelBuffer{ modelMatrix, materialId });

		DirectX::XMMATRIX scale{ DirectX::XMMatrixScaling(.025f, .025f, .025f) };
		for (size_t i{ 1 }; i < InstMaxCount; ++i) {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_real_distribution<float> posDist(-10.f, 10.f);
			pObj->SetModelBuffer(i, ModelBuffer{
				scale * DirectX::XMMatrixTranslation(posDist(gen), -1.f, posDist(gen)),
				materialId
				});
		}

		pObj->UpdateIndirectCommandBuffer(pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect);

		return pObj;
	}
};
