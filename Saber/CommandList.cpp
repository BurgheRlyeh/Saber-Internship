#include "CommandList.h"

#include "pix3.h"

#include "CommandListManager.h"
#include "CommandQueue.h"
#include "GPUResource.h"

CommandList::CommandList(
	const std::wstring& name,
	CommandListManager& manager,
	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> pCommandList,
	size_t priority,
	std::function<void()> beforeExec,
	std::function<void()> afterExec
) : m_name(name),
	m_manager(manager),
	m_pD3D12CommandList(pCommandList),
	m_stateTracker(ToListType(pCommandList->GetType())),
	m_priority(priority)
{
	m_pD3D12CommandList->SetName(name.c_str());

	AddBeforeTask(beforeExec);
	AddAfterTask(afterExec);
}

Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> CommandList::GetD3D12CommandList() const {
	return m_pD3D12CommandList;
}

CommandListType CommandList::GetType() const {
	return ToListType(m_pD3D12CommandList->GetType());
}

std::shared_ptr<CommandQueue> CommandList::GetQueue() {
	return m_manager.GetCommandQueue(ToQueueType(GetType()));
}

void CommandList::TransitionBarrier(
	const GPUResource& resource,
	D3D12_RESOURCE_STATES stateAfter,
	UINT subresource,
	bool flushBarriers
) {
	m_stateTracker.TransitionResource(resource, stateAfter, subresource);

	if (flushBarriers) {
		FlushResourceBarriers();
	}
}

void CommandList::TransitionBarrier(
	const std::shared_ptr<GPUResource>& pResource,
	D3D12_RESOURCE_STATES stateAfter,
	UINT subresource,
	bool flushBarriers
) {
	m_stateTracker.TransitionResource(pResource, stateAfter, subresource);

	if (flushBarriers) {
		FlushResourceBarriers();
	}
}

void CommandList::UavBarrier(
	const std::shared_ptr<GPUResource>& pResource,
	bool flushBarriers
) {
	m_stateTracker.UavBarrier(pResource);

	if (flushBarriers) {
		FlushResourceBarriers();
	}
}

void CommandList::AliasBarrier(
	const std::shared_ptr<GPUResource>& pResourceBefore,
	const std::shared_ptr<GPUResource>& pResourceAfter,
	bool flushBarriers
) {
	m_stateTracker.AliasBarrier(pResourceBefore, pResourceAfter);

	if (flushBarriers) {
		FlushResourceBarriers();
	}
}

void CommandList::FlushResourceBarriers() {
	m_stateTracker.FlushResourceBarriers(shared_from_this());
}

uint32_t CommandList::FlushPendingResourceBarriers(
	const Microsoft::WRL::ComPtr<D3D12GraphicsCommandList>& pPendingD3D12CommandList
) {
	assert(pPendingD3D12CommandList);
	assert(pPendingD3D12CommandList != m_pD3D12CommandList);
	return m_stateTracker.FlushPendingResourceBarriers(pPendingD3D12CommandList);
}

void CommandList::CommitFinalResourceStates() {
	m_stateTracker.CommitFinalResourceStates();
}

void CommandList::Close() {
	PixEndAllEvents();
	FlushResourceBarriers();

	ThrowIfFailed(m_pD3D12CommandList->Close());
}

uint64_t CommandList::Execute() {
	return m_manager.ExecuteCommandList(shared_from_this());
}

void CommandList::ExecuteImmediately() {
	m_manager.ExecuteCommandListImmediately(shared_from_this());
}

void CommandList::PushForExecution() {
	m_manager.PushForExecution(shared_from_this());
}

void CommandList::BeforeExecute() {
	for (auto& task : m_beforeTasks) {
		task();
	}
	m_beforeTasks.clear();
}

void CommandList::AfterExecute() {
	for (auto& task : m_afterTasks) {
		task();
	}
	m_afterTasks.clear();
}

void CommandList::PixBeginEvent(const std::wstring& name, uint8_t r, uint8_t g, uint8_t b) {
	assert(m_pixEventsBegan < std::numeric_limits<uint8_t>::max());
	++m_pixEventsBegan;
	PIXBeginEvent(GetD3D12CommandList().Get(), PIX_COLOR(r, g, b), name.c_str());
}

void CommandList::PixEndEvent() {
	assert(m_pixEventsBegan > 0);
	--m_pixEventsBegan;
	PIXEndEvent(GetD3D12CommandList().Get());
}

void CommandList::PixEndAllEvents() {
	while (m_pixEventsBegan) {
		PixEndEvent();
	}
}
