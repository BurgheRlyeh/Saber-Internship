#pragma once

#include "Headers.h"

#include "MemoryMappedFile.h"

class PSOLibrary {
	MemoryMappedFile m_file;

	Microsoft::WRL::ComPtr<ID3D12PipelineLibrary1> m_pPipelineLibrary{};

public:
	PSOLibrary(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::wstring filename
	);

	~PSOLibrary();

	Microsoft::WRL::ComPtr<ID3D12PipelineState> Find(
		LPCWSTR filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);

	bool Add(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		LPCWSTR filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);

	Microsoft::WRL::ComPtr<ID3D12PipelineState> Assign(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		LPCWSTR filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);
};