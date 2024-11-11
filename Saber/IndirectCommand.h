#ifndef INDIRECT_COMMAND
#define INDIRECT_COMMAND

#include "CppHlslTypesRedefine.h"

#ifdef __cplusplus
#include <type_traits>

template <typename IndirectCommand>
struct IndirectCommandBase {
    static void Assert() {
        static_assert(
            std::is_base_of_v<IndirectCommandBase, IndirectCommand>,
            "IndirectCommand must be a subclass of IndirectCommandBase"
        );
    }

    static D3D12_COMMAND_SIGNATURE_DESC GetCommandSignatureDesc() {
        Assert();
        return D3D12_COMMAND_SIGNATURE_DESC{
            .ByteStride{ sizeof(IndirectCommand) },
            .NumArgumentDescs{ _countof(IndirectCommand::indirectArgumentDescs) },
            .pArgumentDescs{ IndirectCommand::indirectArgumentDescs }
        };
    }
};
#define DEFINE_INDIRECT_COMMAND(IndirectCommand) \
	struct IndirectCommand : IndirectCommandBase<IndirectCommand>
#else
#define DEFINE_INDIRECT_COMMAND(IndirectCommand) \
	struct IndirectCommand
#endif

DEFINE_INDIRECT_COMMAND(MeshCbIndirectCommand) {
    D3D12_GPU_VIRTUAL_ADDRESS constantBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
#ifdef __cplusplus
    static inline D3D12_INDIRECT_ARGUMENT_DESC indirectArgumentDescs[4]{
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW }, .ConstantBufferView{ 1 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{} },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED } },
    };
#endif
};

#endif
