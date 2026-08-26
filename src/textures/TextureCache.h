#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace stui::textures {

using TextureCacheClock = std::chrono::steady_clock;
using TextureCacheTimePoint = TextureCacheClock::time_point;
using TextureCacheDuration = TextureCacheClock::duration;

template <class Resource>
class TextureCache {
public:
    explicit TextureCache(
        std::size_t capacity,
        TextureCacheDuration idleTtl = TextureCacheDuration::max())
        : m_capacity(capacity), m_idleTtl(idleTtl) {}

    [[nodiscard]] std::shared_ptr<Resource> get(
        std::string_view path,
        TextureCacheTimePoint now = TextureCacheClock::now()) {
        const auto found = m_entries.find(std::string(path));
        if (found == m_entries.end()) {
            return nullptr;
        }

        m_recency.splice(m_recency.begin(), m_recency, found->second.recency);
        found->second.lastAccess = now;
        return found->second.resource;
    }

    template <class Loader>
    std::shared_ptr<Resource> getOrLoad(
        std::string_view path,
        Loader&& loader,
        TextureCacheTimePoint now = TextureCacheClock::now()) {
        if (auto cached = get(path, now)) {
            return cached;
        }

        auto resource = std::invoke(std::forward<Loader>(loader));
        if (!resource) {
            return nullptr;
        }
        m_recency.emplace_front(path);
        m_entries.emplace(
            m_recency.front(), Entry{resource, m_recency.begin(), now});
        if (m_entries.size() > m_capacity) {
            m_entries.erase(m_recency.back());
            m_recency.pop_back();
        }
        return resource;
    }

    void pruneExpired(TextureCacheTimePoint now = TextureCacheClock::now()) {
        if (m_idleTtl == TextureCacheDuration::max()) {
            return;
        }

        for (auto entry = m_entries.begin(); entry != m_entries.end();) {
            if (now - entry->second.lastAccess >= m_idleTtl) {
                m_recency.erase(entry->second.recency);
                entry = m_entries.erase(entry);
            } else {
                ++entry;
            }
        }
    }

    void clear() noexcept {
        m_entries.clear();
        m_recency.clear();
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }

private:
    struct Entry {
        std::shared_ptr<Resource> resource;
        typename std::list<std::string>::iterator recency;
        TextureCacheTimePoint lastAccess;
    };

    std::size_t m_capacity;
    TextureCacheDuration m_idleTtl;
    std::list<std::string> m_recency;
    std::unordered_map<std::string, Entry> m_entries;
};

}  // namespace stui::textures
