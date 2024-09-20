#pragma once

#include "Headers.h"

#include "Atlas.h"
#include "ConstantBuffer.h"
#include "DDSTexture.h"
#include "DescriptorHeapManager.h"
#include "Texture.h"

//class RenderMaterial {
//	std::shared_ptr<Texture> pAlbedo{};
//	D3D12_CPU_DESCRIPTOR_HANDLE albedoHandle{};
//	size_t albedoId{};
//	
//	std::shared_ptr<Texture> pNormals{};
//
//};
//
//struct MaterialBuffer {
//	size_t albedoId{};
//	size_t normalMapId{};
//	DirectX::XMFLOAT4 diffuseColorAndPower{};
//	DirectX::XMFLOAT4 specularColorAndPower{};
//};

/*                                    */





struct MaterialCB {
	DirectX::XMUINT4 texsId{};

	MaterialCB(size_t albedoId, size_t normalId) : texsId{
		static_cast<UINT>(albedoId),
		static_cast<UINT>(normalId),
		0,
		0
	} {}
};

struct RenderMaterial {
	std::shared_ptr<Texture> pAlbedo{};
	std::shared_ptr<Texture> pNormal{};
};

class MaterialManager {
	std::shared_ptr<DescHeapRange> m_pSRVsRange{};
	std::shared_ptr<DescHeapRange> m_pCBVsRange{};
	std::vector<std::shared_ptr<RenderMaterial>> m_pMaterials{};
	std::vector<std::shared_ptr<ConstantBuffer>> m_pMaterialCBs{};

	std::shared_ptr<Atlas<DDSTexture>> m_pTextureAtlas{};

public:
	MaterialManager(
		const std::wstring& resourceFolder,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManager,
		const size_t& capacity
	) {
		m_pTextureAtlas = std::make_shared<Atlas<DDSTexture>>(resourceFolder);

		m_pSRVsRange = pDescHeapManager->AllocateRange(
			L"Materials/SRVs",
			2 * capacity,
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV
		);
		m_pCBVsRange = pDescHeapManager->AllocateRange(
			L"Materials/CBVs",
			capacity,
			D3D12_DESCRIPTOR_RANGE_TYPE_CBV
		);
		
		m_pMaterials.reserve(capacity);
		m_pMaterialCBs.reserve(capacity);
	}
	~MaterialManager() {
		m_pMaterials.clear();
	}

	std::shared_ptr<DescHeapRange> GetMaterialCBVsRange() const {
		return m_pCBVsRange;
	}
	std::shared_ptr<DescHeapRange> GetMaterialSRVsRange() const {
		return m_pSRVsRange;
	}

	size_t AddMaterial(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		const std::wstring& albedoFilepath,
		const std::wstring& normalFilepath
	) {
		std::shared_ptr<Texture> pAlbedo{
			m_pTextureAtlas->Assign(albedoFilepath, pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect)
		};
		std::shared_ptr<Texture> pNormal{
			m_pTextureAtlas->Assign(normalFilepath, pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect)
		};
		m_pMaterials.push_back(std::make_shared<RenderMaterial>(pAlbedo, pNormal));

		MaterialCB materialCB{
			AddTexture(pDevice, pAlbedo),
			AddTexture(pDevice, pNormal)
		};
		std::shared_ptr<ConstantBuffer> pMaterialCB{
			std::make_shared<ConstantBuffer>(pAllocator, sizeof(materialCB), &materialCB)
		};
		m_pMaterialCBs.push_back(pMaterialCB);

		return AddCB(pDevice, pMaterialCB);
	}

private:
	size_t AddTexture(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::shared_ptr<Texture> pTex) {
		size_t id{ m_pSRVsRange->GetNextId() };

		auto handle = m_pSRVsRange->GetCpuHandle(id);
		pTex->CreateShaderResourceView(
			pDevice,
			handle,
			pTex->GetSrvDesc()
		);

		return id;
	}

	size_t AddCB(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::shared_ptr<ConstantBuffer> pBuf) {
		size_t id{ m_pCBVsRange->GetNextId() };
		pBuf->CreateConstantBufferView(pDevice, m_pCBVsRange->GetCpuHandle(id));
		return id;
	}
};

template<
	size_t RtvCapacity = 3,
	size_t CbvCapacity = 0,
	size_t SrvCapacity = 7,
	size_t UavCapacity = 1,
	typename STRING_TYPE = atlas_string
>
class GPUResourceLibrary : Atlas<GPUResource> {
	Microsoft::WRL::ComPtr<ID3D12Device2> m_pDevice{};

