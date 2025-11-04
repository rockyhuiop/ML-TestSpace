#include "wav_writer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace edgeclear {
namespace audio_io {

// WAV file format structures
#pragma pack(push, 1)
struct WavHeader {
    // RIFF chunk
    char riff_id[4];        // "RIFF"
    uint32_t riff_size;     // File size - 8 bytes
    char wave_id[4];        // "WAVE"

    // fmt chunk
    char fmt_id[4];         // "fmt "
    uint32_t fmt_size;      // 16 for PCM
    uint16_t audio_format;  // 1 for PCM
    uint16_t num_channels;  // 1 for mono, 2 for stereo
    uint32_t sample_rate;   // 48000
    uint32_t byte_rate;     // sample_rate * num_channels * bytes_per_sample
    uint16_t block_align;   // num_channels * bytes_per_sample
    uint16_t bits_per_sample; // 16

    // data chunk
    char data_id[4];        // "data"
    uint32_t data_size;     // Number of bytes in data
};
#pragma pack(pop)

WavWriter::~WavWriter() {
    if (IsOpen()) {
        Close();
    }
}

bool WavWriter::Open(const std::string& file_path, int sample_rate, int num_channels) {
    if (IsOpen()) {
        return false;
    }

    file_path_ = file_path;
    sample_rate_ = sample_rate;
    num_channels_ = num_channels;
    samples_written_ = 0;

    // Open file for binary writing
    file_ = fopen(file_path.c_str(), "wb");
    if (!file_) {
        return false;
    }

    // Write placeholder header (will be updated in Close())
    if (!WriteHeader()) {
        fclose(file_);
        file_ = nullptr;
        return false;
    }

    return true;
}

bool WavWriter::WriteSamples(const int16_t* samples, size_t num_samples) {
    if (!IsOpen() || !samples) {
        return false;
    }

    // Write samples directly (int16 is already WAV format)
    size_t written = fwrite(samples, sizeof(int16_t), num_samples, file_);
    if (written != num_samples) {
        return false;
    }

    samples_written_ += num_samples;
    return true;
}

bool WavWriter::WriteSamplesFloat(const float* samples, size_t num_samples) {
    if (!IsOpen() || !samples) {
        return false;
    }

    // Convert float to int16 with clamping
    constexpr float kScale = 32767.0f;
    constexpr int16_t kMin = -32768;
    constexpr int16_t kMax = 32767;

    // Process in chunks to avoid large stack allocations
    constexpr size_t kChunkSize = 1024;
    int16_t buffer[kChunkSize];

    size_t remaining = num_samples;
    const float* src = samples;

    while (remaining > 0) {
        size_t chunk = std::min(remaining, kChunkSize);

        for (size_t i = 0; i < chunk; ++i) {
            float sample = src[i] * kScale;
            // Clamp to int16 range
            if (sample > kMax) {
                buffer[i] = kMax;
            } else if (sample < kMin) {
                buffer[i] = kMin;
            } else {
                buffer[i] = static_cast<int16_t>(std::round(sample));
            }
        }

        if (!WriteSamples(buffer, chunk)) {
            return false;
        }

        src += chunk;
        remaining -= chunk;
    }

    return true;
}

bool WavWriter::Close() {
    if (!IsOpen()) {
        return false;
    }

    // Update header with final size
    bool success = UpdateHeader();

    fclose(file_);
    file_ = nullptr;

    return success;
}

double WavWriter::GetDurationSeconds() const {
    if (sample_rate_ == 0) {
        return 0.0;
    }
    return static_cast<double>(samples_written_) / (sample_rate_ * num_channels_);
}

bool WavWriter::WriteHeader() {
    WavHeader header = {};

    // RIFF chunk
    std::memcpy(header.riff_id, "RIFF", 4);
    header.riff_size = 0; // Placeholder, updated in Close()
    std::memcpy(header.wave_id, "WAVE", 4);

    // fmt chunk
    std::memcpy(header.fmt_id, "fmt ", 4);
    header.fmt_size = 16; // PCM
    header.audio_format = 1; // PCM
    header.num_channels = static_cast<uint16_t>(num_channels_);
    header.sample_rate = static_cast<uint32_t>(sample_rate_);
    header.bits_per_sample = kBitsPerSample;
    header.block_align = static_cast<uint16_t>(num_channels_ * kBytesPerSample);
    header.byte_rate = sample_rate_ * header.block_align;

    // data chunk
    std::memcpy(header.data_id, "data", 4);
    header.data_size = 0; // Placeholder, updated in Close()

    // Write header
    size_t written = fwrite(&header, sizeof(WavHeader), 1, file_);
    return written == 1;
}

bool WavWriter::UpdateHeader() {
    // Calculate sizes
    uint32_t data_size = static_cast<uint32_t>(samples_written_ * kBytesPerSample);
    uint32_t riff_size = data_size + sizeof(WavHeader) - 8; // Exclude RIFF header (8 bytes)

    // Seek to RIFF size field (offset 4)
    if (fseek(file_, 4, SEEK_SET) != 0) {
        return false;
    }
    if (fwrite(&riff_size, sizeof(uint32_t), 1, file_) != 1) {
        return false;
    }

    // Seek to data size field (offset 40)
    if (fseek(file_, 40, SEEK_SET) != 0) {
        return false;
    }
    if (fwrite(&data_size, sizeof(uint32_t), 1, file_) != 1) {
        return false;
    }

    return true;
}

// MetadataWriter implementation

bool MetadataWriter::WriteMetadata(const std::string& file_path, const RecordingMetadata& metadata) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return false;
    }

    // Write JSON manually (simple format, no external dependency)
    file << "{\n";
    file << "  \"session_id\": \"" << EscapeJson(metadata.session_id) << "\",\n";
    file << "  \"timestamp\": \"" << EscapeJson(metadata.timestamp) << "\",\n";
    file << "  \"duration_seconds\": " << metadata.duration_seconds << ",\n";
    file << "  \"sample_rate\": " << metadata.sample_rate << ",\n";
    file << "  \"files\": {\n";
    file << "    \"raw\": \"" << EscapeJson(metadata.raw_file_path) << "\",\n";
    file << "    \"far_end\": \"" << EscapeJson(metadata.far_end_file_path) << "\",\n";
    file << "    \"enhanced\": \"" << EscapeJson(metadata.enhanced_file_path) << "\"\n";
    file << "  },\n";
    file << "  \"component_states\": {\n";
    file << "    \"aec_enabled\": " << (metadata.aec_enabled ? "true" : "false") << ",\n";
    file << "    \"res_enabled\": " << (metadata.res_enabled ? "true" : "false") << ",\n";
    file << "    \"denoiser_enabled\": " << (metadata.denoiser_enabled ? "true" : "false") << ",\n";
    file << "    \"av_vad_enabled\": " << (metadata.av_vad_enabled ? "true" : "false") << "\n";
    file << "  },\n";
    file << "  \"device\": {\n";
    file << "    \"model\": \"" << EscapeJson(metadata.device_model) << "\",\n";
    file << "    \"os_version\": \"" << EscapeJson(metadata.os_version) << "\"\n";
    file << "  }\n";
    file << "}\n";

    file.close();
    return true;
}

std::string MetadataWriter::EscapeJson(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b";  break;
            case '\f': result += "\\f";  break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                result += c;
                break;
        }
    }

    return result;
}

} // namespace audio_io
} // namespace edgeclear
