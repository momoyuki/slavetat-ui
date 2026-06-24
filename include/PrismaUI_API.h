/*
 * PrismaUI API header — copy from PrismaUI-SKSE/example-skse-plugin
 * https://github.com/PrismaUI-SKSE/example-skse-plugin
 */
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <stdint.h>

typedef uint64_t PrismaView;

namespace PRISMA_UI_API
{
    constexpr const auto PrismaUIPluginName = "PrismaUI";

    enum class InterfaceVersion : uint8_t
    {
        V1,
        V2
    };

    typedef void (*OnDomReadyCallback)(PrismaView view);
    typedef void (*JSCallback)(const char *result);
    typedef void (*JSListenerCallback)(const char *argument);

    enum class ConsoleMessageLevel : uint8_t
    {
        Log = 0,
        Warning,
        Error,
        Debug,
        Info
    };

    typedef void (*ConsoleMessageCallback)(PrismaView view, ConsoleMessageLevel level, const char *message);

    class IVPrismaUI1
    {
    protected:
        ~IVPrismaUI1() = default;

    public:
        virtual PrismaView CreateView(const char *htmlPath,
                                      OnDomReadyCallback onDomReadyCallback = nullptr) noexcept = 0;
        virtual void Invoke(PrismaView view, const char *script, JSCallback callback = nullptr) noexcept = 0;
        virtual void InteropCall(PrismaView view, const char *functionName, const char *argument) noexcept = 0;
        virtual void RegisterJSListener(PrismaView view, const char *functionName,
                                        JSListenerCallback callback) noexcept = 0;
        virtual bool HasFocus(PrismaView view) noexcept = 0;
        virtual bool Focus(PrismaView view, bool pauseGame = false, bool disableFocusMenu = false) noexcept = 0;
        virtual void Unfocus(PrismaView view) noexcept = 0;
        virtual void Show(PrismaView view) noexcept = 0;
        virtual void Hide(PrismaView view) noexcept = 0;
        virtual bool IsHidden(PrismaView view) noexcept = 0;
        virtual int GetScrollingPixelSize(PrismaView view) noexcept = 0;
        virtual void SetScrollingPixelSize(PrismaView view, int pixelSize) noexcept = 0;
        virtual bool IsValid(PrismaView view) noexcept = 0;
        virtual void Destroy(PrismaView view) noexcept = 0;
        virtual void SetOrder(PrismaView view, int order) noexcept = 0;
        virtual int GetOrder(PrismaView view) noexcept = 0;
        virtual void CreateInspectorView(PrismaView view) noexcept = 0;
        virtual void SetInspectorVisibility(PrismaView view, bool visible) noexcept = 0;
        virtual bool IsInspectorVisible(PrismaView view) noexcept = 0;
        virtual void SetInspectorBounds(PrismaView view, float topLeftX, float topLeftY, unsigned int width,
                                        unsigned int height) noexcept = 0;
        virtual bool HasAnyActiveFocus() noexcept = 0;
    };

    class IVPrismaUI2 : public IVPrismaUI1
    {
    protected:
        ~IVPrismaUI2() = default;

    public:
        virtual void RegisterConsoleCallback(PrismaView view, ConsoleMessageCallback callback) noexcept = 0;
    };

    template <typename T>
    struct InterfaceVersionMap;

    template <>
    struct InterfaceVersionMap<IVPrismaUI1>
    {
        static constexpr InterfaceVersion version = InterfaceVersion::V1;
    };

    template <>
    struct InterfaceVersionMap<IVPrismaUI2>
    {
        static constexpr InterfaceVersion version = InterfaceVersion::V2;
    };

    typedef void *(*RequestPluginAPIFunc)(InterfaceVersion interfaceVersion);

    [[nodiscard]] inline void *RequestPluginAPI(InterfaceVersion a_interfaceVersion = InterfaceVersion::V1)
    {
        auto pluginHandle = GetModuleHandleA("PrismaUI.dll");
        if (!pluginHandle)
            return nullptr;

        auto requestAPIFunction =
            reinterpret_cast<RequestPluginAPIFunc>(GetProcAddress(pluginHandle, "RequestPluginAPI"));

        if (requestAPIFunction)
            return requestAPIFunction(a_interfaceVersion);

        return nullptr;
    }

    template <typename T>
    [[nodiscard]] inline T *RequestPluginAPI()
    {
        return static_cast<T *>(RequestPluginAPI(InterfaceVersionMap<T>::version));
    }
}
