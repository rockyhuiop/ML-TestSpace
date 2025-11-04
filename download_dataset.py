#!/usr/bin/env python3
"""
Download DNS Challenge 2020 Dataset with progress bar
"""

import requests
from tqdm import tqdm
from pathlib import Path

def download_file(url, output_path):
    """Download file with progress bar"""
    print(f"Downloading from: {url}")
    print(f"Saving to: {output_path}")

    response = requests.get(url, stream=True, allow_redirects=True)
    total_size = int(response.headers.get('content-length', 0))

    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'wb') as f, tqdm(
        desc="Downloading",
        total=total_size,
        unit='B',
        unit_scale=True,
        unit_divisor=1024,
    ) as pbar:
        for chunk in response.iter_content(chunk_size=8192):
            if chunk:
                f.write(chunk)
                pbar.update(len(chunk))

    print(f"\n✓ Download complete: {output_path}")
    print(f"  File size: {output_path.stat().st_size / (1024**3):.2f} GB")

if __name__ == "__main__":
    url = "https://dns-challenge.s3.amazonaws.com/datasets/dataset_4.tar.gz"
    output = "dns_challenge_data/dataset_4.tar.gz"

    download_file(url, output)
