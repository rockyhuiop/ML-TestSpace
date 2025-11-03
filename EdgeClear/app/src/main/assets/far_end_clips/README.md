# Far-End Test Clips

This directory contains reference audio clips for AEC testing.

## Required Files (T030)

- `music_01.wav` - Classical piano (30 sec, 48 kHz, 16-bit PCM)
- `speech_far_01.wav` - Male speech (30 sec, 48 kHz, 16-bit PCM)
- `silence.wav` - Digital silence (30 sec, 48 kHz, 16-bit PCM)

## TODO

Add actual WAV files before running golden clip validation.
For now, these are placeholders to allow project compilation.

Generate using:
```bash
ffmpeg -f lavfi -i anullsrc=r=48000:cl=mono -t 30 -acodec pcm_s16le silence.wav
```
