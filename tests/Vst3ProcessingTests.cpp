// SPDX-License-Identifier: GPL-3.0-only
#include "travesty/audio_processor.h"
#include "travesty/component.h"
#include "travesty/factory.h"

#include <dlfcn.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

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

struct ValueQueue {
    v3_funknown unknown{};
    v3_param_value_queue queue{};
    v3_param_id id = 0;
    int32_t points = 0;
    int32_t offset = 0;
    double value = 0.0;

    static v3_param_id V3_API getId(void *self)
    {
        return reinterpret_cast<ValueQueue *>(*static_cast<v3_param_value_queue **>(self))->id;
    }
    static int32_t V3_API getPointCount(void *self)
    {
        return reinterpret_cast<ValueQueue *>(*static_cast<v3_param_value_queue **>(self))->points;
    }
    static v3_result V3_API getPoint(void *self, int32_t index, int32_t *sampleOffset,
                                     double *pointValue)
    {
        auto &queue = *reinterpret_cast<ValueQueue *>(*static_cast<v3_param_value_queue **>(self));
        if (index != 0 || queue.points == 0)
            return V3_INVALID_ARG;
        *sampleOffset = queue.offset;
        *pointValue = queue.value;
        return V3_OK;
    }
    static v3_result V3_API addPoint(void *self, int32_t sampleOffset, double pointValue,
                                     int32_t *index)
    {
        auto &queue = *reinterpret_cast<ValueQueue *>(*static_cast<v3_param_value_queue **>(self));
        queue.offset = sampleOffset;
        queue.value = pointValue;
        queue.points = 1;
        *index = 0;
        return V3_OK;
    }

    ValueQueue()
    {
        queue.get_param_id = getId;
        queue.get_point_count = getPointCount;
        queue.get_point = getPoint;
        queue.add_point = addPoint;
    }
};

struct OutputChanges {
    v3_funknown unknown{};
    v3_param_changes changes{};
    std::array<ValueQueue, 64> queues{};
    std::array<v3_param_value_queue *, 64> queueInterfaces{};
    int32_t count = 0;

    static int32_t V3_API getCount(void *self)
    {
        return reinterpret_cast<OutputChanges *>(*static_cast<v3_param_changes **>(self))->count;
    }
    static v3_param_value_queue **V3_API getData(void *self, int32_t index)
    {
        auto &changes = *reinterpret_cast<OutputChanges *>(*static_cast<v3_param_changes **>(self));
        if (index < 0 || index >= changes.count)
            return nullptr;
        return &changes.queueInterfaces[index];
    }
    static v3_param_value_queue **V3_API addData(void *self, const v3_param_id *id, int32_t *index)
    {
        auto &changes = *reinterpret_cast<OutputChanges *>(*static_cast<v3_param_changes **>(self));
        for (int32_t i = 0; i < changes.count; ++i) {
            if (changes.queues[i].id == *id) {
                *index = i;
                return getData(self, i);
            }
        }
        if (changes.count == static_cast<int32_t>(changes.queues.size()))
            return nullptr;
        const int32_t added = changes.count++;
        changes.queues[added].id = *id;
        *index = added;
        return getData(self, added);
    }

    OutputChanges()
    {
        changes.get_param_count = getCount;
        changes.get_param_data = getData;
        changes.add_param_data = addData;
        for (std::size_t i = 0; i < queues.size(); ++i)
            queueInterfaces[i] = reinterpret_cast<v3_param_value_queue *>(&queues[i]);
    }
};

