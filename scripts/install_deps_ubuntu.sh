#!/usr/bin/env bash
set -euo pipefail

# Install all required build dependencies for tee-fusion + OP-TEE QEMU v8
# Usage: sudo bash scripts/install_deps_ubuntu.sh

if [ "${EUID}" -ne 0 ]; then
  echo "[!] Please run with sudo: sudo bash $0" >&2
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update

BASE_PACKAGES=(
  build-essential cmake pkg-config git curl wget
  python3 python3-pip python3-venv python3-pyelftools
  device-tree-compiler
  ninja-build bison flex
  rsync cpio unzip bc
  libglib2.0-dev libpixman-1-dev libfdt-dev zlib1g-dev
  libssl-dev uuid-dev
  libgnutls28-dev
  libmpc-dev libmpfr-dev libgmp-dev
)

CROSS_PACKAGES=(
  gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
  gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
)

echo "[+] Installing base packages"
apt-get install -y "${BASE_PACKAGES[@]}"

echo "[+] Installing cross toolchains (recommended)"
apt-get install -y "${CROSS_PACKAGES[@]}"

# repo tool (preferred via package, otherwise users can rely on script fallback)
if ! command -v repo >/dev/null 2>&1; then
  echo "[=] 'repo' package not found in PATH, attempting to install"
  apt-get install -y repo || true
fi

echo "[Done] Dependencies installed. You can now run cmake/make targets."
