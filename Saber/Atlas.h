#pragma once

#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>

template <typename T, typename STRING_TYPE = std::string>
class StringAtlas {
    STRING_TYPE m_resourse_folder;
    std::map<const STRING_TYPE, std::weak_ptr<T>> m_map;

    struct Deleter {
        Deleter(StringAtlas* atlas, const STRING_TYPE& filename)
            : m_atlas(atlas)
            , m_filename(filename)
        {}

        void operator()(T* item) {
            m_atlas->m_map.erase(m_filename);

            delete item;
        }

        StringAtlas* m_atlas;
        STRING_TYPE m_filename;
    };

public:
    StringAtlas(const STRING_TYPE& ResourseFolder) {
        m_resourse_folder = ResourseFolder;
    }
    ~StringAtlas() {
        assert(m_map.empty());
    }

    template <typename... Params>
    std::shared_ptr<T> Assign(const STRING_TYPE& filename, Params... params) {
        auto it = m_map.find(filename);
        if (it != m_map.end()) {
            return it->second.lock();
        }
        auto resource = std::shared_ptr<T>(new T(m_resourse_folder + filename, params...), Deleter(this, filename));
        m_map.insert(std::pair<const STRING_TYPE, std::weak_ptr<T>>(filename, resource));
        return resource;

    }

    std::shared_ptr<T> Find(const STRING_TYPE& filename) {
        auto it = m_map.find(filename);
        if (it != m_map.end()) {
            return it->second.lock();
        }
        return std::shared_ptr<T>(nullptr);
    }

    void Clean() {

    }

    const STRING_TYPE& GetResourceFolder() const {
        return m_resourse_folder;
    }
};

template <typename T, typename STRING_TYPE = std::string>
class HashAtlas {
    std::hash<STRING_TYPE> m_hasher;
    STRING_TYPE m_resourse_folder;
    std::map<const size_t, std::weak_ptr<T>> m_map;

    struct Deleter {
        Deleter(HashAtlas* atlas, const size_t& filename_hash)
            : m_atlas(atlas)
            , m_filename_hash(filename_hash)
        {}

        void operator()(T* item) {
            m_atlas->m_map.erase(m_filename_hash);

            delete item;
        }

        HashAtlas* m_atlas;
        size_t m_filename_hash;
    };

public:
    HashAtlas(const STRING_TYPE& ResourseFolder) {
        m_resourse_folder = ResourseFolder;
    }
    ~HashAtlas() {
        assert(m_map.empty());
    }

    template <typename... Params>
    std::shared_ptr<T> Assign(const STRING_TYPE& filename, Params... params) {
        size_t hash = m_hasher(filename);
        auto it = m_map.find(hash);
        if (it != m_map.end()) {
            return it->second.lock();
        }
        auto resource = std::shared_ptr<T>(new T(m_resourse_folder + filename, params...), Deleter(this, hash));
        m_map.insert(std::pair<const size_t, std::weak_ptr<T>>(hash, resource));
        return resource;
    }

    std::shared_ptr<T> Find(const STRING_TYPE& filename) {
        size_t hash = m_hasher(filename);
        auto it = m_map.find(hash);
        if (it != m_map.end())
        {
            return it->second.lock();
        }
        return std::shared_ptr<T>(nullptr);
    }

    void Clean() {}

    const STRING_TYPE& GetResourceFolder() const {
        return m_resourse_folder;
    }
};

#ifdef _DEBUG
#define  Atlas StringAtlas
#else
#define  Atlas HashAtlas
#endif