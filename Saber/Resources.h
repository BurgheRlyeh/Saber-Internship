#pragma once

#include "Headers.h"

struct ShaderResource {
	Microsoft::WRL::ComPtr<ID3DBlob> pShaderBlob{};

	ShaderResource(const std::wstring& filename) {
		ThrowIfFailed(D3DReadFileToBlob(filename.c_str(), &pShaderBlob));
	}
};

struct RootSignatureResource {
	Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature{};

	RootSignatureResource(
		const std::wstring& filename,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob
	) {
		ThrowIfFailed(pDevice->CreateRootSignature(
			0,
			pRootSignatureBlob->GetBufferPointer(),
			pRootSignatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&pRootSignature)
		));
	}
};

struct PipelineStateResource {
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipelineState;

	struct PipelineStateResourceData {
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice{};
		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc{};
	};

	PipelineStateResource(
		const std::string& filename,
		const PipelineStateResourceData& data
	) {
		ThrowIfFailed(data.pDevice->CreatePipelineState(
			&data.pipelineStateStreamDesc,
			IID_PPV_ARGS(&pPipelineState)
		));
	}
};
