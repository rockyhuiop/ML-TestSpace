# EdgeClear Denoiser Training - GPU-Enabled Docker Image
# Based on official TensorFlow GPU image with CUDA 11.8 support

FROM tensorflow/tensorflow:2.15.0-gpu

# Set working directory
WORKDIR /app

# Install system dependencies
RUN apt-get update && apt-get install -y \
    libsndfile1 \
    && rm -rf /var/lib/apt/lists/*

# Install Python dependencies
RUN pip install --no-cache-dir \
    soundfile==0.13.1 \
    scipy==1.11.4 \
    tqdm==4.66.1 \
    matplotlib==3.8.2 \
    librosa==0.10.1 \
    tensorflow-model-optimization==0.8.0

# Copy training script
COPY train_edgeclear_denoiser.py /app/

# Copy dataset (will be mounted as volume)
# Dataset structure: /app/dns_challenge_data/extracted/

# Set environment variables for GPU
ENV CUDA_VISIBLE_DEVICES=0
ENV TF_FORCE_GPU_ALLOW_GROWTH=true

# Default command
CMD ["python", "train_edgeclear_denoiser.py", \
     "--clean_dir", "/app/dns_challenge_data/extracted/clean_train", \
     "--noise_dir", "/app/dns_challenge_data/extracted/noise_train", \
     "--output_path", "/app/output/denoiser_tcn_int8.tflite"]
