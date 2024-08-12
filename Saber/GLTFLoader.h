#pragma once

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/Deserialize.h>

#include "Headers.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

#include "Vertices.h"

class GLTFLoader {
    std::unique_ptr<Microsoft::glTF::GLBResourceReader> m_pResourceReader{};
    Microsoft::glTF::Document m_document{};

public:
    GLTFLoader(std::filesystem::path& filepath);

    template<typename T>
    void GetIndices(std::vector<T>& indices) {
        const auto& mesh = m_document.meshes.Elements()[0];
        const Microsoft::glTF::Accessor& accessor = m_document.accessors.Get(mesh.primitives.front().indicesAccessorId);
        indices = m_pResourceReader->ReadBinaryData<T>(m_document, accessor);
    }

    DXGI_FORMAT GetIndicesFormat();
    DXGI_FORMAT GetDXGIFormat(const Microsoft::glTF::Accessor& accessor);

    bool GetVerticesData(std::vector<float>& data, const std::string& attributeName);

private:
    void CheckFilepathCorrectness(std::filesystem::path& filepath);

    void GetResourceReaderAndDocument(
        const std::filesystem::path& filepath,
        std::unique_ptr<Microsoft::glTF::GLBResourceReader>& pResourceReader,
        Microsoft::glTF::Document& document
    );
};
