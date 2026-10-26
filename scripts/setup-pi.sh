#!/usr/bin/env bash
# Installs the build and runtime dependencies on the Raspberry Pi.
# Run once over SSH: VS Code task "Pi: install dependencies".
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    gdb \
    libgpiod-dev \
    gpiod \
    libssl-dev \
    zlib1g-dev \
    ca-certificates

# GPIO access without sudo.
if ! id -nG "$USER" | grep -qw gpio; then
    sudo usermod -aG gpio "$USER"
    echo "Added $USER to the gpio group. Log out and back in (or reboot) for it to apply."
fi

echo
echo "cmake:    $(cmake --version | head -n1)"
echo "gcc:      $(gcc --version | head -n1)"
echo "libgpiod: $(pkg-config --modversion libgpiodcxx)"
echo "GPIO chips:"
gpiodetect
