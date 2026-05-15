/**
 * @file GLTFLoader.h
 * @brief Loads geometry data from GLB (binary glTF) files using the glTF-SDK.
 */
#pragma once

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/Deserialize.h>

#include "Headers.h"

#include <filesystem>

/**
 * @brief Reads index and vertex attribute data from a GLB file.
 *
 * Only the first mesh in the document is currently supported.
 */
class GLTFLoader {
    std::unique_ptr<Microsoft::glTF::GLBResourceReader> m_pResourceReader{}; /**< @brief glTF-SDK reader for binary data. */
    Microsoft::glTF::Document m_document{};                                    /**< @brief Parsed glTF JSON document. */

public:
    /**
     * @brief Opens and parses the given GLB file.
     * @param filepath Path to the @c .glb file.
     */
    GLTFLoader(const std::filesystem::path& filepath);

    /**
     * @brief Reads the index buffer of the first mesh primitive into @p indices.
     * @tparam T  Unsigned integer type to use for indices (@c uint16_t or @c uint32_t).
     * @param[out] indices Vector to populate with triangle indices.
     */
    template<typename T>
    void GetIndices(std::vector<T>& indices) {
        const auto& mesh = m_document.meshes.Elements()[0];
        const Microsoft::glTF::Accessor& accessor = m_document.accessors.Get(mesh.primitives.front().indicesAccessorId);
        indices = m_pResourceReader->ReadBinaryData<T>(m_document, accessor);
    }

    /**
     * @brief Returns the DXGI index format (@c R16_UINT or @c R32_UINT) for the first mesh.
     * @return DXGI format corresponding to the index component type in the glTF file.
     */
    DXGI_FORMAT GetIndicesFormat();

    /**
     * @brief Maps a glTF accessor's component type and count to a DXGI format.
     * @param accessor glTF accessor to inspect.
     * @return Corresponding DXGI format.
     */
    DXGI_FORMAT GetDXGIFormat(const Microsoft::glTF::Accessor& accessor);

    /**
     * @brief Reads a named vertex attribute into @p data as packed floats.
     * @param[out] data          Destination vector; resized and filled by this call.
     * @param      attributeName glTF attribute semantic (e.g. @c ACCESSOR_POSITION).
     * @param      meshId        Zero-based mesh index (default 0).
     * @return @c true if the attribute was found and read; @c false otherwise.
     */
    bool GetVerticesData(std::vector<float>& data, const std::string& attributeName, size_t meshId = 0);

private:
    /**
     * @brief Normalises the file path (e.g. handles relative paths).
     * @param filepath Input path.
     * @return Corrected canonical path.
     */
    std::filesystem::path FilepathToCorrect(const std::filesystem::path& filepath);

    /**
     * @brief Opens the GLB file and populates the resource reader and document.
     * @param[in]  filepath       Path to the GLB file.
     * @param[out] pResourceReader Initialised GLB resource reader.
     * @param[out] document        Parsed glTF document.
     */
    void GetResourceReaderAndDocument(
        const std::filesystem::path& filepath,
        std::unique_ptr<Microsoft::glTF::GLBResourceReader>& pResourceReader,
        Microsoft::glTF::Document& document
    );
};
