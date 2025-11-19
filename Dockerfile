# Dockerfile
# Stage 1: Build the C++ application
FROM gcc:latest AS builder
WORKDIR /app

# Install dependencies: libcurl, json (assuming you have the nlohmann/json single-header file, or install it)
# If using nlohmann/json and libcurl
RUN apt-get update && apt-get install -y libcurl4-openssl-dev

# Copy your source code
COPY server.cpp server.cpp
COPY json.hpp json.hpp
COPY httplib.h httplib.h

# Compile the application. Link against curl and the standard C++ library.
RUN g++ -o server server.cpp -lcurl -std=c++17

# Stage 2: Create a minimal runtime image
FROM debian:stable-slim

# Install libcurl runtime dependency
RUN apt-get update && apt-get install -y libcurl4
WORKDIR /app

# Copy the compiled binary
COPY --from=builder /app/server /app/server

# 🛑 CRITICAL FIX: Copy index.html from the local root into the container's /app directory
# This assumes index.html is in your local project root.
COPY index.html /app/ 

# Optional: Set permissions
RUN chmod -R 755 /app

EXPOSE 8080
CMD ["./server"]