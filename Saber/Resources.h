#pragma once

#include "Headers.h"

#include "Device.h"

struct ShaderResource {
	Microsoft::WRL::ComPtr<D3DBlob> pShaderBlob{};

	ShaderResource(const std::wstring& filename) {
		ThrowIfFailed(D3DReadFileToBlob(filename.c_str(), &pShaderBlob));
	}
};

struct RootSignatureResource {
	Microsoft::WRL::ComPtr<D3D12RootSignature> pRootSignature{};

	RootSignatureResource(
		const std::wstring& filename,
		std::shared_ptr<Device> pDevice,
		Microsoft::WRL::ComPtr<D3DBlob> pRootSignatureBlob
	) {
		ThrowIfFailed(pDevice->GetD3D12Device()->CreateRootSignature(
			0,
			pRootSignatureBlob->GetBufferPointer(),
			pRootSignatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&pRootSignature)
		));
		pRootSignature->SetName(filename.c_str());
	}
};
