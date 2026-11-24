#!/bin/bash

# WhatsApp Desktop - Unified Build Script
# Handles all build types: local, AppImage, and system packages

set -e

# Configuration
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
export QTFRAMEWORK_BYPASS_LICENSE_CHECK=1

# Auto-detect Qt 6 Installation Path
if [ -z "$QT_DIR" ] && [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    CACHED_QT6_DIR=$(grep -E '^Qt6_DIR:PATH=' "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2-)
    if [ -n "$CACHED_QT6_DIR" ] && [[ "$CACHED_QT6_DIR" == */lib/cmake/Qt6 ]]; then
        CANDIDATE_QT="${CACHED_QT6_DIR%/lib/cmake/Qt6}"
        if [ -d "$CANDIDATE_QT" ]; then
            QT_DIR="$CANDIDATE_QT"
        fi
    fi
fi

if [ -z "$QT_DIR" ]; then
    for pattern in "$HOME"/Qt/6.8.*/gcc_64 "$HOME"/Qt/6.*/gcc_64; do
        for dir in $(ls -d $pattern 2>/dev/null | sort -V -r); do
            if [ -f "$dir/lib/libQt6WebEngineWidgets.so.6" ] && [ -f "$dir/lib/libQt6WebChannel.so.6" ]; then
                QT_DIR="$dir"
                break 2
            fi
        done
    done
fi

if [ -n "$QT_DIR" ] && [ -d "$QT_DIR" ]; then
    export CMAKE_PREFIX_PATH="$QT_DIR${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
    export PATH="$QT_DIR/bin:$PATH"
    export LD_LIBRARY_PATH="$QT_DIR/lib:/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    if [ -f "$QT_DIR/bin/qmake" ]; then
        export QMAKE="$QT_DIR/bin/qmake"
    fi
else
    # Fallback to system Qt6
    if command -v qmake6 >/dev/null 2>&1; then
        export QMAKE="$(command -v qmake6)"
    elif command -v qmake >/dev/null 2>&1; then
        export QMAKE="$(command -v qmake)"
    fi
fi

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

print_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
print_success() { echo -e "${GREEN}✅ $1${NC}"; }
print_error() { echo -e "${RED}❌ $1${NC}"; }
print_header() { echo -e "${CYAN}================================\n$1\n================================${NC}"; }

show_help() {
    echo "WhatsApp Desktop - Unified Build Script"
    echo
    echo "Usage: $0 <command>"
    echo
    echo "Commands:"
    echo "  local      Build for local development"
    echo "  appimage   Build portable AppImage"
    echo "  flatpak    Build and install Flatpak with full codecs (H.264/AAC)"
    echo "  clean      Clean build files"
    echo "  help       Show this help"
    echo
}

build_local() {
    print_header "🔨 Building Local Development Version"

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    print_info "Configuring with CMake..."
    if [ -n "$QT_DIR" ]; then
        cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_DIR"
    else
        cmake .. -DCMAKE_BUILD_TYPE=Release
    fi

    print_info "Building with $(nproc) cores (capped at -j4 to prevent RAM exhaustion)..."
    cmake --build . -j4

    if [ -f "WHATSAPP_REBASE" ]; then
        print_success "Local build completed: $BUILD_DIR/WHATSAPP_REBASE"
        chmod +x "WHATSAPP_REBASE"
    else
        print_error "Build failed"
        exit 1
    fi

    cd "$PROJECT_DIR"
}

build_appimage() {
    print_header "📦 Building AppImage"

    # Ensure local build exists
    if [ ! -f "$BUILD_DIR/WHATSAPP_REBASE" ]; then
        build_local
    fi

    cd "$BUILD_DIR"

    # Download tools if needed
    if [ ! -f "linuxdeploy-x86_64.AppImage" ]; then
        print_info "Downloading linuxdeploy..."
        curl -fsSL --connect-timeout 10 -m 60 "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" -o linuxdeploy-x86_64.AppImage || true
        chmod +x linuxdeploy-x86_64.AppImage 2>/dev/null || true
    fi

    if [ ! -f "linuxdeploy-plugin-qt-x86_64.AppImage" ]; then
        print_info "Downloading Qt plugin..."
        curl -fsSL --connect-timeout 10 -m 60 "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" -o linuxdeploy-plugin-qt-x86_64.AppImage || true
        chmod +x linuxdeploy-plugin-qt-x86_64.AppImage 2>/dev/null || true
    fi

    # Create AppDir
    rm -rf WhatsApp-Desktop.AppDir
    mkdir -p WhatsApp-Desktop.AppDir

    # Desktop file
    cat > WhatsApp-Desktop.AppDir/whatsapp-desktop.desktop << 'EOD'
[Desktop Entry]
Name=WhatsApp Desktop
Comment=Fast, native WhatsApp client with system tray and calling integration
Exec=WHATSAPP_REBASE
Icon=whatsapp-desktop
Type=Application
Categories=Network;InstantMessaging;
EOD

    # Icon
    if [ -f "$PROJECT_DIR/resources/icons/whatsapp.png" ]; then
        cp "$PROJECT_DIR/resources/icons/whatsapp.png" WhatsApp-Desktop.AppDir/whatsapp-desktop.png
    fi

    # Build AppImage
    print_info "Creating AppImage..."
    export QML_SOURCES_PATHS="$PROJECT_DIR"

    local qt_plugins_dir=""
    if [ -n "${QMAKE:-}" ] && [ -x "$QMAKE" ]; then
        qt_plugins_dir="$("$QMAKE" -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
    fi

    local extra_platforms=()
    for p in libqwayland-generic.so libqwayland-egl.so; do
        if [ -n "$qt_plugins_dir" ] && [ -f "$qt_plugins_dir/platforms/$p" ]; then
            extra_platforms+=("$p")
        fi
    done
    if [ ${#extra_platforms[@]} -gt 0 ]; then
        export EXTRA_PLATFORM_PLUGINS="$(IFS=';'; echo "${extra_platforms[*]}")"
    fi

    local extra_qt_plugins=()
    for dir in wayland-decoration-client wayland-shell-integration; do
        if [ -n "$qt_plugins_dir" ] && [ -d "$qt_plugins_dir/$dir" ]; then
            extra_qt_plugins+=("$dir")
        fi
    done
    if [ ${#extra_qt_plugins[@]} -gt 0 ]; then
        export EXTRA_QT_PLUGINS="$(IFS=';'; echo "${extra_qt_plugins[*]}")"
    fi

    rm -f WhatsApp*Desktop*.AppImage

    ./linuxdeploy-x86_64.AppImage --appdir WhatsApp-Desktop.AppDir \
        --executable WHATSAPP_REBASE \
        --desktop-file WhatsApp-Desktop.AppDir/whatsapp-desktop.desktop \
        --icon-file WhatsApp-Desktop.AppDir/whatsapp-desktop.png \
        --plugin qt

    # linuxdeploy-plugin-qt omits wayland-graphics-integration-client, which is required for Wayland EGL rendering
    if [ -n "$qt_plugins_dir" ] && [ -d "$qt_plugins_dir/wayland-graphics-integration-client" ]; then
        mkdir -p WhatsApp-Desktop.AppDir/usr/plugins/wayland-graphics-integration-client
        cp -a "$qt_plugins_dir/wayland-graphics-integration-client/"*.so \
            WhatsApp-Desktop.AppDir/usr/plugins/wayland-graphics-integration-client/
    fi

    ./linuxdeploy-x86_64.AppImage --appdir WhatsApp-Desktop.AppDir --output appimage

    # Rename result if created
    local appimage=$(find . -maxdepth 1 -name "WhatsApp*Desktop*.AppImage" ! -name "linuxdeploy*" | head -1)
    if [ -n "$appimage" ]; then
        if [ "$appimage" != "./WhatsApp-Desktop-x86_64.AppImage" ]; then
            mv "$appimage" "WhatsApp-Desktop-x86_64.AppImage"
        fi
        chmod +x "WhatsApp-Desktop-x86_64.AppImage"
        print_success "AppImage created: $BUILD_DIR/WhatsApp-Desktop-x86_64.AppImage"
    fi

    cd "$PROJECT_DIR"
}

clean_build() {
    print_header "🧹 Cleaning Build Files"

    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    fi
}

build_all() {
    build_local
    build_appimage
    print_success "All builds completed!"
}

# Main script logic
case "$1" in
    local)
        build_local
        ;;
    appimage)
        build_appimage
        ;;
    flatpak)
        ./build_flatpak.sh
        ;;
    all)
        build_all
        ;;
    clean)
        clean_build
        ;;
    help|--help|-h|"")
        show_help
        ;;
    *)
        print_error "Unknown command: $1"
        show_help
        exit 1
        ;;
esac
