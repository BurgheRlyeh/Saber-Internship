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
public:
    static void LoadMeshFromGLTF(
        std::filesystem::path& filepath,
        std::vector<uint32_t>& indices,
        std::vector<VertexPositionColor>& vertices
    );
};
