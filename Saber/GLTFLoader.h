#pragma once

#include "Headers.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/Deserialize.h>

#include "Vertices.h"

// The glTF SDK is decoupled from all file I/O by the IStreamReader (and IStreamWriter)
// interface(s) and the C++ stream-based I/O library. This allows the glTF SDK to be used in
// sandboxed environments, such as WebAssembly modules and UWP apps, where any file I/O code
// must be platform or use-case specific.
class StreamReader : public Microsoft::glTF::IStreamReader {
public:
    StreamReader(std::filesystem::path pathBase) : m_pathBase(std::move(pathBase))
    {
        assert(m_pathBase.has_root_path());
    }

    // Resolves the relative URIs of any external resources declared in the glTF manifest
    std::shared_ptr<std::istream> GetInputStream(const std::string& filename) const override
    {
        // In order to construct a valid stream:
        // 1. The filename argument will be encoded as UTF-8 so use filesystem::u8path to
        //    correctly construct a path instance.
        // 2. Generate an absolute path by concatenating m_pathBase with the specified filename
        //    path. The filesystem::operator/ uses the platform's preferred directory separator
        //    if appropriate.
        // 3. Always open the file stream in binary mode. The glTF SDK will handle any text
        //    encoding issues for us.
        auto streamPath = m_pathBase / filename;
        std::shared_ptr<std::ifstream> stream{
            std::make_shared<std::ifstream>(streamPath, std::ios_base::binary)
        };

        // Check if the stream has no errors and is ready for I/O operations
        if (!stream || !(*stream)) {
            throw std::runtime_error("Unable to create a valid input stream for uri: " + filename);
        }

        return stream;
    }

private:
    std::filesystem::path m_pathBase;
};

class GLTFLoader {
public:
    static void LoadMeshFromGLTF(
        std::filesystem::path& filepath,
        std::vector<uint32_t>& indices,
        std::vector<VertexPositionColor>& vertices
    ) {
        if (filepath.is_relative()) {
            std::filesystem::path pathCurrent{ std::filesystem::current_path() };

            // Convert the relative path into an absolute path by appending the command line argument to the current path
            pathCurrent /= filepath;
            pathCurrent.swap(filepath);
        }
        if (!filepath.has_filename()) {
            throw std::runtime_error("Filepath has no filename");
        }
        if (!filepath.has_extension()) {
            throw std::runtime_error("Filepath has no filename extension");
        }

        // Pass the absolute path, without the filename, to the stream reader
        auto streamReader = std::make_unique<StreamReader>(filepath.parent_path());

        std::filesystem::path filename = filepath.filename();
        std::filesystem::path fileExtension = filename.extension();


        auto MakePathExt = [](const std::string& ext) { return "." + ext; };
        assert(fileExtension == MakePathExt(Microsoft::glTF::GLB_EXTENSION));

        // Pass a UTF-8 encoded filename to GetInputString
        std::u8string u8string{ filename.u8string() };
        std::shared_ptr<std::istream> glbStream{
            streamReader->GetInputStream(std::string(u8string.begin(), u8string.end()))
        };
        std::unique_ptr<Microsoft::glTF::GLBResourceReader> glbResourceReader{
            std::make_unique<Microsoft::glTF::GLBResourceReader>(
                std::move(streamReader),
                std::move(glbStream)
            )
        };

        std::string manifest{ glbResourceReader->GetJson() }; // Get the manifest from the JSON chunk

        std::unique_ptr<Microsoft::glTF::GLTFResourceReader> resourceReader{
            std::move(glbResourceReader)
        };

        if (!resourceReader) {
            throw std::runtime_error("Command line argument path filename extension must be .gltf or .glb");
        }

        Microsoft::glTF::Document document{};
        try {
            document = Microsoft::glTF::Deserialize(manifest);
        }
        catch (const Microsoft::glTF::GLTFException& ex) {
            std::stringstream ss;

            ss << "Microsoft::glTF::Deserialize failed: ";
            ss << ex.what();

            throw std::runtime_error(ss.str());
        }

        const auto& mesh = document.meshes.Elements()[0];

        for (const auto& meshPrimitive : mesh.primitives) {
            std::string accessorId;

            std::vector<float> vPositions;
            size_t num_vert = 0;

            if (meshPrimitive.TryGetAttributeAccessorId(Microsoft::glTF::ACCESSOR_POSITION, accessorId)) {
                const Microsoft::glTF::Accessor& accessor = document.accessors.Get(accessorId);
                num_vert = accessor.count;
                const auto data = resourceReader->ReadBinaryData<float>(document, accessor);
                vPositions = std::move(data);
            }

            DirectX::XMFLOAT4 color{};
            if (meshPrimitive.TryGetAttributeAccessorId(Microsoft::glTF::ACCESSOR_COLOR_0, accessorId)) {
                const Microsoft::glTF::Accessor& accessor{ document.accessors.Get(accessorId) };
                const auto data = resourceReader->ReadBinaryData<float>(document, accessor);
                color = { data[0], data[1], data[2], data[3]};
            }

            //if (meshPrimitive.TryGetAttributeAccessorId(Microsoft::glTF::ACCESSOR_TEXCOORD_0, accessorId)) {
            //    const Microsoft::glTF::Accessor& accessor = document.accessors.Get(accessorId);
            //    const auto data = resourceReader->ReadBinaryData<float>(document, accessor);
            //    vUV = std::move(data);
            //}

            //std::vector<float> vUV;
            //std::vector<float> vNormals;
            //if (meshPrimitive.TryGetAttributeAccessorId(Microsoft::glTF::ACCESSOR_NORMAL, accessorId)) {
            //    const Microsoft::glTF::Accessor& accessor = document.accessors.Get(accessorId);
            //    const auto data = resourceReader->ReadBinaryData<float>(document, accessor);
            //    vNormals = std::move(data);
            //}

            //std::vector<float> vTangent;
            //if (meshPrimitive.TryGetAttributeAccessorId(Microsoft::glTF::ACCESSOR_TANGENT, accessorId)) {
            //    const Microsoft::glTF::Accessor& accessor = document.accessors.Get(accessorId);
            //    const auto data = resourceReader->ReadBinaryData<float>(document, accessor);
            //    vTangent = std::move(data);
            //}

            indices.clear();
            {
                const Microsoft::glTF::Accessor& accessor = document.accessors.Get(meshPrimitive.indicesAccessorId);
                const auto indexes = resourceReader->ReadBinaryData<uint32_t>(document, accessor);
                indices.reserve(indexes.size());
                for (size_t i{}; i < indexes.size() / 3; ++i) {
                    size_t i3 = i * 3;
                    indices.emplace_back(indexes[i3]);
                    indices.emplace_back(indexes[i3 + 1]);
                    indices.emplace_back(indexes[i3 + 2]);
                }
            }

            vertices.clear();
            {
                vertices.reserve(num_vert);

                for (size_t i{}; i < num_vert; ++i) {
                    size_t i3 = i * 3;
                    size_t i4 = i * 4;
                    size_t i2 = i * 2;
                    vertices.emplace_back(VertexPositionColor{
                        { vPositions[i3], vPositions[i3 + 1], -vPositions[i3 + 2], 1.f },
                        color
                        });
                }
            }
        }
    }
};
