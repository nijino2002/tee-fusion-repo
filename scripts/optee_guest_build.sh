#!/usr/bin/env bash
set -euo pipefail

# Cross-compile a guest binary (AArch64) that uses libteec and runs inside QEMU guest.
# Requires optee_ws workspace prepared by scripts/optee_qemu_v8_setup.sh.

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
WS_DIR="${ROOT_DIR}/third_party/optee_ws"

if [ ! -d "${WS_DIR}" ]; then
  echo "[!] Missing ${WS_DIR}. Run: make optee_qemu_v8_setup" >&2
  exit 1
fi

GCC_AARCH64="${WS_DIR}/toolchains/aarch64/bin/aarch64-linux-gnu-gcc"
SYSROOT="${WS_DIR}/out-br/host/aarch64-buildroot-linux-gnu/sysroot"
INCLUDE_DIRS=(
  "${WS_DIR}/optee_client/out/export/include"
  "${SYSROOT}/usr/include"
)
LIB_DIRS=(
  "${WS_DIR}/optee_client/out/export/lib"
  "${SYSROOT}/usr/lib"
)

if [ ! -x "${GCC_AARCH64}" ]; then
  echo "[!] AArch64 toolchain not found at ${GCC_AARCH64}" >&2
  exit 1
fi

SRC="${ROOT_DIR}/examples/optee_smoke_guest/smoke_guest.c"
OUT="${ROOT_DIR}/build/bin/optee_smoke_guest"
mkdir -p "$(dirname "${OUT}")"

INCS=""; for d in "${INCLUDE_DIRS[@]}"; do INCS+=" -I${d}"; done
LIBS=""; for d in "${LIB_DIRS[@]}"; do LIBS+=" -L${d}"; done

set -x
"${GCC_AARCH64}" --sysroot="${SYSROOT}" -O2 -o "${OUT}" \
  ${INCS} ${LIBS} "${SRC}" -lteec
set +x

echo "[+] Built guest binary: ${OUT}"
echo "[i] Copy it into the guest and run: ./optee_smoke_guest"
