// SPDX-License-Identifier: GPL-3.0-only
#include "travesty/component.h"
#include "travesty/edit_controller.h"
#include "travesty/factory.h"
#include "travesty/host.h"
#include "travesty/message.h"
#include "travesty/view.h"
#include "travesty/audio_processor.h"
#include "travesty/bstream.h"
#include "PracticeBoardArtwork.hpp"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../external/dpf/dgl/src/nanovg/stb_image.h"

#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#if defined(STORMSURGE_HAVE_JACK)
#include <jack/jack.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
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

struct HostParameterState {
    static constexpr std::size_t Capacity = 64;
    std::array<std::atomic<double>, Capacity> values{};
    std::array<std::atomic<bool>, Capacity> dirty{};
    std::atomic<bool> bypassed{false};
    v3_param_id bypassId = 0;
    bool hasBypass = false;

    void set(v3_param_id id, double value)
    {
        if (id >= Capacity)
            return;
        values[id].store(value, std::memory_order_relaxed);
        dirty[id].store(true, std::memory_order_release);
        if (hasBypass && id == bypassId)
            bypassed.store(value >= 0.5, std::memory_order_release);
    }
};

struct HostValueQueue {
    v3_funknown unknown{};
    v3_param_value_queue queue{};
    v3_param_id id = 0;
    int32_t points = 0;
    int32_t offset = 0;
    double value = 0.0;

    static v3_param_id V3_API getId(void *self)
    {
        return reinterpret_cast<HostValueQueue *>(*static_cast<v3_param_value_queue **>(self))->id;
    }
    static int32_t V3_API getPointCount(void *self)
    {
        return reinterpret_cast<HostValueQueue *>(*static_cast<v3_param_value_queue **>(self))
            ->points;
    }
    static v3_result V3_API getPoint(void *self, int32_t index, int32_t *sampleOffset,
                                     double *pointValue)
    {
        auto &item =
            *reinterpret_cast<HostValueQueue *>(*static_cast<v3_param_value_queue **>(self));
        if (index != 0 || item.points == 0)
            return V3_INVALID_ARG;
        *sampleOffset = item.offset;
        *pointValue = item.value;
        return V3_OK;
    }
    static v3_result V3_API addPoint(void *self, int32_t sampleOffset, double pointValue,
                                     int32_t *index)
    {
        auto &item =
            *reinterpret_cast<HostValueQueue *>(*static_cast<v3_param_value_queue **>(self));
        item.offset = sampleOffset;
        item.value = pointValue;
        item.points = 1;
        *index = 0;
        return V3_OK;
    }

    HostValueQueue()
    {
        queue.get_param_id = getId;
        queue.get_point_count = getPointCount;
        queue.get_point = getPoint;
        queue.add_point = addPoint;
    }
};

struct HostParameterChanges {
    v3_funknown unknown{};
    v3_param_changes changes{};
    std::array<HostValueQueue, HostParameterState::Capacity> queues{};
    std::array<v3_param_value_queue *, HostParameterState::Capacity> interfaces{};
    int32_t count = 0;

    HostParameterChanges()
    {
        changes.get_param_count = getCount;
        changes.get_param_data = getData;
        changes.add_param_data = addData;
        for (std::size_t i = 0; i < queues.size(); ++i)
            interfaces[i] = reinterpret_cast<v3_param_value_queue *>(&queues[i]);
    }
    void collect(HostParameterState &state)
    {
        count = 0;
        for (std::size_t id = 0; id < queues.size(); ++id) {
            if (!state.dirty[id].exchange(false, std::memory_order_acq_rel))
                continue;
            auto &queue = queues[static_cast<std::size_t>(count++)];
            queue.id = static_cast<v3_param_id>(id);
            queue.offset = 0;
            queue.value = state.values[id].load(std::memory_order_relaxed);
            queue.points = 1;
        }
    }
    v3_param_changes *interface() { return reinterpret_cast<v3_param_changes *>(this); }

  private:
    static int32_t V3_API getCount(void *self)
    {
        return reinterpret_cast<HostParameterChanges *>(*static_cast<v3_param_changes **>(self))
            ->count;
    }
    static v3_param_value_queue **V3_API getData(void *self, int32_t index)
    {
        auto &changes =
            *reinterpret_cast<HostParameterChanges *>(*static_cast<v3_param_changes **>(self));
        if (index < 0 || index >= changes.count)
            return nullptr;
        return &changes.interfaces[static_cast<std::size_t>(index)];
    }
    static v3_param_value_queue **V3_API addData(void *self, const v3_param_id *id, int32_t *index)
    {
        auto &changes =
            *reinterpret_cast<HostParameterChanges *>(*static_cast<v3_param_changes **>(self));
        for (int32_t i = 0; i < changes.count; ++i) {
            if (changes.queues[static_cast<std::size_t>(i)].id == *id) {
                *index = i;
                return getData(self, i);
            }
        }
        if (changes.count == static_cast<int32_t>(changes.queues.size()))
            return nullptr;
        const int32_t added = changes.count++;
        changes.queues[static_cast<std::size_t>(added)].id = *id;
        *index = added;
        return getData(self, added);
    }
};

struct HostMemoryStream : v3_bstream_cpp {
    v3_bstream *interfacePointer = reinterpret_cast<v3_bstream *>(this);
    std::vector<std::uint8_t> bytes;
    std::int64_t position = 0;

