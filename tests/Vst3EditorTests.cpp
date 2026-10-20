// SPDX-License-Identifier: GPL-3.0-only
#include "travesty/component.h"
#include "travesty/edit_controller.h"
#include "travesty/factory.h"
#include "travesty/host.h"
#include "travesty/message.h"
#include "travesty/view.h"

#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

using Attribute = std::variant<int64_t, double, std::vector<int16_t>, std::vector<uint8_t>>;

struct HostAttributes : v3_attribute_list_cpp {
    v3_attribute_list *interfacePointer = reinterpret_cast<v3_attribute_list *>(this);
    uint32_t references = 1;
    std::map<std::string, Attribute> values;
    bool owned = false;

    HostAttributes()
    {
        query_interface = query;
        ref = addRef;
        unref = release;
        attrlist.set_int = setInt;
        attrlist.get_int = getInt;
        attrlist.set_float = setFloat;
        attrlist.get_float = getFloat;
        attrlist.set_string = setString;
        attrlist.get_string = getString;
        attrlist.set_binary = setBinary;
        attrlist.get_binary = getBinary;
    }

    static HostAttributes &from(void *self)
    {
        return *reinterpret_cast<HostAttributes *>(*static_cast<v3_attribute_list **>(self));
    }
    static v3_result V3_API query(void *self, const v3_tuid iid, void **object)
    {
        if (std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) != 0 &&
            std::memcmp(iid, v3_attribute_list_iid, sizeof(v3_tuid)) != 0) {
            *object = nullptr;
            return V3_NO_INTERFACE;
        }
        auto &attributes = from(self);
        *object = &attributes.interfacePointer;
        addRef(self);
        return V3_OK;
    }
    static uint32_t V3_API addRef(void *self) { return ++from(self).references; }
    static uint32_t V3_API release(void *self)
    {
        auto &attributes = from(self);
        const uint32_t references = --attributes.references;
        if (references == 0 && attributes.owned)
            delete &attributes;
        return references;
    }
    static v3_result V3_API setInt(void *self, const char *id, int64_t value)
    {
        from(self).values[id] = value;
        return V3_OK;
    }
    static v3_result V3_API getInt(void *self, const char *id, int64_t *value)
    {
        auto &values = from(self).values;
        const auto found = values.find(id);
        if (found == values.end() || !std::holds_alternative<int64_t>(found->second))
            return V3_FALSE;
        *value = std::get<int64_t>(found->second);
        return V3_OK;
    }
    static v3_result V3_API setFloat(void *self, const char *id, double value)
    {
        from(self).values[id] = value;
        return V3_OK;
    }
    static v3_result V3_API getFloat(void *self, const char *id, double *value)
    {
        auto &values = from(self).values;
        const auto found = values.find(id);
        if (found == values.end() || !std::holds_alternative<double>(found->second))
            return V3_FALSE;
        *value = std::get<double>(found->second);
        return V3_OK;
    }
    static v3_result V3_API setString(void *self, const char *id, const int16_t *value)
    {
        std::vector<int16_t> copy;
        while (*value != 0)
            copy.push_back(*value++);
        copy.push_back(0);
        from(self).values[id] = std::move(copy);
        return V3_OK;
    }
    static v3_result V3_API getString(void *self, const char *id, int16_t *value, uint32_t size)
    {
        auto &values = from(self).values;
        const auto found = values.find(id);
        if (found == values.end() || !std::holds_alternative<std::vector<int16_t>>(found->second))
            return V3_FALSE;
        const auto &string = std::get<std::vector<int16_t>>(found->second);
        const auto count = std::min<std::size_t>(string.size(), size);
        std::copy_n(string.data(), count, value);
        if (count != 0)
            value[count - 1] = 0;
        return V3_OK;
    }
    static v3_result V3_API setBinary(void *self, const char *id, const void *data, uint32_t size)
    {
        const auto *bytes = static_cast<const uint8_t *>(data);
        from(self).values[id] = std::vector<uint8_t>(bytes, bytes + size);
        return V3_OK;
    }
    static v3_result V3_API getBinary(void *self, const char *id, const void **data, uint32_t *size)
    {
        auto &values = from(self).values;
        const auto found = values.find(id);
        if (found == values.end() || !std::holds_alternative<std::vector<uint8_t>>(found->second))
            return V3_FALSE;
        const auto &bytes = std::get<std::vector<uint8_t>>(found->second);
        *data = bytes.data();
        *size = static_cast<uint32_t>(bytes.size());
        return V3_OK;
    }
};

