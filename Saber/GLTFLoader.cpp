#include "GLTFLoader.h"

// The glTF SDK is decoupled from all file I/O by the IStreamReader (and IStreamWriter)
// interface(s) and the C++ stream-based I/O library. This allows the glTF SDK to be used in
// sandboxed environments, such as WebAssembly modules and UWP apps, where any file I/O code
// must be platform or use-case specific.
class StreamReader : public Microsoft::glTF::IStreamReader {
public:
    StreamReader(std::filesystem::path pathBase) : m_pathBase(std::move(pathBase)) {
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

GLTFLoader::GLTFLoader(std::filesystem::path& filepath) {
    CheckFilepathCorrectness(filepath);
    GetResourceReaderAndDocument(filepath, m_pResourceReader, m_document);
}


DXGI_FORMAT GLTFLoader::GetIndicesFormat()
{
    const auto& mesh = m_document.meshes.Elements()[0];
    const Microsoft::glTF::Accessor& accessor = m_document.accessors.Get(mesh.primitives.front().indicesAccessorId);

    return GetDXGIFormat(accessor);
}

DXGI_FORMAT GLTFLoader::GetDXGIFormat(const Microsoft::glTF::Accessor& accessor)
{
    switch (accessor.componentType)
    {
    case Microsoft::glTF::COMPONENT_BYTE:
        return DXGI_FORMAT_R8_SINT;
    case Microsoft::glTF::COMPONENT_UNSIGNED_BYTE:
        return DXGI_FORMAT_R8_UINT;
        break;
    case Microsoft::glTF::COMPONENT_SHORT:
        return DXGI_FORMAT_R16_SINT;
        break;
    case Microsoft::glTF::COMPONENT_UNSIGNED_SHORT:
        return DXGI_FORMAT_R16_UINT;
        break;
    case Microsoft::glTF::COMPONENT_UNSIGNED_INT:
        return DXGI_FORMAT_R32_UINT;
        break;
    case Microsoft::glTF::COMPONENT_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
        break;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

bool GLTFLoader::GetVerticesData(std::vector<float>& data, const std::string& attributeName) {
    const auto& mesh = m_document.meshes.Elements()[0];

    std::string accessorId{};
    if (!mesh.primitives.front().TryGetAttributeAccessorId(attributeName, accessorId)) {
        return false;
    }

    const Microsoft::glTF::Accessor& accessor{ m_document.accessors.Get(accessorId) };
    data = m_pResourceReader->ReadBinaryData<float>(m_document, accessor);

    return true;
}

void GLTFLoader::CheckFilepathCorrectness(std::filesystem::path& filepath) {
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
}

void GLTFLoader::GetResourceReaderAndDocument(const std::filesystem::path& filepath, std::unique_ptr<Microsoft::glTF::GLBResourceReader>& pResourceReader, Microsoft::glTF::Document& document) {
    // Pass the absolute path, without the filename, to the stream reader
    std::unique_ptr<StreamReader> streamReader{ std::make_unique<StreamReader>(filepath.parent_path()) };

    std::filesystem::path filename{ filepath.filename() };
    std::filesystem::path fileExtension{ filename.extension() };

    auto MakePathExt = [](const std::string& ext) { return "." + ext; };
    assert(fileExtension == MakePathExt(Microsoft::glTF::GLB_EXTENSION));

    // Pass a UTF-8 encoded filename to GetInputString
    std::u8string u8string{ filename.u8string() };
    std::shared_ptr<std::istream> pGLBStream{
        streamReader->GetInputStream(std::string(u8string.begin(), u8string.end()))
    };
    pResourceReader = std::make_unique<Microsoft::glTF::GLBResourceReader>(
        std::move(streamReader),
        std::move(pGLBStream)
    );

    if (!pResourceReader) {
        throw std::runtime_error("Command line argument path filename extension must be .gltf or .glb");
    }

    std::string manifest{ pResourceReader->GetJson() }; // Get the manifest from the JSON chunk

    try {
        document = Microsoft::glTF::Deserialize(manifest);
    }
    catch (const Microsoft::glTF::GLTFException& ex) {
        std::stringstream ss;

        ss << "Microsoft::glTF::Deserialize failed: ";
        ss << ex.what();

        throw std::runtime_error(ss.str());
    }
}
