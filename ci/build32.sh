#!/usr/bin/env bash

# Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

set -euo pipefail

# This script will be used for ARMOR tool - 32-bit ARM compilation
# ===== DOCKER CONFIGURATION =====
USER_ID="${USER_ID:-}"
GROUP_ID="${GROUP_ID:-}"
USER_NAME="${USER_NAME:-}"
DOCKER_IMAGE="ssg-image:v1.1"

AWS_REGION="us-west-2"
ECR_REGISTRY="418295714268.dkr.ecr.us-west-2.amazonaws.com"
ECR_REPO="ssg"

IMAGE_NAME="ssg-image"
TAG="v1.1"

# ===== Environment variables for 32-bit ARM (armhf) =====
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export CMAKE_SYSTEM_NAME=Linux
export CMAKE_SYSTEM_PROCESSOR=arm
export CMAKE_C_COMPILER=arm-linux-gnueabihf-gcc
export CMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++


# ===== Create user/group if all variables are provided =====
if [[ -n "$USER_ID" && -n "$GROUP_ID" && -n "$USER_NAME" ]]; then
    echo "Creating user ${USER_NAME} (${USER_ID}:${GROUP_ID})"

    # Create group if it does not exist
    if ! getent group "$GROUP_ID" >/dev/null 2>&1; then
        sudo groupadd -g "$GROUP_ID" "$USER_NAME"
    fi

    # Create user if it does not exist
    if ! id "$USER_NAME" >/dev/null 2>&1; then
        sudo useradd \
          -m \
          -u "$USER_ID" \
          -g "$GROUP_ID" \
          -s /bin/bash \
          "$USER_NAME"
    fi
fi

#---------------------------
# Pulling docker image 
#---------------------------
# ===== LOGIN TO ECR =====
echo "Logging in to Amazon ECR..."
aws ecr get-login-password --region "${AWS_REGION}" \
  | docker login --username AWS --password-stdin "${ECR_REGISTRY}"
echo "Login successful."

# ===== PULL DOCKER IMAGE =====
FULL_IMAGE="${ECR_REGISTRY}/${ECR_REPO}/${IMAGE_NAME}:${TAG}"

echo "Pulling Docker image: ${FULL_IMAGE}"
docker pull "${FULL_IMAGE}"
echo "Docker image pulled successfully."

# Create local tag
docker tag "${FULL_IMAGE}" "${IMAGE_NAME}:${TAG}"

echo "Docker image pulled and tagged successfully."

# ===== Configure APT sources (deb822 format) for amd64 + armhf =====
echo "Configuring APT sources for amd64 + armhf (32-bit ARM)..."
 
sudo tee /etc/apt/sources.list.d/base-amd64.sources >/dev/null <<EOF
Types: deb
URIs: http://archive.ubuntu.com/ubuntu/
Suites: noble noble-updates noble-security
Components: main restricted universe multiverse
Architectures: amd64
EOF
 
sudo tee /etc/apt/sources.list.d/ports-armhf.sources >/dev/null <<EOF
Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: noble noble-updates noble-security
Components: main restricted universe multiverse
Architectures: armhf
EOF
 
# ===== Update package lists =====
sudo apt-get update
 
# ===== Install packages for 32-bit ARM compilation =====
 
sudo apt-get install -y \
    build-essential git clang lld flex bison bc \
    libssl-dev curl kmod systemd-ukify \
    debhelper-compat libdw-dev:amd64 libelf-dev:amd64 \
    rsync mtools dosfstools u-boot-tools b4 cpio \
    gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
    gcc-arm-linux-gnueabi g++-arm-linux-gnueabi \
    python3-pip swig yamllint \
    python3-setuptools python3-wheel \
    yq abigail-tools sparse \
    cmake libyaml-dev \
    pigz
 
# ===== Python packages =====
python3 -m pip install --break-system-packages \
    dtschema==2024.11 \
    jinja2 \
    ply \
    GitPython
 
# ===== Cleanup =====
sudo rm -rf /var/lib/apt/lists/*

ROOT_DIR="${GITHUB_WORKSPACE:-$(pwd)}"

# --------------------
# Repo locations
# --------------------
QCBOR_DIR_32="$ROOT_DIR/QCBOR"
QCOMTEE_DIR_32="$ROOT_DIR/QCOMTEE"

# --------------------
# Clone or update repos
# --------------------
if [ ! -d "$QCBOR_DIR_32" ]; then
  git clone --branch master https://github.com/laurencelundblade/QCBOR.git "$QCBOR_DIR_32"
else
  cd "$QCBOR_DIR_32"
  git pull
fi

if [ ! -d "$QCOMTEE_DIR_32" ]; then
  git clone --branch main https://github.com/quic/quic-teec "$QCOMTEE_DIR_32"
  cd $QCOMTEE_DIR_32
else
  cd "$QCOMTEE_DIR_32"
  git pull
 fi

# --------------------
# Build QCBOR (32-bit)
# --------------------
cd "$QCBOR_DIR_32"
cat > CMakeToolchain.txt <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
EOF
cat CMakeToolchain.txt
cmake  -DBUILD_UNITTEST=ON -DCMAKE_TOOLCHAIN_FILE=CMakeToolchain.txt
cmake --build .
make install DESTDIR="$GITHUB_WORKSPACE/LIBQCBOR_32"

echo "QCBOR (32-bit) built ✅"

# --------------------
# Build QCOMTEE (32-bit, depends on QCBOR)
# --------------------
cd "$QCOMTEE_DIR_32"
cat > CMakeToolchain.txt <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
EOF
cat CMakeToolchain.txt

cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=CMakeToolchain.txt \
    -DBUILD_UNITTEST=ON\
    -DQCBOR_DIR_HINT="$GITHUB_WORKSPACE/LIBQCBOR_32/usr/local"
cmake --build build
cd build
make install DESTDIR="${GITHUB_WORKSPACE}/QUIC_TEEC_32"
cd ..

echo "QCOMTEE (32-bit) built ✅"

# Pulling submodules 
pushd "$ROOT_DIR" >/dev/null
if [[ -d .git && -f .gitmodules ]]; then
    git submodule sync --recursive
    git submodule update --init --recursive
fi
popd >/dev/null

cat > CMakeToolchain.txt <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
EOF
cat CMakeToolchain.txt
# --------------------
# Build MINKIPC (32-bit, depends on both)
# --------------------
docker run -i --rm \
  --user "$(id -u):$(id -g)" \
  --workdir "$GITHUB_WORKSPACE" \
  -v "$GITHUB_WORKSPACE/..:$GITHUB_WORKSPACE/.." \
  -e GITHUB_WORKSPACE="$GITHUB_WORKSPACE" \
  "$DOCKER_IMAGE" bash -c '
    set -euo pipefail
  cmake -S . -B build \
    -DBUILD_UNITTEST=ON \
    -DCMAKE_TOOLCHAIN_FILE=CMakeToolchain.txt \
    -DQCBOR_DIR_HINT="$GITHUB_WORKSPACE/LIBQCBOR_32/usr/local" \
    -DQCOMTEE_DIR_HINT="$GITHUB_WORKSPACE/QUIC_TEEC_32/usr/local"
  cd build
  make install DESTDIR="${GITHUB_WORKSPACE}/MINKIPC_LIBS_32"
'

echo "MINKIPC (32-bit) built ✅"
