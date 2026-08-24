#include "textures/TextureCache.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void run(std::string_view name, void (*test)()) {
    try {
        test();
        std::cout << "PASS " << name << '\n';
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        std::exit(1);
    }
}

void reusesCachedTextureWithoutLoadingAgain() {
    stui::textures::TextureCache<int> cache(2);
    int loadCount = 0;
    const auto loader = [&]() {
        ++loadCount;
        return std::make_shared<int>(42);
    };

    const auto first = cache.getOrLoad("pack\\tattoo.dds", loader);
    const auto second = cache.getOrLoad("pack\\tattoo.dds", loader);

    expect(first == second, "expected the cached resource instance");
    expect(loadCount == 1, "expected one resource load");
    expect(cache.size() == 1, "expected one cache entry");
}

void evictsLeastRecentlyUsedTextureAtCapacity() {
    stui::textures::TextureCache<int> cache(2);
    int nextValue = 0;
    const auto load = [&]() {
        return std::make_shared<int>(++nextValue);
    };

    const auto firstA = cache.getOrLoad("a.dds", load);
    const auto firstB = cache.getOrLoad("b.dds", load);
    const auto recentA = cache.getOrLoad("a.dds", load);
    cache.getOrLoad("c.dds", load);
    const auto secondB = cache.getOrLoad("b.dds", load);

    expect(firstA == recentA, "expected cache hit to refresh recency");
    expect(firstB != secondB, "expected least recently used texture eviction");
    expect(nextValue == 4, "expected evicted texture to load again");
    expect(cache.size() == 2, "expected cache to remain within capacity");
}

}  // namespace

int main() {
    run("cache hit skips duplicate texture load", reusesCachedTextureWithoutLoadingAgain);
    run("capacity evicts least recently used texture", evictsLeastRecentlyUsedTextureAtCapacity);
    return 0;
}