struct HostMessage : v3_message_cpp {
    v3_message *interfacePointer = reinterpret_cast<v3_message *>(this);
    uint32_t references = 1;
    std::string id;
    HostAttributes attributes;

    HostMessage()
    {
        query_interface = query;
        ref = addRef;
        unref = release;
        msg.get_message_id = getId;
        msg.set_message_id = setId;
        msg.get_attributes = getAttributes;
    }
    static HostMessage &from(void *self)
    {
        return *reinterpret_cast<HostMessage *>(*static_cast<v3_message **>(self));
    }
    static v3_result V3_API query(void *self, const v3_tuid iid, void **object)
    {
        if (std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) != 0 &&
            std::memcmp(iid, v3_message_iid, sizeof(v3_tuid)) != 0) {
            *object = nullptr;
            return V3_NO_INTERFACE;
        }
        auto &message = from(self);
        *object = &message.interfacePointer;
        addRef(self);
        return V3_OK;
    }
    static uint32_t V3_API addRef(void *self) { return ++from(self).references; }
    static uint32_t V3_API release(void *self)
    {
        auto &message = from(self);
        const uint32_t references = --message.references;
        if (references == 0)
            delete &message;
        return references;
    }
    static const char *V3_API getId(void *self) { return from(self).id.c_str(); }
    static void V3_API setId(void *self, const char *id) { from(self).id = id; }
    static v3_attribute_list **V3_API getAttributes(void *self)
    {
        auto &message = from(self);
        return &message.attributes.interfacePointer;
    }
};

struct HostRunLoop : v3_run_loop_cpp {
    struct Timer {
        v3_timer_handler *handler;
        std::chrono::steady_clock::time_point deadline;
        std::chrono::milliseconds interval;
    };
    struct Event {
        v3_event_handler *handler;
        int fd;
    };
    v3_run_loop *interfacePointer = reinterpret_cast<v3_run_loop *>(this);
    uint32_t references = 1;
    std::vector<Timer> timers;
    std::vector<Event> events;

    HostRunLoop()
    {
        query_interface = query;
        ref = addRef;
        unref = release;
        loop.register_event_handler = registerEvent;
        loop.unregister_event_handler = unregisterEvent;
        loop.register_timer = registerTimer;
        loop.unregister_timer = unregisterTimer;
    }
    static HostRunLoop &from(void *self)
    {
        return *reinterpret_cast<HostRunLoop *>(*static_cast<v3_run_loop **>(self));
    }
    static v3_result V3_API query(void *self, const v3_tuid iid, void **object)
    {
        if (std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) != 0 &&
            std::memcmp(iid, v3_run_loop_iid, sizeof(v3_tuid)) != 0) {
            *object = nullptr;
            return V3_NO_INTERFACE;
        }
        auto &runLoop = from(self);
        *object = &runLoop.interfacePointer;
        addRef(self);
        return V3_OK;
    }
    static uint32_t V3_API addRef(void *self) { return ++from(self).references; }
    static uint32_t V3_API release(void *self)
    {
        auto &runLoop = from(self);
        if (runLoop.references > 1)
            --runLoop.references;
        return runLoop.references;
    }
    static v3_result V3_API registerEvent(void *self, v3_event_handler **handler, int fd)
    {
        from(self).events.push_back({*handler, fd});
        return V3_OK;
    }
    static v3_result V3_API unregisterEvent(void *self, v3_event_handler **handler)
    {
        auto &events = from(self).events;
        std::erase_if(events, [handler](const Event &event) { return event.handler == *handler; });
        return V3_OK;
    }
    static v3_result V3_API registerTimer(void *self, v3_timer_handler **handler, uint64_t ms)
    {
        auto &timers = from(self).timers;
        const auto interval = std::chrono::milliseconds(std::max<uint64_t>(1, ms));
        timers.push_back({*handler, std::chrono::steady_clock::now() + interval, interval});
        return V3_OK;
    }
    static v3_result V3_API unregisterTimer(void *self, v3_timer_handler **handler)
    {
        auto &timers = from(self).timers;
        std::erase_if(timers, [handler](const Timer &timer) { return timer.handler == *handler; });
        return V3_OK;
    }
    void dispatch()
    {
        const auto now = std::chrono::steady_clock::now();
        for (auto &timer : timers) {
            if (timer.deadline > now)
                continue;
            timer.deadline = now + timer.interval;
            auto *handler = timer.handler;
            v3_cpp_obj(&handler)->on_timer(&handler);
        }
        if (events.empty())
            return;
        std::vector<pollfd> descriptors;
        descriptors.reserve(events.size());
        for (const auto &event : events)
            descriptors.push_back({event.fd, POLLIN, 0});
        if (poll(descriptors.data(), descriptors.size(), 0) <= 0)
            return;
        for (std::size_t i = 0; i < descriptors.size(); ++i) {
            if ((descriptors[i].revents & POLLIN) == 0)
                continue;
            auto *handler = events[i].handler;
            v3_cpp_obj(&handler)->on_fd_is_set(&handler, events[i].fd);
        }
    }
};

