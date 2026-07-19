# Build Stage 1: C++ Tools
FROM ubuntu:22.04 AS cpp-builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3-dev \
    libomp-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY CMakeLists.txt *.hpp *.cpp *.h ./
COPY tests ./tests/
RUN mkdir -p build && cd build && cmake .. && make -j4

# Build Stage 2: Node Frontend
FROM node:18 AS node-builder
WORKDIR /app
COPY web/frontend ./
RUN npm install
RUN npm run build

# Final Stage: Python FastAPI
FROM python:3.10-slim
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    libgomp1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy C++ binaries
COPY --from=cpp-builder /app/build/dsp_plotter /app/build/dsp_plotter
COPY --from=cpp-builder /app/build/dsp_fft /app/build/dsp_fft
COPY --from=cpp-builder /app/build/dsp_psd /app/build/dsp_psd
COPY --from=cpp-builder /app/build/dsp_filter /app/build/dsp_filter
COPY --from=cpp-builder /app/build/dsp_tuner /app/build/dsp_tuner
COPY --from=cpp-builder /app/build/dsp_resample /app/build/dsp_resample
COPY --from=cpp-builder /app/build/dsp_whitener /app/build/dsp_whitener
COPY --from=cpp-builder /app/build/dsp_format /app/build/dsp_format
COPY --from=cpp-builder /app/build/dsp_convert /app/build/dsp_convert
COPY --from=cpp-builder /app/build/dsp_plotter_py*.so /app/build/
# Copy frontend static build
COPY --from=node-builder /app/dist /app/web/frontend/dist

# Copy backend Python code
COPY web/backend /app/web/backend

# Install Python requirements
WORKDIR /app/web/backend
RUN pip install --no-cache-dir -r requirements.txt

# Set env vars for data and bin paths
ENV DSP_BIN_DIR="/app/build"
ENV DSP_DATA_DIR="/app/data"

# Ensure data dir exists
RUN mkdir -p /app/data

# Run uvicorn
EXPOSE 8000
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
