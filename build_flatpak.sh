#!/usr/bin/env bash
# WhatsApp Desktop - Flatpak Build & Packaging Script
# Uses KDE 6.8 Platform + io.qt.qtwebengine.BaseApp 6.8 with full proprietary codecs (H.264/AAC)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}====================================================${NC}"
echo -e "${CYAN}   WhatsApp Desktop - Native Flatpak Builder        ${NC}"
echo -e "${CYAN}   (Includes full H.264, AAC, and WebRTC codecs)   ${NC}"
echo -e "${CYAN}====================================================${NC}"

# 1. Check for flatpak
if ! command -v flatpak &>/dev/null; then
    echo -e "${RED}Error: flatpak is not installed on your system.${NC}"
    echo "Install it with: sudo apt install flatpak"
    exit 1
fi

# 2. Check for flatpak-builder
if ! command -v flatpak-builder &>/dev/null; then
    echo -e "${YELLOW}Notice: flatpak-builder is not installed.${NC}"
    echo -e "Please install it by running:"
    echo -e "    ${GREEN}sudo apt install -y flatpak-builder${NC}"
    echo ""
    read -p "Would you like to try installing flatpak-builder now via sudo? [y/N]: " -r install_ans
    if [[ $install_ans =~ ^[Yy]$ ]]; then
        sudo apt update && sudo apt install -y flatpak-builder
    else
        echo "Please install flatpak-builder and re-run this script."
        exit 1
    fi
fi

# 3. Ensure Flathub remote is configured
echo -e "${BLUE}ℹ️  Verifying Flathub remote...${NC}"
if ! flatpak remotes | grep -q "flathub"; then
    echo "Adding Flathub remote..."
    flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
fi

# 4. Install required runtimes (KDE 6.8 + QtWebEngine BaseApp 6.8)
echo -e "${BLUE}ℹ️  Verifying Flatpak runtimes (KDE 6.8 & QtWebEngine BaseApp with codecs)...${NC}"
flatpak install -y --noninteractive flathub \
    org.kde.Platform//6.8 \
    org.kde.Sdk//6.8 \
    io.qt.qtwebengine.BaseApp//6.8 \
    org.freedesktop.Platform.ffmpeg-full//24.08

# 5. Build and install the Flatpak locally
echo -e "${BLUE}🔨 Building and packaging WhatsApp Desktop Flatpak...${NC}"
flatpak-builder --user --install --force-clean build-flatpak com.github.circuito.whatsapp.yaml

echo -e "${GREEN}====================================================${NC}"
echo -e "${GREEN}✅ WhatsApp Desktop Flatpak successfully installed! ${NC}"
echo -e "${GREEN}====================================================${NC}"
echo ""
echo -e "You can launch it anytime by running:"
echo -e "    ${CYAN}flatpak run com.github.circuito.whatsapp${NC}"
echo ""
echo -e "Or search for 'WhatsApp Desktop' in your application menu."
