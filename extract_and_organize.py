#!/usr/bin/env python3
"""
Extract and organize DNS Challenge 4 dataset
"""

import tarfile
from pathlib import Path
from tqdm import tqdm
import shutil

def extract_archive(archive_path, output_dir):
    """Extract tar.bz2 archive with progress"""
    print(f"\n{'='*70}")
    print(f"Extracting: {archive_path.name}")
    print(f"{'='*70}")

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        with tarfile.open(archive_path, 'r:bz2') as tar:
            members = tar.getmembers()
            print(f"Total files: {len(members)}")

            with tqdm(total=len(members), desc="Extracting", unit="files") as pbar:
                for member in members:
                    tar.extract(member, path=output_dir)
                    pbar.update(1)

        print(f"✓ Extracted successfully")
        return True
    except Exception as e:
        print(f"✗ Extraction failed: {e}")
        return False

def organize_files(base_dir):
    """Organize extracted files into clean_train and noise_train"""
    base_dir = Path(base_dir)

    # Create organized directories
    clean_dir = base_dir / "clean_train"
    noise_dir = base_dir / "noise_train"
    clean_dir.mkdir(exist_ok=True)
    noise_dir.mkdir(exist_ok=True)

    print(f"\n{'='*70}")
    print("Organizing files into clean_train and noise_train")
    print(f"{'='*70}")

    # Find all WAV files
    all_wavs = list(base_dir.rglob("*.wav"))
    print(f"Found {len(all_wavs)} WAV files")

    clean_count = 0
    noise_count = 0

    for wav_file in tqdm(all_wavs, desc="Organizing"):
        path_str = str(wav_file).lower()

        # Determine if clean or noise based on path
        if any(x in path_str for x in ['clean', 'speech', 'vocal', 'german', 'french', 'emotional']):
            dest = clean_dir / wav_file.name
            if not dest.exists():
                try:
                    shutil.copy2(wav_file, dest)
                    clean_count += 1
                except Exception as e:
                    print(f"  Error copying {wav_file.name}: {e}")
        elif any(x in path_str for x in ['noise', 'audioset', 'freesound']):
            dest = noise_dir / wav_file.name
            if not dest.exists():
                try:
                    shutil.copy2(wav_file, dest)
                    noise_count += 1
                except Exception as e:
                    print(f"  Error copying {wav_file.name}: {e}")

    print(f"\n✓ Organized {clean_count} clean speech files → {clean_dir}")
    print(f"✓ Organized {noise_count} noise files → {noise_dir}")

    return clean_count, noise_count

def main():
    archives_dir = Path("dns_challenge_data/archives")
    extract_dir = Path("dns_challenge_data/extracted")

    print(f"\n{'='*70}")
    print("DNS Challenge 4 - Extract and Organize")
    print(f"{'='*70}")

    # Find all tar.bz2 archives
    archives = list(archives_dir.glob("*.tar.bz2"))
    print(f"\nFound {len(archives)} archives to extract")

    # Extract all archives
    for i, archive in enumerate(archives, 1):
        print(f"\n[{i}/{len(archives)}] Processing {archive.name}...")
        extract_archive(archive, extract_dir)

    print(f"\n{'='*70}")
    print("All archives extracted successfully!")
    print(f"{'='*70}")

    # Organize files
    clean_count, noise_count = organize_files(extract_dir)

    # Final summary
    print(f"\n{'='*70}")
    print("✓ Dataset preparation complete!")
    print(f"{'='*70}")
    print(f"\nDataset statistics:")
    print(f"  Clean speech files: {clean_count}")
    print(f"  Noise files: {noise_count}")
    print(f"\nDataset locations:")
    print(f"  Clean: {extract_dir / 'clean_train'}")
    print(f"  Noise: {extract_dir / 'noise_train'}")
    print(f"\nReady to train! Run:")
    print(f"  python train_edgeclear_denoiser.py \\")
    print(f"      --clean_dir dns_challenge_data/extracted/clean_train \\")
    print(f"      --noise_dir dns_challenge_data/extracted/noise_train \\")
    print(f"      --output_path denoiser_tcn_int8.tflite")

if __name__ == "__main__":
    main()