    HostMemoryStream()
    {
        query_interface = query;
        ref = addRef;
        unref = release;
        stream.read = read;
        stream.write = write;
        stream.seek = seek;
        stream.tell = tell;
    }
    static HostMemoryStream &from(void *self)
    {
        return *reinterpret_cast<HostMemoryStream *>(*static_cast<v3_bstream **>(self));
    }
    static v3_result V3_API query(void *self, const v3_tuid iid, void **object)
    {
        if (std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) != 0 &&
            std::memcmp(iid, v3_bstream_iid, sizeof(v3_tuid)) != 0) {
            *object = nullptr;
            return V3_NO_INTERFACE;
        }
        *object = *static_cast<v3_bstream **>(self);
        addRef(self);
        return V3_OK;
    }
    static uint32_t V3_API addRef(void *) { return 2; }
    static uint32_t V3_API release(void *) { return 1; }
    static v3_result V3_API read(void *self, void *buffer, int32_t count, int32_t *readCount)
    {
        auto &memory = from(self);
        if (count < 0 || memory.position < 0)
            return V3_INVALID_ARG;
        const auto available = memory.position < static_cast<std::int64_t>(memory.bytes.size())
                                   ? memory.bytes.size() - static_cast<std::size_t>(memory.position)
                                   : 0;
        const auto copied = std::min<std::size_t>(available, static_cast<std::size_t>(count));
        if (copied != 0)
            std::memcpy(buffer, memory.bytes.data() + memory.position, copied);
        memory.position += static_cast<std::int64_t>(copied);
        if (readCount != nullptr)
            *readCount = static_cast<int32_t>(copied);
        return V3_OK;
    }
    static v3_result V3_API write(void *self, void *buffer, int32_t count, int32_t *writtenCount)
    {
        auto &memory = from(self);
        if (count < 0 || memory.position < 0 ||
            memory.position + count > static_cast<std::int64_t>(memory.bytes.max_size()))
            return V3_INVALID_ARG;
        const auto end = static_cast<std::size_t>(memory.position + count);
        if (end > memory.bytes.size())
            memory.bytes.resize(end);
        if (count != 0)
            std::memcpy(memory.bytes.data() + memory.position, buffer,
                        static_cast<std::size_t>(count));
        memory.position += count;
        if (writtenCount != nullptr)
            *writtenCount = count;
        return V3_OK;
    }
    static v3_result V3_API seek(void *self, int64_t offset, int32_t mode, int64_t *result)
    {
        auto &memory = from(self);
        const int64_t base = mode == V3_SEEK_SET   ? 0
                             : mode == V3_SEEK_CUR ? memory.position
                             : mode == V3_SEEK_END ? static_cast<int64_t>(memory.bytes.size())
                                                   : -1;
        if (base < 0 || (offset < 0 && base < -offset) ||
            (offset > 0 && base > std::numeric_limits<int64_t>::max() - offset))
            return V3_INVALID_ARG;
        memory.position = base + offset;
        if (result != nullptr)
            *result = memory.position;
        return V3_OK;
    }
    static v3_result V3_API tell(void *self, int64_t *position)
    {
        *position = from(self).position;
        return V3_OK;
    }
};

