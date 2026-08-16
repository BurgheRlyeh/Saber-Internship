#include "ResourceStateTracker.h"

#include <sstream>
#include <string>

#include "CommandList.h"
#include "GPUResource.h"

namespace {
	// The states a command list of the given type is allowed to name in a barrier.
	// COMMON is zero, so it always passes the mask test in IsCompatibleState.
	constexpr D3D12_RESOURCE_STATES GetSupportedStates(CommandListType listType) {
		constexpr D3D12_RESOURCE_STATES copyStates{
			D3D12_RESOURCE_STATE_COPY_DEST | D3D12_RESOURCE_STATE_COPY_SOURCE
		};

		constexpr D3D12_RESOURCE_STATES computeStates{
			copyStates
				| D3D12_RESOURCE_STATE_UNORDERED_ACCESS
				| D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
				| D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
		};

		constexpr D3D12_RESOURCE_STATES videoDecodeStates{
			D3D12_RESOURCE_STATE_VIDEO_DECODE_READ | D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE
		};
		constexpr D3D12_RESOURCE_STATES videoProcessStates{
			D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ | D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE
		};
		constexpr D3D12_RESOURCE_STATES videoEncodeStates{
			D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ | D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE
		};
		constexpr D3D12_RESOURCE_STATES videoStates{
			videoDecodeStates | videoProcessStates | videoEncodeStates
		};

		switch (listType) {
		case CommandListType::Direct:
		case CommandListType::Bundle:
			return ~videoStates;
		case CommandListType::Compute:
			return computeStates;
		case CommandListType::Copy:
			return copyStates;
#ifdef ENABLE_VIDEO_COMMAND_LISTS
		case CommandListType::VideoDecode:
			return videoDecodeStates;
		case CommandListType::VideoProcess:
			return videoProcessStates;
		case CommandListType::VideoEncode:
			return videoEncodeStates;
#endif
		case CommandListType::Count:
		case CommandListType::None:
			assert(false);
			return D3D12_RESOURCE_STATE_COMMON;
		}
	}

	constexpr bool IsCompatibleState(CommandListType listType, D3D12_RESOURCE_STATES state) {
		return !(state & ~GetSupportedStates(listType));
	}

	constexpr bool IsCompatibleBeforeState(CommandListType listType, D3D12_RESOURCE_STATES state) {
		return listType == CommandListType::Copy
			? state == D3D12_RESOURCE_STATE_COMMON
			: IsCompatibleState(listType, state);
	}
}

ResourceStateTracker::ResourceStateMap ResourceStateTracker::s_globalResourceState{};
std::mutex ResourceStateTracker::s_globalMutex{};
bool ResourceStateTracker::s_isLocked{};

ResourceStateTracker::GlobalLock::GlobalLock() {
	s_globalMutex.lock();
	s_isLocked = true;
}

ResourceStateTracker::GlobalLock::~GlobalLock() {
	s_isLocked = false;
	s_globalMutex.unlock();
}

void ResourceStateTracker::ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier) {
	if (barrier.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
		assert(barrier.Type != D3D12_RESOURCE_BARRIER_TYPE_UAV
			|| IsCompatibleState(m_listType, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		m_resourceBarriers.push_back(barrier);
		return;
	}

	const D3D12_RESOURCE_TRANSITION_BARRIER& transition{ barrier.Transition };
	assert(IsCompatibleState(m_listType, transition.StateAfter));

	// TODO: add checks for:
	// "However, resources created on UPLOAD heaps must start in and cannot change from the GENERIC_READ state
	//  since only the CPU will be doing writing.
	//  Conversely, committed resources created in READBACK heaps must start in and cannot change from the COPY_DEST state."

	auto it{ m_finalResourceState.find(transition.pResource) };
	if (it == m_finalResourceState.end()) {
		// First use of the resource by this command list
		// Actual stateBefore is unknown, so defer the barrier until submit (FlushPendingResourceBarriers)
		m_pendingResourceBarriers.push_back(barrier);
	}
	else {
		// The resource has already been used by this command list, so the state it
		// is in at this point is known and StateBefore can be filled in right away
		const ResourceState& resourceState{ it->second };

		if (transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && resourceState.HasDifferentSubresourceState()) {
			// The subresources are in different states, transition them one by one
			for (const auto& [subresource, subresourceState] : resourceState.subresourceStates) {
				if (subresourceState != transition.StateAfter) {
					D3D12_RESOURCE_BARRIER subBarrier{ barrier };
					subBarrier.Transition.Subresource = subresource;
					subBarrier.Transition.StateBefore = subresourceState;
					m_resourceBarriers.push_back(subBarrier);
				}
			}
		}
		else if (auto finalState = resourceState.GetSubresourceState(transition.Subresource); transition.StateAfter != finalState) {
			D3D12_RESOURCE_BARRIER newBarrier{ barrier };
			newBarrier.Transition.StateBefore = finalState;
			m_resourceBarriers.push_back(newBarrier);
		}
	}

	m_finalResourceState[transition.pResource].SetSubresourceState(
		transition.Subresource,
		transition.StateAfter
	);
}

void ResourceStateTracker::TransitionResource(
	const GPUResource& resource,
	D3D12_RESOURCE_STATES stateAfter,
	UINT subresource
) {
	// StateBefore is a placeholder here,
	// real one filled in ResourceBarrier or by FlushPendingResourceBarriers
	ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(
		resource.GetD3D12Resource().Get(),
		D3D12_RESOURCE_STATE_COMMON,
		stateAfter,
		subresource
	));
}

void ResourceStateTracker::TransitionResource(
	const std::shared_ptr<GPUResource>& pResource,
	D3D12_RESOURCE_STATES stateAfter,
	UINT subresource
) {
	assert(pResource);

	// StateBefore is a placeholder here,
	// real one filled in ResourceBarrier or by FlushPendingResourceBarriers
	ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(
		pResource->GetD3D12Resource().Get(),
		D3D12_RESOURCE_STATE_COMMON,
		stateAfter,
		subresource
	));
}

