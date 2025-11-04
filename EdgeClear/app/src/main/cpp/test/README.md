# C++ Unit Tests

This directory contains unit tests for the EdgeClear audio processing pipeline.

## Tests

### test_wav_alignment.cpp (Phase 6 T084)

**Purpose**: Verifies that A/B recording produces 3 WAV files that are time-aligned within ±1 sample (20.8 μs @ 48 kHz).

**Test Coverage**:
1. Basic write/read test - Verifies WAV writer correctly writes and closes files
2. Three-file alignment test - Core T084 requirement, tests synchronized recording
3. Float conversion test - Verifies float32 to int16 conversion

**Build**:
```bash
# Linux/Mac
cd EdgeClear/app/src/main/cpp/test
g++ -std=c++20 test_wav_alignment.cpp ../audio_io/wav_writer.cpp -o test_wav_alignment

# Windows (MinGW)
cd EdgeClear\app\src\main\cpp\test
g++ -std=c++20 test_wav_alignment.cpp ..\audio_io\wav_writer.cpp -o test_wav_alignment.exe
```

**Run**:
```bash
# Linux/Mac
./test_wav_alignment

# Windows
test_wav_alignment.exe
```

**Expected Output**:
```
========================================
Phase 6 T084: WAV Alignment Unit Tests
========================================

=== Test 1: Basic Write/Read ===
✓ Basic write/read test passed

=== Test 2: Three-File Alignment (T084) ===
Writing 4800 samples to 3 files...
Closing files...
Reading back files...
Impulse peaks found at:
  Raw:      sample 2000
  Far-end:  sample 2000
  Enhanced: sample 2000
Maximum deviation: 0 sample(s)
Tolerance: ±1 sample(s)
✓ Three-file alignment test passed (T084)
  All files time-aligned within ±1 sample (±20.8 μs @ 48 kHz)

=== Test 3: Float Conversion ===
✓ Float conversion test passed

========================================
Test Results:
  Passed: 3
  Failed: 0
========================================

✓ All tests passed! Phase 6 T084 complete.
  A/B recording WAV files are time-aligned within ±1 sample.
```

## Integration with Android Build

To integrate these tests into the Android CMake build:

1. Update `EdgeClear/app/CMakeLists.txt`:
```cmake
# Add test executable (optional, for development)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_executable(test_wav_alignment
        src/main/cpp/test/test_wav_alignment.cpp
        src/main/cpp/audio_io/wav_writer.cpp
    )
    target_include_directories(test_wav_alignment PRIVATE src/main/cpp)
endif()
```

2. Run on Android device:
```bash
# Push to device
adb push test_wav_alignment /data/local/tmp/

# Run
adb shell /data/local/tmp/test_wav_alignment
```

## Future Tests

Additional tests to be added:
- AEC performance test (ERLE verification)
- RES post-filter test (coherence-based gain)
- Denoiser inference test (SI-SDR improvement)
- STFT/ISTFT COLA validation
- Ring buffer thread safety test