struct HostComponentHandler : v3_component_handler_cpp {
    v3_component_handler *interfacePointer = reinterpret_cast<v3_component_handler *>(this);
    uint32_t references = 1;
    HostParameterState *parameters = nullptr;

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
    static v3_result V3_API performEdit(void *self, v3_param_id id, double value)
    {
        auto *parameters = from(self).parameters;
        if (parameters != nullptr)
            parameters->set(id, value);
        return V3_OK;
    }
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

class HostEditor
{
  public:
    HostEditor(Module &module, HostApplication &host) : module(module)
    {
        auto *factory = v3_cpp_obj(module.factory);
        v3_plugin_factory_3 **factory3 = nullptr;
        require(v3_cpp_obj_query_interface(module.factory, v3_plugin_factory_3_iid, &factory3) ==
                        V3_OK &&
                    factory3 != nullptr,
                "VST3 factory has no host-context interface");
        const auto setHostResult =
            v3_cpp_obj(factory3)->set_host_context(factory3, &host.unknownPointer);
        v3_cpp_obj_unref(factory3);
        require(setHostResult == V3_OK, "VST3 factory rejected the host context");

        require(factory->get_class_info(module.factory, 0, &info) == V3_OK,
                "Could not read the VST3 processor class");
        require(factory->create_instance(module.factory, info.class_id, v3_component_iid,
                                         reinterpret_cast<void **>(&component)) == V3_OK &&
                    component != nullptr,
                "Could not create VST3 component");
        require(v3_cpp_obj_initialize(component, &host.unknownPointer) == V3_OK,
                "Could not initialize VST3 component");
        componentInitialized = true;
        require(v3_cpp_obj_query_interface(component, v3_audio_processor_iid, &processor) ==
                        V3_OK &&
                    processor != nullptr,
                "VST3 component has no audio processor interface");

        v3_tuid controllerClass{};
        require(v3_cpp_obj(component)->get_controller_class_id(component, controllerClass) == V3_OK,
                "VST3 component did not identify its edit controller");
        require(factory->create_instance(module.factory, controllerClass, v3_edit_controller_iid,
                                         reinterpret_cast<void **>(&controller)) == V3_OK &&
                    controller != nullptr,
                "Could not create VST3 edit controller");
        require(v3_cpp_obj_initialize(controller, &host.unknownPointer) == V3_OK,
                "Could not initialize VST3 edit controller");
        controllerInitialized = true;
        HostMemoryStream state;
        v3_bstream *stateInterface = state.interfacePointer;
        require(v3_cpp_obj(component)->get_state(component, &stateInterface) == V3_OK,
                "Could not read VST3 processor state for its editor");
        state.position = 0;
        require(v3_cpp_obj(controller)->set_component_state(controller, &stateInterface) == V3_OK,
                "Could not synchronize VST3 editor state with its processor");
        componentHandler.parameters = &parameterState;
        for (int32_t index = 0; index < v3_cpp_obj(controller)->get_parameter_count(controller);
             ++index) {
            v3_param_info parameter{};
            if (v3_cpp_obj(controller)->get_parameter_info(controller, index, &parameter) ==
                    V3_OK &&
                ((parameter.flags & V3_PARAM_IS_BYPASS) != 0 ||
                 parameterTitleIsBypass(parameter.title)) &&
                parameter.param_id < HostParameterState::Capacity) {
                parameterState.bypassId = parameter.param_id;
                parameterState.hasBypass = true;
                const double value = v3_cpp_obj(controller)
                                         ->get_parameter_normalised(controller, parameter.param_id);
                parameterState.set(parameter.param_id, value);
                parameterState.dirty[parameter.param_id].store(false, std::memory_order_relaxed);
                break;
            }
        }
        require(parameterState.hasBypass,
                "StormSurge VST3 does not expose a host-controllable bypass parameter");
        require(v3_cpp_obj(controller)
                        ->set_component_handler(controller, &componentHandler.interfacePointer) ==
                    V3_OK,
                "Could not install the VST3 component handler");

        view = v3_cpp_obj(controller)->create_view(controller, "editor");
        require(view != nullptr, "VST3 did not provide its existing editor view");
        require(v3_cpp_obj(view)->is_platform_type_supported(view, V3_VIEW_PLATFORM_TYPE_X11) ==
                    V3_OK,
                "VST3 editor does not support Linux X11 embedding");
        require(v3_cpp_obj(view)->get_size(view, &bounds) == V3_OK && bounds.right > bounds.left &&
                    bounds.bottom > bounds.top,
                "VST3 editor reported invalid dimensions");
    }

    ~HostEditor()
    {
        stopProcessing();
        if (attached)
            v3_cpp_obj(view)->removed(view);
        if (view != nullptr)
            v3_cpp_obj_unref(view);
        if (controllerInitialized)
            v3_cpp_obj_terminate(controller);
        if (controller != nullptr)
            v3_cpp_obj_unref(controller);
        if (processor != nullptr)
            v3_cpp_obj_unref(processor);
        if (componentInitialized)
            v3_cpp_obj_terminate(component);
        if (component != nullptr)
            v3_cpp_obj_unref(component);
        frame.reset();
        if (display != nullptr && child != 0)
            XDestroyWindow(display, child);
    }

    void prepare(double sampleRate, int32_t maxBlockSize)
    {
        if (processing)
            stopProcessing();
        require(v3_cpp_obj(component)->get_bus_count(component, V3_AUDIO, V3_INPUT) == 1 &&
                    v3_cpp_obj(component)->get_bus_count(component, V3_AUDIO, V3_OUTPUT) == 1,
                "Practice host requires one main audio input and output bus");
        require(v3_cpp_obj(component)->activate_bus(component, V3_AUDIO, V3_INPUT, 0, true) ==
                        V3_OK &&
                    v3_cpp_obj(component)->activate_bus(component, V3_AUDIO, V3_OUTPUT, 0, true) ==
                        V3_OK,
                "Could not activate the VST3 audio buses");
        v3_process_setup setup{};
        setup.process_mode = V3_REALTIME;
        setup.symbolic_sample_size = V3_SAMPLE_32;
        setup.max_block_size = maxBlockSize;
        setup.sample_rate = sampleRate;
        require(v3_cpp_obj(processor)->setup_processing(processor, &setup) == V3_OK,
                "VST3 rejected the JACK processing setup");
        require(v3_cpp_obj(component)->set_active(component, true) == V3_OK,
                "Could not activate the VST3 component");
        active = true;
        require(v3_cpp_obj(processor)->set_processing(processor, true) == V3_OK,
                "Could not start VST3 processing");
        processing = true;
    }

    void stopProcessing()
    {
        if (processing) {
            v3_cpp_obj(processor)->set_processing(processor, false);
            processing = false;
        }
        if (active) {
            v3_cpp_obj(component)->set_active(component, false);
            active = false;
        }
    }

