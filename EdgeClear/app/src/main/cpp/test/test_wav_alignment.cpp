/**
 * Phase 6 T084: WAV Time Alignment Test
 *
 * Verifies that 3 WAV files written by A/B recording are time-aligned
 * within ±1 sample (20.8 μs @ 48 kHz).
 *
 * Test strategy:
 * 1. Create 3 WavWriter instances (raw, far-end, enhanced)
 * 2. Write identical impulse pattern to all 3 files
 * 3. Close files to finalize headers
 * 4. Read files back and locate impulse peak
 * 5. Verify peak occurs at same sample index in all 3 files (±1 sample tolerance)
 *
 * Build with: g++ -std=c++20 test_wav_alignment.cpp ../audio_io/wav_writer.cpp -o test_wav_alignment
 * Run with: ./test_wav_alignment
 */

#include "../audio_io/wav_writer.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstring>
#include <cassert>

namespace test {

// Simple WAV header parser for reading back test files
struct WavHeader {
    char riff[4];           // "RIFF"
    uint32_t file_size;
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];           // "data"
    uint32_t data_size;
};

/**
 * Read WAV file and return samples
 */
std::vector<int16_t> ReadWavFile(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open: " << file_path << std::endl;
        return {};
    }

    // Read header
    WavHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));

    // Validate header
    if (std::strncmp(header.riff, "RIFF", 4) != 0 ||
        std::strncmp(header.wave, "WAVE", 4) != 0 ||
        std::strncmp(header.data, "data", 4) != 0) {
        std::cerr << "Invalid WAV header: " << file_path << std::endl;
        return {};
    }

    // Read samples
    size_t num_samples = header.data_size / sizeof(int16_t);
    std::vector<int16_t> samples(num_samples);
    file.read(reinterpret_cast<char*>(samples.data()), header.data_size);

    return samples;
}

/**
 * Find impulse peak in samples (returns sample index)
 */
int FindImpulsePeak(const std::vector<int16_t>& samples, int16_t threshold = 16000) {
    for (size_t i = 0; i < samples.size(); ++i) {
        if (std::abs(samples[i]) >= threshold) {
            return static_cast<int>(i);
        }
    }
    return -1;  // Not found
}

/**
 * Test 1: Basic write/read test
 */
bool TestBasicWriteRead() {
    std::cout << "\n=== Test 1: Basic Write/Read ===" << std::endl;

    const std::string test_file = "/tmp/test_basic.wav";
    const int sample_rate = 48000;
    const int num_samples = 4800;  // 0.1 seconds

    // Write test file
    edgeclear::audio_io::WavWriter writer;
    if (!writer.Open(test_file, sample_rate, 1)) {
        std::cerr << "Failed to open test file for writing" << std::endl;
        return false;
    }

    // Create test signal: impulse at sample 1000
    std::vector<int16_t> samples(num_samples, 0);
    samples[1000] = 32000;  // Impulse

    if (!writer.WriteSamples(samples.data(), num_samples)) {
        std::cerr << "Failed to write samples" << std::endl;
        return false;
    }

    if (!writer.Close()) {
        std::cerr << "Failed to close file" << std::endl;
        return false;
    }

    // Read back and verify
    std::vector<int16_t> read_samples = ReadWavFile(test_file);
    if (read_samples.size() != num_samples) {
        std::cerr << "Sample count mismatch: expected " << num_samples
                  << ", got " << read_samples.size() << std::endl;
        return false;
    }

    int peak_index = FindImpulsePeak(read_samples);
    if (peak_index != 1000) {
        std::cerr << "Impulse at wrong index: expected 1000, got " << peak_index << std::endl;
        return false;
    }

    std::cout << "✓ Basic write/read test passed" << std::endl;
    return true;
}

/**
 * Test 2: Three-file alignment test (main test for T084)
 */
