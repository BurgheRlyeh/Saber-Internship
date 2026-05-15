/**
 * @file PSOLibrary.h
 * @brief Disk-backed D3D12 pipeline-state object (PSO) cache.
 *
 * @ref PSOLibrary wraps @c ID3D12PipelineLibrary1 and persists the compiled PSO
 * blobs to a memory-mapped file via @ref MemoryMappedFile.  On first use the
 * library is populated from the file; subsequent runs can load PSOs by name
 * without recompilation.
 *
 * Thread-safe: all @c ID3D12PipelineLibrary1 calls are guarded by a mutex.
 */
#pragma once

#include "Headers.h"

#include <mutex>

#include "MemoryMappedFile.h"

class Device;

/**
 * @brief Manages a D3D12 pipeline-state library with on-disk persistence.
 *
 * Use @ref Find to look up an existing PSO by name, @ref Add to compile and
 * store a new PSO, or @ref Assign which combines both (returns the cached PSO
 * if present, otherwise compiles and caches it).  Call @ref FlushCacheToFile
 * to persist newly added PSOs to disk.
 */
class PSOLibrary {
private:
	MemoryMappedFile m_file; /**< @brief Memory-mapped backing file for the PSO blobs. */

	Microsoft::WRL::ComPtr<ID3D12PipelineLibrary1> m_pPipelineLibrary{}; /**< @brief D3D12 pipeline library object. */
	std::mutex m_pipelineLibraryMutex;

	std::atomic<bool> m_isRenewed{}; /**< @brief Set to @c true when new PSOs have been added since last flush. */

public:
	/**
	 * @brief Opens or creates the PSO cache file and deserialises the pipeline library.
	 * @param pDevice   D3D12 device used to create the @c ID3D12PipelineLibrary1.
	 * @param filename  Path to the backing cache file.
	 */
	PSOLibrary(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename
	);

	~PSOLibrary();

	/**
	 * @brief Closes the library and optionally removes the cache file.
	 * @param ClearPsoCache If @c true the backing file is deleted.
	 */
	void Destroy(bool ClearPsoCache);

	/** @brief Flushes the in-memory library blob to the backing file if new PSOs were added. */
	void FlushCacheToFile();

	/**
	 * @brief Looks up a graphics PSO by name without compiling.
	 * @param filename  Cache key (name under which the PSO was stored).
	 * @param pPSODesc  Graphics PSO descriptor (used if the PSO is found but not cached in RAM).
	 * @return The cached @c ID3D12PipelineState, or @c nullptr if not found.
	 */
	Microsoft::WRL::ComPtr<ID3D12PipelineState> Find(
		const std::wstring& filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);

	/**
	 * @brief Looks up a compute PSO by name without compiling.
	 * @param filename  Cache key.
	 * @param pPSODesc  Compute PSO descriptor.
	 * @return The cached @c ID3D12PipelineState, or @c nullptr if not found.
	 */
	Microsoft::WRL::ComPtr<ID3D12PipelineState> Find(
		const std::wstring& filename,
		const D3D12_COMPUTE_PIPELINE_STATE_DESC* pPSODesc
	);

	/**
	 * @brief Compiles and stores a graphics PSO under @p filename.
	 * @param pDevice   Device for PSO compilation.
	 * @param filename  Cache key.
	 * @param pPSODesc  Graphics PSO descriptor.
	 * @return @c true if the PSO was added; @c false if it already existed.
	 */
	bool Add(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);

	/**
	 * @brief Compiles and stores a compute PSO under @p filename.
	 * @param pDevice   Device for PSO compilation.
	 * @param filename  Cache key.
	 * @param pPSODesc  Compute PSO descriptor.
	 * @return @c true if the PSO was added; @c false if it already existed.
	 */
	bool Add(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename,
		const D3D12_COMPUTE_PIPELINE_STATE_DESC* pPSODesc
	);

	/**
	 * @brief Returns the cached graphics PSO, compiling and caching it if necessary.
	 * @param pDevice   Device for PSO compilation.
	 * @param filename  Cache key.
	 * @param pPSODesc  Graphics PSO descriptor.
	 * @return Valid @c ID3D12PipelineState.
	 */
	Microsoft::WRL::ComPtr<ID3D12PipelineState> Assign(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);

	/**
	 * @brief Returns the cached compute PSO, compiling and caching it if necessary.
	 * @param pDevice   Device for PSO compilation.
	 * @param filename  Cache key.
	 * @param pPSODesc  Compute PSO descriptor.
	 * @return Valid @c ID3D12PipelineState.
	 */
	Microsoft::WRL::ComPtr<ID3D12PipelineState> Assign(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename,
		const D3D12_COMPUTE_PIPELINE_STATE_DESC* pPSODesc
	);
};