void ResourceStateTracker::UavBarrier(const std::shared_ptr<GPUResource>& pResource) {
	ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(
		pResource ? pResource->GetD3D12Resource().Get() : nullptr
	));
}

void ResourceStateTracker::AliasBarrier(
	const std::shared_ptr<GPUResource>& pResourceBefore,
	const std::shared_ptr<GPUResource>& pResourceAfter
) {
	ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(
		pResourceBefore ? pResourceBefore->GetD3D12Resource().Get() : nullptr,
		pResourceAfter ? pResourceAfter->GetD3D12Resource().Get() : nullptr
	));
}

void ResourceStateTracker::FlushResourceBarriers(
	const std::shared_ptr<CommandList>& pCommandList
) {
	assert(pCommandList);

	if (m_resourceBarriers.empty()) {
		return;
	}

	pCommandList->GetD3D12CommandList()->ResourceBarrier(
		static_cast<UINT>(m_resourceBarriers.size()),
		m_resourceBarriers.data()
	);
	m_resourceBarriers.clear();
}

uint32_t ResourceStateTracker::FlushPendingResourceBarriers(
	const Microsoft::WRL::ComPtr<D3D12GraphicsCommandList>& pD3D12CommandList
) {
	assert(s_isLocked);
	assert(pD3D12CommandList);
	assert(pD3D12CommandList->GetType() == ToD3D12Type(m_listType));

	ResourceBarriers resolvedBarriers{};
	resolvedBarriers.reserve(m_pendingResourceBarriers.size());

	for (D3D12_RESOURCE_BARRIER& pendingBarrier : m_pendingResourceBarriers) {
		// Only transition barriers are deferred
		assert(pendingBarrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);

		const D3D12_RESOURCE_TRANSITION_BARRIER& transition{ pendingBarrier.Transition };

		auto it{ s_globalResourceState.find(transition.pResource) };

		// The resource was never passed to AddGlobalResourceState,
		// or it was already removed by RemoveGlobalResourceState
		assert(it != s_globalResourceState.end());

		const ResourceState& resourceState{ it->second };
		assert(IsCompatibleBeforeState(m_listType, resourceState.GetSubresourceState(transition.Subresource)));

		// TODO: maybe don't transitions on COPY at all, as this is allowed by docs:
		// "The COMMON state can be used for all usages on a Copy queue using the implicit state transitions"
		//if (m_listType == CommandListType::Copy) {
		//	continue;	
		//}

		if (transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && resourceState.HasDifferentSubresourceState()) {
			// The subresources are in different states, transition them one by one
			for (const auto& [subresource, subresourceState] : resourceState.subresourceStates) {
				if (transition.StateAfter == subresourceState) {
					D3D12_RESOURCE_BARRIER newBarrier{ pendingBarrier };
					newBarrier.Transition.Subresource = subresource;
					newBarrier.Transition.StateBefore = subresourceState;
					resolvedBarriers.push_back(newBarrier);
				}
			}
		}
		else if (auto globalState = resourceState.GetSubresourceState(transition.Subresource); transition.StateAfter != globalState) {
			pendingBarrier.Transition.StateBefore = globalState;
			resolvedBarriers.push_back(pendingBarrier);
		}
	}

	UINT numBarriers{ static_cast<UINT>(resolvedBarriers.size()) };
	if (numBarriers) {
		pD3D12CommandList->ResourceBarrier(numBarriers, resolvedBarriers.data());
	}

	m_pendingResourceBarriers.clear();

	return numBarriers;
}

void ResourceStateTracker::CommitFinalResourceStates() {
	assert(s_isLocked);

	for (const auto& [pResource, resourceState] : m_finalResourceState) {
		// The following resources will decay when an ExecuteCommandLists operation is completed on the GPU:
		// 1. Resources being accessed on a Copy queue, or
		// 2. Buffer resources on any queue type, or
		// 3. Texture resources on any queue type that have the D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS flag set, or
		// 4. Any resource implicitly promoted to a read - only state.

		// TODO: add support for points 2-4?
		bool decayToCommon{ m_listType == CommandListType::Copy };

		s_globalResourceState[pResource] = decayToCommon
			? ResourceState{ D3D12_RESOURCE_STATE_COMMON }
			: resourceState;
	}

	m_finalResourceState.clear();
}

void ResourceStateTracker::Reset() {
	m_resourceBarriers.clear();
	m_pendingResourceBarriers.clear();
	m_finalResourceState.clear();
}

void ResourceStateTracker::AddGlobalResourceState(
	const GPUResource& resource,
	D3D12_RESOURCE_STATES state
) {
	std::scoped_lock<std::mutex> lock(s_globalMutex);
	s_globalResourceState[resource.GetD3D12Resource().Get()].SetSubresourceState(
		D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		state
	);
}

void ResourceStateTracker::RemoveGlobalResourceState(const GPUResource& resource) {
	std::scoped_lock<std::mutex> lock(s_globalMutex);
	s_globalResourceState.erase(resource.GetD3D12Resource().Get());
}
