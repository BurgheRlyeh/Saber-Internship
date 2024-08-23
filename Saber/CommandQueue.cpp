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
std::shared_ptr<CommandList> CommandQueue::GetCommandList(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	bool isDeffered,
	uint8_t priority,
	std::function<void(void)> beforeExecuteTask,
	std::function<void(void)> afterExecuteTask
) {
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator{};
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pD3D12CommandList{};

	std::unique_lock commandAllocatorQueueLock{ m_commandAllocatorQueueMutex };
	if (!m_commandAllocatorQueue.empty() && IsFenceComplete(m_commandAllocatorQueue.front().fenceValue)) {
		pCommandAllocator = m_commandAllocatorQueue.front().pCommandAllocator;
		m_commandAllocatorQueue.pop();
		commandAllocatorQueueLock.unlock();

		ThrowIfFailed(pCommandAllocator->Reset());
	}
	else {
		commandAllocatorQueueLock.unlock();
		pCommandAllocator = CreateCommandAllocator(pDevice);
	}

	std::unique_lock commandListQueueLock{ m_commandListQueueMutex };
	if (!m_commandListQueue.empty()) {
		pD3D12CommandList = m_commandListQueue.front();
		m_commandListQueue.pop();
		commandListQueueLock.unlock();

		ThrowIfFailed(pD3D12CommandList->Reset(pCommandAllocator.Get(), nullptr));
	}
	else {
		commandListQueueLock.unlock();
		pD3D12CommandList = CreateCommandList(pDevice, pCommandAllocator);
	}

	// Associate the command allocator with the command list so that it can be
	// retrieved when the command list is executed.
	ThrowIfFailed(pD3D12CommandList->SetPrivateDataInterface(
		__uuidof(ID3D12CommandAllocator),
		pCommandAllocator.Get()
	));

	std::shared_ptr<CommandList> pCommandList{ std::make_shared<CommandList>(
		pD3D12CommandList,
		priority,
		beforeExecuteTask,
		afterExecuteTask
	) };

	if (isDeffered) {
		std::scoped_lock<std::mutex> setsLock(m_commandListsSetsMutex);
		if (m_commandListSets.size() <= priority) {
			m_commandListSets.resize(priority + 1);
		}
		if (!m_commandListSets.at(priority)) {
			m_commandListSets.at(priority) = std::make_unique<PrioritySet>();
		}
		std::scoped_lock<std::mutex> setLock(m_commandListSets.at(priority)->mutex);
		m_commandListSets.at(priority)->pCommandLists.insert(pCommandList);
	}

	return pCommandList;
}

// Execute a command list.
// Returns the fence value to wait for for this command list.
uint64_t CommandQueue::ExecuteCommandList(std::shared_ptr<CommandList> commandList) {
	ThrowIfFailed(commandList->m_pCommandList->Close());
	
	ID3D12CommandAllocator* pCommandAllocator{};
	uint32_t dataSize{ sizeof(pCommandAllocator) };
	ThrowIfFailed(commandList->m_pCommandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &pCommandAllocator));

	ID3D12CommandList* const pCommandLists[]{ commandList->m_pCommandList.Get() };
	commandList->BeforeExecute();
	m_pCommandQueue->ExecuteCommandLists(_countof(pCommandLists), pCommandLists);
	commandList->AfterExecute();
	uint64_t fenceValue{ Signal() };

	std::unique_lock allocatorListQueueLock{ m_commandAllocatorQueueMutex };
	m_commandAllocatorQueue.emplace(CommandAllocatorEntry{ fenceValue, pCommandAllocator });
	allocatorListQueueLock.unlock();

	std::unique_lock commandListQueueLock{ m_commandListQueueMutex };
	m_commandListQueue.push(commandList->m_pCommandList);
	commandListQueueLock.unlock();

	// The ownership of the command allocator has been transferred to the ComPtr
	// in the command allocator queue. It is safe to release the reference 
	// in this temporary COM pointer here.
	pCommandAllocator->Release();

	return fenceValue;
}

void CommandQueue::ExecuteCommandListImmediately(
	std::shared_ptr<CommandList> commandList
) {
	uint64_t fenceValue{ ExecuteCommandList(commandList) };
	WaitForFenceValue(fenceValue);
}

void CommandQueue::PushForExecution(std::shared_ptr<CommandList> pCommandList) {
	pCommandList->SetReadyForExection();
}

uint64_t CommandQueue::ExecutionTask() {
	uint64_t lastFrameValue{};

	for (std::unique_ptr<PrioritySet>& priorityVector : m_commandListSets) {
		std::unordered_multiset<std::shared_ptr<CommandList>>& pCommandLists{ priorityVector->pCommandLists };

		auto iter = pCommandLists.begin();
		while (!pCommandLists.empty()) {
			if (iter == pCommandLists.end()) {
				iter = pCommandLists.begin();
			}
			if ((*iter)->IsReadyForExection()) {
				lastFrameValue = ExecuteCommandList(*iter);
				pCommandLists.erase(iter);
				iter = pCommandLists.begin();
			}
			else {
				++iter;
			}
		}
	}

	return lastFrameValue;
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
