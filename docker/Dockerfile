# Stage 1: Build
FROM rust:1.82-bookworm AS builder

RUN apt-get update && apt-get install -y g++ cmake && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy C++ engine sources needed by build.rs
COPY include/ include/
COPY src/OrderBook.cpp src/OrderBook.cpp
COPY ffi/ ffi/

# Copy Rust project
COPY orderflow-api/ orderflow-api/

WORKDIR /build/orderflow-api
RUN cargo build --release

# Stage 2: Runtime
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/orderflow-api/target/release/orderflow-api /usr/local/bin/orderflow-api
COPY orderflow-api/config.toml /etc/orderflow-api/config.toml

WORKDIR /etc/orderflow-api
EXPOSE 8080

ENV RUST_LOG=info

CMD ["orderflow-api"]