struct HostFrame : v3_plugin_frame_cpp {
    v3_plugin_frame *interfacePointer = reinterpret_cast<v3_plugin_frame *>(this);
    uint32_t references = 1;
    HostRunLoop &runLoop;
    Display *display;
    Window parent;

    HostFrame(HostRunLoop &loop, Display *xDisplay, Window xParent)
        : runLoop(loop), display(xDisplay), parent(xParent)
    {
        query_interface = query;
        ref = addRef;
        unref = release;
        frame.resize_view = resizeView;
    }
    static HostFrame &from(void *self)
    {
        return *reinterpret_cast<HostFrame *>(*static_cast<v3_plugin_frame **>(self));
    }
    static v3_result V3_API query(void *self, const v3_tuid iid, void **object)
    {
        auto &host = from(self);
        if (std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) == 0 ||
            std::memcmp(iid, v3_plugin_frame_iid, sizeof(v3_tuid)) == 0) {
            *object = &host.interfacePointer;
            addRef(self);
            return V3_OK;
        }
        if (std::memcmp(iid, v3_run_loop_iid, sizeof(v3_tuid)) == 0) {
            *object = &host.runLoop.interfacePointer;
            HostRunLoop::addRef(&host.runLoop.interfacePointer);
            return V3_OK;
        }
        *object = nullptr;
        return V3_NO_INTERFACE;
    }
    static uint32_t V3_API addRef(void *self) { return ++from(self).references; }
    static uint32_t V3_API release(void *self)
    {
        auto &host = from(self);
        if (host.references > 1)
            --host.references;
        return host.references;
    }
    static v3_result V3_API resizeView(void *self, v3_plugin_view **view, v3_view_rect *rect)
    {
        auto &host = from(self);
        const auto width = rect->right - rect->left;
        const auto height = rect->bottom - rect->top;
        XResizeWindow(host.display, host.parent, static_cast<unsigned>(width),
                      static_cast<unsigned>(height));
        return v3_cpp_obj(view)->on_size(view, rect);
    }
};

struct HostComponentHandler : v3_component_handler_cpp {
    v3_component_handler *interfacePointer = reinterpret_cast<v3_component_handler *>(this);
    uint32_t references = 1;

    HostComponentHandler()
    {
        query_interface = query;
        ref = addRef;
        unref = release;
        comp.begin_edit = beginEdit;
        comp.perform_edit = performEdit;
        comp.end_edit = endEdit;
        comp.restart_component = restartComponent;
    }
    static HostComponentHandler &from(void *self)
    {
        return *reinterpret_cast<HostComponentHandler *>(
            *static_cast<v3_component_handler **>(self));
    }
    static v3_result V3_API query(void *self, const v3_tuid iid, void **object)
    {
        if (std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) != 0 &&
            std::memcmp(iid, v3_component_handler_iid, sizeof(v3_tuid)) != 0) {
            *object = nullptr;
            return V3_NO_INTERFACE;
        }
        auto &handler = from(self);
        *object = &handler.interfacePointer;
        addRef(self);
        return V3_OK;
    }
    static uint32_t V3_API addRef(void *self) { return ++from(self).references; }
    static uint32_t V3_API release(void *self)
    {
        auto &handler = from(self);
        if (handler.references > 1)
            --handler.references;
        return handler.references;
    }
    static v3_result V3_API beginEdit(void *, v3_param_id) { return V3_OK; }
    static v3_result V3_API performEdit(void *, v3_param_id, double) { return V3_OK; }
    static v3_result V3_API endEdit(void *, v3_param_id) { return V3_OK; }
    static v3_result V3_API restartComponent(void *, int32_t) { return V3_OK; }
};

struct HostApplication : v3_host_application_cpp {
    uint32_t references = 1;
    v3_host_application *interfacePointer = reinterpret_cast<v3_host_application *>(this);
    v3_funknown *unknownPointer = reinterpret_cast<v3_funknown *>(this);

