#include "CommandList.h"

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

bool CommandList::IsReadyForExection() const {
	return m_isReadyForExecution.load();
}

void CommandList::SetReadyForExection() {
	m_isReadyForExecution.store(true);
}

void CommandList::BeforeExecute() const {
	m_beforeExec();
}

void CommandList::AfterExecute() const {
	m_afterExec();
}
