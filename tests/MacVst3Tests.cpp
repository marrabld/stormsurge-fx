// SPDX-License-Identifier: GPL-3.0-only
#include "travesty/factory.h"

#include <CoreFoundation/CoreFoundation.h>

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
    CFBundleRef bundle = nullptr;
    v3_plugin_factory **factory = nullptr;
    bool (*exit)() = nullptr;
    explicit Module(const char *path)
    {
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
        factory = static_cast<v3_plugin_factory **>(const_cast<void *>(getFactory()));
        require(factory != nullptr, "VST3 returned no factory");
    }
    ~Module()
    {
        if (factory != nullptr)
            v3_cpp_obj_unref(factory);
        if (exit != nullptr)
            exit();
        if (bundle != nullptr)
            CFRelease(bundle);
    }
};
} // namespace

int main(int argc, char **argv)
{
    try {
        require(argc == 2, "Supply the built .vst3 directory");
        Module module(argv[1]);
        auto *factory = v3_cpp_obj(module.factory);
        const auto count = factory->num_classes(module.factory);
        require(count >= 1 && count <= 2, "Unexpected VST3 class count");
        v3_factory_info info{};
        require(factory->get_factory_info(module.factory, &info) == V3_OK &&
                    std::strstr(info.vendor, "StormSurge FX") != nullptr,
                "Incorrect VST3 factory metadata");
        v3_class_info processor{};
        require(factory->get_class_info(module.factory, 0, &processor) == V3_OK &&
                    std::strcmp(processor.name, "Undertow Preview") == 0 &&
                    std::strcmp(processor.category, "Audio Module Class") == 0,
                "Incorrect VST3 processor metadata");
        std::puts("PASS: loaded Mac VST3, entered bundle and read/released factory without devices "
                  "or installation");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
