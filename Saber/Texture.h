/**
 * @file Texture.h
 * @brief 2-D texture array with SRV/RTV/UAV descriptor ranges, and the G-buffer specialisation.
 *
 * @ref Texture manages a fixed-capacity array of @ref TextureResource objects
 * sharing the same @c D3D12_RESOURCE_DESC.  It allocates descriptor ranges on
 * construction and recreates all resources on @ref Resize.
 *
 * @ref GBuffer extends @ref Texture with two render-target slices
 * (UV+materialId, TBN) and an @ref EnumFence<GBufferState> for write/read
 * synchronisation between the geometry pass (direct queue) and the deferred
 * shading pass (compute queue).
 */
#pragma once

#include "Headers.h"

#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "TextureResource.h"

/**
 * @brief Manages a fixed-size array of same-format 2-D textures with optional
 *        SRV, UAV, and RTV descriptor ranges.
 *
 * @ref Resize clears and recreates all textures and their descriptors.  Each
 * texture can be individually accessed via @ref GetTexture.  Descriptor handles
 * are obtained through @ref GetSrvDescHandle, @ref GetUavDescHandle, and
 * @ref GetRtvDescHandle.
 */
class Texture {
	std::wstring m_name{};

	D3D12_RESOURCE_DESC m_desc{}; /**< @brief Resource descriptor template (Width/Height updated on resize). */
	UINT64 m_width{};
	UINT m_height{};

	size_t m_capacity{};          /**< @brief Number of texture slices in the array. */

	std::vector<std::shared_ptr<TextureResource>> m_pTextures{};

	std::shared_ptr<DescRange> m_pSrvsRange{}; /**< @brief SRV descriptor range (null if SRV not supported). */
	std::shared_ptr<DescRange> m_pRtvsRange{}; /**< @brief RTV descriptor range (null if RTV not supported). */
	std::shared_ptr<DescRange> m_pUavsRange{}; /**< @brief UAV descriptor range (null if UAV not supported). */

public:
	/**
	 * @brief Constructs the texture array and allocates descriptor ranges based on @p desc flags.
	 * @param name           Debug name.
	 * @param pDeviceContext Device context providing descriptor heaps and the device.
	 * @param desc           D3D12 resource descriptor; flags determine which views are created.
	 * @param capacity       Number of texture slices (default 1).
	 */
	Texture(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		const D3D12_RESOURCE_DESC& desc,
		size_t capacity = 1
	) : m_name(name),
		m_desc(desc),
		m_width(desc.Width),
		m_height(desc.Height),
		m_capacity(capacity)
	{
		if (IsSrvDesc(desc)) {
			m_pSrvsRange = pDeviceContext->GetDescriptorHeap()->AllocateRange(m_name + L"/Ranges/SRV", m_capacity, D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
		}
		if (IsUavDesc(desc)) {
			m_pUavsRange = pDeviceContext->GetDescriptorHeap()->AllocateRange(m_name + L"/Ranges/UAV", m_capacity, D3D12_DESCRIPTOR_RANGE_TYPE_UAV);
		}
		if (IsRtvDesc(desc)) {
			m_pRtvsRange = pDeviceContext->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)->AllocateRange(m_name + L"/Ranges/RTV", m_capacity);
		}

		Resize(pDeviceContext->GetDevice(), desc.Width, desc.Height);
	}

	/** @brief Returns the number of texture slices. */
	size_t GetSize() const {
		return m_capacity;
	}

	/** @brief Returns the current texture width in texels. */
	UINT64 GetWidth() const {
		return m_width;
	}

	/** @brief Returns the current texture height in texels. */
	UINT64 GetHeight() const {
		return m_height;
	}

	/** @brief Returns the resource state of the first texture slice. */
	D3D12_RESOURCE_STATES GetState() const {
		return m_pTextures[0]->GetState();
	}

