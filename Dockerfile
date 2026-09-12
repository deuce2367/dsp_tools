# Build Stage 1: C++ Tools
ARG JOBS=5
FROM python:3.13-slim AS cpp-builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3-dev \
    libomp-dev \
    libliquid-dev \
    gcovr \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY CMakeLists.txt ./
RUN mkdir -p build && cd build && cmake -DDOWNLOAD_DEPS_ONLY=ON ..
COPY *.hpp *.cpp *.h ./
COPY tests ./tests/
RUN echo ${JOBS}
RUN cd build && cmake -DDOWNLOAD_DEPS_ONLY=OFF -DDSP_TOOLS_COVERAGE=ON .. && make -j${JOBS}
RUN cd build && make coverage

# Build Stage 2: Node Frontend
FROM node:18 AS node-builder
WORKDIR /app
COPY web/frontend ./
RUN npm install --no-audit --no-fund
RUN npm run build

# Final Stage: Python FastAPI
FROM python:3.13-slim
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    libgomp1 \
    libliquid-dev \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user and group
ARG UID=1000
ARG GID=1000
RUN groupadd -g ${GID} tools \
    && useradd -u ${UID} -g tools -m -s /bin/bash dsp

WORKDIR /app

# Copy C++ binaries
COPY --chown=dsp:tools --from=cpp-builder /app/build/dsp_plotter /app/build/dsp_plotter
COPY --chown=dsp:tools --from=cpp-builder /app/build/dsp_fft /app/build/dsp_fft
COPY --chown=dsp:tools --from=cpp-builder /app/build/dsp_psd /app/build/dsp_psd
COPY --chown=dsp:tools --from=cpp-builder /app/build/dsp_filter /app/build/dsp_filter
COPY --chown=dsp:tools --from=cpp-builder /app/build/dsp_tuner /app/build/dsp_tuner
COPY --chown=dsp:tools --from=cpp-builder /app/build/dsp_resample /app/build/dsp_resample
COPY --chown=dsp:tools --from=cpp-builder /app/build/dsp_whitener /app/build/dsp_whitener
COPY --chown=dsp:tools --from=cpp-builder /app/build/dsp_format /app/build/dsp_format
COPY --chown=dsp:tools --from=cpp-builder /app/build/dsp_convert /app/build/dsp_convert
COPY --chown=dsp:tools --from=cpp-builder /app/build/dsp_plotter_py*.so /app/build/
# Copy frontend static build
COPY --chown=dsp:tools --from=node-builder /app/dist /app/web/frontend/dist

# Copy coverage reports
RUN mkdir -p /app/coverage && chown -R dsp:tools /app/coverage
COPY --chown=dsp:tools --from=cpp-builder /app/build/coverage.xml /app/coverage/
COPY --chown=dsp:tools --from=cpp-builder /app/build/coverage.txt /app/coverage/

# Copy backend Python code
COPY --chown=dsp:tools web/backend /app/web/backend

# Install Python requirements
WORKDIR /app/web/backend
RUN apt-get update && apt-get install -y build-essential \
    && pip install --no-cache-dir -r requirements.txt \
    && apt-get remove -y build-essential \
    && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/*

# Set env vars for data and bin paths
ENV DSP_BIN_DIR="/app/build"
ENV DSP_DATA_DIR="/app/data"
ENV PATH ${PATH}:${DSP_BIN_DIR}

# Ensure data dir exists
RUN mkdir -p /app/data && chown -R dsp:tools /app/data

# Switch to non-root user
USER dsp
COPY --chown=dsp:tools .bash_prompt /home/dsp/.bash_prompt
RUN echo "if [ -f ~/.bash_prompt ]; then source ~/.bash_prompt; fi" >> /home/dsp/.bashrc

# Run uvicorn
EXPOSE 8000
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