    HostApplication()
    {
        query_interface = query;
        ref = addRef;
        unref = release;
        app.get_name = getName;
        app.create_instance = createInstance;
    }

    static HostApplication &from(void *self)
    {
        auto **interfacePointer = static_cast<v3_host_application **>(self);
        return *reinterpret_cast<HostApplication *>(*interfacePointer);
    }

    static v3_result V3_API query(void *self, const v3_tuid iid, void **object)
    {
        if (std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) != 0 &&
            std::memcmp(iid, v3_host_application_iid, sizeof(v3_tuid)) != 0) {
            *object = nullptr;
            return V3_NO_INTERFACE;
        }
        auto &host = from(self);
        *object = &host.interfacePointer;
        addRef(self);
        return V3_OK;
    }

    static uint32_t V3_API addRef(void *self) { return ++from(self).references; }

    static uint32_t V3_API release(void *self)
    {
        auto &host = from(self);
        if (host.references > 1)
            --host.references;
        return host.references;
    }

    static v3_result V3_API getName(void *, v3_str_128 name)
    {
        constexpr char16_t hostName[] = u"StormSurge Practice Host";
        for (std::size_t i = 0; i < std::size(hostName); ++i)
            name[i] = static_cast<int16_t>(hostName[i]);
        return V3_OK;
    }

    static v3_result V3_API createInstance(void *, v3_tuid classId, v3_tuid iid, void **object)
    {
        *object = nullptr;
        if (std::memcmp(classId, v3_message_iid, sizeof(v3_tuid)) == 0 &&
            std::memcmp(iid, v3_message_iid, sizeof(v3_tuid)) == 0) {
            auto *message = new HostMessage();
            *object = &message->interfacePointer;
            return V3_OK;
        }
        if (std::memcmp(classId, v3_attribute_list_iid, sizeof(v3_tuid)) == 0 &&
            std::memcmp(iid, v3_attribute_list_iid, sizeof(v3_tuid)) == 0) {
            auto *attributes = new HostAttributes();
            attributes->owned = true;
            *object = &attributes->interfacePointer;
            return V3_OK;
        }
        return V3_NO_INTERFACE;
    }
};

class Module
{
  public:
    explicit Module(const char *path)
    {
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr)
            throw std::runtime_error(dlerror());
        entry = reinterpret_cast<bool (*)(void *)>(dlsym(handle, "ModuleEntry"));
        exit = reinterpret_cast<bool (*)()>(dlsym(handle, "ModuleExit"));
        const auto getFactory =
            reinterpret_cast<const void *(*)()>(dlsym(handle, "GetPluginFactory"));
        require(entry != nullptr && exit != nullptr && getFactory != nullptr,
                "VST3 module is missing its entry points");
        require(entry(handle), "VST3 module entry failed");
        factory = static_cast<v3_plugin_factory **>(const_cast<void *>(getFactory()));
        require(factory != nullptr, "VST3 returned no factory");
    }

    ~Module()
    {
        if (factory != nullptr)
            v3_cpp_obj_unref(factory);
        if (exit != nullptr)
            exit();
        if (handle != nullptr)
            dlclose(handle);
    }

    v3_plugin_factory **factory = nullptr;

  private:
    void *handle = nullptr;
    bool (*entry)(void *) = nullptr;
    bool (*exit)() = nullptr;
};

