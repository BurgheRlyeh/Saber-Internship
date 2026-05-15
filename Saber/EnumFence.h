#pragma once

#include "Fence.h"
#include "EnumHelpers.h"

template <EnumConcept Enum>
class EnumFence : public Fence {
    using Fence::GetValue;

public:
    EnumFence(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        Enum fenceInitValue
    ) : Fence(name, pDevice, static_cast<uint64_t>(fenceInitValue)) {}

    Enum GetValue() const {
        return static_cast<Enum>(Fence::GetValue());
    }
};
