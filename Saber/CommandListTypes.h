#pragma once

#include "Headers.h"

#include <array>

//#define ENABLE_VIDEO_COMMAND_LISTS

enum class CommandListType : uint8_t {
    Direct,
    Bundle,
    Compute,
    Copy,
#ifdef ENABLE_VIDEO_COMMAND_LISTS
    VideoDecode,
    VideoProcess,
    VideoEncode,
#endif
    Count,
    None = std::numeric_limits<std::underlying_type_t<CommandListType>>::max()
};

inline std::wstring ToName(CommandListType type) {
    switch (type) {
    case CommandListType::Direct:       return L"Direct";
    case CommandListType::Bundle:       return L"Bundle";
    case CommandListType::Compute:      return L"Compute";
    case CommandListType::Copy:         return L"Copy";
#ifdef ENABLE_VIDEO_COMMAND_LISTS
    case CommandListType::VideoDecode:  return L"VideoDecode";
    case CommandListType::VideoProcess: return L"VideoProcess";
    case CommandListType::VideoEncode:  return L"VideoEncode";
#endif
    case CommandListType::Count:        return L"Count";
    case CommandListType::None:         return L"None";

    default:                            return L"Unknown";
    }
}

enum class CommandQueueType : uint8_t {
    Direct,
    Compute,
    Copy,
#ifdef ENABLE_VIDEO_COMMAND_LISTS
    VideoDecode,
    VideoProcess,
    VideoEncode,
#endif
    Count,
    None = std::numeric_limits<std::underlying_type_t<CommandQueueType>>::max()
};

using FrameFenceValues = std::array<uint64_t, static_cast<size_t>(CommandQueueType::Count)>;

inline std::wstring ToName(CommandQueueType type) {
    switch (type) {
    case CommandQueueType::Direct:      return L"Direct";
    case CommandQueueType::Compute:     return L"Compute";
    case CommandQueueType::Copy:        return L"Copy";
#ifdef ENABLE_VIDEO_COMMAND_LISTS
    case CommandQueueType::VideoDecode:  return L"VideoDecode";
    case CommandQueueType::VideoProcess: return L"VideoProcess";
    case CommandQueueType::VideoEncode:  return L"VideoEncode";
#endif
    case CommandQueueType::Count:       return L"Count";
    case CommandQueueType::None:        return L"None";

    default:                            return L"Unknown";
    }
}

struct CommandListTypeInfo {
    CommandListType listType;
    CommandQueueType queueType;
    D3D12_COMMAND_LIST_TYPE d3d12Type;
};

constexpr CommandListType ToListType(CommandQueueType queueType) noexcept {
    switch (queueType) {
    case CommandQueueType::Direct:      return CommandListType::Direct;
    case CommandQueueType::Compute:     return CommandListType::Compute;
    case CommandQueueType::Copy:        return CommandListType::Copy;
#ifdef ENABLE_VIDEO_COMMAND_LISTS
    case CommandQueueType::VideoDecode: return CommandListType::VideoDecode;
    case CommandQueueType::VideoProcess:return CommandListType::VideoProcess;
    case CommandQueueType::VideoEncode: return CommandListType::VideoEncode;
#endif

    case CommandQueueType::Count:
    case CommandQueueType::None:
        break;
    }

    assert(false);
    return CommandListType::None;
}

constexpr CommandListType ToListType(D3D12_COMMAND_LIST_TYPE d3d12Type) noexcept {
    switch (d3d12Type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:        return CommandListType::Direct;
    case D3D12_COMMAND_LIST_TYPE_BUNDLE:        return CommandListType::Bundle;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:       return CommandListType::Compute;
    case D3D12_COMMAND_LIST_TYPE_COPY:          return CommandListType::Copy;
#ifdef ENABLE_VIDEO_COMMAND_LISTS
    case D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE:  return CommandListType::VideoDecode;
    case D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS: return CommandListType::VideoProcess;
    case D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE:  return CommandListType::VideoEncode;
#endif

    case D3D12_COMMAND_LIST_TYPE_NONE:
        break;
    }

    assert(false);
    return CommandListType::None;
}

constexpr CommandQueueType ToQueueType(CommandListType listType) noexcept {
    switch (listType) {
    case CommandListType::Direct:       return CommandQueueType::Direct;
    case CommandListType::Bundle:       return CommandQueueType::Direct;
    case CommandListType::Compute:      return CommandQueueType::Compute;
    case CommandListType::Copy:         return CommandQueueType::Copy;
#ifdef ENABLE_VIDEO_COMMAND_LISTS
    case CommandListType::VideoDecode:  return CommandQueueType::VideoDecode;
    case CommandListType::VideoProcess: return CommandQueueType::VideoProcess;
    case CommandListType::VideoEncode:  return CommandQueueType::VideoEncode;
#endif

    case CommandListType::Count:
    case CommandListType::None:
        break;
    }

    assert(false);
    return CommandQueueType::None;
}

constexpr CommandQueueType ToQueueType(D3D12_COMMAND_LIST_TYPE d3d12Type) noexcept {
    switch (d3d12Type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:        return CommandQueueType::Direct;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:       return CommandQueueType::Compute;
    case D3D12_COMMAND_LIST_TYPE_COPY:          return CommandQueueType::Copy;
#ifdef ENABLE_VIDEO_COMMAND_LISTS
    case D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE:  return CommandQueueType::VideoDecode;
    case D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS: return CommandQueueType::VideoProcess;
    case D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE:  return CommandQueueType::VideoEncode;
#endif

    case D3D12_COMMAND_LIST_TYPE_NONE:
        break;
    }

    assert(false);
    return CommandQueueType::None;
}

constexpr D3D12_COMMAND_LIST_TYPE ToD3D12Type(CommandListType listType) noexcept {
    switch (listType) {
    case CommandListType::Direct:       return D3D12_COMMAND_LIST_TYPE_DIRECT;
    case CommandListType::Bundle:       return D3D12_COMMAND_LIST_TYPE_BUNDLE;
    case CommandListType::Compute:      return D3D12_COMMAND_LIST_TYPE_COMPUTE;
    case CommandListType::Copy:         return D3D12_COMMAND_LIST_TYPE_COPY;
#ifdef ENABLE_VIDEO_COMMAND_LISTS
    case CommandListType::VideoDecode:  return D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE;
    case CommandListType::VideoProcess: return D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS;
    case CommandListType::VideoEncode:  return D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE;
#endif

    case CommandListType::Count:
    case CommandListType::None:
        break;
    }

    assert(false);
    return D3D12_COMMAND_LIST_TYPE_NONE;
}

constexpr D3D12_COMMAND_LIST_TYPE ToD3D12Type(CommandQueueType queueType) noexcept {
    switch (queueType) {
    case CommandQueueType::Direct:      return D3D12_COMMAND_LIST_TYPE_DIRECT;
    case CommandQueueType::Compute:     return D3D12_COMMAND_LIST_TYPE_COMPUTE;
    case CommandQueueType::Copy:        return D3D12_COMMAND_LIST_TYPE_COPY;
#ifdef ENABLE_VIDEO_COMMAND_LISTS
    case CommandQueueType::VideoDecode: return D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE;
    case CommandQueueType::VideoProcess:return D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS;
    case CommandQueueType::VideoEncode: return D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE;
#endif

    case CommandQueueType::Count:
    case CommandQueueType::None:
        break;
    }

    assert(false);
    return D3D12_COMMAND_LIST_TYPE_NONE;
}
