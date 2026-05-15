/**
 * @file Atlas.h
 * @brief Resource caches (atlases) that manage shared ownership of named assets.
 *
 * Provides two cache variants:
 *  - @ref StringAtlas — keys resources by their filename string.
 *  - @ref HashAtlas   — keys resources by the hash of their filename for faster lookup.
 *
 * In Debug builds the macro @c Atlas expands to @ref StringAtlas for easier
 * debugging; in Release builds it expands to the faster @ref HashAtlas.
 */
#pragma once

#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>

/** @brief Default string type used as atlas key (wide-character string). */
using atlas_string = std::wstring;

/**
 * @brief Resource cache keyed by filename string.
 *
 * Maintains weak references to shared resources; a resource is evicted from
 * the map automatically when its last @c shared_ptr owner is destroyed.
 *
 * @tparam T           Resource type managed by this atlas.
 * @tparam STRING_TYPE String type used as the map key (default: @ref atlas_string).
 */
template <typename T, typename STRING_TYPE = atlas_string>
class StringAtlas {
    STRING_TYPE m_resourceFolder;
    std::map<const STRING_TYPE, std::weak_ptr<T>> m_map;

    /** @brief Custom deleter that removes the entry from the atlas on destruction. */
    struct Deleter {
        Deleter(StringAtlas* pAtlas, const STRING_TYPE& filename)
            : m_pAtlas(pAtlas)
            , m_filename(filename)
        {}

        void operator()(T* pItem) {
            m_pAtlas->m_map.erase(m_filename);
            delete pItem;
        }

        StringAtlas* m_pAtlas;
        STRING_TYPE m_filename;
    };

public:
    /**
     * @brief Constructs an atlas rooted at the given resource folder.
     * @param resourceFolder Base directory prepended to every filename.
     */
    StringAtlas(const STRING_TYPE& resourceFolder)
        : m_resourceFolder(resourceFolder)
    {}

    /** @brief Asserts that all resources have been released before destruction. */
    ~StringAtlas() {
        assert(m_map.empty());
    }

    /**
     * @brief Looks up a resource by filename.
     * @param filename Key to look up (relative to the resource folder).
     * @return Shared pointer to the resource, or an empty pointer if not found.
     */
    std::shared_ptr<T> Find(const STRING_TYPE& filename) {
        auto res = m_map.find(filename);
        return res != m_map.end() ? res->second.lock() : std::shared_ptr<T>(nullptr);
    }

    /**
     * @brief Returns an existing resource or creates and caches a new one.
     *
     * The resource is constructed as @c T(resourceFolder + filename, params...).
     * If a live instance already exists it is returned without constructing a new one.
     *
     * @tparam Params Constructor argument types forwarded after the filename.
     * @param  filename Relative filename used as the cache key.
     * @param  params   Additional constructor arguments.
     * @return Shared pointer to the (possibly newly created) resource.
     */
    template <typename... Params>
    std::shared_ptr<T> Assign(const STRING_TYPE& filename, Params... params) {
        std::shared_ptr<T> res{ Find(filename) };
        if (res) {
            return res;
        }
        res = std::shared_ptr<T>(new T(m_resourceFolder + filename, params...), Deleter(this, filename));
        m_map.insert(std::pair<const STRING_TYPE, std::weak_ptr<T>>(filename, res));
        return res;
    }

    /**
     * @brief Inserts a pre-existing shared pointer into the cache under @p filename.
     * @param filename Cache key.
     * @param val      Resource to register.
     * @return @c true if inserted; @c false if the key was already occupied.
     */
    bool Add(const STRING_TYPE& filename, std::shared_ptr<T> val) {
        if (Find(filename)) {
            return false;
        }
        m_map.insert(std::pair<const STRING_TYPE, std::weak_ptr<T>>(filename, val));
        return true;
    }

    /** @brief Reserved for future use; currently a no-op. */
    void Clean() {}

    /**
     * @brief Returns the resource folder this atlas is rooted at.
     * @return Const reference to the folder string.
     */
    const STRING_TYPE& GetResourceFolder() const {
        return m_resourceFolder;
    }
};

