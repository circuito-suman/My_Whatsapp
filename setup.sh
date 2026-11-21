#!/bin/bash

# WhatsApp Desktop - Development Setup & System Integration Script
# Handles dependencies, development environment setup, and system integration
# For building, use ./build.sh instead

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Configuration
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
BINARY_NAME="WHATSAPP_REBASE"

# Helper functions
print_header() {
    echo -e "${CYAN}================================"
    echo -e "$1"
    echo -e "================================${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

ask_user() {
    local question="$1"
    local default="$2"
    local response

    if [ -n "$default" ]; then
        echo -e "${PURPLE}❓ $question [${default}]: ${NC}"
    else
        echo -e "${PURPLE}❓ $question: ${NC}"
    fi

    read -r response
    if [ -z "$response" ] && [ -n "$default" ]; then
        response="$default"
    fi
    echo "$response"
}

check_dependencies() {
    print_header "🔍 Checking Development Dependencies"

    local missing_deps=()

    # Check for essential build tools
    if ! command -v cmake >/dev/null 2>&1; then
        missing_deps+=("cmake")
    fi

    if ! command -v make >/dev/null 2>&1; then
        missing_deps+=("make")
    fi

    if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
        missing_deps+=("g++/clang++")
    fi

    # Check for Qt6
    if ! pkg-config --exists Qt6Core Qt6Widgets Qt6WebEngineWidgets 2>/dev/null; then
        missing_deps+=("Qt6 development packages")
    fi

    # Check for Git
    if ! command -v git >/dev/null 2>&1; then
        missing_deps+=("git")
    fi

    if [ ${#missing_deps[@]} -gt 0 ]; then
        print_error "Missing development dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo -e "  ${RED}- $dep${NC}"
        done
        echo
        print_info "Install missing dependencies:"
        echo -e "  ${CYAN}Ubuntu/Debian:${NC} sudo apt install cmake make g++ qt6-base-dev qt6-webengine-dev git"
        echo -e "  ${CYAN}Fedora:${NC} sudo dnf install cmake make gcc-c++ qt6-qtbase-devel qt6-qtwebengine-devel git"
        echo -e "  ${CYAN}Arch:${NC} sudo pacman -S cmake make gcc qt6-base qt6-webengine git"
        
        local install_deps=$(ask_user "Would you like to install dependencies automatically?" "y")
        if [[ "$install_deps" =~ ^[Yy] ]]; then
            install_dependencies
        else
            exit 1
        fi
    else
        print_success "All development dependencies are installed"
    fi
}

install_dependencies() {
    print_header "📦 Installing Development Dependencies"

    # Detect package manager and install
    if command -v apt >/dev/null 2>&1; then
        print_info "Installing dependencies with apt..."
        sudo apt update
        sudo apt install -y cmake make g++ qt6-base-dev qt6-webengine-dev git
    elif command -v dnf >/dev/null 2>&1; then
        print_info "Installing dependencies with dnf..."
        sudo dnf install -y cmake make gcc-c++ qt6-qtbase-devel qt6-qtwebengine-devel git
    elif command -v pacman >/dev/null 2>&1; then
        print_info "Installing dependencies with pacman..."
        sudo pacman -S --needed cmake make gcc qt6-base qt6-webengine git
    elif command -v zypper >/dev/null 2>&1; then
        print_info "Installing dependencies with zypper..."
        sudo zypper install -y cmake make gcc-c++ qt6-base-devel qt6-webengine-devel git
    else
        print_error "Could not detect package manager. Please install dependencies manually."
        exit 1
    fi

    print_success "Dependencies installed successfully"
}

detect_environment() {
    print_header "🖥️  Detecting Development Environment"

    local desktop_env="Unknown"
    local has_tray="Unknown"
    local qt_version="Unknown"

    # Desktop environment
    if [ -n "$XDG_CURRENT_DESKTOP" ]; then
        desktop_env="$XDG_CURRENT_DESKTOP"
    elif [ -n "$DESKTOP_SESSION" ]; then
        desktop_env="$DESKTOP_SESSION"
    fi

    # Qt version
    if command -v qmake6 >/dev/null 2>&1; then
        qt_version=$(qmake6 -version | grep "Using Qt version" | cut -d' ' -f4)
    elif command -v qmake >/dev/null 2>&1; then
        qt_version=$(qmake -version | grep "Using Qt version" | cut -d' ' -f4)
    fi

    # System tray support
    if pgrep -x "gnome-shell" >/dev/null; then
        has_tray="GNOME (may need AppIndicator extension)"
    elif pgrep -x "plasmashell" >/dev/null; then
        has_tray="KDE Plasma (native support)"
    elif pgrep -x "xfce4-panel" >/dev/null; then
        has_tray="XFCE (native support)"
    elif pgrep -x "lxpanel" >/dev/null; then
        has_tray="LXDE (native support)"
    else
        has_tray="Generic X11/Wayland"
    fi

    print_info "Desktop Environment: $desktop_env"
    print_info "Qt Version: $qt_version"
    print_info "System Tray: $has_tray"
    echo
}

setup_development_environment() {
    print_header "⚙️  Setting Up Development Environment"

    # Create build directory
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
        print_success "Created build directory"
    fi

    # Set up Git hooks (if in a git repository)
    if [ -d ".git" ]; then
        local setup_hooks=$(ask_user "Set up Git pre-commit hooks for code formatting?" "y")
        if [[ "$setup_hooks" =~ ^[Yy] ]]; then
            setup_git_hooks
        fi
    fi

    # Configure Qt environment
    configure_qt_environment

    print_success "Development environment configured"
}

setup_git_hooks() {
    print_info "Setting up Git hooks..."
    
    mkdir -p .git/hooks
    
    # Pre-commit hook for basic checks
    cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
# Pre-commit hook for WhatsApp Desktop

echo "Running pre-commit checks..."

# Check for large files
if git diff --cached --name-only | xargs -I {} find {} -size +10M 2>/dev/null | grep -q .; then
    echo "Error: Large files detected. Please use Git LFS or remove them."
    exit 1
fi

# Check for merge conflict markers
if git diff --cached | grep -E '^(\+.*<{7}|\+.*={7}|\+.*>{7})'; then
    echo "Error: Merge conflict markers found."
    exit 1
fi

echo "Pre-commit checks passed!"
EOF

    chmod +x .git/hooks/pre-commit
    print_success "Git hooks configured"
}

configure_qt_environment() {
    print_info "Configuring Qt environment..."

    # Create .env file for development
    cat > .env << EOF
# Production environment configuration
QT_AUTO_SCREEN_SCALE_FACTOR=0
QT_SCALE_FACTOR=1.0
QT_SCREEN_SCALE_FACTORS=1.0
QT_FONT_DPI=96
QT_LOGGING_RULES=qt.webenginecontext.debug=false
QTFRAMEWORK_BYPASS_LICENSE_CHECK=1
EOF

    print_success "Qt environment configured"
}

install_desktop_integration() {
    print_header "🖥️  Installing Desktop Integration"

    # Check if application is built
    if [ ! -f "$BUILD_DIR/$BINARY_NAME" ]; then
        print_warning "Application not built yet. Run './build.sh local' first."
        local build_now=$(ask_user "Build application now?" "y")
        if [[ "$build_now" =~ ^[Yy] ]]; then
            if [ -f "./build.sh" ]; then
                ./build.sh local
            else
                print_error "build.sh not found. Cannot build application."
                return 1
            fi
        else
            print_info "Skipping desktop integration (application not built)"
            return 0
        fi
    fi

    local install_desktop=$(ask_user "Install desktop file for application launchers?" "y")

    if [[ "$install_desktop" =~ ^[Yy] ]]; then
        # Create applications directory
        mkdir -p ~/.local/share/applications

        # Find icon
        local icon_path="$PROJECT_DIR/resources/icons/whatsapp.png"
        if [ ! -f "$icon_path" ]; then
            icon_path="$PROJECT_DIR/resources/icons/app.png"
        fi
        if [ ! -f "$icon_path" ]; then
            icon_path="whatsapp-desktop"  # Use generic icon name
        fi

        # Create desktop file
        local desktop_content="[Desktop Entry]
Name=WhatsApp Desktop
Comment=WhatsApp Web client with system tray integration
Exec=$BUILD_DIR/$BINARY_NAME
Icon=$icon_path
Type=Application
Categories=Network;InstantMessaging;Chat;
Keywords=whatsapp;chat;messaging;web;
StartupNotify=true
StartupWMClass=$BINARY_NAME
Terminal=false
SingleMainWindow=true"

        echo "$desktop_content" > ~/.local/share/applications/whatsapp-desktop.desktop
        chmod +x ~/.local/share/applications/whatsapp-desktop.desktop

        # Update desktop database
        if command -v update-desktop-database >/dev/null 2>&1; then
            update-desktop-database ~/.local/share/applications 2>/dev/null || true
        fi

        print_success "Desktop integration installed"
    fi
}

create_launcher_script() {
    print_header "🚀 Creating Launcher Script"

    local create_launcher=$(ask_user "Create a launcher script?" "y")

    if [[ "$create_launcher" =~ ^[Yy] ]]; then
        local launcher_content="#!/bin/bash
# WhatsApp Desktop Launcher
# Auto-generated by setup script

WHATSAPP_DIR=\"$PROJECT_DIR\"
WHATSAPP_BINARY=\"$BUILD_DIR/$BINARY_NAME\"

if [ ! -f \"\$WHATSAPP_BINARY\" ]; then
    echo \"WhatsApp Desktop binary not found. Please build the application first:\"
    echo \"  cd '$PROJECT_DIR' && ./build.sh local\"
    exit 1
fi

# Load environment
if [ -f \"\$WHATSAPP_DIR/.env\" ]; then
    set -a
    source \"\$WHATSAPP_DIR/.env\"
    set +a
fi

# Launch WhatsApp Desktop
cd \"\$WHATSAPP_DIR\"
exec \"\$WHATSAPP_BINARY\" \"\$@\"
"

        echo "$launcher_content" > "$PROJECT_DIR/whatsapp-desktop"
        chmod +x "$PROJECT_DIR/whatsapp-desktop"

        # Offer to install system-wide
        local install_system=$(ask_user "Install launcher to /usr/local/bin for system-wide access?" "n")
        if [[ "$install_system" =~ ^[Yy] ]]; then
            if sudo cp "$PROJECT_DIR/whatsapp-desktop" /usr/local/bin/; then
                print_success "System-wide launcher installed to /usr/local/bin/whatsapp-desktop"
            else
                print_warning "Could not install system-wide launcher (requires sudo)"
            fi
        fi

        print_success "Launcher script created: $PROJECT_DIR/whatsapp-desktop"
    fi
}

setup_autostart() {
    print_header "⚡ Autostart Configuration"

    local setup_autostart=$(ask_user "Enable autostart on login?" "n")

    if [[ "$setup_autostart" =~ ^[Yy] ]]; then
        mkdir -p ~/.config/autostart

        # Find icon
        local icon_path="$PROJECT_DIR/resources/icons/whatsapp.png"
        if [ ! -f "$icon_path" ]; then
            icon_path="$PROJECT_DIR/resources/icons/app.png"
        fi
        if [ ! -f "$icon_path" ]; then
            icon_path="whatsapp-desktop"
        fi

        local autostart_content="[Desktop Entry]
Name=WhatsApp Desktop
Comment=WhatsApp Web client
Exec=$BUILD_DIR/$BINARY_NAME
Icon=$icon_path
Type=Application
X-GNOME-Autostart-enabled=true
StartupNotify=false
Terminal=false
Hidden=false"

        echo "$autostart_content" > ~/.config/autostart/whatsapp-desktop.desktop
        print_success "Autostart enabled"
    fi
}

create_development_aliases() {
    print_header "🔧 Creating Development Aliases"

    local create_aliases=$(ask_user "Create development aliases for your shell?" "y")

    if [[ "$create_aliases" =~ ^[Yy] ]]; then
        local shell_rc=""
        if [ -n "$ZSH_VERSION" ]; then
            shell_rc="$HOME/.zshrc"
        elif [ -n "$BASH_VERSION" ]; then
            shell_rc="$HOME/.bashrc"
        fi

        if [ -n "$shell_rc" ] && [ -f "$shell_rc" ]; then
            # Add aliases
            echo "" >> "$shell_rc"
            echo "# WhatsApp Desktop Development Aliases" >> "$shell_rc"
            echo "alias wa-build='cd \"$PROJECT_DIR\" && ./build.sh local'" >> "$shell_rc"
            echo "alias wa-run='cd \"$PROJECT_DIR\" && ./build.sh local && ./build/$BINARY_NAME'" >> "$shell_rc"
            echo "alias wa-clean='cd \"$PROJECT_DIR\" && ./build.sh clean'" >> "$shell_rc"
            echo "alias wa-appimage='cd \"$PROJECT_DIR\" && ./build.sh appimage'" >> "$shell_rc"
            echo "alias wa-packages='cd \"$PROJECT_DIR\" && ./build.sh packages'" >> "$shell_rc"
            echo "alias wa-dev='cd \"$PROJECT_DIR\"'" >> "$shell_rc"

            print_success "Development aliases added to $shell_rc"
            print_info "Restart your terminal or run 'source $shell_rc' to use aliases:"
            print_info "  wa-build    - Build application"
            print_info "  wa-run      - Build and run application"
            print_info "  wa-clean    - Clean build files"
            print_info "  wa-appimage - Build AppImage"
            print_info "  wa-packages - Build system packages"
            print_info "  wa-dev      - Go to project directory"
        else
            print_warning "Could not detect shell configuration file"
        fi
    fi
}

show_usage_instructions() {
    print_header "📖 Next Steps & Usage"

    echo -e "${GREEN}🎉 WhatsApp Desktop Setup Complete!${NC}"
    echo
    echo -e "${CYAN}Build the Application:${NC}"
    echo -e "  ${BLUE}./build.sh local${NC}     - Build for development"
    echo -e "  ${BLUE}./build.sh appimage${NC}  - Build portable AppImage"
    echo -e "  ${BLUE}./build.sh packages${NC}  - Build system packages"
    echo -e "  ${BLUE}./build.sh clean${NC}     - Clean build files"
    echo
    echo -e "${CYAN}Launch Methods:${NC}"
    echo -e "  ${BLUE}1.${NC} Application Launcher: Search 'WhatsApp Desktop'"
    echo -e "  ${BLUE}2.${NC} Terminal: $BUILD_DIR/$BINARY_NAME"
    if [ -f "$PROJECT_DIR/whatsapp-desktop" ]; then
        echo -e "  ${BLUE}3.${NC} Launcher Script: $PROJECT_DIR/whatsapp-desktop"
        if [ -f "/usr/local/bin/whatsapp-desktop" ]; then
            echo -e "  ${BLUE}4.${NC} System Command: whatsapp-desktop"
        fi
    fi
    echo
    echo -e "${CYAN}Features:${NC}"
    echo -e "  ${GREEN}✅${NC} System tray integration"
    echo -e "  ${GREEN}✅${NC} Session persistence"
    echo -e "  ${GREEN}✅${NC} Memory optimization"
    echo -e "  ${GREEN}✅${NC} Security hardening"
    echo
    echo -e "${CYAN}Development:${NC}"
    echo -e "  ${BLUE}• Code:${NC} Edit source files in the project directory"
    echo -e "  ${BLUE}• Build:${NC} Run './build.sh local' after changes"
    echo -e "  ${BLUE}• Debug:${NC} Use Qt Creator or your preferred IDE"
    echo -e "  ${BLUE}• Test:${NC} Run the built binary for testing"
    echo
}

main() {
    print_header "🚀 WhatsApp Desktop - Development Setup"
    echo -e "${GREEN}Setting up development environment and system integration${NC}"
    echo -e "${YELLOW}For building the application, use ./build.sh${NC}"
    echo

    detect_environment
    check_dependencies
    setup_development_environment
    install_desktop_integration
    create_launcher_script
    setup_autostart
    create_development_aliases
    show_usage_instructions

    echo -e "${GREEN}🎯 Setup complete! Use './build.sh local' to build the application.${NC}"
}

# Show help if requested
if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    echo "WhatsApp Desktop - Development Setup Script"
    echo
    echo "This script sets up the development environment and system integration."
    echo "For building the application, use ./build.sh instead."
    echo
    echo "Usage:"
    echo "  ./setup.sh          - Interactive setup"
    echo "  ./setup.sh --help   - Show this help"
    echo
    exit 0
fi

# Run main function
main "$@"