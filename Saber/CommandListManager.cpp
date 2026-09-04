#include "CommandListManager.h"

#include <stdexcept>

#include "CommandList.h"
#include "CommandQueue.h"
#include "Device.h"
#include "IncrementFence.h"
#include "Fence.h"

CommandListManager::CommandListManager(
    std::shared_ptr<Device> pDevice
) : m_pDevice(pDevice) {
	for (size_t i{}; i < static_cast<size_t>(CommandQueueType::Count); ++i) {
		CommandQueueType type{ FromId<CommandQueueType>(i) };
		m_pCommandQueues[i] = std::make_shared<CommandQueue>(
			L"CommandListManager",
            m_pDevice,
            type
		);
	}
}

CommandListManager::~CommandListManager() {
    Flush();
    for (auto& deferred : m_deferredLists) {
        std::scoped_lock<std::mutex> lock(deferred.mutex);
        assert(deferred.unreadyCount == 0);
    }
}

std::shared_ptr<CommandList> CommandListManager::GetCommandList(
    CommandListType type
) {
    return std::make_shared<CommandList>(
        GetCommandQueue(ToQueueType(type))->GetName() + L"/CommandList",
        *this,
        GetCommandQueue(ToQueueType(type))->GetD3D12CommandList(m_pDevice)
    );
}

std::shared_ptr<CommandList> CommandListManager::GetDeferredCommandList(
    const std::wstring& name, 
    CommandListType type,
    size_t priority,
    std::function<void()> beforeExecuteTask,
    std::function<void()> afterExecuteTask
) {
    assert(priority < MaxPriority);

    std::shared_ptr<CommandList> pCommandList{ std::make_shared<CommandList>(
        GetCommandQueue(ToQueueType(type))->GetName() + L"/CommandList/" + name,
        *this,
        GetCommandQueue(ToQueueType(type))->GetD3D12CommandList(m_pDevice),
        priority,
        beforeExecuteTask,
        afterExecuteTask
    ) };

    DeferredLists& deferred{ m_deferredLists[priority] };
    std::unique_lock<std::mutex> lock(deferred.mutex);
    ++deferred.unreadyCount;
    lock.unlock();

    return pCommandList;
}

uint64_t CommandListManager::ExecuteCommandList(std::shared_ptr<CommandList> commandList) {
    return GetCommandQueue(ToQueueType(commandList->GetType()))->ExecuteCommandList(commandList);
}

void CommandListManager::ExecuteCommandListImmediately(
    std::shared_ptr<CommandList> commandList
) {
    return GetCommandQueue(ToQueueType(commandList->GetType()))
        ->ExecuteCommandListImmediately(commandList);
}

void CommandListManager::PushForExecution(std::shared_ptr<CommandList> pCommandList) {
    DeferredLists& deferred{ m_deferredLists[pCommandList->GetPriority()] };
    std::unique_lock<std::mutex> readyLock(deferred.mutex);
    deferred.pCommandLists.push_back(pCommandList);
    --deferred.unreadyCount;
    readyLock.unlock();
    deferred.readyCv.notify_one();
}

void CommandListManager::ExecutionTask(FrameFenceValues& waitFenceValues) {
    std::array<bool, static_cast<size_t>(CommandQueueType::Count)> fenceWaited{};

    for (size_t p{}; p < MaxPriority;) {
        DeferredLists& deferred{ m_deferredLists[p] };
        std::unique_lock<std::mutex> lock(deferred.mutex);

        bool haveToWait{ deferred.unreadyCount != 0 };
        bool haveToExecute{ deferred.pCommandLists.size() != 0 };
        if (!haveToWait && !haveToExecute) {
            ++p;    // finished with this priority, move to the next
            continue;
        }

        if (haveToExecute) {
            std::shared_ptr<CommandList> pCommandList{ deferred.pCommandLists.back() };
            deferred.pCommandLists.pop_back();
            lock.unlock();

            size_t queueId{ ToId(ToQueueType(pCommandList->GetType())) };
            std::shared_ptr<CommandQueue>& pCommandQueue{ m_pCommandQueues[queueId] };

            if (!fenceWaited[queueId]) {
                pCommandQueue->CpuWait(waitFenceValues[queueId]);
                fenceWaited[queueId] = true;
            }

            waitFenceValues[queueId] = pCommandQueue->ExecuteCommandList(pCommandList);
            continue;
        }

        if (haveToWait && !deferred.readyCv.wait_for(lock, kReadyTimeout, [&deferred] {
            return deferred.pCommandLists.size() > 0;
        })) {
            throw std::runtime_error(
                "At least one command list with priority " + std::to_string(p) + " was never pushed for execution"
            );
        }
    }
}

void CommandListManager::ReleaseCompletedResources() {
    for (auto& pCommandQueue : m_pCommandQueues) {
        pCommandQueue->ReleaseCompletedResources();
    }
}

void CommandListManager::Flush() {
    for (auto pCommandQueue : m_pCommandQueues) {
        pCommandQueue->Flush();
    }
}