/**
 * @brief Resource cache keyed by the hash of the filename string.
 *
 * Identical semantics to @ref StringAtlas but stores @c size_t hashes as map
 * keys, which gives O(log n) comparisons without string allocations.
 *
 * @tparam T           Resource type managed by this atlas.
 * @tparam STRING_TYPE String type whose hash is used as the map key.
 */
template <typename T, typename STRING_TYPE = atlas_string>
class HashAtlas {
    std::hash<STRING_TYPE> m_hasher;
    STRING_TYPE m_resourceFolder;
    std::map<const size_t, std::weak_ptr<T>> m_map;

    /** @brief Custom deleter that removes the hash entry from the atlas on destruction. */
    struct Deleter {
        Deleter(HashAtlas* pAtlas, const size_t& filenameHash)
            : m_pAtlas(pAtlas)
            , m_filenameHash(filenameHash)
        {}

        void operator()(T* pItem) {
            m_pAtlas->m_map.erase(m_filenameHash);
            delete pItem;
        }

        HashAtlas* m_pAtlas;
        size_t m_filenameHash;
    };

public:
    /**
     * @brief Constructs a hash atlas rooted at the given resource folder.
     * @param resourceFolder Base directory prepended to every filename.
     */
    HashAtlas(const STRING_TYPE& resourceFolder)
        : m_resourceFolder(resourceFolder)
    {}

    /** @brief Asserts that all resources have been released before destruction. */
    ~HashAtlas() {
        assert(m_map.empty());
    }

    /**
     * @brief Looks up a resource by pre-computed hash.
     * @param hash Hash of the filename key.
     * @return Shared pointer to the resource, or an empty pointer if not found.
     */
    std::shared_ptr<T> Find(const size_t& hash) {
        auto res = m_map.find(hash);
        return res != m_map.end() ? res->second.lock() : std::shared_ptr<T>(nullptr);
    }

    /**
     * @brief Looks up a resource by filename (hashes internally).
     * @param filename Filename to hash and look up.
     * @return Shared pointer to the resource, or an empty pointer if not found.
     */
    std::shared_ptr<T> Find(const STRING_TYPE& filename) {
        return Find(m_hasher(filename));
    }

    /**
     * @brief Returns an existing resource or creates and caches a new one.
     * @tparam Params Constructor argument types.
     * @param  filename Relative filename; hashed for the cache key.
     * @param  params   Additional constructor arguments forwarded to @c T.
     * @return Shared pointer to the resource.
     */
    template <typename... Params>
    std::shared_ptr<T> Assign(const STRING_TYPE& filename, Params... params) {
        size_t hash{ m_hasher(filename) };
        std::shared_ptr<T> res{ Find(hash) };
        if (res) {
            return res;
        }

        res = std::shared_ptr<T>(new T(m_resourceFolder + filename, params...), Deleter(this, hash));
        m_map.insert(std::pair<const size_t, std::weak_ptr<T>>(hash, res));
        return res;
    }

    /**
     * @brief Inserts a pre-existing shared pointer under the hash of @p filename.
     * @param filename Filename whose hash serves as the cache key.
     * @param val      Resource to register.
     * @return @c true if inserted; @c false if the hash was already occupied.
     */
    bool Add(const STRING_TYPE& filename, std::shared_ptr<T> val) {
        size_t hash{ m_hasher(filename) };
        if (Find(hash)) {
            return false;
        }
        m_map.insert(std::pair<const size_t, std::weak_ptr<T>>(hash, val));
        return true;
    }

    /** @brief Reserved for future use; currently a no-op. */
    void Clean() {}

    /**
     * @brief Returns the resource folder this atlas is rooted at.
     * @return Const reference to the folder string.
     */
    const STRING_TYPE& GetResourceFolder() const {
        return m_resourceFolder;
    }
};

/**
 * @brief Selects @ref StringAtlas in debug builds and @ref HashAtlas in release builds.
 *
 * Use this alias everywhere resources are cached so that debug builds keep
 * human-readable keys while release builds use faster hashed lookup.
 */
#ifdef _DEBUG
#define  Atlas StringAtlas
#else
#define  Atlas HashAtlas
#endif
