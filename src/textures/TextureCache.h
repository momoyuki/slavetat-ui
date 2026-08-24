#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace stui::textures {

template <class Resource>
class TextureCache {
public:
    explicit TextureCache(std::size_t capacity) : m_capacity(capacity) {}

    template <class Loader>
    std::shared_ptr<Resource> getOrLoad(std::string_view path, Loader&& loader) {
        const auto found = m_entries.find(std::string(path));
        if (found != m_entries.end()) {
            m_recency.splice(m_recency.begin(), m_recency, found->second.recency);
            return found->second.resource;
        }

        auto resource = std::invoke(std::forward<Loader>(loader));
        m_recency.emplace_front(path);
        m_entries.emplace(m_recency.front(), Entry{resource, m_recency.begin()});
        if (m_entries.size() > m_capacity) {
            m_entries.erase(m_recency.back());
            m_recency.pop_back();
        }
        return resource;
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }

private:
    struct Entry {
        std::shared_ptr<Resource> resource;
        typename std::list<std::string>::iterator recency;
    };

    std::size_t m_capacity;
    std::list<std::string> m_recency;
    std::unordered_map<std::string, Entry> m_entries;
};

}  // namespace stui::textures
