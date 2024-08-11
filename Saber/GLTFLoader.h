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

    void GetIndices(std::vector<uint32_t>& indices);

    bool GetVerticesData(std::vector<float>& data, const std::string& attributeName);

private:
    void CheckFilepathCorrectness(std::filesystem::path& filepath);

    void GetResourceReaderAndDocument(
        const std::filesystem::path& filepath,
        std::unique_ptr<Microsoft::glTF::GLBResourceReader>& pResourceReader,
        Microsoft::glTF::Document& document
    );
};
