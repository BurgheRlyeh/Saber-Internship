#include "CommandQueue.h"

#include "CommandList.h"
#include "Device.h"
#include "IncrementFence.h"

CommandQueue::CommandQueue(
	const std::wstring& baseName,
	std::shared_ptr<Device> pDevice,
	D3D12_COMMAND_LIST_TYPE type
) : m_name(baseName + L"/CommandQueue" + std::to_wstring(type)) {
	m_pCommandQueue = CreateCommandQueue(pDevice, type);
	m_pCommandQueue->SetName(m_name.c_str());

	m_pIncFence = std::make_shared<IncrementFence>(m_name + L"/Fence", pDevice);
}

CommandQueue::~CommandQueue() {
	// Make sure the command queue has finished all commands before closing.
	Flush();
}

Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const {
	return m_pCommandQueue;
}

D3D12_COMMAND_LIST_TYPE CommandQueue::GetCommandListType() const {
	return m_pCommandQueue->GetDesc().Type;
}

std::shared_ptr<CommandList> CommandQueue::GetCommandList(
	std::shared_ptr<Device> pDevice
) {
	return std::make_shared<CommandList>(
		m_name + L"/CommandList" + std::to_wstring(m_pIncFence->GetValue()),
		GetD3D12CommandList(pDevice)
	);
}

std::shared_ptr<CommandList> CommandQueue::GetDeferredCommandList(
	const std::wstring& name,
	std::shared_ptr<Device> pDevice,
	size_t priority,
	std::function<void(void)> beforeExecuteTask,
	std::function<void(void)> afterExecuteTask
) {
	std::shared_ptr<CommandList> pCommandList{ std::make_shared<CommandList>(
		m_name + L"/CommandList/" + name,
		GetD3D12CommandList(pDevice),
		beforeExecuteTask,
		afterExecuteTask
	) };

	std::scoped_lock<std::mutex> setsLock(m_commandListsSetsMutex);
	if (m_commandListSets.size() <= priority) {
		m_commandListSets.resize(priority + 1);
	}
	if (!m_commandListSets.at(priority)) {
		m_commandListSets.at(priority) = std::make_unique<PrioritySet>();
	}
	std::scoped_lock<std::mutex> setLock(m_commandListSets.at(priority)->mutex);
	m_commandListSets.at(priority)->pCommandLists.insert(pCommandList);

	return pCommandList;
}

uint64_t CommandQueue::ExecuteCommandList(std::shared_ptr<CommandList> commandList) {
	uint64_t fenceValue;

	commandList->BeforeExecute();
	fenceValue = ExecuteD3D12CommandList(commandList->GetD3D12CommandList());
	commandList->AfterExecute();

	return fenceValue;
}

void CommandQueue::ExecuteCommandListImmediately(
	std::shared_ptr<CommandList> commandList
) {
	uint64_t fenceValue{ ExecuteCommandList(commandList) };
	CpuWait(fenceValue);
}

void CommandQueue::PushForExecution(std::shared_ptr<CommandList> pCommandList) {
	pCommandList->SetReadyForExection();
}

uint64_t CommandQueue::ExecutionTask(uint64_t waitFenceValue) {
	uint64_t lastFrameValue{};
	bool waitFence { true };
	for (std::unique_ptr<PrioritySet>& priorityVector : m_commandListSets) {
		if (!priorityVector) {
			continue;
		}
		std::unordered_multiset<std::shared_ptr<CommandList>>& pCommandLists{ priorityVector->pCommandLists };

		auto iter = pCommandLists.begin();
		while (!pCommandLists.empty()) {
			if (iter == pCommandLists.end()) {
				iter = pCommandLists.begin();
			}
			if (!(*iter)->IsReadyForExection()) {
				++iter;
				continue;
			}
			if (waitFence) {
				CpuWait(waitFenceValue);
				waitFence = false;
			}
			lastFrameValue = ExecuteCommandList(*iter);
			pCommandLists.erase(iter);
			iter = pCommandLists.begin();
		}
	}

	return lastFrameValue;
}

// Fence
void CommandQueue::Signal(std::shared_ptr<Fence>& pFence, uint64_t fenceValue) {
	::Signal(this, pFence.get(), fenceValue);
}
void CommandQueue::CpuWait(std::shared_ptr<Fence>& pFence, uint64_t fenceValue) {
	::CpuWait(pFence.get(), fenceValue);
}
void CommandQueue::GpuWait(std::shared_ptr<Fence>& pFence, uint64_t fenceValue) {
	::GpuWait(this, pFence.get(), fenceValue);
}
void CommandQueue::Flush(std::shared_ptr<Fence>& pFence, uint64_t fenceValue) {
	::Flush(this, pFence.get(), fenceValue);
}

