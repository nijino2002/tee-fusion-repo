#!/usr/bin/env bash
set -euo pipefail

# This script fetches and builds an OP-TEE QEMU v8 environment using optee/build,
# builds the TA in this repo, and prints next steps to run QEMU and validate.

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TP_DIR="${ROOT_DIR}/third_party"
# Workspace layout expected by OP-TEE: build/, optee_os/, linux/, u-boot/, etc under a common root
WS_DIR="${TP_DIR}/optee_ws"
BUILD_DIR="${WS_DIR}/build"
PLATFORM="vexpress-qemu_armv8a"  # optee_os platform (for info)

echo "[+] Preparing directories"
mkdir -p "${WS_DIR}"

# Ensure we have a usable repo binary (prefer system; fallback to local download)
if command -v repo >/dev/null 2>&1; then
  REPO_BIN="repo"
elif [ -x "${WS_DIR}/repo" ]; then
  REPO_BIN="${WS_DIR}/repo"
else
  REPO_BIN="${WS_DIR}/repo"
  echo "[+] Downloading repo tool to ${REPO_BIN}"
  mkdir -p "${WS_DIR}"
  curl -fsSL -o "${REPO_BIN}" https://storage.googleapis.com/git-repo-downloads/repo || wget -O "${REPO_BIN}" https://storage.googleapis.com/git-repo-downloads/repo
  chmod +x "${REPO_BIN}"
fi

