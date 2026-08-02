#include "CommandQueue.h"

#include "CommandAllocatorPool.h"
#include "CommandList.h"
#include "CommandListPool.h"
#include "Device.h"
#include "IncrementFence.h"

namespace {
	Microsoft::WRL::ComPtr<D3D12CommandQueue> CreateCommandQueue(
		std::shared_ptr<Device> pDevice,
		D3D12_COMMAND_LIST_TYPE type
	) {
		Microsoft::WRL::ComPtr<D3D12CommandQueue> pCommandQueue{};

		D3D12_COMMAND_QUEUE_DESC desc{
			.Type{ type }
		};
		ThrowIfFailed(pDevice->GetD3D12Device()->CreateCommandQueue(
			&desc,
			IID_PPV_ARGS(&pCommandQueue)
		));

		return pCommandQueue;
	}
}

CommandQueue::CommandQueue(
	const std::wstring& baseName,
	std::shared_ptr<Device> pDevice,
	CommandQueueType type
) : m_name(baseName + L"/CommandQueue" + ToName(type)) {
	m_pCommandQueue = CreateCommandQueue(pDevice, ToD3D12Type(type));
	m_pCommandQueue->SetName(m_name.c_str());

	m_pIncFence = std::make_shared<IncrementFence>(m_name + L"/Fence", pDevice);

	m_pAllocatorPool = std::make_unique<CommandAllocatorPool>(ToListType(type));
	m_pListPool = std::make_unique<CommandListPool>(ToListType(type));
}

CommandQueue::~CommandQueue() {
	// Make sure the command queue has finished all commands before closing.
	Flush();
}

Microsoft::WRL::ComPtr<D3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const {
	return m_pCommandQueue;
}

D3D12_COMMAND_LIST_TYPE CommandQueue::GetCommandListType() const {
	return m_pCommandQueue->GetDesc().Type;
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

Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> CommandQueue::GetD3D12CommandList(
	std::shared_ptr<Device> pDevice
) {
	return m_pListPool->Request(pDevice, m_pAllocatorPool->Request(pDevice, m_pIncFence));
}

uint64_t CommandQueue::ExecuteD3D12CommandList(
	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> pD3D12CommandList
) {
	ThrowIfFailed(pD3D12CommandList->Close());

	Microsoft::WRL::ComPtr<D3D12CommandAllocator> pCommandAllocator{};
	uint32_t dataSize{ sizeof(pCommandAllocator) };
	ThrowIfFailed(pD3D12CommandList->GetPrivateData(
		__uuidof(D3D12CommandAllocator),
		&dataSize,
		pCommandAllocator.GetAddressOf()
	));

	D3D12CommandList* const pCommandLists[]{ pD3D12CommandList.Get() };
	m_pCommandQueue->ExecuteCommandLists(_countof(pCommandLists), pCommandLists);
	uint64_t fenceValue{ Signal() };

	m_pAllocatorPool->Discard(pCommandAllocator, fenceValue);
	m_pListPool->Discard(pD3D12CommandList);

	return fenceValue;
}