class Processor
{
  public:
    Processor(Module &module, double sampleRate, std::uint32_t blockSize)
    {
        auto *factory = v3_cpp_obj(module.factory);
        require(factory->num_classes(module.factory) >= 1, "VST3 has no classes");
        v3_class_info info{};
        require(factory->get_class_info(module.factory, 0, &info) == V3_OK &&
                    std::strcmp(info.category, "Audio Module Class") == 0,
                "First VST3 class is not an audio processor");
        require(factory->create_instance(module.factory, info.class_id, v3_component_iid,
                                         reinterpret_cast<void **>(&component)) == V3_OK &&
                    component != nullptr,
                "Could not create VST3 component");

        require(v3_cpp_obj_initialize(component, nullptr) == V3_OK,
                "Could not initialize VST3 component");
        initialized = true;
        require(v3_cpp_obj_query_interface(component, v3_audio_processor_iid, &processor) ==
                        V3_OK &&
                    processor != nullptr,
                "VST3 component has no audio processor interface");

        const int32_t inputs = v3_cpp_obj(component)->get_bus_count(component, V3_AUDIO, V3_INPUT);
        const int32_t outputs =
            v3_cpp_obj(component)->get_bus_count(component, V3_AUDIO, V3_OUTPUT);
        require(inputs == 1 && outputs == 1, "Expected one main audio input and output bus");
        require(v3_cpp_obj(component)->activate_bus(component, V3_AUDIO, V3_INPUT, 0, true) ==
                        V3_OK &&
                    v3_cpp_obj(component)->activate_bus(component, V3_AUDIO, V3_OUTPUT, 0, true) ==
                        V3_OK,
                "Could not activate VST3 audio buses");

        v3_process_setup setup{};
        setup.process_mode = V3_REALTIME;
        setup.symbolic_sample_size = V3_SAMPLE_32;
        setup.max_block_size = static_cast<int32_t>(blockSize);
        setup.sample_rate = sampleRate;
        require(v3_cpp_obj(processor)->setup_processing(processor, &setup) == V3_OK,
                "VST3 rejected the processing setup");
        require(v3_cpp_obj(component)->set_active(component, true) == V3_OK,
                "Could not activate VST3 component");
        active = true;
        require(v3_cpp_obj(processor)->set_processing(processor, true) == V3_OK,
                "Could not start VST3 processing");
        processing = true;
    }

    ~Processor()
    {
        if (processing)
            v3_cpp_obj(processor)->set_processing(processor, false);
        if (active)
            v3_cpp_obj(component)->set_active(component, false);
        if (processor != nullptr)
            v3_cpp_obj_unref(processor);
        if (initialized)
            v3_cpp_obj_terminate(component);
        if (component != nullptr)
            v3_cpp_obj_unref(component);
    }

    void process(float *left, float *right, std::uint32_t frames)
    {
        std::array<float *, 2> inputChannels{left, right};
        std::array<float *, 2> outputChannels{};
        std::array<float, 256> outputLeft{}, outputRight{};
        outputChannels = {outputLeft.data(), outputRight.data()};
        v3_audio_bus_buffers inputBus{};
        inputBus.num_channels = 2;
        inputBus.channel_buffers_32 = inputChannels.data();
        v3_audio_bus_buffers outputBus{};
        outputBus.num_channels = 2;
        outputBus.channel_buffers_32 = outputChannels.data();
        v3_process_data data{};
        data.process_mode = V3_REALTIME;
        data.symbolic_sample_size = V3_SAMPLE_32;
        data.nframes = static_cast<int32_t>(frames);
        data.num_input_buses = 1;
        data.num_output_buses = 1;
        data.inputs = &inputBus;
        data.outputs = &outputBus;
        OutputChanges outputChanges;
        v3_param_changes *outputChangesInterface =
            reinterpret_cast<v3_param_changes *>(&outputChanges);
        data.output_params = &outputChangesInterface;
        require(v3_cpp_obj(processor)->process(processor, &data) == V3_OK,
                "VST3 processor rejected an audio block");
        for (std::uint32_t i = 0; i < frames; ++i) {
            left[i] = outputLeft[i];
            right[i] = outputRight[i];
        }
    }

  private:
    v3_component **component = nullptr;
    v3_audio_processor **processor = nullptr;
    bool initialized = false;
    bool active = false;
    bool processing = false;
};
} // namespace

int main(int argc, char **argv)
{
    try {
        require(argc == 2, "Supply the Linux VST3 module path");
        constexpr std::uint32_t frames = 256;
        Module module(argv[1]);
        Processor effect(module, 48000.0, frames);
        std::array<float, frames> left{}, right{};
        for (std::uint32_t i = 0; i < frames; ++i) {
            left[i] = static_cast<float>(0.2 * std::sin(0.03 * i));
            right[i] = static_cast<float>(0.2 * std::cos(0.025 * i));
        }
        effect.process(left.data(), right.data(), frames);
        for (std::uint32_t i = 0; i < frames; ++i)
            require(std::isfinite(left[i]) && std::isfinite(right[i]),
                    "VST3 emitted a non-finite sample");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    std::puts("PASS: instantiated StormSurge VST3 and processed a stereo block without a device");
    return 0;
}