    bool process(const float *inputLeft, const float *inputRight, float *outputLeft,
                 float *outputRight, int32_t frames)
    {
        if (!processing || frames <= 0 || frames > MaximumBlockSize)
            return false;
        std::array<float *, 2> inputChannels{const_cast<float *>(inputLeft),
                                             const_cast<float *>(inputRight)};
        std::array<float *, 2> outputChannels{outputLeft, outputRight};
        v3_audio_bus_buffers inputBus{};
        inputBus.num_channels = 2;
        inputBus.channel_buffers_32 = inputChannels.data();
        v3_audio_bus_buffers outputBus{};
        outputBus.num_channels = 2;
        outputBus.channel_buffers_32 = outputChannels.data();
        HostParameterChanges inputChanges, outputChanges;
        inputChanges.collect(parameterState);
        v3_param_changes *inputInterface = inputChanges.interface();
        v3_param_changes *outputInterface = outputChanges.interface();
        v3_process_data data{};
        data.process_mode = V3_REALTIME;
        data.symbolic_sample_size = V3_SAMPLE_32;
        data.nframes = frames;
        data.num_input_buses = 1;
        data.num_output_buses = 1;
        data.inputs = &inputBus;
        data.outputs = &outputBus;
        data.input_params = &inputInterface;
        data.output_params = &outputInterface;
        return v3_cpp_obj(processor)->process(processor, &data) == V3_OK;
    }

    void toggleBypass()
    {
        if (!parameterState.hasBypass)
            return;
        const bool bypass = !parameterState.bypassed.load(std::memory_order_acquire);
        const double value = bypass ? 1.0 : 0.0;
        if (v3_cpp_obj(controller)
                ->set_parameter_normalised(controller, parameterState.bypassId, value) == V3_OK)
            parameterState.set(parameterState.bypassId, value);
    }

    void attach(Display *display, Window parent, HostRunLoop &runLoop, int x, int y)
    {
        this->display = display;
        const auto width = static_cast<unsigned>(bounds.right - bounds.left);
        const auto height = static_cast<unsigned>(bounds.bottom - bounds.top);
        child = XCreateSimpleWindow(display, parent, x, y, width, height, 0,
                                    BlackPixel(display, DefaultScreen(display)),
                                    BlackPixel(display, DefaultScreen(display)));
        XMapWindow(display, child);
        XSync(display, False);
        frame = std::make_unique<HostFrame>(runLoop, display, child);
        require(v3_cpp_obj(view)->set_frame(view, &frame->interfacePointer) == V3_OK,
                "VST3 editor rejected the host frame");
        require(v3_cpp_obj(view)->attached(view, reinterpret_cast<void *>(child),
                                           V3_VIEW_PLATFORM_TYPE_X11) == V3_OK,
                "Could not attach VST3 editor to the board");
        attached = true;
    }

    const char *name() const { return info.name; }
    int width() const { return bounds.right - bounds.left; }
    int height() const { return bounds.bottom - bounds.top; }
    Window window() const { return child; }
    bool bypassed() const { return parameterState.bypassed.load(std::memory_order_acquire); }

  private:
    static bool parameterTitleIsBypass(const v3_str_128 &title)
    {
        constexpr char bypass[] = "Bypass";
        for (std::size_t i = 0; i < sizeof(bypass); ++i) {
            if (title[i] != bypass[i])
                return false;
        }
        return true;
    }

    static constexpr int32_t MaximumBlockSize = 2048;
    Module &module;
    v3_class_info info{};
    v3_component **component = nullptr;
    v3_audio_processor **processor = nullptr;
    v3_edit_controller **controller = nullptr;
    v3_plugin_view **view = nullptr;
    HostComponentHandler componentHandler;
    HostParameterState parameterState;
    std::unique_ptr<HostFrame> frame;
    Display *display = nullptr;
    Window child = 0;
    v3_view_rect bounds{};
    bool componentInitialized = false;
    bool controllerInitialized = false;
    bool active = false;
    bool processing = false;
    bool attached = false;
};

constexpr std::size_t MaximumBoardPedals = 8;

#if defined(STORMSURGE_HAVE_JACK)
constexpr bool builtWithJack = true;
#else
constexpr bool builtWithJack = false;
#endif

#if defined(STORMSURGE_HAVE_JACK)
class JackAudioHost
{
  public:
    static constexpr std::uint32_t MaximumFrames = 2048;

    explicit JackAudioHost(const std::vector<HostEditor *> &editors)
        : editors(editors), editorCount(editors.size())
    {
        require(editorCount >= 1 && editorCount <= MaximumBoardPedals,
                "The practice host supports one to eight pedals");
        packedOrder.store(identityOrder(editorCount), std::memory_order_relaxed);
        try {
            jack_status_t status{};
            client = jack_client_open("StormSurgePractice", JackNoStartServer, &status);
            require(client != nullptr,
                    "Could not connect to JACK. Start an existing JACK/PipeWire-JACK server; the "
                    "practice host will not start a device or fall back to native audio.");
            inputLeft = jack_port_register(client, "input_left", JACK_DEFAULT_AUDIO_TYPE,
                                           JackPortIsInput, 0);
            inputRight = jack_port_register(client, "input_right", JACK_DEFAULT_AUDIO_TYPE,
                                            JackPortIsInput, 0);
            outputLeft = jack_port_register(client, "output_left", JACK_DEFAULT_AUDIO_TYPE,
                                            JackPortIsOutput, 0);
            outputRight = jack_port_register(client, "output_right", JACK_DEFAULT_AUDIO_TYPE,
                                             JackPortIsOutput, 0);
            require(inputLeft != nullptr && inputRight != nullptr && outputLeft != nullptr &&
                        outputRight != nullptr,
                    "Could not register JACK stereo ports");
            require(jack_set_process_callback(client, processCallback, this) == 0,
                    "Could not install the JACK audio callback");
            require(jack_set_sample_rate_callback(client, sampleRateCallback, this) == 0,
                    "Could not install the JACK sample-rate callback");
            jack_on_shutdown(client, shutdownCallback, this);

            const double sampleRate = static_cast<double>(jack_get_sample_rate(client));
            for (HostEditor *editor : editors)
                editor->prepare(sampleRate, static_cast<int32_t>(MaximumFrames));
            if (jack_activate(client) != 0)
                throw std::runtime_error("Could not activate the JACK practice-host client");
            active = true;
        } catch (...) {
            for (HostEditor *editor : editors)
                editor->stopProcessing();
            if (client != nullptr) {
                jack_client_close(client);
                client = nullptr;
            }
            throw;
        }
    }

