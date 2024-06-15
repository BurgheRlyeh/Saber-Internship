#include "CommandQueue.h"

CommandQueue::CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, D3D12_COMMAND_LIST_TYPE type) 
	: m_commandListType(type)
{
	D3D12_COMMAND_QUEUE_DESC desc{
		.Type{ m_commandListType },
		.Priority{ D3D12_COMMAND_QUEUE_PRIORITY_NORMAL },
		.Flags{ D3D12_COMMAND_QUEUE_FLAG_NONE },
		.NodeMask{}
	};

	ThrowIfFailed(pDevice->CreateCommandQueue(
		&desc,
		IID_PPV_ARGS(&m_pCommandQueue)
	));
	ThrowIfFailed(pDevice->CreateFence(
		m_fenceValue,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_pFence)
	));

	m_fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_fenceEvent && "Failed to create fence event handle.");
}

CommandQueue::~CommandQueue() {
	// Make sure the command queue has finished all commands before closing.
	Flush();
	::CloseHandle(m_fenceEvent);
}

D3D12_COMMAND_LIST_TYPE CommandQueue::GetCommandListType() const {
	return m_commandListType;
}

// Get an available command list from the command queue.
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
) {
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator{};
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList{};

	if (!m_commandAllocatorQueue.empty() && IsFenceComplete(m_commandAllocatorQueue.front().fenceValue)) {
		pCommandAllocator = m_commandAllocatorQueue.front().pCommandAllocator;
		m_commandAllocatorQueue.pop();

		ThrowIfFailed(pCommandAllocator->Reset());
	}
	else {
		pCommandAllocator = CreateCommandAllocator(pDevice);
	}

	if (!m_commandListQueue.empty()) {
		pCommandList = m_commandListQueue.front();
		m_commandListQueue.pop();

		ThrowIfFailed(pCommandList->Reset(pCommandAllocator.Get(), nullptr));
	}
	else {
		pCommandList = CreateCommandList(pDevice, pCommandAllocator);
	}

	// Associate the command allocator with the command list so that it can be
	// retrieved when the command list is executed.
	ThrowIfFailed(pCommandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), pCommandAllocator.Get()));

	return pCommandList;
}

// Execute a command list.
// Returns the fence value to wait for for this command list.
uint64_t CommandQueue::ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList) {
	ThrowIfFailed(pCommandList->Close());

	ID3D12CommandAllocator* pCommandAllocator{};
	uint32_t dataSize{ sizeof(pCommandAllocator) };
	ThrowIfFailed(pCommandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &pCommandAllocator));

	ID3D12CommandList* const pCommandLists[]{ pCommandList.Get() };
	m_pCommandQueue->ExecuteCommandLists(_countof(pCommandLists), pCommandLists);
	uint64_t fenceValue{ Signal() };

	m_commandAllocatorQueue.emplace(CommandAllocatorEntry{ fenceValue, pCommandAllocator });
	m_commandListQueue.push(pCommandList);

	// The ownership of the command allocator has been transferred to the ComPtr
	// in the command allocator queue. It is safe to release the reference 
	// in this temporary COM pointer here.
	pCommandAllocator->Release();

	return fenceValue;
}

void CommandQueue::ExecuteCommandListImmediately(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList
) {
	uint64_t fenceValue{ ExecuteCommandList(pCommandList) };
	WaitForFenceValue(fenceValue);
}

uint64_t CommandQueue::Signal() {
	uint64_t fenceValue{ ++m_fenceValue };
	m_pCommandQueue->Signal(m_pFence.Get(), fenceValue);
	return fenceValue;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue) {
	return m_pFence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue) {
	if (!IsFenceComplete(fenceValue)) {
		ThrowIfFailed(m_pFence->SetEventOnCompletion(fenceValue, m_fenceEvent));
		::WaitForSingleObject(m_fenceEvent, DWORD_MAX);
	}
}

void CommandQueue::Flush() {
	WaitForFenceValue(Signal());
}

Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const {
	return m_pCommandQueue;
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
) {
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator{};

	ThrowIfFailed(pDevice->CreateCommandAllocator(
		m_commandListType,
		IID_PPV_ARGS(&pCommandAllocator)
	));

	return pCommandAllocator;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pAllocator
) {
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList{};

	ThrowIfFailed(pDevice->CreateCommandList(
		0,
		m_commandListType,
		pAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&pCommandList)
	));

	return pCommandList;
}
