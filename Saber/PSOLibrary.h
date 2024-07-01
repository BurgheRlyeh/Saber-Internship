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
	) { 
		m_file.Init(filename);
		
		ThrowIfFailed(pDevice->CreatePipelineLibrary(
			m_file.GetData(),
			m_file.GetSize(),
			IID_PPV_ARGS(&m_pPipelineLibrary)
		));
	}

	~PSOLibrary() {
		auto librarySize = m_pPipelineLibrary->GetSerializedSize();
		const size_t neededSize = sizeof(UINT) + librarySize;
		auto currentFileSize = m_file.GetSize();
		if(neededSize > currentFileSize)
		{
			void* pTempData = new BYTE[librarySize];
			if (pTempData)
			{
				ThrowIfFailed(m_pPipelineLibrary->Serialize(pTempData, librarySize));
				m_file.GrowMapping(librarySize);
				memcpy(m_file.GetData(), pTempData, librarySize);
				m_file.SetSize(librarySize);

				delete[] pTempData;
			}
		}
		else
		{
			m_pPipelineLibrary->Serialize(m_file.GetData(), librarySize);
		}

		
		m_file.Destroy(false);
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> Find(
		LPCWSTR filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	) {
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO{};
		HRESULT hr{ m_pPipelineLibrary->LoadGraphicsPipeline(
			filename,
			pPSODesc,
			IID_PPV_ARGS(&pPSO)
		) };
		return SUCCEEDED(hr) ? pPSO : nullptr;
	}

	bool Add(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		LPCWSTR filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	) {
		if (Find(filename, pPSODesc)) {
			return false;
		}

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO{};
		pDevice->CreateGraphicsPipelineState(pPSODesc, IID_PPV_ARGS(&pPSO));
		ThrowIfFailed(m_pPipelineLibrary->StorePipeline(filename, pPSO.Get()));

		return true;
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> Assign(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		LPCWSTR filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	) {
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO{ Find(filename, pPSODesc) };
		if (pPSO) {
			return pPSO;
		}

		pDevice->CreateGraphicsPipelineState(pPSODesc, IID_PPV_ARGS(&pPSO));
		ThrowIfFailed(m_pPipelineLibrary->StorePipeline(filename, pPSO.Get()));
		//m_pPipelineLibrary->
		return pPSO;
	}
};