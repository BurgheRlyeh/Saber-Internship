#pragma once

#include <cassert>
#include <functional>
#include <concepts>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

// KeyHashers

template <typename T>
concept KeyHasherConcept = requires {
    typename T::KeyType;
};

template<typename StringType>
struct IdentityKeyHasher {
    using KeyType = StringType;

    KeyType operator()(const StringType& str) const {
        return str;
    }
};

template<typename StringType>
struct HashKeyHasher {
    using KeyType = size_t;

    KeyType operator()(const StringType& str) const {
        return std::hash<StringType>{}(str);
    }
};

// Default types

using DefaultAtlasString = std::wstring;

template <typename StringType>
using DefaultAtlasKeyHasher =
#ifdef _DEBUG
    IdentityKeyHasher<StringType>;
#else
    HashKeyHasher<StringType>;
#endif

// Atlas

template <
    typename T,
    typename StringType = DefaultAtlasString,
    KeyHasherConcept KeyHasher = DefaultAtlasKeyHasher<StringType>
>
class Atlas : public std::enable_shared_from_this<Atlas<T, StringType, KeyHasher>> {
    using AtlasType = Atlas<T, StringType, KeyHasher>;
    using KeyType = typename KeyHasher::KeyType;

    StringType m_prefix;

    struct IdentityHasher {
        size_t operator()(size_t v) const noexcept {
            return v;
        }
    };
    using MapHasher = std::conditional_t<
        std::is_same_v<KeyType, size_t>,
        IdentityHasher,
        std::hash<KeyType>
    >;

    std::unordered_map<KeyType, std::weak_ptr<T>, MapHasher> m_map;

    mutable std::mutex m_mutex;

    [[no_unique_address]]
    KeyHasher m_keyProvider;

    struct Deleter {
        std::weak_ptr<AtlasType> m_pAtlas;
        KeyType m_key;

        Deleter(const std::shared_ptr<AtlasType>& pAtlas, const KeyType& key)
            : m_pAtlas(pAtlas), m_key(key)
        {}

        template<typename U>
        void operator()(U* pItem) const {
            if (auto atlas = m_pAtlas.lock()) {
                std::scoped_lock<std::mutex> lock(atlas->m_mutex);

				// Make sure entry still refers to the item being destroyed
                // but not some other that was assigned under the same key
                auto it{ atlas->m_map.find(m_key) };
                if (it != atlas->m_map.end() && it->second.expired()) {
                    atlas->m_map.erase(it);
                }
            }

            delete pItem;
        }
    };

public:
    explicit Atlas(const StringType& prefix)
        : m_prefix(prefix)
    {}

    ~Atlas() {
        std::scoped_lock<std::mutex> lock(m_mutex);
        assert(m_map.empty());
    }

    const StringType& GetPrefix() const {
        return m_prefix;
    }

    size_t Size() const {
        std::scoped_lock<std::mutex> lock(m_mutex);
        return m_map.size();
    }

    bool Empty() const {
        std::scoped_lock<std::mutex> lock(m_mutex);
        return m_map.empty();
    }

    template<std::derived_from<T> U = T>
    std::shared_ptr<U> Find(const StringType& name) {
        return FindByKey<U>(ToKey(name));
    }

    template <std::derived_from<T> U = T, typename... Params>
    std::shared_ptr<U> Assign(const StringType& name, Params&&... params) {
        KeyType key{ ToKey(name) };

        auto existing{ FindByKey<U>(key) };
        if (existing) {
            return existing;
        }

        std::shared_ptr<U> res{
            new U(m_prefix + name, std::forward<Params>(params)...),
            Deleter(this->shared_from_this(), key)
        };

        std::scoped_lock<std::mutex> lock(m_mutex);

        auto [it, inserted] { m_map.try_emplace(key, res) };
        if (!inserted) {
            // Check if the same item was built by the other thread
            if (auto winner{ std::dynamic_pointer_cast<U>(it->second.lock()) }) {
                return winner;
            }
            it->second = res;   // other is expired, replace
        }
        return res;
    }

private:
    KeyType ToKey(const StringType& name) const {
        return m_keyProvider(name);
    }

    template <std::derived_from<T> U = T>
    std::shared_ptr<U> FindByKey(const KeyType& key) {
        std::scoped_lock<std::mutex> lock(m_mutex);

        auto it{ m_map.find(key) };
        if (it == m_map.end()) {
            return nullptr;
        }

		assert(!it->second.expired() && "Atlas entry expired unexpectedly");
        auto casted{ std::dynamic_pointer_cast<U>(it->second.lock()) };
        assert(casted && "Type mismatch in Atlas");
        return casted;
    }
};
