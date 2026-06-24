#pragma once

// Minimal JContainers wrapper — only the functions SlaveTatsUI needs.
// Initialized from the jc::root_interface message sent by JContainers at kDataLoaded.

#include "JContainers/jc_interface.h"
#include "RE/Skyrim.h"
#include <string>

namespace jcmini {

inline void* g_domain = nullptr;

// JArray
inline int32_t (*fn_jarr_object)(void*) = nullptr;
inline int32_t (*fn_jarr_count)(void*, int32_t) = nullptr;
inline int32_t (*fn_jarr_getObj)(void*, int32_t, int32_t, int32_t) = nullptr;
inline void    (*fn_jarr_addObj)(void*, int32_t, int32_t, int32_t) = nullptr;
inline void    (*fn_jarr_addInt)(void*, int32_t, int32_t, int32_t) = nullptr;
inline void    (*fn_jarr_clear)(void*, int32_t) = nullptr;

// JMap
inline RE::BSFixedString (*fn_jmap_getStr)(void*, int32_t, RE::BSFixedString, RE::BSFixedString) = nullptr;
inline int32_t           (*fn_jmap_getInt)(void*, int32_t, RE::BSFixedString, int32_t) = nullptr;
inline float             (*fn_jmap_getFlt)(void*, int32_t, RE::BSFixedString, float) = nullptr;

// JValue
inline int32_t (*fn_jval_addToPool)(void*, int32_t, RE::BSFixedString) = nullptr;
inline void    (*fn_jval_cleanPool)(void*, RE::BSFixedString) = nullptr;

template <typename F>
static void bindFunc(const jc::reflection_interface* refl, const char* fn, const char* cls, F& out) {
    out = reinterpret_cast<F>(refl->tes_function_of_class(fn, cls));
}

inline bool Init(const jc::root_interface* root) {
    auto* refl   = root->query_interface<jc::reflection_interface>();
    auto* domain = root->query_interface<jc::domain_interface>();
    if (!refl || !domain) return false;

    g_domain = domain->get_default_domain();

    bindFunc(refl, "object",     "JArray", fn_jarr_object);
    bindFunc(refl, "count",      "JArray", fn_jarr_count);
    bindFunc(refl, "getObj",     "JArray", fn_jarr_getObj);
    bindFunc(refl, "addObj",     "JArray", fn_jarr_addObj);
    bindFunc(refl, "addInt",     "JArray", fn_jarr_addInt);
    bindFunc(refl, "clear",      "JArray", fn_jarr_clear);

    bindFunc(refl, "getStr",     "JMap",   fn_jmap_getStr);
    bindFunc(refl, "getInt",     "JMap",   fn_jmap_getInt);
    bindFunc(refl, "getFlt",     "JMap",   fn_jmap_getFlt);

    bindFunc(refl, "addToPool",  "JValue", fn_jval_addToPool);
    bindFunc(refl, "cleanPool",  "JValue", fn_jval_cleanPool);

    return true;
}

struct JArray {
    static int object() {
        return fn_jarr_object ? fn_jarr_object(g_domain) : 0;
    }
    static int count(int arr) {
        return fn_jarr_count ? fn_jarr_count(g_domain, arr) : 0;
    }
    static int getObj(int arr, int idx, int def = 0) {
        return fn_jarr_getObj ? fn_jarr_getObj(g_domain, arr, idx, def) : def;
    }
    static void addObj(int arr, int obj, int idx = -1) {
        if (fn_jarr_addObj) fn_jarr_addObj(g_domain, arr, obj, idx);
    }
    static void addInt(int arr, int val, int idx = -1) {
        if (fn_jarr_addInt) fn_jarr_addInt(g_domain, arr, val, idx);
    }
    static void clear(int arr) {
        if (fn_jarr_clear) fn_jarr_clear(g_domain, arr);
    }
};

struct JMap {
    static std::string getStr(int obj, const char* key, const char* def = "") {
        if (!fn_jmap_getStr) return def;
        return fn_jmap_getStr(g_domain, obj, RE::BSFixedString(key), RE::BSFixedString(def)).c_str();
    }
    static int getInt(int obj, const char* key, int def = 0) {
        return fn_jmap_getInt ? fn_jmap_getInt(g_domain, obj, RE::BSFixedString(key), def) : def;
    }
    static float getFlt(int obj, const char* key, float def = 0.f) {
        return fn_jmap_getFlt ? fn_jmap_getFlt(g_domain, obj, RE::BSFixedString(key), def) : def;
    }
};

struct JValue {
    static int addToPool(int obj, const char* pool) {
        return fn_jval_addToPool ? fn_jval_addToPool(g_domain, obj, RE::BSFixedString(pool)) : obj;
    }
    static void cleanPool(const char* pool) {
        if (fn_jval_cleanPool) fn_jval_cleanPool(g_domain, RE::BSFixedString(pool));
    }
};

inline bool isReady() { return g_domain != nullptr; }

}  // namespace jcmini
