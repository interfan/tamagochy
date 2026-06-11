FROM mcr.microsoft.com/devcontainers/base:bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
    llvm \
    clang \
    lld \
    wget \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install WASI libc
RUN mkdir -p /opt/wasi-libc && \
    wget -qO- https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-22/libclang_rt.builtins-wasm32-wasi-22.0.tar.gz \
    | tar xz -C /opt/wasi-libc

# Add wasm32 target
RUN mkdir -p /opt/wasi-libc/share

ENV PATH="/opt/wasi-libc/bin:${PATH}"
