#pragma once

#include "Headers.h"

#include "MemoryMappedFile.h"

class PSOLibrary {
private:
	MemoryMappedFile m_file;

	Microsoft::WRL::ComPtr<ID3D12PipelineLibrary1> m_pPipelineLibrary{};
	bool m_Renewed;

public:
	PSOLibrary(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::wstring filename
	);

	~PSOLibrary();

	void Destroy(bool ClearPsoCache);
	void SaveCacheToFile();

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