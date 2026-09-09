#pragma once

#include "EnumHelpers.h"

enum class RenderSubsystemType : uint32_t {
	Default = 0 << 0,
	Dynamic = 1 << 0,
	AlphaKill = 1 << 1,
	Count = 1 << 2,

	Invalid = static_cast<uint32_t>(-1)
};
ENABLE_ENUM_FLAGS(RenderSubsystemType);

using RenderObjectIdType = size_t;
inline constexpr RenderObjectIdType InvalidRenderObjectId{
	static_cast<RenderObjectIdType>(-1)
};

// Addresses an object inside the scene
struct RenderObjectHandle {
	EnumFlags<RenderSubsystemType> type{ RenderSubsystemType::Invalid };
	RenderObjectIdType id{ InvalidRenderObjectId };

	bool IsValid() const {
		return type != RenderSubsystemType::Invalid && id != InvalidRenderObjectId;
	}
};