    ~JackAudioHost() { stop(); }

    void stop()
    {
        if (client == nullptr)
            return;
        if (active)
            jack_deactivate(client);
        active = false;
        for (HostEditor *editor : editors)
            editor->stopProcessing();
        jack_client_close(client);
        client = nullptr;
    }

    void setOrder(const std::vector<HostEditor *> &order)
    {
        if (order.size() != editorCount)
            return;
        std::uint64_t packed = 0;
        for (std::size_t position = 0; position < order.size(); ++position) {
            const auto found = std::find(editors.begin(), editors.end(), order[position]);
            if (found == editors.end())
                return;
            const auto index = static_cast<std::uint64_t>(std::distance(editors.begin(), found));
            packed |= index << (position * 4);
        }
        packedOrder.store(packed, std::memory_order_release);
    }

    bool liveInputEnabled() const { return liveInput.load(std::memory_order_acquire); }
    void setLiveInputEnabled(bool enabled) { liveInput.store(enabled, std::memory_order_release); }
    bool serverStopped() const { return serverIsStopped.load(std::memory_order_acquire); }

    void reconfigureIfSampleRateChanged()
    {
        const std::uint32_t sampleRate = pendingSampleRate.exchange(0, std::memory_order_acq_rel);
        if (sampleRate == 0 || client == nullptr)
            return;
        liveInput.store(false, std::memory_order_release);
        jack_deactivate(client);
        active = false;
        for (HostEditor *editor : editors)
            editor->stopProcessing();
        for (HostEditor *editor : editors)
            editor->prepare(static_cast<double>(sampleRate), static_cast<int32_t>(MaximumFrames));
        if (jack_activate(client) == 0)
            active = true;
        else
            serverIsStopped.store(true, std::memory_order_release);
    }

  private:
    static std::uint64_t identityOrder(std::size_t count) noexcept
    {
        std::uint64_t packed = 0;
        for (std::size_t position = 0; position < count; ++position)
            packed |= static_cast<std::uint64_t>(position) << (position * 4);
        return packed;
    }

    static int processCallback(jack_nframes_t frames, void *context)
    {
        return static_cast<JackAudioHost *>(context)->process(frames);
    }

    static int sampleRateCallback(jack_nframes_t sampleRate, void *context)
    {
        static_cast<JackAudioHost *>(context)->pendingSampleRate.store(sampleRate,
                                                                       std::memory_order_release);
        return 0;
    }

    static void shutdownCallback(void *context)
    {
        auto &host = *static_cast<JackAudioHost *>(context);
        host.serverIsStopped.store(true, std::memory_order_release);
        host.liveInput.store(false, std::memory_order_release);
    }

    int process(jack_nframes_t frameCount)
    {
        const auto *sourceLeft =
            static_cast<const float *>(jack_port_get_buffer(inputLeft, frameCount));
        const auto *sourceRight =
            static_cast<const float *>(jack_port_get_buffer(inputRight, frameCount));
        auto *destinationLeft = static_cast<float *>(jack_port_get_buffer(outputLeft, frameCount));
        auto *destinationRight =
            static_cast<float *>(jack_port_get_buffer(outputRight, frameCount));
        if (sourceLeft == nullptr || sourceRight == nullptr || destinationLeft == nullptr ||
            destinationRight == nullptr) {
            return 0;
        }

        const bool inputEnabled = liveInput.load(std::memory_order_acquire);
        const std::uint64_t order = packedOrder.load(std::memory_order_acquire);
        for (jack_nframes_t offset = 0; offset < frameCount;) {
            const auto count = static_cast<std::uint32_t>(
                std::min<jack_nframes_t>(MaximumFrames, frameCount - offset));
            float *currentLeft = leftBuffers[0].data();
            float *currentRight = rightBuffers[0].data();
            float *nextLeft = leftBuffers[1].data();
            float *nextRight = rightBuffers[1].data();
            if (inputEnabled) {
                std::copy_n(sourceLeft + offset, count, currentLeft);
                std::copy_n(sourceRight + offset, count, currentRight);
            } else {
                std::fill_n(currentLeft, count, 0.0f);
                std::fill_n(currentRight, count, 0.0f);
            }

            bool valid = true;
            for (std::size_t position = 0; position < editorCount; ++position) {
                const std::uint32_t index =
                    static_cast<std::uint32_t>((order >> (position * 4)) & 0xFu);
                if (index >= editors.size() ||
                    !editors[index]->process(currentLeft, currentRight, nextLeft, nextRight,
                                             static_cast<int32_t>(count))) {
                    valid = false;
                    break;
                }
                std::swap(currentLeft, nextLeft);
                std::swap(currentRight, nextRight);
            }

            if (valid && inputEnabled) {
                std::copy_n(currentLeft, count, destinationLeft + offset);
                std::copy_n(currentRight, count, destinationRight + offset);
            } else {
                std::fill_n(destinationLeft + offset, count, 0.0f);
                std::fill_n(destinationRight + offset, count, 0.0f);
            }
            offset += count;
        }
        return 0;
    }

