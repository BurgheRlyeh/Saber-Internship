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
#include "MeshRenderObject.h"
#include "ModelBuffers.h"

template <typename ModelBuffer, size_t InstMaxCount>
class IndirectMeshRenderObject : public RenderObject {
protected:
	std::shared_ptr<Mesh> m_pMesh{};

	std::shared_ptr<DescHeapRange> m_pDescHeapRangeCbvs{};
	std::vector<ModelBuffer> m_modelBuffers{ InstMaxCount };
	std::shared_ptr<ConstantBuffer> m_pModelCB{};

	struct IndirectCommand {
		D3D12_GPU_VIRTUAL_ADDRESS cbv;
		D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
	};
	std::shared_ptr<IndirectCommandBuffer<IndirectCommand, InstMaxCount>> m_pIndirectCommandBuffer{};

public:
	IndirectMeshRenderObject(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		ModelBuffer* pModelBuffers = nullptr
	) {
		ModelBuffer* pModelBuffer{ pModelBuffers };
		for (size_t i{}; i < InstMaxCount; ++i) {
			if (pModelBuffer) {
				m_modelBuffers[i] = *pModelBuffer++;
			}
		}
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
		const std::wstring& objectName
	) {
		D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[2]{};
		argumentDescs[0] = {
			.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW },
			.ConstantBufferView{.RootParameterIndex{ 1 } }
		};
		argumentDescs[1] = {
			.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED }
		};

		D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc{
			.ByteStride{ sizeof(IndirectCommand) },
			.NumArgumentDescs{ _countof(argumentDescs) },
			.pArgumentDescs{ argumentDescs }
		};

		m_pIndirectCommandBuffer = std::make_shared<
			IndirectCommandBuffer<IndirectCommand, InstMaxCount>
		>(
			pDevice,
			pAllocator,
			commandSignatureDesc,
			m_pRootSignatureResource->pRootSignature,
			pDescHeapManagerCbvSrvUav,
			objectName
		);
	}

	void FillIndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) {
		D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress{ m_pModelCB->GetResource()->GetGPUVirtualAddress() };
		std::vector<IndirectCommand> indirectCommands{ InstMaxCount };
		for (size_t i{}; i < InstMaxCount; ++i) {
			indirectCommands[i].cbv = cbvGpuAddress;
			indirectCommands[i].drawArguments = D3D12_DRAW_INDEXED_ARGUMENTS{
				.IndexCountPerInstance{ static_cast<UINT>(m_pMesh->GetIndicesCount()) },
				.InstanceCount{ 1 },
			};

			cbvGpuAddress += sizeof(ModelBuffer);
		}

		m_pIndirectCommandBuffer->CopyToBuffer(
			pDevice,
			pAllocator,
			pCommandQueueCopy,
			pCommandQueueDirect,
			indirectCommands.data()
		);
	}

	ModelBuffer& GetModelBuffer(size_t id) {
		return m_modelBuffers[id];
	}
	void SetModelBuffer(size_t id, const ModelBuffer& modelBuffer) {
		m_modelBuffers[id] = modelBuffer;
		m_pModelCB->Update(&m_modelBuffers[id], id * sizeof(ModelBuffer), sizeof(ModelBuffer));
	}
	void UpdateModelBuffers() {
		m_pModelCB->Update(m_modelBuffers.data());
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

template <size_t InstMaxCount = 1000>
class TestIndirectMeshRenderObject : public IndirectMeshRenderObject<ModelBuffer, InstMaxCount> {
	static inline D3D12_INPUT_ELEMENT_DESC m_inputLayoutSoA[4]{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

public:
	static std::shared_ptr<IndirectMeshRenderObject<ModelBuffer, InstMaxCount>> CreateIndirectAlphaModelFromGLTF(
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
		pObj->InitMesh(pDevice, pAllocator, pCommandQueueCopy, MeshInitData(pMeshAtlas, data, L"AlphaGrassGLTF"));

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

		pObj->InitIndirectCommandBuffer(pDevice, pAllocator, pDescHeapCbvSrvUav, L"IndirectAlphaGrassGLTF");
		pObj->FillIndirectCommandBuffer(pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect);

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
