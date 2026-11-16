#!/usr/bin/env bash
# Creates the ECDSA P-256 key pair that signs OTA release manifests (docs/OTA_UPDATES.md, "Signing").
#
#   tools/ota/make_signing_key.sh [--rotate] [private-key-file]
#       default private-key-file: ~/ota-signing-key/ota_signing_key.pem
#
# - The PRIVATE key is written outside the repository (mode 600) and never printed. Put it into the
#   GitHub secret OTA_SIGNING_KEY, keep an offline backup, then delete the local copy.
# - The PUBLIC key is written into src/update/OtaPublicKey.h: commit that. Firmware built from it
#   only accepts manifests signed with this private key.
#
# There is a key already: the script refuses to replace it. A new key pair locks out every device
# that still has the old public key (each then needs a USB flash), so --rotate is only for a lost
# or leaked private key. The release job also refuses a changed key unless "rotate_key" is ticked.
set -euo pipefail

ROTATE=0
if [[ "${1:-}" == "--rotate" ]]; then
  ROTATE=1
  shift
fi
# Resolve the key path from the caller's directory, before changing into the repository.
KEY="$(realpath -m "${1:-$HOME/ota-signing-key/ota_signing_key.pem}")"
REPO="$(cd "$(dirname "$0")/../.." && pwd -P)"
HEADER="$REPO/src/update/OtaPublicKey.h"

case "$KEY" in
  "$REPO"/*) echo "Refusing to write the private key inside the repository: $KEY" >&2; exit 1 ;;
esac
if [[ -e "$KEY" ]]; then
  echo "$KEY already exists: not overwritten. Move it away to create a new key pair." >&2
  exit 1
fi
if [[ -f "$HEADER" ]] && grep -q "BEGIN PUBLIC KEY" "$HEADER" && (( ! ROTATE )); then
  echo "$HEADER already holds a public key: devices carry it, replacing it locks them out." >&2
  echo "The private key belongs in the GitHub secret OTA_SIGNING_KEY and your offline backup." >&2
  echo "Only if it is lost or leaked: run again with --rotate (then flash every device over USB)." >&2
  exit 1
fi

DIR="$(dirname "$KEY")"
if [[ ! -d "$DIR" ]]; then
  mkdir -p "$DIR"
  chmod 700 "$DIR"  # only a folder created here; never change an existing one (e.g. $HOME)
fi
( umask 077; openssl ecparam -name prime256v1 -genkey -noout -out "$KEY" )
PUB="$(openssl ec -in "$KEY" -pubout 2>/dev/null)"

{
  echo "#pragma once"
  echo "// Public key for OTA manifest signatures (ECDSA P-256), made by tools/ota/make_signing_key.sh."
  echo "// The matching private key is the GitHub secret OTA_SIGNING_KEY (never in this repository)."
  echo "// Changing this key locks out devices with the old key until they are flashed over USB."
  echo "static const char OTA_PUBLIC_KEY_PEM[] ="
  while IFS= read -r line; do echo "    \"$line\\n\""; done <<< "$PUB"
  echo "    ;"
} > "$HEADER"

echo "Private key: $KEY (mode 600). Next:"
echo "  1. GitHub -> Settings -> Secrets and variables -> Actions -> New repository secret"
echo "     name OTA_SIGNING_KEY, value = the whole file content (or: gh secret set OTA_SIGNING_KEY < \"$KEY\")"
echo "  2. Keep an offline backup (password manager / USB stick), then delete the local copy."
echo "  3. Commit src/update/OtaPublicKey.h."