    std::vector<HostEditor *> editors;
    std::size_t editorCount = 0;
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::atomic<std::uint64_t> packedOrder{0};
    std::atomic<std::uint32_t> pendingSampleRate{0};
    std::atomic<bool> liveInput{false};
    std::atomic<bool> serverIsStopped{false};
    std::array<std::array<float, MaximumFrames>, 2> leftBuffers{};
    std::array<std::array<float, MaximumFrames>, 2> rightBuffers{};
    jack_client_t *client = nullptr;
    jack_port_t *inputLeft = nullptr;
    jack_port_t *inputRight = nullptr;
    jack_port_t *outputLeft = nullptr;
    jack_port_t *outputRight = nullptr;
    bool active = false;
};
#else
// No JACK development files at configure time. The board still attaches the real
// editors for layout and bypass work, but the audio host is never constructed, so
// the window truthfully reports "AUDIO OFF" instead of pretending JACK is running.
class JackAudioHost
{
  public:
    explicit JackAudioHost(const std::vector<HostEditor *> &) {}
    void stop() {}
    void setOrder(const std::vector<HostEditor *> &) {}
    void setLiveInputEnabled(bool) {}
    bool liveInputEnabled() const { return false; }
    bool serverStopped() const { return true; }
    void reconfigureIfSampleRateChanged() {}
};
#endif

void runBoard(const std::vector<const char *> &paths, HostApplication &host, bool smoke)
{
    require(!paths.empty() && paths.size() <= MaximumBoardPedals,
            "Supply one to eight StormSurge VST3 module paths");
    HostRunLoop runLoop;
    std::vector<std::unique_ptr<Module>> modules;
    std::vector<std::unique_ptr<HostEditor>> editors;
    modules.reserve(paths.size());
    editors.reserve(paths.size());
    for (const char *path : paths)
        modules.push_back(std::make_unique<Module>(path));
    for (auto &module : modules)
        editors.push_back(std::make_unique<HostEditor>(*module, host));

    std::vector<HostEditor *> order;
    for (auto &editor : editors)
        order.push_back(editor.get());
    std::unique_ptr<JackAudioHost> audio;
#if defined(STORMSURGE_HAVE_JACK)
    if (!smoke) {
        audio = std::make_unique<JackAudioHost>(order);
        audio->setOrder(order);
    }
#else
    if (!smoke)
        std::fprintf(stderr,
                     "Built without JACK development files; --board runs the editors without "
                     "audio. Install the JACK pkg-config package and rebuild to enable it.\n");
#endif

    constexpr int panelWidth = 540;
    constexpr int editorTop = 84;
    const int bypassTop =
        editorTop +
        std::max_element(editors.begin(), editors.end(),
                         [](const auto &a, const auto &b) { return a->height() < b->height(); })
            ->get()
            ->height() +
        6;
    const int height = bypassTop + 48;
    constexpr int width = 1180;
    Display *display = XOpenDisplay(nullptr);
    require(display != nullptr, "Could not open the X11 display");
    const int screen = DefaultScreen(display);
    const Window parent = XCreateSimpleWindow(
        display, RootWindow(display, screen), 20, 20, static_cast<unsigned>(width),
        static_cast<unsigned>(height), 0, BlackPixel(display, screen), BlackPixel(display, screen));
    XStoreName(display, parent, "StormSurge practice board, VST3 editor host");
    XSelectInput(display, parent,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     StructureNotifyMask);
    XGrabButton(display, Button4, AnyModifier, parent, False, ButtonPressMask, GrabModeAsync,
                GrabModeAsync, None, None);
    XGrabButton(display, Button5, AnyModifier, parent, False, ButtonPressMask, GrabModeAsync,
                GrabModeAsync, None, None);
    const Atom closeAtom = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, parent, const_cast<Atom *>(&closeAtom), 1);

