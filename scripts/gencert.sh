#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
if [[ -f server.crt && -f server.key ]]; then echo "certs exist"; exit 0; fi
openssl req -x509 -newkey rsa:2048 -sha256 -days 365 -nodes -keyout server.key -out server.crt -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
echo "OK"
