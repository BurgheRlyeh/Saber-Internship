#include "CommandList.h"

#include "pix3.h"

#include "CommandListManager.h"

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
	m_priority(priority),
	m_beforeExec(beforeExec),
	m_afterExec(afterExec) {
	m_pD3D12CommandList->SetName(name.c_str());
}

Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> CommandList::GetD3D12CommandList() const {
	return m_pD3D12CommandList;
}

CommandListType CommandList::GetType() const {
	return ToListType(m_pD3D12CommandList->GetType());
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

void CommandList::BeforeExecute() const {
	m_beforeExec();
}

void CommandList::AfterExecute() const {
	m_afterExec();
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
