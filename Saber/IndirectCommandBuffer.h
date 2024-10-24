#ifndef _INDIRECT_COMMAND_BUFFER
#define _INDIRECT_COMMAND_BUFFER
// better protection than pragma once
#include "Headers.h"

#include "GPUResource.h"
// From Microsoft's IndirectDraw example
// 
    // We pack the UAV counter into the same buffer as the commands rather than create
    // a separate 64K resource/heap for it. The counter must be aligned on 4K boundaries,
    // so we pad the command buffer (if necessary) such that the counter will be placed
    // at a valid location in the buffer.
static inline UINT AlignForUavCounter(UINT bufferSize)
{
    const UINT alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
    return (bufferSize + (alignment - 1)) & ~(alignment - 1);
}

template<typename CommandType>
class IndirectCommandBuffer : public GPUResource {
private:



public:
	IndirectCommandBuffer(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const HeapData& heapData,
		const ResourceData& resData,
		unsigned int objectsCount,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	) : GPUResource(pAllocator, heapData, resData, allocationFlags), {}


#endif
