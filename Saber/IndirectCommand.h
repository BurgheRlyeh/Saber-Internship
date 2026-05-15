/**
 * @file IndirectCommand.h
 * @brief Defines the indirect draw command structures used with ExecuteIndirect.
 *
 * Each struct describes a single GPU indirect draw command and carries the
 * corresponding @c D3D12_INDIRECT_ARGUMENT_DESC array needed to create a
 * @c ID3D12CommandSignature.
 *
 * The @ref DEFINE_INDIRECT_COMMAND macro generates a struct that derives from
 * @ref IndirectCommandBase in C++ and is a plain struct in HLSL, keeping the
 * layout identical in both contexts.
 */
#ifndef INDIRECT_COMMAND
#define INDIRECT_COMMAND

#include "CppHlslTypesRedefine.h"

#ifdef __cplusplus
#include <type_traits>

#pragma pack(push, 1)   // todo is it needed

/**
 * @brief CRTP base providing static helpers for indirect command structs.
 *
 * @tparam IndirectCommand Concrete derived type (CRTP pattern).
 */
template <typename IndirectCommand>
struct IndirectCommandBase {
    /** @brief Compile-time check that @p IndirectCommand derives from this base. */
    static void Assert() {
        static_assert(
            std::is_base_of_v<IndirectCommandBase, IndirectCommand>,
            "IndirectCommand must be a subclass of IndirectCommandBase"
        );
    }

    /**
     * @brief Returns the @c D3D12_COMMAND_SIGNATURE_DESC for this command layout.
     *
     * Reads @c IndirectCommand::indirectArgumentDescs to populate the descriptor.
     *
     * @return Fully initialised command signature descriptor.
     */
    static D3D12_COMMAND_SIGNATURE_DESC GetCommandSignatureDesc() {
        Assert();
        return D3D12_COMMAND_SIGNATURE_DESC{
            .ByteStride{ sizeof(IndirectCommand) },
            .NumArgumentDescs{ _countof(IndirectCommand::indirectArgumentDescs) },
            .pArgumentDescs{ IndirectCommand::indirectArgumentDescs }
        };
    }
};

/**
 * @brief Concept satisfied when @p Impl is an @ref IndirectCommandBase subclass.
 * @tparam Impl Concrete indirect command type.
 */
template <typename Impl>
concept IndirectCommandConcept = std::derived_from<Impl, IndirectCommandBase<Impl>>;

/** @brief Defines a C++ indirect command struct that inherits @ref IndirectCommandBase. */
#define DEFINE_INDIRECT_COMMAND(IndirectCommand) \
	struct IndirectCommand : IndirectCommandBase<IndirectCommand>
#else
/** @brief In HLSL, defines a plain struct with no base class. */
#define DEFINE_INDIRECT_COMMAND(IndirectCommand) \
	struct IndirectCommand
#endif

/**
 * @brief Indirect draw command with CBV, index buffer, one vertex buffer, and draw-indexed args.
 *
 * Layout: [ConstantBufferView | IndexBufferView | VertexBufferView | DrawIndexedArguments]
 */
DEFINE_INDIRECT_COMMAND(CbMeshIndirectCommand) {
    D3D12_GPU_VIRTUAL_ADDRESS constantBufferView; /**< @brief Per-object constant buffer GPU address. */
    D3D12_INDEX_BUFFER_VIEW   indexBufferView;    /**< @brief Index buffer view. */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView;   /**< @brief Vertex buffer view (stream 0). */
    D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;   /**< @brief Draw call parameters. */
#ifdef __cplusplus
    static inline D3D12_INDIRECT_ARGUMENT_DESC indirectArgumentDescs[4]{
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW }, .ConstantBufferView{ 1 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{} },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED } },
    };
#endif
};

/**
 * @brief Indirect draw command with CBV, index buffer, four vertex streams, and draw-indexed args.
 *
 * Layout: [ConstantBufferView | IndexBufferView | VertexBufferView×4 | DrawIndexedArguments]
 */
DEFINE_INDIRECT_COMMAND(CbMesh4IndirectCommand) {
    D3D12_GPU_VIRTUAL_ADDRESS constantBufferView;  /**< @brief Per-object constant buffer GPU address. */
    D3D12_INDEX_BUFFER_VIEW   indexBufferView;     /**< @brief Index buffer view. */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView;    /**< @brief Vertex stream 0 (positions). */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView1;   /**< @brief Vertex stream 1 (normals). */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView2;   /**< @brief Vertex stream 2 (tangents). */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView3;   /**< @brief Vertex stream 3 (UVs). */
    D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;    /**< @brief Draw call parameters. */
#ifdef __cplusplus
    static inline D3D12_INDIRECT_ARGUMENT_DESC indirectArgumentDescs[7]{
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW }, .ConstantBufferView{ 1 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{} },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{ 1 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{ 2 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{ 3 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED } },
    };
#endif
};

/**
 * @brief Indirect draw command with CBV, a root constant, index buffer, four vertex streams,
 *        and draw-indexed args.
 *
 * Layout: [ConstantBufferView | RootConstant(4×uint) | IndexBufferView | VertexBufferView×4 | DrawIndexedArguments]
 */
DEFINE_INDIRECT_COMMAND(CbConstMesh4IndirectCommand) {
    D3D12_GPU_VIRTUAL_ADDRESS constantBufferView;  /**< @brief Per-object constant buffer GPU address. */
    DirectX::XMUINT4          rootConstant;        /**< @brief 4-component root constant (e.g. model buffer index). */
    D3D12_INDEX_BUFFER_VIEW   indexBufferView;     /**< @brief Index buffer view. */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView;    /**< @brief Vertex stream 0. */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView1;   /**< @brief Vertex stream 1. */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView2;   /**< @brief Vertex stream 2. */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView3;   /**< @brief Vertex stream 3. */
    D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;    /**< @brief Draw call parameters. */
#ifdef __cplusplus
    static inline D3D12_INDIRECT_ARGUMENT_DESC indirectArgumentDescs[8]{
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW }, .ConstantBufferView{ 1 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT }, .Constant{ 2, 0, 4 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{} },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{ 1 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{ 2 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{ 3 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED } },
    };
#endif
};

/**
 * @brief Indirect draw command with a root constant only (no CBV), index buffer,
 *        four vertex streams, and draw-indexed args.
 *
 * Layout: [RootConstant(4×uint) | IndexBufferView | VertexBufferView×4 | DrawIndexedArguments]
 */
DEFINE_INDIRECT_COMMAND(ConstMesh4IndirectCommand) {
    DirectX::XMUINT4          rootConstant;        /**< @brief 4-component root constant (e.g. model buffer index). */
    D3D12_INDEX_BUFFER_VIEW   indexBufferView;     /**< @brief Index buffer view. */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView;    /**< @brief Vertex stream 0. */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView1;   /**< @brief Vertex stream 1. */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView2;   /**< @brief Vertex stream 2. */
    D3D12_VERTEX_BUFFER_VIEW  vertexBufferView3;   /**< @brief Vertex stream 3. */
    D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;    /**< @brief Draw call parameters. */
#ifdef __cplusplus
    static inline D3D12_INDIRECT_ARGUMENT_DESC indirectArgumentDescs[7]{
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT }, .Constant{ 1, 0, 4 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{} },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{ 1 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{ 2 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{ 3 } },
        {.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED } },
    };
#endif
};

#ifdef __cplusplus
#pragma pack(pop)
#endif

#endif  // INDIRECT_COMMAND
