#!/usr/bin/env sh
# Nori installer for Linux/macOS.
#
#   curl -fsSL https://raw.githubusercontent.com/mirkoattiladaniel/nori/main/install.sh | sh
#
# Downloads the `roll` project tool into ~/.nori/bin, adds it to your PATH, then
# (unless NORI_NO_COMPILER=1) runs `roll install stable` to fetch the matching
# compiler (noric). install.ps1 is the Windows equivalent.
set -eu

BASE="${NORI_INSTALL_BASE:-https://github.com/mirkoattiladaniel/nori}"
VERSION="${NORI_VERSION:-latest}"
NORI_HOME="${NORI_HOME:-$HOME/.nori}"
BIN_DIR="$NORI_HOME/bin"
ROLL="$BIN_DIR/roll"

info() { printf 'nori: %s\n' "$1" >&2; }
warn() { printf 'nori: %s\n' "$1" >&2; }

# target token: "<arch>-<os>", lowercased; must match std/sys sys::target().
arch=$(uname -m)
os=$(uname -s | tr '[:upper:]' '[:lower:]')
case "$os" in darwin) os=darwin ;; linux) os=linux ;; esac
TARGET="$arch-$os"

asset="roll-$TARGET"
if [ "$VERSION" = "latest" ]; then
    url="$BASE/releases/latest/download/$asset"
else
    url="$BASE/releases/download/$VERSION/$asset"
fi

mkdir -p "$BIN_DIR"
info "downloading $asset"
info "  from $url"
if command -v curl >/dev/null 2>&1; then
    curl -fSL "$url" -o "$ROLL" || { warn "download failed — is a $TARGET release published?"; exit 1; }
elif command -v wget >/dev/null 2>&1; then
    wget -qO "$ROLL" "$url" || { warn "download failed — is a $TARGET release published?"; exit 1; }
else
    warn "need curl or wget"; exit 1
fi
chmod +x "$ROLL"
info "installed roll -> $ROLL"

# add ~/.nori/bin to PATH via the shell rc (idempotent).
if [ "${NORI_NO_MODIFY_PATH:-0}" != "1" ]; then
    case ":$PATH:" in
        *":$BIN_DIR:"*) : ;;  # already present
        *)
            line="export PATH=\"$BIN_DIR:\$PATH\""
            for rc in "$HOME/.profile" "$HOME/.bashrc" "$HOME/.zshrc"; do
                [ -f "$rc" ] || continue
                grep -qF "$BIN_DIR" "$rc" 2>/dev/null || printf '\n# nori\n%s\n' "$line" >> "$rc"
            done
            info "added $BIN_DIR to PATH (restart your shell or: $line)"
            ;;
    esac
fi

if [ "${NORI_NO_COMPILER:-0}" != "1" ]; then
    info "installing the compiler (roll install stable) ..."
    "$ROLL" install stable || warn "roll install did not complete — retry with: roll install stable"
fi

info "done. Open a new shell and run:  roll --help"
