#include "CommandList.h"

#include "pix3.h"

CommandList::CommandList(
	const std::wstring& name,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
	std::function<void()> beforeExec,
	std::function<void()> afterExec
) : m_name(name),
	m_pD3D12CommandList(pCommandList),
	m_beforeExec(beforeExec),
	m_afterExec(afterExec) {
	m_pD3D12CommandList->SetName(name.c_str());
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandList::GetD3D12CommandList() const {
	return m_pD3D12CommandList;
}

D3D12_COMMAND_LIST_TYPE CommandList::GetType() const {
	return m_pD3D12CommandList->GetType();
}

bool CommandList::IsReadyForExecution() const {
	return m_isReadyForExecution.load();
}

void CommandList::SetReadyForExecution() {
	while (m_pixEventsBegan) {
		PixEndEvent();
	}
	m_isReadyForExecution.store(true);
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
