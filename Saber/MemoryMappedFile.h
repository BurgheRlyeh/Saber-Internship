/**
 * @file MemoryMappedFile.h
 * @brief Win32 memory-mapped file wrapper used by @ref PSOLibrary for on-disk PSO caching.
 *
 * The file layout reserves the first @c sizeof(UINT) bytes as a length field
 * (written via @ref SetSize / read via @ref GetSize).  @ref GetData returns a
 * pointer to the byte immediately after the length.  The mapping can grow
 * on demand via @ref GrowMapping.
 */

//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "Headers.h"

/**
 * @brief Thin RAII wrapper around a Win32 memory-mapped file.
 *
 * The mapped region starts with a 4-byte length header followed by the
 * payload data.  Use @ref Init to open (or create) the file, @ref Flush to
 * flush dirty pages to disk, and @ref Destroy to close and optionally delete
 * the file.
 */
class MemoryMappedFile
{
public:
    MemoryMappedFile();
    ~MemoryMappedFile();

    /**
     * @brief Opens or creates the memory-mapped file.
     * @param filename Path to the backing file.
     * @param filesize Initial file size in bytes (default @ref DefaultFileSize).
     */
    void Init(std::wstring filename, UINT filesize = DefaultFileSize);

    /**
     * @brief Closes the mapping and optionally deletes the backing file.
     * @param deleteFile If @c true the file is deleted after unmapping.
     */
    void Destroy(bool deleteFile);

    /**
     * @brief Grows the file and remaps it to at least @p size bytes.
     * @param size New minimum file size in bytes.
     */
    void GrowMapping(UINT size);

    /** @brief Flushes modified pages to the backing file. */
    void Flush();

    /**
     * @brief Writes @p size into the 4-byte length header of the mapped region.
     * @param size Payload size in bytes to record.
     */
    void SetSize(UINT size)
    {
        if (m_mapAddress)
        {
            static_cast<UINT*>(m_mapAddress)[0] = size;
        }
    }

    /**
     * @brief Reads the 4-byte length header from the mapped region.
     * @return Recorded payload size, or 0 if the file is not currently mapped.
     */
    UINT GetSize() const
    {
        if (m_mapAddress)
        {
            return static_cast<UINT*>(m_mapAddress)[0];
        }
        return 0;
    }

    /**
     * @brief Returns a pointer to the payload region (immediately after the length header).
     * @return CPU pointer to the payload, or @c nullptr if not mapped.
     */
    void* GetData()
    {
        if (m_mapAddress)
        {
            // The actual data comes after the length.
            return &static_cast<UINT*>(m_mapAddress)[1];
        }
        return nullptr;
    }

public:
    /** @brief Returns @c true if the file is currently mapped into CPU address space. */
    bool IsMapped() const { return m_mapAddress != nullptr; }

protected:
    static const UINT DefaultFileSize = 64; /**< @brief Default initial file size in bytes. */

    HANDLE m_mapFile;          /**< @brief File-mapping kernel object. */
    HANDLE m_file;             /**< @brief Underlying file handle. */
    LPVOID m_mapAddress;       /**< @brief Base address of the mapped view. */
    std::wstring m_filename;   /**< @brief Path to the backing file. */

    UINT m_currentFileSize;    /**< @brief Current size of the file in bytes. */
};
