#pragma once

#include "Headers.h"

#include "CommandListTypes.h"
#include "ResourceStateTracker.h"

#include <functional>
#include <memory>

class CommandListManager;
class CommandQueue;
class GPUResource;

class CommandList : public std::enable_shared_from_this<CommandList> {
	std::wstring m_name{};

	CommandListManager& m_manager;

	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> m_pD3D12CommandList{};

	// Resource states are tracked per command list, which is what makes recording
	// several command lists in parallel possible, see ResourceStateTracker
	ResourceStateTracker m_stateTracker;

	size_t m_priority{};

	std::vector<std::function<void()>> m_beforeTasks{};
	std::vector<std::function<void()>> m_afterTasks{};

	uint8_t m_pixEventsBegan{};

public:
	CommandList(
		const std::wstring& name,
		CommandListManager& manager,
		Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> pCommandList,
		size_t priority = 0,
		std::function<void()> beforeExec = []{},
		std::function<void()> afterExec = []{}
	);

	const std::wstring& GetName() const { return m_name; }
	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> GetD3D12CommandList() const;
	CommandListType GetType() const;
	size_t GetPriority() const { return m_priority; }

	std::shared_ptr<CommandQueue> GetQueue();

	void AddBeforeTask(std::function<void()> task) {
		m_beforeTasks.push_back(task);
	}

	void AddAfterTask(std::function<void()> task) {
		m_afterTasks.push_back(task);
	}

	// Resource barriers.
	// TODO: do smth with with ref/ptr overrides
	// Also do smth with uav and alias, as it cannot be done with reference
	void TransitionBarrier(
		const GPUResource& resource,
		D3D12_RESOURCE_STATES stateAfter,
		UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		bool flushBarriers = true
	);
	void TransitionBarrier(
		const std::shared_ptr<GPUResource>& pResource,
		D3D12_RESOURCE_STATES stateAfter,
		UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		bool flushBarriers = true
	);
	void UavBarrier(
		const std::shared_ptr<GPUResource>& pResource = nullptr,
		bool flushBarriers = true
	);
	void AliasBarrier(
		const std::shared_ptr<GPUResource>& pResourceBefore = nullptr,
		const std::shared_ptr<GPUResource>& pResourceAfter = nullptr,
		bool flushBarriers = true
	);

	void FlushResourceBarriers();
	uint32_t FlushPendingResourceBarriers(
		const Microsoft::WRL::ComPtr<D3D12GraphicsCommandList>& pPendingD3D12CommandList
	);
	void CommitFinalResourceStates();

	void Close();

	uint64_t Execute();
	void ExecuteImmediately();
	void PushForExecution();

	void BeforeExecute();
	void AfterExecute();

	void PixBeginEvent(const std::wstring& name, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);
	void PixEndEvent();
	void PixEndAllEvents();
};