bool TestThreeFileAlignment() {
    std::cout << "\n=== Test 2: Three-File Alignment (T084) ===" << std::endl;

    const std::string raw_path = "/tmp/test_raw.wav";
    const std::string far_end_path = "/tmp/test_far_end.wav";
    const std::string enhanced_path = "/tmp/test_enhanced.wav";
    const int sample_rate = 48000;
    const int num_samples = 4800;  // 0.1 seconds
    const int impulse_index = 2000;

    // Create 3 WAV writers (simulating A/B recording)
    edgeclear::audio_io::WavWriter raw_writer;
    edgeclear::audio_io::WavWriter far_end_writer;
    edgeclear::audio_io::WavWriter enhanced_writer;

    if (!raw_writer.Open(raw_path, sample_rate, 1) ||
        !far_end_writer.Open(far_end_path, sample_rate, 1) ||
        !enhanced_writer.Open(enhanced_path, sample_rate, 1)) {
        std::cerr << "Failed to open WAV files for writing" << std::endl;
        return false;
    }

    // Create identical impulse pattern
    std::vector<int16_t> impulse_signal(num_samples, 0);
    impulse_signal[impulse_index] = 30000;  // Strong impulse

    // Write IDENTICAL signal to all 3 files (simulating synchronized recording)
    std::cout << "Writing " << num_samples << " samples to 3 files..." << std::endl;
    for (int hop = 0; hop < num_samples; hop += 160) {
        int samples_this_hop = std::min(160, num_samples - hop);

        if (!raw_writer.WriteSamples(&impulse_signal[hop], samples_this_hop) ||
            !far_end_writer.WriteSamples(&impulse_signal[hop], samples_this_hop) ||
            !enhanced_writer.WriteSamples(&impulse_signal[hop], samples_this_hop)) {
            std::cerr << "Failed to write samples at hop " << hop << std::endl;
            return false;
        }
    }

    // Close all files
    std::cout << "Closing files..." << std::endl;
    if (!raw_writer.Close() || !far_end_writer.Close() || !enhanced_writer.Close()) {
        std::cerr << "Failed to close WAV files" << std::endl;
        return false;
    }

    // Read back all 3 files
    std::cout << "Reading back files..." << std::endl;
    std::vector<int16_t> raw_samples = ReadWavFile(raw_path);
    std::vector<int16_t> far_end_samples = ReadWavFile(far_end_path);
    std::vector<int16_t> enhanced_samples = ReadWavFile(enhanced_path);

    if (raw_samples.size() != num_samples ||
        far_end_samples.size() != num_samples ||
        enhanced_samples.size() != num_samples) {
        std::cerr << "Sample count mismatch after read" << std::endl;
        return false;
    }

    // Find impulse peak in each file
    int raw_peak = FindImpulsePeak(raw_samples);
    int far_end_peak = FindImpulsePeak(far_end_samples);
    int enhanced_peak = FindImpulsePeak(enhanced_samples);

    std::cout << "Impulse peaks found at:" << std::endl;
    std::cout << "  Raw:      sample " << raw_peak << std::endl;
    std::cout << "  Far-end:  sample " << far_end_peak << std::endl;
    std::cout << "  Enhanced: sample " << enhanced_peak << std::endl;

    if (raw_peak == -1 || far_end_peak == -1 || enhanced_peak == -1) {
        std::cerr << "❌ Failed to find impulse in one or more files" << std::endl;
        return false;
    }

    // Verify alignment: all peaks within ±1 sample
    const int tolerance = 1;  // ±1 sample = ±20.8 μs @ 48 kHz
    int max_deviation = std::max({
        std::abs(raw_peak - far_end_peak),
        std::abs(raw_peak - enhanced_peak),
        std::abs(far_end_peak - enhanced_peak)
    });

    std::cout << "Maximum deviation: " << max_deviation << " sample(s)" << std::endl;
    std::cout << "Tolerance: ±" << tolerance << " sample(s)" << std::endl;

    if (max_deviation > tolerance) {
        std::cerr << "❌ FAILED: Files not aligned within ±1 sample" << std::endl;
        return false;
    }

    // Also verify impulse is at expected index
    if (std::abs(raw_peak - impulse_index) > tolerance) {
        std::cerr << "❌ FAILED: Impulse shifted from original position" << std::endl;
        std::cerr << "   Expected: " << impulse_index << ", got: " << raw_peak << std::endl;
        return false;
    }

    std::cout << "✓ Three-file alignment test passed (T084)" << std::endl;
    std::cout << "  All files time-aligned within ±1 sample (±20.8 μs @ 48 kHz)" << std::endl;
    return true;
}

/**
 * Test 3: Float conversion test
 */
bool TestFloatConversion() {
    std::cout << "\n=== Test 3: Float Conversion ===" << std::endl;

    const std::string test_file = "/tmp/test_float.wav";
    const int sample_rate = 48000;
    const int num_samples = 160;

    edgeclear::audio_io::WavWriter writer;
    if (!writer.Open(test_file, sample_rate, 1)) {
        std::cerr << "Failed to open test file" << std::endl;
        return false;
    }

    // Create float samples ranging from -1.0 to 1.0
    std::vector<float> float_samples(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        float_samples[i] = -1.0f + (2.0f * i / (num_samples - 1));
    }

    if (!writer.WriteSamplesFloat(float_samples.data(), num_samples)) {
        std::cerr << "Failed to write float samples" << std::endl;
        return false;
    }

    if (!writer.Close()) {
        std::cerr << "Failed to close file" << std::endl;
        return false;
    }

    // Read back and verify conversion
    std::vector<int16_t> read_samples = ReadWavFile(test_file);
    if (read_samples.size() != num_samples) {
        std::cerr << "Sample count mismatch" << std::endl;
        return false;
    }

    // Verify first and last samples
    if (read_samples[0] < -32768 || read_samples[0] > -32700) {
        std::cerr << "First sample conversion error: " << read_samples[0] << std::endl;
        return false;
    }

    if (read_samples[num_samples - 1] < 32700 || read_samples[num_samples - 1] > 32767) {
        std::cerr << "Last sample conversion error: " << read_samples[num_samples - 1] << std::endl;
        return false;
    }

    std::cout << "✓ Float conversion test passed" << std::endl;
    return true;
}

} // namespace test

/**
 * Main test runner
 */
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Phase 6 T084: WAV Alignment Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0;
    int failed = 0;

    // Run tests
    if (test::TestBasicWriteRead()) {
        ++passed;
    } else {
        ++failed;
    }

    if (test::TestThreeFileAlignment()) {
        ++passed;
    } else {
        ++failed;
    }

    if (test::TestFloatConversion()) {
        ++passed;
    } else {
        ++failed;
    }

    // Summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Results:" << std::endl;
    std::cout << "  Passed: " << passed << std::endl;
    std::cout << "  Failed: " << failed << std::endl;
    std::cout << "========================================" << std::endl;

    if (failed == 0) {
        std::cout << "\n✓ All tests passed! Phase 6 T084 complete." << std::endl;
        std::cout << "  A/B recording WAV files are time-aligned within ±1 sample." << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Some tests failed." << std::endl;
        return 1;
    }
}