// IncrementFence
uint64_t CommandQueue::Signal(std::shared_ptr<IncrementFence>& pFence) {
	return ::Signal(this, pFence.get());
}
void CommandQueue::CpuWait(std::shared_ptr<IncrementFence>& pFence, uint64_t fenceValue) {
	CpuWait(std::static_pointer_cast<Fence>(pFence), fenceValue);
}
void CommandQueue::GpuWait(std::shared_ptr<IncrementFence>& pFence, uint64_t fenceValue) {
	GpuWait(std::static_pointer_cast<Fence>(pFence), fenceValue);
}
void CommandQueue::Flush(std::shared_ptr<IncrementFence>& pFence) {
	::Flush(this, pFence.get());
}

// CommandQueue's fence
uint64_t CommandQueue::Signal() {
	return Signal(m_pIncFence);
}
void CommandQueue::CpuWait(uint64_t fenceValue) {
	CpuWait(m_pIncFence, fenceValue);
}
void CommandQueue::GpuWait(uint64_t fenceValue) {
	GpuWait(m_pIncFence, fenceValue);
}
void CommandQueue::Flush() {
	Flush(m_pIncFence);
}
bool CommandQueue::IsFenceComplete(uint64_t fenceValue) {
	return m_pIncFence->IsCompleted(fenceValue);
}

Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::CreateCommandQueue(
	std::shared_ptr<Device> pDevice,
	D3D12_COMMAND_LIST_TYPE type
) {
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> pCommandQueue{};

	D3D12_COMMAND_QUEUE_DESC desc{ .Type{ type } };
	ThrowIfFailed(pDevice->GetD3D12Device()->CreateCommandQueue(
		&desc,
		IID_PPV_ARGS(&pCommandQueue)
	));

	return pCommandQueue;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetD3D12CommandList(
	std::shared_ptr<Device> pDevice
) {
	CommandAllocatorEntry allocatorEntry;
	if (m_commandAllocatorQueue.Dequeue(allocatorEntry) && IsFenceComplete(allocatorEntry.fenceValue)) {
		allocatorEntry.pCommandAllocator = allocatorEntry.pCommandAllocator;
		ThrowIfFailed(allocatorEntry.pCommandAllocator->Reset());
	}
	else {
		allocatorEntry.pCommandAllocator = CreateCommandAllocator(pDevice);
	}

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pD3D12CommandList{};
	if (m_commandListQueue.Dequeue(pD3D12CommandList)) {
		ThrowIfFailed(pD3D12CommandList->Reset(allocatorEntry.pCommandAllocator.Get(), nullptr));
	}
	else {
		pD3D12CommandList = CreateCommandList(pDevice, allocatorEntry.pCommandAllocator);
	}

	// Associate the command allocator with the command list so that it can be
	// retrieved when the command list is executed.
	ThrowIfFailed(pD3D12CommandList->SetPrivateDataInterface(
		__uuidof(ID3D12CommandAllocator),
		allocatorEntry.pCommandAllocator.Get()
	));

	return pD3D12CommandList;
}

uint64_t CommandQueue::ExecuteD3D12CommandList(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pD3D12CommandList
) {
	ThrowIfFailed(pD3D12CommandList->Close());

	ID3D12CommandAllocator* pCommandAllocator{};
	uint32_t dataSize{ sizeof(pCommandAllocator) };
	ThrowIfFailed(pD3D12CommandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &pCommandAllocator));

	ID3D12CommandList* const pCommandLists[]{ pD3D12CommandList.Get() };
	m_pCommandQueue->ExecuteCommandLists(_countof(pCommandLists), pCommandLists);
	uint64_t fenceValue{ Signal() };

	CommandAllocatorEntry commandAllocEntry{ fenceValue, pCommandAllocator };
	if (!m_commandAllocatorQueue.Enqueue(commandAllocEntry)) {
		std::string commandQueueName{ m_name.begin(), m_name.end() };
		throw std::runtime_error("CommandAllocator Queue of " + commandQueueName + " is full");
	}

	if (!m_commandListQueue.Enqueue(pD3D12CommandList)) {
		std::string commandQueueName{ m_name.begin(), m_name.end() };
		throw std::runtime_error("CommandList Queue of CommandQueue with type " + commandQueueName + " is full");
	}

	// The ownership of the command allocator has been transferred to the ComPtr
	// in the command allocator queue. It is safe to release the reference 
	// in this temporary COM pointer here.
	pCommandAllocator->Release();

	return fenceValue;
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator(
	std::shared_ptr<Device> pDevice
) const {
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator{};

	ThrowIfFailed(pDevice->GetD3D12Device()->CreateCommandAllocator(
		GetCommandListType(),
		IID_PPV_ARGS(&pCommandAllocator)
	));

	return pCommandAllocator;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(
	std::shared_ptr<Device> pDevice,
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pAllocator
) const {
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList{};

	ThrowIfFailed(pDevice->GetD3D12Device()->CreateCommandList(
		0,
		GetCommandListType(),
		pAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&pCommandList)
	));

	return pCommandList;
}