# Ensure the official repo-based workspace (uses manifest to fetch all components)
if [ ! -d "${WS_DIR}/.repo" ]; then
  echo "[+] Initializing OP-TEE repo workspace in ${WS_DIR}"
  ( cd "${WS_DIR}" && "${REPO_BIN}" init -u https://github.com/OP-TEE/manifest.git -m qemu_v8.xml )
else
  echo "[=] OP-TEE workspace already initialized: ${WS_DIR}"
fi

echo "[+] Syncing OP-TEE workspace (this may take a while)"
# Robust sync: shallow, no bundle, single job, retry a few times to survive flaky networks
REPO_SYNC_FLAGS=( -c --no-clone-bundle --fetch-submodules -j1 )
SYNC_OK=0
for i in 1 2 3; do
  if ( cd "${WS_DIR}" && "${REPO_BIN:-repo}" sync "${REPO_SYNC_FLAGS[@]}" ); then SYNC_OK=1; break; fi
  echo "[!] repo sync attempt ${i} failed, retrying..." >&2
  sleep 3
done
if [ "${SYNC_OK}" != "1" ]; then
  echo "[!] repo sync failed after retries. You can try manually: cd ${WS_DIR} && repo sync -c -j1 --no-clone-bundle --fetch-submodules --fail-fast" >&2
  exit 1
fi

echo "[+] Checking host build dependencies"
MISSING=()
for bin in bison flex ninja; do
  if ! command -v "$bin" >/dev/null 2>&1; then MISSING+=("$bin"); fi
done
# Check GnuTLS headers for U-Boot mkeficapsule
if ! pkg-config --exists gnutls 2>/dev/null; then MISSING+=("libgnutls28-dev"); fi

# Helper to detect headers in multiarch include dirs
has_header(){
  local h="$1"
  if [ -f "/usr/include/${h}" ]; then return 0; fi
  local ma
  ma=$(gcc -print-multiarch 2>/dev/null || true)
  if [ -n "$ma" ] && [ -f "/usr/include/${ma}/${h}" ]; then return 0; fi
  # Fallback glob in case gcc -print-multiarch is unavailable
  if ls /usr/include/*-linux-gnu/"${h}" >/dev/null 2>&1; then return 0; fi
  return 1
}

# Check libmpc/mpfr/gmp for kernel GCC plugins (headers may live in multiarch dirs)
has_header mpc.h  || MISSING+=("libmpc-dev")
has_header mpfr.h || MISSING+=("libmpfr-dev")
has_header gmp.h  || MISSING+=("libgmp-dev")
if [ ${#MISSING[@]} -ne 0 ]; then
  echo "[!] Missing host tools: ${MISSING[*]}" >&2
  if [ "${OPTEE_AUTO_APT:-0}" = "1" ] && command -v apt-get >/dev/null 2>&1; then
    echo "[+] Attempting to install missing tools with sudo apt-get"
    sudo apt-get update && sudo apt-get install -y ninja-build bison flex libgnutls28-dev libmpc-dev libmpfr-dev libgmp-dev || true
  else
    echo "    Install on Ubuntu/Debian: sudo apt-get install -y ninja-build bison flex libgnutls28-dev libmpc-dev libmpfr-dev libgmp-dev" >&2
    echo "    Or run: sudo bash ${ROOT_DIR}/scripts/install_deps_ubuntu.sh" >&2
    exit 1
  fi
fi

echo "[+] Preparing toolchains and exporting TA dev kit (qemu_v8.mk)"
# Common make variables to stabilize build (disable Rust examples to avoid extra deps)
MAKE_VARS=( RUST_ENABLE=n )
# 1) Toolchains (use system toolchains if present, else fetch prebuilt)
if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1 && command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
  echo "[=] Using system cross toolchains"
else
  echo "[=] Fetching prebuilt cross toolchains via optee/build"
  make -C "${BUILD_DIR}" -f qemu_v8.mk -j"$(nproc)" ${MAKE_VARS[@]} toolchains
fi
# 2) Export OP-TEE OS TA Dev Kit explicitly
make -C "${BUILD_DIR}" -f qemu_v8.mk -j"$(nproc)" ${MAKE_VARS[@]} optee-os-devkit
# 3) Build Buildroot rootfs (provides userspace + hostfs mount helpers)
make -C "${BUILD_DIR}" -f qemu_v8.mk -j"$(nproc)" ${MAKE_VARS[@]} buildroot

echo "[+] Locating OP-TEE OS TA dev kit export directory"
# Try common locations produced by optee_os
CANDIDATES=()
CANDIDATES+=("${WS_DIR}/optee_os/out/arm-plat-vexpress/export-ta_arm64")
CANDIDATES+=("${WS_DIR}/optee_os/out/arm-plat-vexpress/export-ta_arm32")
CANDIDATES+=("${WS_DIR}/optee_os/out/arm/export-ta_arm64")
CANDIDATES+=("${WS_DIR}/optee_os/out/arm/export-ta_arm32")
# As a fallback, search under optee_os/out for any export-ta_*
if ! ls -d "${WS_DIR}/optee_os/out" >/dev/null 2>&1; then
  echo "[!] optee_os/out not found; did optee-os-devkit fail?" >&2
  exit 1
fi
FOUND=""
for d in "${CANDIDATES[@]}"; do
  if [ -d "$d" ]; then FOUND="$d"; break; fi
done
if [ -z "$FOUND" ]; then
  FOUND=$(find "${WS_DIR}/optee_os/out" -type d -name 'export-ta_arm64' -o -name 'export-ta_arm32' | head -n1 || true)
fi
if [ -z "$FOUND" ]; then
  echo "[!] TA dev kit export directory not found under ${WS_DIR}/optee_os/out. Try: make -C ${BUILD_DIR} -f qemu_v8.mk optee-os-devkit V=1" >&2
  exit 1
fi
DEV_KIT="$FOUND"
case "$DEV_KIT" in
  *export-ta_arm32) TA_CC="arm-linux-gnueabihf-" ;;
  *export-ta_arm64) TA_CC="aarch64-linux-gnu-" ;;
  *) TA_CC="aarch64-linux-gnu-" ;;
esac

echo "[+] Building TA using dev kit: ${DEV_KIT} (CROSS_COMPILE=${TA_CC})"
# Clean stale TA objects/deps generated against previous dev kits
make -C "${ROOT_DIR}/optee/ta" TA_DEV_KIT_DIR="${DEV_KIT}" clean || true
rm -f "${ROOT_DIR}/optee/ta"/*.o "${ROOT_DIR}/optee/ta"/*.d || true
rm -rf "${ROOT_DIR}/optee/ta"/export-ta_arm32 "${ROOT_DIR}/optee/ta"/export-ta_arm64 || true
make -C "${ROOT_DIR}/optee/ta" TA_DEV_KIT_DIR="${DEV_KIT}" CROSS_COMPILE="${TA_CC}"

TA_UUID=7a9b3b24-3e2f-4d5f-912d-8b7c1355629a
TA_BIN_PATH32="${ROOT_DIR}/optee/ta/export-ta_arm32/ta/${TA_UUID}.ta"
TA_BIN_PATH64="${ROOT_DIR}/optee/ta/export-ta_arm64/ta/${TA_UUID}.ta"
if [ -f "${TA_BIN_PATH32}" ]; then TA_BIN_PATH="${TA_BIN_PATH32}"; elif [ -f "${TA_BIN_PATH64}" ]; then TA_BIN_PATH="${TA_BIN_PATH64}"; else TA_BIN_PATH=""; fi

if [ -n "${TA_BIN_PATH}" ]; then
  echo "[=] TA built: ${TA_BIN_PATH}"
else
  echo "[!] TA output not found under export-ta_*; check TA build logs." >&2
fi

cat <<EOF

[Next steps]
- Start QEMU (in another terminal):
    make -C ${BUILD_DIR} -f qemu_v8.mk run

- In the guest (login as root), ensure tee-supplicant is running, then copy the TA:
    mkdir -p /lib/optee_armtz
    # Option A: Use a 9p mount to share host repo into guest, then copy TA
    #   (Requires re-running QEMU with an added -virtfs option; see OP-TEE build docs.)
    # Option B: Rebuild rootfs to include TA by placing it under buildroot target and re-packing.

- On the host, you can also run (fallback path works without TA):
    (in ${ROOT_DIR}/build) make run_optee_smoke

EOF

echo "[Done] optee/build ready at: ${BUILD_DIR}"