void verifyEditor(Module &module, HostApplication &host, bool show)
{
    auto *factory = v3_cpp_obj(module.factory);
    v3_plugin_factory_3 **factory3 = nullptr;
    require(v3_cpp_obj_query_interface(module.factory, v3_plugin_factory_3_iid, &factory3) ==
                    V3_OK &&
                factory3 != nullptr,
            "VST3 factory has no host-context interface");
    require(v3_cpp_obj(factory3)->set_host_context(factory3, &host.unknownPointer) == V3_OK,
            "VST3 factory rejected the host context");

    v3_class_info processorInfo{};
    require(factory->get_class_info(module.factory, 0, &processorInfo) == V3_OK,
            "Could not read the VST3 processor class");
    v3_component **component = nullptr;
    require(factory->create_instance(module.factory, processorInfo.class_id, v3_component_iid,
                                     reinterpret_cast<void **>(&component)) == V3_OK &&
                component != nullptr,
            "Could not create VST3 component");
    require(v3_cpp_obj_initialize(component, &host.unknownPointer) == V3_OK,
            "Could not initialize VST3 component");

    v3_tuid controllerClass{};
    require(v3_cpp_obj(component)->get_controller_class_id(component, controllerClass) == V3_OK,
            "VST3 component did not identify its edit controller");
    v3_edit_controller **controller = nullptr;
    require(factory->create_instance(module.factory, controllerClass, v3_edit_controller_iid,
                                     reinterpret_cast<void **>(&controller)) == V3_OK &&
                controller != nullptr,
            "Could not create VST3 edit controller");
    require(v3_cpp_obj_initialize(controller, &host.unknownPointer) == V3_OK,
            "Could not initialize VST3 edit controller");
    HostComponentHandler componentHandler;
    require(v3_cpp_obj(controller)
                    ->set_component_handler(controller, &componentHandler.interfacePointer) ==
                V3_OK,
            "Could not install the VST3 component handler");

    v3_plugin_view **view = v3_cpp_obj(controller)->create_view(controller, "editor");
    require(view != nullptr, "VST3 did not provide its existing editor view");
    const auto supported =
        v3_cpp_obj(view)->is_platform_type_supported(view, V3_VIEW_PLATFORM_TYPE_X11);
    require(supported == V3_OK, "VST3 editor does not support the Linux X11 embedding type");
    v3_view_rect bounds{};
    require(v3_cpp_obj(view)->get_size(view, &bounds) == V3_OK && bounds.right > bounds.left &&
                bounds.bottom > bounds.top,
            "VST3 editor reported invalid dimensions");

    std::printf("PASS: VST3 editor view created for %s (%d x %d)\n", processorInfo.name,
                bounds.right - bounds.left, bounds.bottom - bounds.top);

    if (show) {
        Display *display = XOpenDisplay(nullptr);
        require(display != nullptr, "Could not open the X11 display");
        const int screen = DefaultScreen(display);
        const auto width = static_cast<unsigned>(bounds.right - bounds.left);
        const auto height = static_cast<unsigned>(bounds.bottom - bounds.top);
        const Window parent =
            XCreateSimpleWindow(display, RootWindow(display, screen), 20, 20, width, height, 0,
                                BlackPixel(display, screen), BlackPixel(display, screen));
        XStoreName(display, parent, processorInfo.name);
        XSelectInput(display, parent, StructureNotifyMask | ExposureMask);
        const Atom closeAtom = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, parent, const_cast<Atom *>(&closeAtom), 1);
        XMapWindow(display, parent);
        XSync(display, False);

        HostRunLoop runLoop;
        HostFrame frame(runLoop, display, parent);
        require(v3_cpp_obj(view)->set_frame(view, &frame.interfacePointer) == V3_OK,
                "VST3 editor rejected its host frame");
        require(v3_cpp_obj(view)->attached(view, reinterpret_cast<void *>(parent),
                                           V3_VIEW_PLATFORM_TYPE_X11) == V3_OK,
                "Could not attach the VST3 editor to the X11 host window");
        std::printf("PASS: attached the real %s editor; close the host window to finish\n",
                    processorInfo.name);
        std::fflush(stdout);
        bool keepOpen = true;
        while (keepOpen) {
            while (XPending(display) != 0) {
                XEvent event{};
                XNextEvent(display, &event);
                if (event.type == ClientMessage &&
                    static_cast<Atom>(event.xclient.data.l[0]) == closeAtom)
                    keepOpen = false;
            }
            runLoop.dispatch();
            pollfd descriptor{ConnectionNumber(display), POLLIN, 0};
            poll(&descriptor, 1, 5);
        }
        require(v3_cpp_obj(view)->removed(view) == V3_OK, "VST3 editor did not detach cleanly");
        std::puts("PASS: detached the VST3 editor cleanly");
        v3_cpp_obj_unref(view);
        view = nullptr;
        XDestroyWindow(display, parent);
        XCloseDisplay(display);
    }

    if (view != nullptr)
        v3_cpp_obj_unref(view);
    require(v3_cpp_obj_terminate(controller) == V3_OK, "Could not terminate VST3 edit controller");
    v3_cpp_obj_unref(controller);
    require(v3_cpp_obj_terminate(component) == V3_OK, "Could not terminate VST3 component");
    v3_cpp_obj_unref(component);
    v3_cpp_obj_unref(factory3);
}
} // namespace

int main(int argc, char **argv)
{
    try {
        const bool show = argc == 3 && std::strcmp(argv[2], "--view") == 0;
        require((argc == 2 || show), "Usage: vst3_editor_tests <module> [--view]");
        HostApplication host;
        Module module(argv[1]);
        verifyEditor(module, host, show);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    return 0;
}
