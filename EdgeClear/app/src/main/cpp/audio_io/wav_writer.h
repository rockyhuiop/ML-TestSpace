#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace edgeclear {
namespace audio_io {

/**
 * WAV File Writer - writes 48 kHz, 16-bit PCM mono WAV files
 *
 * Format:
 * - RIFF/WAVE format
 * - PCM audio (uncompressed)
 * - 48000 Hz sample rate
 * - 16-bit samples (int16)
 * - 1 channel (mono)
 *
 * Usage:
 * 1. Open file with Open()
 * 2. Write samples with WriteSamples()
 * 3. Close file with Close() (updates header with final size)
 *
 * Thread safety: NOT thread-safe. Caller must synchronize access.
 */
class WavWriter {
public:
    WavWriter() = default;
    ~WavWriter();

    // Delete copy/move
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;
    WavWriter(WavWriter&&) = delete;
    WavWriter& operator=(WavWriter&&) = delete;

    /**
     * Opens WAV file for writing
     *
     * @param file_path Path to output WAV file
     * @param sample_rate Sample rate (typically 48000 Hz)
     * @param num_channels Number of channels (1 = mono, 2 = stereo)
     * @return true on success, false on failure
     */
    bool Open(const std::string& file_path, int sample_rate = 48000, int num_channels = 1);

    /**
     * Writes int16 samples to WAV file
     *
     * @param samples Pointer to int16 sample buffer
     * @param num_samples Number of samples to write
     * @return true on success, false on failure
     */
    bool WriteSamples(const int16_t* samples, size_t num_samples);

    /**
     * Writes float32 samples to WAV file (converts to int16)
     *
     * @param samples Pointer to float32 sample buffer (range [-1.0, 1.0])
     * @param num_samples Number of samples to write
     * @return true on success, false on failure
     */
    bool WriteSamplesFloat(const float* samples, size_t num_samples);

    /**
     * Closes WAV file and updates header with final size
     *
     * MUST be called to write a valid WAV file!
     *
     * @return true on success, false on failure
     */
    bool Close();

    /**
     * Returns number of samples written
     */
    size_t GetSamplesWritten() const { return samples_written_; }

    /**
     * Returns duration in seconds
     */
    double GetDurationSeconds() const;

    /**
     * Returns true if file is currently open
     */
    bool IsOpen() const { return file_ != nullptr; }

private:
    /**
     * Writes WAV header (placeholder, updated in Close())
     */
    bool WriteHeader();

    /**
     * Updates WAV header with final data size
     */
    bool UpdateHeader();

    FILE* file_ = nullptr;
    std::string file_path_;
    int sample_rate_ = 48000;
    int num_channels_ = 1;
    size_t samples_written_ = 0;

    // WAV format constants
    static constexpr int kBitsPerSample = 16;
    static constexpr int kBytesPerSample = 2; // 16-bit = 2 bytes
};

/**
 * Metadata JSON writer for A/B recordings
 *
 * Generates JSON file with:
 * - Recording session info (timestamp, duration, sample rate)
 * - Component states (AEC, RES, denoiser, AV-VAD enabled/disabled)
 * - Device info (model, OS version)
 * - File paths and sizes
 */
class MetadataWriter {
public:
    struct RecordingMetadata {
        std::string session_id;
        std::string timestamp;
        int duration_seconds;
        int sample_rate;

        std::string raw_file_path;
        std::string far_end_file_path;
        std::string enhanced_file_path;

        bool aec_enabled;
        bool res_enabled;
        bool denoiser_enabled;
        bool av_vad_enabled;

        std::string device_model;
        std::string os_version;
    };

    /**
     * Writes metadata JSON file
     *
     * @param file_path Path to output JSON file
     * @param metadata Recording metadata
     * @return true on success, false on failure
     */
    static bool WriteMetadata(const std::string& file_path, const RecordingMetadata& metadata);

private:
    /**
     * Escapes string for JSON
     */
    static std::string EscapeJson(const std::string& str);
};

} // namespace audio_io
} // namespace edgeclear
