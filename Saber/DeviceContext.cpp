#include "DeviceContext.h"

#include "Device.h"
#include "CommandQueue.h"
#include "DescriptorHeapManager.h"
#include "MaterialManager.h"
#include "PSOLibrary.h"

DeviceContext::DeviceContext(
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter
) {
	std::wstring adapterName{};
	if (DXGI_ADAPTER_DESC adapterDesc; SUCCEEDED(pAdapter->GetDesc(&adapterDesc))) {
		adapterName = adapterDesc.Description;
	}

	m_name = L"(" + adapterName + L")/DeviceContext";

	m_pDevice = std::make_shared<Device>(m_name + L"/Device", pAdapter);

#if defined(_DEBUG)
	SetInfoQueueFilter(m_pDevice->GetD3D12Device());
#endif
}

DeviceContext::~DeviceContext() {
	for (auto& pRingBuffer : m_pRingBuffers) {
		pRingBuffer.reset();
	}
	m_pFrameDataBuffer.reset();

	m_pRootSignatureAtlas.reset();
	m_pShaderAtlas.reset();
	m_pPSOLibrary.reset();

	m_pMeshAtlas.reset();

	m_pCommandQueueDirect.reset();
	m_pCommandQueueCompute.reset();
	m_pCommandQueueCopy.reset();

	m_pDescHeapManager.reset();
}

void DeviceContext::InitializeContext(
	const std::array<DescriptorHeapManager::DescHeapArgs, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>& descHeapArgs
) {
	m_pCommandQueueDirect = std::make_shared<CommandQueue>(m_name, m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
	m_pCommandQueueCompute = std::make_shared<CommandQueue>(m_name, m_pDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	m_pCommandQueueCopy = std::make_shared<CommandQueue>(m_name, m_pDevice, D3D12_COMMAND_LIST_TYPE_COPY);

	// todo: make CounterResetter part of DeviceContext
	//GPUResource::InitCounterResetter(
	//	m_pDevice,
	//	m_pAllocator,
	//	m_pCommandQueueCopy,
	//	m_pCommandQueueDirect
	//);

	m_pDescHeapManager = std::make_shared<DescriptorHeapManager>(
		m_name + L"/DescriptorHeapManager",
		m_pDevice,
		descHeapArgs
	);

	m_pRootSignatureAtlas = std::make_shared<Atlas<RootSignatureResource>>(L"");
	m_pShaderAtlas = std::make_shared<Atlas<ShaderResource>>(L"");
	m_pPSOLibrary = std::make_shared<PSOLibrary>(m_pDevice, L"PSOLibrary");

	m_pMeshAtlas = std::make_shared<Atlas<Mesh>>(L"");
	// TODO: divide name and path
	//m_pMaterialManager = std::make_shared<MaterialManager>(
	//	L"../../Resources/Textures/",
	//	m_pDevice,
	//	m_pDescHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
	//	1024
	//);

	const size_t RingBufferDefaultSize{ 1024 };
	for (size_t i{}; i < static_cast<size_t>(RingBufferType::Count); ++i) {
		m_pRingBuffers[i] = std::make_shared<DynamicUploadHeap>(
			m_pDevice,
			RingBufferDefaultSize,
			static_cast<RingBufferType>(i)
		);
	}

	m_pFrameDataBuffer = std::make_shared<FrameDataBuffer<std::shared_ptr<GPUResource>>>(3);
}

void DeviceContext::SetInfoQueueFilter(Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice) {
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
	ThrowIfFailed(pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue)));

	pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
	pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

	// Suppress whole categories of messages
	//D3D12_MESSAGE_CATEGORY Categories[]{};

	// Suppress messages based on their severity level
	D3D12_MESSAGE_SEVERITY Severities[]{
		D3D12_MESSAGE_SEVERITY_INFO,
	};

	// Suppress individual messages by their ID
	D3D12_MESSAGE_ID DenyIds[] = {
		D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
		D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
		D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.

		D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_HAS_NO_RESOURCE,            // For D3D12MA compatibility

		// TODO: make constant buffer's size at least D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT when GBV is enabled
		D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_INTERSECTS_MULTIPLE_BUFFERS,

		D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND,                     // Occurs when PSOLibrary tries to find unexisted PSO

		D3D12_MESSAGE_ID_GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE, // Occured by GBV when ExecuteIndirect is used

		static_cast<D3D12_MESSAGE_ID>(1422),                            // TODO: RENDER_TARGET_OR_DEPTH_STENCIL_RESOUCE_NOT_INITIALIZED
	};
	D3D12_INFO_QUEUE_FILTER NewFilter{
		.DenyList{
			//.NumCategories{ _countof(Categories) },
			//.pCategoryList{ Categories },
			.NumSeverities{ _countof(Severities) },
			.pSeverityList{ Severities },
			.NumIDs{ _countof(DenyIds) },
			.pIDList{ DenyIds }
		}
	};

	ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
}