	size_t m_gbufferSize{};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRtvDescHeap{};
	UINT m_rtvHandleIncSize{};
	size_t m_rtvCount{};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pCbvSrvUavDescHeap{};
	UINT m_cbvSrvUavHandleIncSize{};
	size_t m_cbvCount{};
	size_t m_srvCount{};
	size_t m_uavCount{};

public:
	GPUResourceLibrary(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const STRING_TYPE& resourceFolder = {}
	) : m_pDevice(pDevice), Atlas<GPUResource>(resourceFolder) {
		m_rtvHandleIncSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_cbvSrvUavHandleIncSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		m_pRtvDescHeap = CreateDescHeap(
			pDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			RtvCapacity
		);

		m_pCbvSrvUavDescHeap = CreateDescHeap(
			pDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			CbvCapacity + SrvCapacity + UavCapacity,
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		);
	}

	size_t AddConstantBuffer(const STRING_TYPE& filename, std::shared_ptr<ConstantBuffer> pBuffer) {
		//if (!Atlas::Replace(filename, pBuffer)) {
		//	if () {
		//		Atlas::Add(filename, pBuffer)
		//	}
		//	else {
		//		return size_t(-1);
		//	}
		//}
		

		if (m_cbvCount == CbvCapacity || !Atlas::Add(filename, pBuffer)) {
			return -1;
		}
		pBuffer->CreateConstantBufferView(m_pDevice, GetCpuCbvSrvUavDescHandle(m_cbvCount));
		return m_cbvCount++;
	}

	//size_t AddShaderResource(const STRING_TYPE& filename, std::shared_ptr<Texture> pTexture) {
	//	if (m_srvUavCount == RtvCapacity) {
	//		return size_t(-1);
	//	}

	//	if (!Atlas::Add(filename, pTexture)) {
	//		pTexture = Atlas::Find(filename);
	//	}

	//	assert(pTexture->IsSrv() || pTexture->IsUav());
	//	if (pTexture->IsSrv()) {
	//		pTexture->CreateShaderResourceView(m_pDevice, GetCpuCbvSrvUavDescHandle(m_cbvCount + m_srvUavCount));
	//	}
	//	if (pTexture->IsUav()) {
	//		pTexture->CreateUnorderedAccessView(m_pDevice, GetCpuCbvSrvUavDescHandle(m_cbvCount + m_srvUavCount));
	//	}

	//	return m_srvUavCount++;
	//}

	size_t AddRenderTarget(const STRING_TYPE& filename, std::shared_ptr<Texture> pTexture) {
		if (m_rtvCount == RtvCapacity) {
			return size_t(-1);
		}

		if (!Atlas::Add(filename, pTexture)) {
			std::shared_ptr<GPUResource> pRes{ Atlas::Find(filename) };
			pTexture = std::static_pointer_cast<Texture>(pRes);
			if (!pTexture) {
				return size_t(-1);
			}
		}

		assert(pTexture->IsRtv());
		pTexture->CreateRenderTargetView(m_pDevice, GetCpuRtvDescHandle(m_rtvCount));

		return m_rtvCount++;
	}

	//bool Resize(const STRING_TYPE& filename, std::shared_ptr<Texture> pTexture, size_t* rtvId, size_t* srvUavId = nullptr) {
	//	assert(rtvId < RtvCapacity);
	//	assert(srvUavId < SrvUavCapacity);

	//	if (!Atlas::Replace(filename, pTexture)) {
	//		return false;
	//	}

	//	if (rtvId) {
	//		pTexture->CreateRenderTargetView(m_pDevice, GetCpuRtvDescHandle(*rtvId));
	//	}
	//	if (srvUavId) {
	//		if (pTexture->IsSrv()) {
	//			pTexture->CreateShaderResourceView(m_pDevice, GetCpuCbvSrvUavDescHandle(m_cbvCount + m_srvUavCount));
	//		}
	//		if (pTexture->IsUav()) {
	//			pTexture->CreateUnorderedAccessView(m_pDevice, GetCpuCbvSrvUavDescHandle(m_cbvCount + m_srvUavCount));
	//		}
	//	}

	//	return resId;
	//}

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuRtvDescHandle(size_t id = 0) const {
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(
			m_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart(),
			static_cast<INT>(id),
			m_rtvHandleIncSize
		);
	}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetCbvSrvUavDescHeap() const {
		return m_pCbvSrvUavDescHeap;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuCbvSrvUavDescHandle(size_t id = 0) const {
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(
			m_pCbvSrvUavDescHeap->GetCPUDescriptorHandleForHeapStart(),
			static_cast<INT>(id),
			m_cbvSrvUavHandleIncSize
		);
	}

private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescHeap(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_DESCRIPTOR_HEAP_TYPE& type,
		const size_t& texturesCount,
		const D3D12_DESCRIPTOR_HEAP_FLAGS& flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	) {
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescHeap{};
		D3D12_DESCRIPTOR_HEAP_DESC desc{ type, static_cast<UINT>(texturesCount), flags };
		ThrowIfFailed(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pDescHeap)));

		return pDescHeap;
	}
};