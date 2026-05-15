/**
 * @file CommandList.h
 * @brief Wraps an @c ID3D12GraphicsCommandList2 together with lifecycle callbacks
 *        and PIX event helpers.
 */
#pragma once

#include "Headers.h"

#include <functional>

/**
 * @brief Manages a single D3D12 graphics/compute/copy command list.
 *
 * Records commands and signals readiness for GPU submission.  Optional
 * @c beforeExec and @c afterExec callbacks allow the owning system to
 * perform work immediately before or after the list is executed on the queue.
 */
class CommandList {
    std::wstring m_name{};

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_pD3D12CommandList{};

    std::atomic<bool> m_isReadyForExecution{};

    std::function<void()> m_beforeExec{};
    std::function<void()> m_afterExec{};

    uint8_t m_pixEventsBegan{}; /**< @brief Tracks unmatched PIX begin-event calls for validation. */

public:
    /**
     * @brief Constructs a CommandList from an existing D3D12 command list object.
     * @param name        Debug name for PIX and validation tooling.
     * @param pCommandList Underlying D3D12 command list (already open or reset).
     * @param beforeExec  Callback invoked just before the list is submitted to the queue.
     * @param afterExec   Callback invoked just after GPU execution of the list completes.
     */
    CommandList(
        const std::wstring& name,
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
        std::function<void()> beforeExec = []{},
        std::function<void()> afterExec = []{}
    );

    /** @brief Returns the underlying D3D12 command list COM pointer. */
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetD3D12CommandList() const;

    /** @brief Returns the command-list type (Direct, Compute, or Copy). */
    D3D12_COMMAND_LIST_TYPE GetType() const;

    /** @brief Returns @c true if the list has been closed and is ready for submission. */
    bool IsReadyForExecution() const;

    /** @brief Marks this list as closed and ready to be submitted to the command queue. */
    void SetReadyForExecution();

    /** @brief Invokes the @c beforeExec callback. Called by the queue before execution. */
    void BeforeExecute() const;

    /** @brief Invokes the @c afterExec callback. Called by the queue after execution. */
    void AfterExecute() const;

    /**
     * @brief Inserts a PIX performance event marker around subsequent commands.
     * @param name Debug label shown in PIX.
     * @param r    Red channel of the event colour (0–255).
     * @param g    Green channel of the event colour (0–255).
     * @param b    Blue channel of the event colour (0–255).
     */
    void PixBeginEvent(const std::wstring& name, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);

    /** @brief Ends the innermost PIX event region opened with @ref PixBeginEvent. */
    void PixEndEvent();
};
