#!/usr/bin/env python3
"""
Download a minimal subset of DNS Challenge 4 dataset for EdgeClear training

Downloads only essential files:
- ~4-5 clean speech archives (~5-10 GB)
- ~3-4 noise archives (~3-5 GB)

Total: ~8-15 GB instead of full 50+ GB dataset
"""

import requests
from tqdm import tqdm
from pathlib import Path
import tarfile
import os

BASE_URL = "https://dns4public.blob.core.windows.net/dns4archive/datasets_fullband"

# Minimal file selection for training
ESSENTIAL_FILES = [
    # Clean speech - diverse languages for better generalization
    "clean_fullband/datasets_fullband.clean_fullband.german_speech_000_0.00_3.47.tar.bz2",
    "clean_fullband/datasets_fullband.clean_fullband.french_speech_000_NA_NA.tar.bz2",
    "clean_fullband/datasets_fullband.clean_fullband.emotional_speech_000_NA_NA.tar.bz2",
    "clean_fullband/datasets_fullband.clean_fullband.VocalSet_48kHz_mono_000_NA_NA.tar.bz2",

    # Noise data - essential for training
    "noise_fullband/datasets_fullband.noise_fullband.audioset_000.tar.bz2",
    "noise_fullband/datasets_fullband.noise_fullband.audioset_001.tar.bz2",
    "noise_fullband/datasets_fullband.noise_fullband.freesound_000.tar.bz2",
]

def download_file(url, output_path):
    """Download file with progress bar"""
    print(f"\nDownloading: {output_path.name}")
    print(f"  From: {url}")

    response = requests.get(url, stream=True, allow_redirects=True)

    if response.status_code != 200:
        print(f"  ✗ Failed: HTTP {response.status_code}")
        return False

    total_size = int(response.headers.get('content-length', 0))

    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'wb') as f, tqdm(
        desc="  Progress",
        total=total_size,
        unit='B',
        unit_scale=True,
        unit_divisor=1024,
    ) as pbar:
        for chunk in response.iter_content(chunk_size=8192):
            if chunk:
                f.write(chunk)
                pbar.update(len(chunk))

    print(f"  ✓ Downloaded: {output_path.stat().st_size / (1024**2):.1f} MB")
    return True

def extract_archive(archive_path, output_dir):
    """Extract tar.bz2 archive"""
    print(f"\nExtracting: {archive_path.name}")

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        with tarfile.open(archive_path, 'r:bz2') as tar:
            # Get total files for progress bar
            members = tar.getmembers()

            with tqdm(total=len(members), desc="  Extracting", unit="files") as pbar:
                for member in members:
                    tar.extract(member, path=output_dir)
                    pbar.update(1)

        print(f"  ✓ Extracted to: {output_dir}")
        return True
    except Exception as e:
        print(f"  ✗ Extraction failed: {e}")
        return False

def organize_files(base_dir):
    """Organize extracted files into clean_train and noise_train directories"""
    base_dir = Path(base_dir)

    # Create organized directories
    clean_dir = base_dir / "clean_train"
    noise_dir = base_dir / "noise_train"
    clean_dir.mkdir(exist_ok=True)
    noise_dir.mkdir(exist_ok=True)

    print("\n=== Organizing files ===")

    # Find all WAV files
    all_wavs = list(base_dir.rglob("*.wav"))

    clean_count = 0
    noise_count = 0

    for wav_file in tqdm(all_wavs, desc="Organizing"):
        # Determine if it's clean speech or noise based on path
        path_str = str(wav_file).lower()

        if "clean" in path_str or "speech" in path_str or "vocal" in path_str:
            # Move to clean_train
            dest = clean_dir / wav_file.name
            if not dest.exists():
                wav_file.rename(dest)
                clean_count += 1
        elif "noise" in path_str or "audioset" in path_str or "freesound" in path_str:
            # Move to noise_train
            dest = noise_dir / wav_file.name
            if not dest.exists():
                wav_file.rename(dest)
                noise_count += 1

    print(f"\n✓ Organized {clean_count} clean speech files")
    print(f"✓ Organized {noise_count} noise files")

    return clean_count, noise_count

def main():
    output_base = Path("dns_challenge_data")
    archives_dir = output_base / "archives"

    print("=" * 70)
    print("DNS Challenge 4 - Minimal Dataset Download")
    print("=" * 70)
    print(f"\nDownloading {len(ESSENTIAL_FILES)} essential archives...")
    print(f"Output directory: {output_base.absolute()}")

    # Download all files
    downloaded_files = []
    for blob_name in ESSENTIAL_FILES:
        url = f"{BASE_URL}/{blob_name}"
        output_path = archives_dir / blob_name.split('/')[-1]

        if output_path.exists():
            print(f"\n✓ Already downloaded: {output_path.name}")
            downloaded_files.append(output_path)
            continue

        if download_file(url, output_path):
            downloaded_files.append(output_path)
        else:
            print(f"  ⚠ Skipping {blob_name}")

    print(f"\n{'=' * 70}")
    print(f"Downloaded {len(downloaded_files)}/{len(ESSENTIAL_FILES)} files")
    print(f"{'=' * 70}")

    # Extract all archives
    print("\n=== Extracting Archives ===")
    for archive in downloaded_files:
        extract_archive(archive, output_base / "extracted")

    # Organize into clean_train and noise_train
    clean_count, noise_count = organize_files(output_base / "extracted")

    print("\n" + "=" * 70)
    print("✓ Dataset preparation complete!")
    print("=" * 70)
    print(f"\nDataset location:")
    print(f"  Clean speech: {output_base / 'clean_train'}")
    print(f"  Noise files:  {output_base / 'noise_train'}")
    print(f"\nReady to train:")
    print(f"  python train_edgeclear_denoiser.py \\")
    print(f"      --clean_dir {output_base / 'clean_train'} \\")
    print(f"      --noise_dir {output_base / 'noise_train'} \\")
    print(f"      --output_path denoiser_tcn_int8.tflite")

if __name__ == "__main__":
    main()
