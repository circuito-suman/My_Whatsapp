# QtWhatsApp for Linux

A lightweight, native Linux desktop client for WhatsApp engineered from scratch using **C++** and the **Qt Framework**. Unlike traditional Electron/Chromium-based clients, QtWhatsApp is compiled directly to native machine code, resulting in significantly lower memory usage, faster startup times, and highly responsive performance.

---

## 🚀 Features

- 🖥️ Native Linux desktop application
- ⚡ Built entirely with modern C++ and Qt
- 💾 Low memory footprint
- 🔄 Real-time asynchronous communication
- 🎯 Responsive, event-driven architecture
- 📦 Simple CMake build system
- 🐧 Designed specifically for Linux environments

---

## 📊 Performance Highlights

- **60% Lower Memory Usage**
  - Consumes significantly less RAM compared to Electron or browser-wrapper clients.

- **Sub-Second Synchronization**
  - Custom asynchronous WebSocket communication enables near real-time message synchronization.

- **Native Execution**
  - Runs as compiled C++ code without embedding Chromium, reducing CPU and memory overhead.

- **Responsive UI**
  - Event-driven architecture keeps the interface smooth even during large data transfers.

---

## 🏗️ Architecture

```
                +-------------------+
                |      Qt UI        |
                +---------+---------+
                          |
                Event/Signal System
                          |
                +---------v---------+
                |  Application Core |
                +---------+---------+
                          |
        +-----------------+-----------------+
        |                                   |
+-------v--------+                 +--------v--------+
| Network Layer  |                 | Local Data Cache|
| WebSockets     |                 | Optimized Models|
+-------+--------+                 +--------+--------+
        |                                   |
        +-----------------+-----------------+
                          |
                  WhatsApp Servers
```

---

## 🛠 Tech Stack

| Component | Technology |
|----------|------------|
| Language | C++17 / C++20 |
| Framework | Qt (Widgets / QML) |
| Networking | Qt Network |
| Real-Time Communication | Qt WebSockets |
| Build System | CMake |
| Platform | Linux |

---



# Prerequisites

Install:

- GCC / Clang
- CMake
- Qt Development Libraries
- Qt WebSockets

For Ubuntu/Debian:

```bash
sudo apt update

sudo apt install \
    build-essential \
    cmake \
    qtbase5-dev \
    qt5-qmake \
    libqt5websockets5-dev
```

---

# Build

Clone the repository:

```bash
git clone https://github.com/circuito-suman/My_Whatsapp.git
cd My_Whatsapp
```

Create the build directory:

```bash
mkdir build
cd build
```

Configure:

```bash
cmake ..
```

Compile:

```bash
make -j$(nproc)
```

---

# Run

```bash
./QtWhatsApp
```

---

# Performance Philosophy

Traditional desktop messaging applications often embed an entire Chromium browser, introducing considerable CPU and memory overhead before any application logic is executed.

QtWhatsApp follows a native-first architecture by leveraging the Qt Framework and modern C++ to eliminate unnecessary runtime layers.

Key optimizations include:

- Native compiled executable
- Event-driven asynchronous networking
- Efficient local conversation caching
- Minimal memory allocations
- Low-latency UI updates
- Responsive rendering without browser overhead

This approach keeps resource consumption low while maintaining a smooth user experience.

---

# Development Goals

- Native Linux experience
- Efficient memory utilization
- Fast startup
- Responsive interface
- Clean modular architecture
- Maintainable modern C++ codebase

---

# Future Roadmap

- [ ] Multiple account support
- [ ] File drag-and-drop
- [ ] Dark/Light theme switching
- [ ] AppImage release
- [ ] Snap package support

---



---

## Author

**Suman Saha**

GitHub: https://github.com/circuito-suman
