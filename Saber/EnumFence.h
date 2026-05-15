/**
 * @file EnumFence.h
 * @brief Provides a type-safe fence wrapper whose value is expressed as an enum.
 */
#pragma once

#include "Fence.h"
#include "EnumHelpers.h"

/**
 * @brief A GPU fence whose value is typed as a user-defined enum.
 *
 * Wraps @ref Fence and exposes @c GetValue() returning @p Enum instead of
 * a raw @c uint64_t, preventing accidental misuse of raw numeric fence values.
 *
 * @tparam Enum An enum type whose underlying type is convertible to @c uint64_t.
 */
template <EnumConcept Enum>
class EnumFence : public Fence {
    using Fence::GetValue;

public:
    /**
     * @brief Constructs an EnumFence with a given initial value.
     * @param name           Debug name for the fence object.
     * @param pDevice        Device used to create the underlying D3D12 fence.
     * @param fenceInitValue Initial fence value expressed as @p Enum.
     */
    EnumFence(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        Enum fenceInitValue
    ) : Fence(name, pDevice, static_cast<uint64_t>(fenceInitValue)) {}

    /**
     * @brief Returns the current fence value typed as @p Enum.
     * @return Current fence value.
     */
    Enum GetValue() const {
        return static_cast<Enum>(Fence::GetValue());
    }
};
