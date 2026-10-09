# The second compiler for the gate (make clang-check; docs/toolchain.md): Ubuntu 24.04's clang-18, from Ubuntu's own
# archive, on the official image pinned by digest. Compile only: gcc stays the ship compiler.
FROM ubuntu:24.04@sha256:008173c23f95b170204355c12626cb5a965d779a7e1283b09e9cffbb1bf33ca3
RUN apt-get update && apt-get install -y --no-install-recommends clang-18 make && rm -rf /var/lib/apt/lists/*
