#include "CommandList.h"

CommandList::CommandList(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
	uint8_t priority,
	std::function<void(void)> beforeExec,
	std::function<void(void)> afterExec
) : m_pCommandList(pCommandList)
	, m_priority(priority)
	, m_beforeExec(beforeExec)
	, m_afterExec(afterExec)
{}

inline uint16_t CommandList::GetPriority() const {
	return m_priority;
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
