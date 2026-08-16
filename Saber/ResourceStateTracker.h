#pragma once

#include "Headers.h"

#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "CommandListTypes.h"

class CommandList;
class GPUResource;

// Based on:
// https://www.3dgep.com/learning-directx-12-3/#Resource_State_Tracking

// Also read official documentation:
// https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#split-barriers

class ResourceStateTracker {
	// State of a resource and subresources that differ from it
	struct ResourceState {
		// Describes every subresource while subresourceStates is empty
		D3D12_RESOURCE_STATES state{ D3D12_RESOURCE_STATE_COMMON };
		std::map<UINT, D3D12_RESOURCE_STATES> subresourceStates{};

		explicit ResourceState(
			D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON
		) : state(initState) {}

		bool HasDifferentSubresourceState() const {
			return !subresourceStates.empty();
		}

		void SetSubresourceState(UINT subresource, D3D12_RESOURCE_STATES newState) {
			if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
				state = newState;
				subresourceStates.clear();
			}
			else {
				subresourceStates[subresource] = newState;
			}
		}

		D3D12_RESOURCE_STATES GetSubresourceState(UINT subresource) const {
			auto it{ subresourceStates.find(subresource) };
			return it != subresourceStates.end() ? it->second : state;
		}
	};


	using ResourceBarriers = std::vector<D3D12_RESOURCE_BARRIER>;
	ResourceBarriers m_resourceBarriers{};			// known StateBefore
	ResourceBarriers m_pendingResourceBarriers{};	// StateBefore is resolved at submit

	using ResourceStateMap = std::unordered_map<D3D12Resource*, ResourceState>;
	ResourceStateMap m_finalResourceState{};		// last states

	static ResourceStateMap s_globalResourceState;	// global last states
	static std::mutex s_globalMutex;
	static bool s_isLocked;

	// The type of the command list this tracker belongs to. Copy lists are handled
	// differently throughout, see IsCopyList.
	CommandListType m_listType{ CommandListType::None };

public:
	explicit ResourceStateTracker(CommandListType listType) : m_listType(listType) {}
	~ResourceStateTracker() = default;

	ResourceStateTracker(const ResourceStateTracker&) = delete;
	ResourceStateTracker& operator=(const ResourceStateTracker&) = delete;

	ResourceStateTracker(ResourceStateTracker&&) = delete;
	ResourceStateTracker& operator=(ResourceStateTracker&&) = delete;

	class GlobalLock {
	public:
		GlobalLock();
		~GlobalLock();

		GlobalLock(const GlobalLock&) = delete;
		GlobalLock& operator=(const GlobalLock&) = delete;

		GlobalLock(GlobalLock&&) = delete;
		GlobalLock& operator=(GlobalLock&&) = delete;
	};

	// Push an arbitrary barrier to the tracker. Transition barriers are resolved
	// against the tracked state, everything else is passed through as is.
	void ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier);

	// Push a transition barrier. The state the resource is currently in is not
	// needed: it is either known to the tracker or resolved at submit time.
	void TransitionResource(
		const GPUResource& resource,
		D3D12_RESOURCE_STATES stateAfter,
		UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
	);
	void TransitionResource(
		const std::shared_ptr<GPUResource>& pResource,
		D3D12_RESOURCE_STATES stateAfter,
		UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
	);

	// Push a UAV barrier. A null resource means that any UAV access may need it.
	void UavBarrier(const std::shared_ptr<GPUResource>& pResource = nullptr);

	// Push an aliasing barrier. Either resource may be null, which means that any
	// placed or reserved resource may be causing the aliasing.
	void AliasBarrier(
		const std::shared_ptr<GPUResource>& pResourceBefore = nullptr,
		const std::shared_ptr<GPUResource>& pResourceAfter = nullptr
	);

	// Write the resolved barriers into the command list. Has to be called before
	// every draw/dispatch/copy that relies on them.
	void FlushResourceBarriers(const std::shared_ptr<CommandList>& pCommandList);

	bool HasPendingResourceBarriers() const {
		return !m_pendingResourceBarriers.empty();
	}

	// Resolve the pending barriers against the global state and write them into the
	// given command list, which is a SEPARATE, still open one, executed right
	// before the list this tracker belongs to. Requires GlobalLock to be held.
	// Returns the number of barriers written, always 0 for a copy list.
	//
	// Deliberately typed differently from FlushResourceBarriers: that one targets
	// the tracker's own list, this one must never do so. Taking the D3D12 list here
	// makes passing the owning CommandList by mistake a compile error.
	uint32_t FlushPendingResourceBarriers(
		const Microsoft::WRL::ComPtr<D3D12GraphicsCommandList>& pD3D12CommandList
	);

	// Commit the final states of this command list to the global state.
	// Has to be called once the command list is closed, but before it is executed.
	// Requires GlobalLock to be held.
	void CommitFinalResourceStates();

	// Drop all tracked state. Has to be called before the command list is reused.
	void Reset();

	// Register a resource with its initial state
	static void AddGlobalResourceState(const GPUResource& resource, D3D12_RESOURCE_STATES state);

	// Unregister a resource
	static void RemoveGlobalResourceState(const GPUResource& resource);
};
