// SPDX-License-Identifier: GPL-3.0-only
#include "WavFile.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace stormsurge::wav
{

namespace
{

constexpr std::uint16_t WaveFormatPcm = 1;
constexpr std::uint16_t WaveFormatFloat = 3;
constexpr std::uint16_t WaveFormatExtensible = 0xFFFE;

std::uint16_t read16(const std::uint8_t *data)
{
    return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
}

std::uint32_t read32(const std::uint8_t *data)
{
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

void append16(std::vector<std::uint8_t> &bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void append32(std::vector<std::uint8_t> &bytes, std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}

void appendId(std::vector<std::uint8_t> &bytes, const char *id)
{
    bytes.insert(bytes.end(), id, id + 4);
}

std::string describe(const std::string &path)
{
    return "WAV file '" + path + "'";
}

std::runtime_error unsupported(const std::string &path, const std::string &why)
{
    return std::runtime_error(describe(path) + " " + why);
}

float sampleFromPcm16(const std::uint8_t *data)
{
    const auto value = static_cast<std::int16_t>(read16(data));
    return static_cast<float>(value / 32768.0);
}

float sampleFromPcm24(const std::uint8_t *data)
{
    std::int32_t value = static_cast<std::int32_t>(data[0]) |
                         (static_cast<std::int32_t>(data[1]) << 8) |
                         (static_cast<std::int32_t>(data[2]) << 16);
    if ((value & 0x00800000) != 0)
        value |= static_cast<std::int32_t>(0xFF000000);
    return static_cast<float>(value / 8388608.0);
}

float sampleFromPcm32(const std::uint8_t *data)
{
    const auto value = static_cast<std::int32_t>(read32(data));
    return static_cast<float>(value / 2147483648.0);
}

float sampleFromFloat32(const std::uint8_t *data)
{
    const std::uint32_t bits = read32(data);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::int32_t pcmFromFloat(float sample, int bits, const std::string &path)
{
    if (!std::isfinite(sample))
        throw unsupported(path, "contains a non-finite sample that cannot be written as PCM");
    if (sample < -1.0f || sample > 1.0f)
        throw unsupported(path,
                          "contains a sample outside -1..1; refusing to clip it, write float32 "
                          "instead");
    const double positive = bits == 16 ? 32767.0 : bits == 24 ? 8388607.0 : 2147483647.0;
    const double negative = bits == 16 ? 32768.0 : bits == 24 ? 8388608.0 : 2147483648.0;
    const double scaled = static_cast<double>(sample) * (sample < 0.0f ? negative : positive);
    return static_cast<std::int32_t>(std::lround(scaled));
}

} // namespace

std::uint64_t Audio::frames() const
{
    return channels == 0 ? 0 : samples.size() / channels;
}

Audio read(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw unsupported(path, "could not be opened");

    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
        throw unsupported(path, "is not a RIFF/WAVE file");

    bool haveFormat = false;
    bool haveData = false;
    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint16_t bits = 0;
    std::uint32_t sampleRate = 0;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;

    for (std::size_t offset = 12; offset + 8 <= bytes.size();) {
        const std::uint8_t *chunk = bytes.data() + offset;
        const std::uint32_t size = read32(chunk + 4);
        const std::size_t body = offset + 8;
        if (size > bytes.size() - body)
            throw unsupported(path, "has a chunk that runs past the end of the file");

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            if (size < 16)
                throw unsupported(path, "has a format chunk that is too short");
            format = read16(bytes.data() + body);
            channels = read16(bytes.data() + body + 2);
            sampleRate = read32(bytes.data() + body + 4);
            bits = read16(bytes.data() + body + 14);
            if (format == WaveFormatExtensible) {
                if (size < 40)
                    throw unsupported(path, "has a truncated extensible format chunk");
                const std::size_t subFormat = body + 24;
                format = read16(bytes.data() + subFormat);
            }
            haveFormat = true;
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            dataOffset = body;
            dataSize = size;
            haveData = true;
        }
        offset = body + size + (size & 1u);
    }

    if (!haveFormat || !haveData)
        throw unsupported(path, "is missing its format or data chunk");
    if (channels == 0)
        throw unsupported(path, "declares no channels");
    if (sampleRate == 0)
        throw unsupported(path, "declares a zero sample rate");

    std::uint16_t bytesPerSample = 0;
    if (format == WaveFormatPcm && (bits == 16 || bits == 24 || bits == 32))
        bytesPerSample = static_cast<std::uint16_t>(bits / 8);
    else if (format == WaveFormatFloat && bits == 32)
        bytesPerSample = 4;
    else
        throw unsupported(path, "uses an unsupported sample format (format " +
                                    std::to_string(format) + ", " + std::to_string(bits) + "-bit)");

    const std::size_t frameBytes = static_cast<std::size_t>(bytesPerSample) * channels;
    if (dataSize % frameBytes != 0)
        throw unsupported(path, "ends with an incomplete sample frame");
    const std::size_t frames = dataSize / frameBytes;
    if (frames > std::numeric_limits<std::size_t>::max() / channels)
        throw unsupported(path, "declares more samples than this tool can hold");

    Audio audio;
    audio.sampleRate = sampleRate;
    audio.channels = channels;
    audio.samples.resize(frames * channels);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const std::uint8_t *sample =
                bytes.data() + dataOffset + (frame * channels + channel) * bytesPerSample;
            float value = 0.0f;
            if (format == WaveFormatFloat)
                value = sampleFromFloat32(sample);
            else if (bits == 16)
                value = sampleFromPcm16(sample);
            else if (bits == 24)
                value = sampleFromPcm24(sample);
            else
                value = sampleFromPcm32(sample);
            if (!std::isfinite(value))
                throw unsupported(path, "contains a non-finite sample");
            audio.samples[frame * channels + channel] = value;
        }
    }
    return audio;
}

void write(const std::string &path, const Audio &audio, Format format)
{
    if (audio.channels == 0)
        throw unsupported(path, "cannot be written with zero channels");
    if (audio.sampleRate == 0)
        throw unsupported(path, "cannot be written with a zero sample rate");
    if (audio.samples.size() % audio.channels != 0)
        throw unsupported(path, "has an incomplete final frame");

    const std::uint16_t bits = format == Format::Float32 ? 32
                               : format == Format::Pcm16 ? 16
                               : format == Format::Pcm24 ? 24
                                                         : 32;
    const std::uint16_t bytesPerSample = static_cast<std::uint16_t>(bits / 8);
    const std::size_t dataSize = audio.samples.size() * bytesPerSample;
    if (dataSize > std::numeric_limits<std::uint32_t>::max() - 64)
        throw unsupported(path, "is too long for a single RIFF file");

    const bool needsFact = format == Format::Float32;
    const std::size_t dataPad = dataSize & 1u;
    std::vector<std::uint8_t> bytes;
    bytes.reserve(64 + dataSize + dataPad);
    appendId(bytes, "RIFF");
    append32(bytes, static_cast<std::uint32_t>(4 + (8 + 16) + (needsFact ? 8 + 4 : 0) +
                                               (8 + dataSize) + dataPad));
    appendId(bytes, "WAVE");

    appendId(bytes, "fmt ");
    append32(bytes, 16);
    append16(bytes, needsFact ? WaveFormatFloat : WaveFormatPcm);
    append16(bytes, audio.channels);
    append32(bytes, audio.sampleRate);
    append32(bytes, audio.sampleRate * bytesPerSample * audio.channels);
    append16(bytes, static_cast<std::uint16_t>(bytesPerSample * audio.channels));
    append16(bytes, bits);

    if (needsFact) {
        appendId(bytes, "fact");
        append32(bytes, 4);
        append32(bytes, static_cast<std::uint32_t>(audio.frames()));
    }

    appendId(bytes, "data");
    append32(bytes, static_cast<std::uint32_t>(dataSize));
    for (float sample : audio.samples) {
        if (format == Format::Float32) {
            std::uint32_t bitsValue = 0;
            std::memcpy(&bitsValue, &sample, sizeof(bitsValue));
            append32(bytes, bitsValue);
            continue;
        }
        const std::int32_t value = pcmFromFloat(sample, bits, path);
        if (bits == 16) {
            append16(bytes, static_cast<std::uint16_t>(value & 0xFFFF));
        } else {
            const auto unsignedValue = static_cast<std::uint32_t>(value);
            for (int shift = 0; shift < bits; shift += 8)
                bytes.push_back(static_cast<std::uint8_t>((unsignedValue >> shift) & 0xFF));
        }
    }
    if (dataPad != 0)
        bytes.push_back(0);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
        throw unsupported(path, "could not be created for writing");
    file.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file)
        throw unsupported(path, "could not be written completely");
}

} // namespace stormsurge::wav