	/**
	 * @brief Recreates all texture resources at the new dimensions.
	 *
	 * Clears all descriptor ranges, destroys the old resources, and allocates
	 * new @ref TextureResource objects at @p width × @p height.
	 *
	 * @param pDevice D3D12 device wrapper.
	 * @param width   New width in texels.
	 * @param height  New height in texels.
	 */
	void Resize(
		std::shared_ptr<Device> pDevice,
		UINT64 width,
		UINT height
	) {
		if (m_pRtvsRange) {
			m_pRtvsRange->Clear();
		}
		if (m_pSrvsRange) {
			m_pSrvsRange->Clear();
		}
		if (m_pUavsRange) {
			m_pUavsRange->Clear();
		}

		m_pTextures.clear();
		m_pTextures.resize(m_capacity);

		m_desc.Width = width;
		m_desc.Height = height;

		for (size_t i{}; i < m_capacity; ++i) {
			m_pTextures[i] = std::make_shared<TextureResource>(
				m_name + L"/Texture" + std::to_wstring(i),
				pDevice,
				GPUResource::AllocationDesc{},
				GPUResource::ResourceDesc{ m_desc }
			);
			if (m_pRtvsRange) {
				m_pTextures[i]->CreateRenderTargetView(pDevice, m_pRtvsRange->GetNextCpuHandle());
			}
			if (m_pSrvsRange) {
				m_pTextures[i]->CreateShaderResourceView(pDevice, m_pSrvsRange->GetNextCpuHandle());
			}
			if (m_pUavsRange) {
				m_pTextures[i]->CreateUnorderedAccessView(pDevice, m_pUavsRange->GetNextCpuHandle());
			}
		}
	}

	/**
	 * @brief Clears all render-target slices to @p pClearValue (or the resource's optimised clear value).
	 * @param pCommandList Command list for the clear commands.
	 * @param pClearValue  Optional explicit RGBA clear value; @c nullptr uses the resource default.
	 */
	void Clear(
		std::shared_ptr<CommandList> pCommandList,
		const float* pClearValue = nullptr
	) {
		assert(m_pRtvsRange);
		for (size_t i{}; i < m_capacity; ++i) {
			m_pTextures[i]->ClearRenderTarget(pCommandList, m_pRtvsRange->GetCpuHandle(i), pClearValue);
		}
	}

	/**
	 * @brief Returns the texture resource at @p id.
	 * @param id Zero-based slice index.
	 */
	std::shared_ptr<TextureResource> GetTexture(size_t id) const {
		return m_pTextures[id];
	}

	/**
	 * @brief Transitions all texture slices to @p toState.
	 * @param pCommandList Command list for the barrier commands.
	 * @param toState      Target resource state.
	 */
	void ChangeState(
		std::shared_ptr<CommandList> pCommandList,
		const D3D12_RESOURCE_STATES& toState
	) {
		for (auto& pTex : m_pTextures) {
			pTex->ResourceTransition(pCommandList, toState);
		}
	}

	/**
	 * @brief Returns the GPU SRV descriptor handle for slice @p id.
	 * @param id Zero-based slice index (default 0).
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvDescHandle(size_t id = 0) const {
		assert(m_pSrvsRange);
		return m_pSrvsRange ? m_pSrvsRange->GetGpuHandle(id) : static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(0);
	}

	/**
	 * @brief Returns the GPU RTV descriptor handle for slice @p id.
	 * @param id Zero-based slice index (default 0).
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetRtvDescHandle(size_t id = 0) const {
		assert(m_pRtvsRange);
		return m_pRtvsRange ? m_pRtvsRange->GetGpuHandle(id) : static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(0);
	}

	/**
	 * @brief Returns the GPU UAV descriptor handle for slice @p id.
	 * @param id Zero-based slice index (default 0).
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavDescHandle(size_t id = 0) const {
		assert(m_pUavsRange);
		return m_pUavsRange ? m_pUavsRange->GetGpuHandle(id) : static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(0);
	}

	/**
	 * @brief Builds a @c D3D12_RT_FORMAT_ARRAY describing all render-target slices.
	 * @return Format array with @c NumRenderTargets == @ref GetSize().
	 */
	D3D12_RT_FORMAT_ARRAY GetRtFormatArray() const {
		assert(m_pRtvsRange);

		UINT numRenderTargets{ m_pRtvsRange ? static_cast<UINT>(m_pRtvsRange->GetSize()) : 0 };
		assert(numRenderTargets == m_capacity);

		D3D12_RT_FORMAT_ARRAY rtFormats{ .NumRenderTargets{ numRenderTargets } };
		for (size_t i{}; i < numRenderTargets; ++i) {
			rtFormats.RTFormats[i] = m_desc.Format;
		}
		return rtFormats;
	}

