// SPDX-License-Identifier: GPL-3.0-only
#include "travesty/factory.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#elif defined(_WIN32)
#include <windows.h>
#include <filesystem>
#else
#include <dlfcn.h>
#endif

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
struct Module {
#ifdef __APPLE__
    CFBundleRef bundle = nullptr;
#elif defined(_WIN32)
    HMODULE bundle = nullptr;
#else
    void *bundle = nullptr;
#endif
    v3_plugin_factory **factory = nullptr;
    bool (*exit)() = nullptr;
    explicit Module(const char *path)
    {
#ifdef __APPLE__
        auto url =
            CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8 *>(path),
                                                    static_cast<CFIndex>(std::strlen(path)), true);
        require(url != nullptr, "Cannot create VST3 bundle URL");
        bundle = CFBundleCreate(nullptr, url);
        CFRelease(url);
        require(bundle != nullptr && CFBundleLoadExecutable(bundle), "Cannot load VST3 executable");
        auto entry = reinterpret_cast<bool (*)(void *)>(
            CFBundleGetFunctionPointerForName(bundle, CFSTR("bundleEntry")));
        exit = reinterpret_cast<bool (*)()>(
            CFBundleGetFunctionPointerForName(bundle, CFSTR("bundleExit")));
        auto getFactory = reinterpret_cast<const void *(*)()>(
            CFBundleGetFunctionPointerForName(bundle, CFSTR("GetPluginFactory")));
        require(entry != nullptr && exit != nullptr && getFactory != nullptr,
                "Missing VST3 entry points");
        require(entry(bundle), "VST3 bundle entry failed");
#elif defined(_WIN32)
        const auto fullPath = std::filesystem::absolute(path).wstring();
        bundle =
            LoadLibraryExW(fullPath.c_str(), nullptr,
                           LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (bundle == nullptr)
            std::fprintf(stderr, "LoadLibraryExW error: %lu\n", GetLastError());
        require(bundle != nullptr, "Cannot load VST3 executable or its runtime dependencies");
        auto entry = reinterpret_cast<bool (*)()>(GetProcAddress(bundle, "InitDll"));
        exit = reinterpret_cast<bool (*)()>(GetProcAddress(bundle, "ExitDll"));
        auto getFactory =
            reinterpret_cast<const void *(*)()>(GetProcAddress(bundle, "GetPluginFactory"));
        require(entry != nullptr && exit != nullptr && getFactory != nullptr,
                "Missing VST3 entry points");
        require(entry(), "VST3 DLL entry failed");
#else
        bundle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (bundle == nullptr)
            std::fprintf(stderr, "dlopen: %s\n", dlerror());
        require(bundle != nullptr, "Cannot load VST3 executable or its runtime dependencies");
        auto entry = reinterpret_cast<bool (*)(void *)>(dlsym(bundle, "ModuleEntry"));
        exit = reinterpret_cast<bool (*)()>(dlsym(bundle, "ModuleExit"));
        auto getFactory = reinterpret_cast<const void *(*)()>(dlsym(bundle, "GetPluginFactory"));
        require(entry != nullptr && exit != nullptr && getFactory != nullptr,
                "Missing VST3 entry points");
        require(entry(bundle), "VST3 module entry failed");
#endif
        factory = static_cast<v3_plugin_factory **>(const_cast<void *>(getFactory()));
        require(factory != nullptr, "VST3 returned no factory");
    }
    ~Module()
    {
        if (factory != nullptr)
            v3_cpp_obj_unref(factory);
        if (exit != nullptr)
            exit();
        if (bundle != nullptr) {
#ifdef __APPLE__
            CFRelease(bundle);
#elif defined(_WIN32)
            FreeLibrary(bundle);
#else
            dlclose(bundle);
#endif
        }
    }
};
} // namespace

int main(int argc, char **argv)
{
    try {
        require(argc >= 2 && argc <= 4,
                "Supply the VST3 bundle on Mac or module file on Linux/Windows, then optional "
                "expected processor name and version");
        Module module(argv[1]);
        auto *factory = v3_cpp_obj(module.factory);
        const auto count = factory->num_classes(module.factory);
        require(count >= 1 && count <= 2, "Unexpected VST3 class count");
        v3_factory_info info{};
        require(factory->get_factory_info(module.factory, &info) == V3_OK &&
                    std::strstr(info.vendor, "StormSurge FX") != nullptr,
                "Incorrect VST3 factory metadata");
        // Each StormSurge plugin has its own name, so the caller states which one
        // it expects rather than the harness hard-coding a single identity.
        const char *expectedName = argc >= 3 ? argv[2] : "Undertow Preview";
        v3_class_info processor{};
        require(factory->get_class_info(module.factory, 0, &processor) == V3_OK &&
                    std::strcmp(processor.name, expectedName) == 0 &&
                    std::strcmp(processor.category, "Audio Module Class") == 0,
                "Incorrect VST3 processor metadata");
        if (argc >= 4) {
            v3_plugin_factory_2 **extended = nullptr;
            require(v3_cpp_obj_query_interface(module.factory, v3_plugin_factory_2_iid,
                                               &extended) == V3_OK &&
                        extended != nullptr,
                    "Missing VST3 version metadata interface");
            v3_class_info_2 versioned{};
            const auto status = v3_cpp_obj(extended)->get_class_info_2(extended, 0, &versioned);
            const bool matches = status == V3_OK && std::strcmp(versioned.version, argv[3]) == 0;
            v3_cpp_obj_unref(extended);
            require(matches, "Packaged plugin version does not match the release manifest");
        }
        std::puts("PASS: loaded VST3, entered module and read/released factory without devices "
                  "or installation");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
