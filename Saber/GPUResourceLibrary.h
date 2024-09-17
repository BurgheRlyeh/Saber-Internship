#pragma once

#include "Headers.h"

#include "Atlas.h"
#include "ConstantBuffer.h"
#include "DDSTexture.h"
#include "Texture.h"

template<
	size_t CbvCapacity = 0,
	size_t SrvUavCapacity = 4 + 4,
	size_t RtvCapacity = 3,
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
	size_t m_srvUavCount{};

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
			CbvCapacity + SrvUavCapacity,
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

	size_t AddShaderResource(const STRING_TYPE& filename, std::shared_ptr<Texture> pTexture) {
		if (m_srvUavCount == RtvCapacity) {
			return size_t(-1);
		}

		if (!Atlas::Add(filename, pTexture)) {
			pTexture = Atlas::Find(filename);
		}

		assert(pTexture->IsSrv() || pTexture->IsUav());
		if (pTexture->IsSrv()) {
			pTexture->CreateShaderResourceView(m_pDevice, GetCpuCbvSrvUavDescHandle(m_cbvCount + m_srvUavCount));
		}
		if (pTexture->IsUav()) {
			pTexture->CreateUnorderedAccessView(m_pDevice, GetCpuCbvSrvUavDescHandle(m_cbvCount + m_srvUavCount));
		}

		return m_srvUavCount++;
	}

	size_t AddRenderTarget(const STRING_TYPE& filename, std::shared_ptr<Texture> pTexture) {
		if (m_rtvCount == RtvCapacity) {
			return size_t(-1);
		}

		if (!Atlas::Add(filename, pTexture)) {
			pTexture = Atlas::Find(filename);
		}

		assert(pTexture->IsRtv());
		pTexture->CreateRenderTargetView(m_pDevice, GetCpuRtvDescHandle(m_rtvCount));

		return m_rtvCount++;
	}

	bool Resize(const STRING_TYPE& filename, std::shared_ptr<Texture> pTexture, size_t* rtvId, size_t* srvUavId = nullptr) {
		assert(rtvId < RtvCapacity);
		assert(srvUavId < SrvUavCapacity);

		if (!Atlas::Replace(filename, pTexture)) {
			return false;
		}

		if (rtvId) {
			pTexture->CreateRenderTargetView(m_pDevice, GetCpuRtvDescHandle(*rtvId));
		}
		if (srvUavId) {
			if (pTexture->IsSrv()) {
				pTexture->CreateShaderResourceView(m_pDevice, GetCpuCbvSrvUavDescHandle(m_cbvCount + m_srvUavCount));
			}
			if (pTexture->IsUav()) {
				pTexture->CreateUnorderedAccessView(m_pDevice, GetCpuCbvSrvUavDescHandle(m_cbvCount + m_srvUavCount));
			}
		}

		return resId;
	}

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