	/**
	 * @brief Returns the CPU RTV handles for all slices as a flat vector.
	 * @return Vector of @c D3D12_CPU_DESCRIPTOR_HANDLE, one per slice.
	 */
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> GetRtvs() const {
		assert(m_pRtvsRange);

		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs{ m_pRtvsRange->GetSize() };
		for (size_t i{}; i < m_pRtvsRange->GetSize(); ++i) {
			rtvs[i] = m_pRtvsRange->GetCpuHandle(i);
		}
		return rtvs;
	}

	/**
	 * @brief Binds all render-target slices as output-merger render targets.
	 * @param pCommandList  Command list.
	 * @param pDepthBuffer  Optional depth-stencil buffer to bind alongside the RTVs.
	 */
	void SetRenderTargets(
		std::shared_ptr<CommandList> pCommandList,
		std::shared_ptr<DepthBuffer> pDepthBuffer = nullptr
	) const {
		assert(m_pRtvsRange);
		pCommandList->GetD3D12CommandList()->OMSetRenderTargets(
			static_cast<UINT>(m_pRtvsRange->GetSize()),
			&(m_pRtvsRange->GetCpuHandle()),
			TRUE,
			pDepthBuffer ? &pDepthBuffer->GetDsvCpuDescHandle() : nullptr
		);
	}
};

#include "EnumFence.h"

/**
 * @brief State machine values for G-buffer synchronisation.
 *
 * @ref GBufferState is used with @ref EnumFence to coordinate the direct-queue
 * geometry pass (sets @c Write state) with the compute-queue deferred shading
 * pass (requires @c Read state).
 */
enum class GBufferState : uint8_t {
	InvalidState = 0,

	Write, /**< @brief G-buffer is being written by the geometry pass. */
	Read,  /**< @brief G-buffer is ready to be read by the deferred shading pass. */

	FlushState = std::numeric_limits<uint8_t>::max() /**< @brief Sentinel for fence flush. */
};

/**
 * @brief Two-slice G-buffer (UV+materialId, TBN) with an @ref EnumFence for queue synchronisation.
 *
 * Slice 0 stores packed UV coordinates and a material index.
 * Slice 1 stores TBN basis vectors for normal-map reconstruction.
 *
 * The @ref EnumFence<GBufferState> starts in @c GBufferState::Write.
 */
class GBuffer : public Texture {
	/** @brief tex0: UV + materialId; tex1: TBN. */
	static inline constexpr size_t GBufferSize{ 2 };

	/** @brief Returns the resource descriptor for a G-buffer slice at the given resolution. */
	static constexpr D3D12_RESOURCE_DESC GetGBufferTexDesc(size_t width, size_t height) {
		return D3D12_RESOURCE_DESC{
			.Dimension{ D3D12_RESOURCE_DIMENSION_TEXTURE2D },
			.Width{ static_cast<UINT64>(width) },
			.Height{ static_cast<UINT>(height) },
			.DepthOrArraySize{ 1 },
			.MipLevels{ 0 },
			.Format{ DXGI_FORMAT_R32G32B32A32_FLOAT },
			.SampleDesc{ 1, 0 },
			.Flags{ D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET }
		};
	}

	std::shared_ptr<EnumFence<GBufferState>> m_pGBufferFence{}; /**< @brief Write/Read synchronisation fence. */

public:
	/**
	 * @brief Constructs the G-buffer at @p width × @p height.
	 * @param name           Debug name.
	 * @param pDeviceContext Device context.
	 * @param width          Width in texels.
	 * @param height         Height in texels.
	 */
	GBuffer(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		size_t width,
		size_t height
	) : Texture(name, pDeviceContext, GetGBufferTexDesc(width, height), GBufferSize),
		m_pGBufferFence(std::make_shared<EnumFence<GBufferState>>(
			name + L"/Fence",
			pDeviceContext->GetDevice(),
			GBufferState::Write
		))
	{}

	/** @brief Returns the fence used to synchronise G-buffer write/read state. */
	std::shared_ptr<EnumFence<GBufferState>> GetFence() const {
		return m_pGBufferFence;
	}
};