    int textureWidth = 0, textureHeight = 0, textureChannels = 0;
    auto *texture =
        stbi_load_from_memory(stormsurge::pedalBoardTextureJpeg,
                              static_cast<int>(sizeof(stormsurge::pedalBoardTextureJpeg)),
                              &textureWidth, &textureHeight, &textureChannels, 3);
    require(texture != nullptr && textureWidth > 0 && textureHeight > 0,
            "Could not decode the prepared pedalboard texture");
    Visual *const visual = DefaultVisual(display, screen);
    XImage *textureImage =
        XCreateImage(display, visual, static_cast<unsigned>(DefaultDepth(display, screen)), ZPixmap,
                     0, nullptr, width, static_cast<unsigned>(height), 32, 0);
    require(textureImage != nullptr, "Could not create the pedalboard texture image");
    textureImage->data = static_cast<char *>(
        std::calloc(static_cast<std::size_t>(textureImage->bytes_per_line), height));
    require(textureImage->data != nullptr, "Could not allocate the pedalboard texture");
    const auto channelPixel = [](unsigned char channel, unsigned long mask) {
        if (mask == 0)
            return 0UL;
        unsigned shift = 0;
        while (((mask >> shift) & 1UL) == 0)
            ++shift;
        const unsigned long maximum = mask >> shift;
        return ((static_cast<unsigned long>(channel) * maximum / 255UL) << shift) & mask;
    };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int sourceX = x * textureWidth / width;
            const int sourceY = y * textureHeight / height;
            const auto *pixel =
                texture + (static_cast<std::size_t>(sourceY) * textureWidth + sourceX) * 3;
            const unsigned long value =
                channelPixel(static_cast<unsigned char>(pixel[0] * 0.62f), visual->red_mask) |
                channelPixel(static_cast<unsigned char>(pixel[1] * 0.62f), visual->green_mask) |
                channelPixel(static_cast<unsigned char>(pixel[2] * 0.62f), visual->blue_mask);
            XPutPixel(textureImage, x, y, value);
        }
    }
    stbi_image_free(texture);
    const Pixmap texturePixmap =
        XCreatePixmap(display, parent, width, static_cast<unsigned>(height),
                      static_cast<unsigned>(DefaultDepth(display, screen)));
    GC textureGc = XCreateGC(display, texturePixmap, 0, nullptr);
    XPutImage(display, texturePixmap, textureGc, textureImage, 0, 0, 0, 0, width,
              static_cast<unsigned>(height));
    XFreeGC(display, textureGc);
    XDestroyImage(textureImage);
    XSetWindowBackgroundPixmap(display, parent, texturePixmap);
    XMapWindow(display, parent);
    XClearWindow(display, parent);
    XSync(display, False);

    for (std::size_t i = 0; i < editors.size(); ++i)
        editors[i]->attach(
            display, parent, runLoop,
            static_cast<int>(i) * panelWidth + (panelWidth - editors[i]->width()) / 2, editorTop);

    int dragIndex = -1;
    int scrollOffset = 0;
    const int maxScroll = std::max(0, panelWidth * static_cast<int>(order.size()) - width);
    auto positionEditors = [&] {
        for (std::size_t i = 0; i < order.size(); ++i)
            XMoveWindow(display, order[i]->window(),
                        static_cast<int>(i) * panelWidth - scrollOffset +
                            (panelWidth - order[i]->width()) / 2,
                        editorTop);
    };
    auto drawHeaders = [&] {
        XSetForeground(display, DefaultGC(display, screen), BlackPixel(display, screen));
        XFillRectangle(display, parent, DefaultGC(display, screen), 8, 6, 300, 30);
        XSetForeground(display, DefaultGC(display, screen), WhitePixel(display, screen));
        const char *inputStatus =
            audio == nullptr
                ? "LIVE INPUT: AUDIO OFF"
                : (audio->serverStopped()
                       ? "JACK STOPPED"
                       : (audio->liveInputEnabled() ? "LIVE INPUT: ENABLED"
                                                    : "LIVE INPUT: MUTED - CLICK TO ENABLE"));
        XDrawString(display, parent, DefaultGC(display, screen), 18, 26, inputStatus,
                    static_cast<int>(std::strlen(inputStatus)));
        const char *routingHint = "JACK ports: connect input and TONE3000 in the audio graph";
        XDrawString(display, parent, DefaultGC(display, screen), 330, 26, routingHint,
                    static_cast<int>(std::strlen(routingHint)));
        for (std::size_t i = 0; i < order.size(); ++i) {
            const std::string title = std::string(order[i]->name()) + "  |  drag here to reorder";
            const int x = static_cast<int>(i) * panelWidth - scrollOffset;
            XSetForeground(display, DefaultGC(display, screen), BlackPixel(display, screen));
            XFillRectangle(display, parent, DefaultGC(display, screen), x + 8, 44, panelWidth - 16,
                           32);
            XSetForeground(display, DefaultGC(display, screen), WhitePixel(display, screen));
            XDrawString(display, parent, DefaultGC(display, screen), x + 18, 65, title.c_str(),
                        static_cast<int>(title.size()));
        }
        XSetForeground(display, DefaultGC(display, screen), BlackPixel(display, screen));
        XFillRectangle(display, parent, DefaultGC(display, screen), width - 96, 6, 88, 30);
        XSetForeground(display, DefaultGC(display, screen), WhitePixel(display, screen));
        XDrawString(display, parent, DefaultGC(display, screen), width - 82, 26, "<", 1);
        XDrawString(display, parent, DefaultGC(display, screen), width - 48, 26, ">", 1);
        for (std::size_t i = 0; i < order.size(); ++i) {
            const int x = static_cast<int>(i) * panelWidth - scrollOffset + 20;
            XSetForeground(display, DefaultGC(display, screen), BlackPixel(display, screen));
            XFillRectangle(display, parent, DefaultGC(display, screen), x, bypassTop,
                           panelWidth - 40, 38);
            XSetForeground(display, DefaultGC(display, screen), WhitePixel(display, screen));
            const char *status = order[i]->bypassed() ? "FOOTSWITCH: BYPASS" : "FOOTSWITCH: ON";
            XDrawString(display, parent, DefaultGC(display, screen), x + 12, bypassTop + 24, status,
                        static_cast<int>(std::strlen(status)));
        }
    };
    std::vector<bool> drawnBypassState(order.size(), false);
    bool haveDrawnBypassState = false;
    auto refreshBypassState = [&] {
        bool changed = !haveDrawnBypassState;
        for (std::size_t i = 0; i < order.size(); ++i) {
            const bool bypassed = order[i]->bypassed();
            changed = changed || drawnBypassState[i] != bypassed;
            drawnBypassState[i] = bypassed;
        }
        haveDrawnBypassState = true;
        if (changed) {
            XClearWindow(display, parent);
            drawHeaders();
        }
    };
    auto scrollBoard = [&](int amount) {
        scrollOffset = std::clamp(scrollOffset + amount, 0, maxScroll);
        positionEditors();
        XClearWindow(display, parent);
        drawHeaders();
    };

    std::printf("Attached %zu real StormSurge VST3 editors. Drag the middle strips to reorder "
                "and click pedal footswitch strips to bypass.%s\n",
                order.size(),
                !builtWithJack
                    ? " Audio is off in this build because JACK support was not compiled in."
                : smoke ? " Smoke test only: no JACK client was opened."
                        : " Live input is off until you click the top-left control; JACK ports "
                          "are never auto-connected and the client starts muted.");
    std::fflush(stdout);
    bool keepOpen = true;
    const auto smokeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (keepOpen) {
        if (smoke && std::chrono::steady_clock::now() >= smokeDeadline)
            break;
        if (audio != nullptr) {
            audio->reconfigureIfSampleRateChanged();
            if (audio->serverStopped())
                audio->setLiveInputEnabled(false);
        }
        while (XPending(display) != 0) {
            XEvent event{};
            XNextEvent(display, &event);
            if (event.type == ClientMessage &&
                static_cast<Atom>(event.xclient.data.l[0]) == closeAtom) {
                keepOpen = false;
            } else if (event.type == Expose && event.xexpose.window == parent &&
                       event.xexpose.count == 0) {
                drawHeaders();
            } else if (event.type == ButtonPress && event.xbutton.window == parent &&
                       event.xbutton.button == Button1) {
                if (event.xbutton.y < 38 && event.xbutton.x < 308 && audio != nullptr) {
                    audio->setLiveInputEnabled(!audio->liveInputEnabled());
                    XClearWindow(display, parent);
                    drawHeaders();
                } else if (event.xbutton.y < 38 && event.xbutton.x >= width - 96) {
                    scrollBoard(event.xbutton.x < width - 58 ? -180 : 180);
                } else if (event.xbutton.y >= 44 && event.xbutton.y < 78) {
                    dragIndex = (event.xbutton.x + scrollOffset) / panelWidth;
                } else if (event.xbutton.y >= bypassTop && event.xbutton.y < bypassTop + 38) {
                    const int index = (event.xbutton.x + scrollOffset) / panelWidth;
                    if (index >= 0 && index < static_cast<int>(order.size())) {
                        order[static_cast<std::size_t>(index)]->toggleBypass();
                        if (audio != nullptr)
                            audio->setOrder(order);
                        XClearWindow(display, parent);
                        drawHeaders();
                    }
                }
            } else if (event.type == ButtonPress && event.xbutton.window == parent &&
                       (event.xbutton.button == Button4 || event.xbutton.button == Button5)) {
                scrollBoard(event.xbutton.button == Button4 ? -180 : 180);
            } else if (event.type == ButtonRelease && dragIndex >= 0 &&
                       event.xbutton.window == parent) {
                const int destination = std::clamp((event.xbutton.x + scrollOffset) / panelWidth, 0,
                                                   static_cast<int>(order.size()) - 1);
                HostEditor *moved = order[static_cast<std::size_t>(dragIndex)];
                order.erase(order.begin() + dragIndex);
                order.insert(order.begin() + destination, moved);
                positionEditors();
                if (audio != nullptr)
                    audio->setOrder(order);
                haveDrawnBypassState = false;
                dragIndex = -1;
                XClearWindow(display, parent);
                drawHeaders();
            } else if (event.type == KeyPress) {
                const KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_Left || key == XK_Right) {
                    scrollBoard(key == XK_Left ? -180 : 180);
                }
            }
        }
        runLoop.dispatch();
        refreshBypassState();
        pollfd descriptor{ConnectionNumber(display), POLLIN, 0};
        poll(&descriptor, 1, 5);
    }

    if (audio != nullptr)
        audio->stop();
    editors.clear();
    XDestroyWindow(display, parent);
    XFreePixmap(display, texturePixmap);
    XCloseDisplay(display);
}

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
        if (argc > 1 &&
            (std::strcmp(argv[1], "--board") == 0 || std::strcmp(argv[1], "--board-smoke") == 0)) {
            const bool smoke = std::strcmp(argv[1], "--board-smoke") == 0;
            require(argc >= 3 && argc <= 2 + static_cast<int>(MaximumBoardPedals),
                    "Usage: vst3_editor_tests --board[-smoke] <module> [<module> ...] "
                    "(one to eight StormSurge VST3 pedals)");
            HostApplication host;
            runBoard(std::vector<const char *>(argv + 2, argv + argc), host, smoke);
            return 0;
        }
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
