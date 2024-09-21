#pragma once

#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>

using atlas_string = std::wstring;

template <typename T, typename STRING_TYPE = atlas_string>
class StringAtlas {
    STRING_TYPE m_resourceFolder;
    std::map<const STRING_TYPE, std::weak_ptr<T>> m_map;

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
    StringAtlas(const STRING_TYPE& resourceFolder)
        : m_resourceFolder(resourceFolder) 
    {}
    ~StringAtlas() {
        assert(m_map.empty());
    }

    std::shared_ptr<T> Find(const STRING_TYPE& filename) {
        auto res = m_map.find(filename);
        return res != m_map.end() ? res->second.lock() : std::shared_ptr<T>(nullptr);
    }

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

    bool Add(const STRING_TYPE& filename, std::shared_ptr<T> val) {
        if (Find(filename)) {
            return false;
        }
        
        m_map.insert(std::pair<const STRING_TYPE, std::weak_ptr<T>>(filename, val));
        return true;
    }

    void Clean() {

    }

    const STRING_TYPE& GetResourceFolder() const {
        return m_resourceFolder;
    }
};

template <typename T, typename STRING_TYPE = atlas_string>
class HashAtlas {
    std::hash<STRING_TYPE> m_hasher;
    STRING_TYPE m_resourceFolder;
    std::map<const size_t, std::weak_ptr<T>> m_map;

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
    HashAtlas(const STRING_TYPE& resourceFolder)
        : m_resourceFolder(resourceFolder)
    {}
    ~HashAtlas() {
        assert(m_map.empty());
    }

    std::shared_ptr<T> Find(const size_t& hash) {
        auto res = m_map.find(hash);
        return res != m_map.end() ? res->second.lock() : std::shared_ptr<T>(nullptr);
    }

    std::shared_ptr<T> Find(const STRING_TYPE& filename) {
        return Find(m_hasher(filename));
    }

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

    bool Add(const STRING_TYPE& filename, std::shared_ptr<T> val) {
        size_t hash{ m_hasher(filename) };
        if (Find(hash)) {
            return false;
        }

        m_map.insert(std::pair<const size_t, std::weak_ptr<T>>(hash, val));
        return true;
    }

    void Clean() {}

    const STRING_TYPE& GetResourceFolder() const {
        return m_resourceFolder;
    }
};

#ifdef _DEBUG
#define  Atlas StringAtlas
#else
#define  Atlas HashAtlas
#endif