#!/bin/bash
# ==============================================================================
# XDJ-UNX DRM/KMS Display & Kernel DRM Optimization for Amlogic S905X
# Target OS: Armbian / Linux ARM64 (Mali-450 / Meson DRM)
# ==============================================================================

set -e

echo "[DRM_KMS] Setting up DRM/KMS Kernel & Display Configurations..."

# 1. DRM Device Permissions & Udev Rules for Direct Rendering
echo "[DRM_KMS] 1. Setting up /dev/dri/card0 & renderD128 udev rules..."
cat << 'EOF' > /etc/udev/rules.d/99-drm-kms.rules
# Grant video & input access to direct DRM/KMS framebuffer and DRM rendering
KERNEL=="card*", SUBSYSTEM=="drm", GROUP="video", MODE="0666"
KERNEL=="renderD*", SUBSYSTEM=="drm", GROUP="video", MODE="0666"
KERNEL=="tty1", GROUP="tty", MODE="0666"
EOF

udevadm control --reload-rules && udevadm trigger || true

# 2. Modprobe Configuration for Amlogic Meson DRM & Mali Driver
echo "[DRM_KMS] 2. Configuring /etc/modprobe.d/meson-drm.conf..."
cat << 'EOF' > /etc/modprobe.d/meson-drm.conf
# Meson DRM & Mali 450 Driver options
options meson_drm preferred_depth=32
options lima max_gatt_size=256
EOF

# 3. Armbian Environment DRM Kernel Parameters (/boot/armbianEnv.txt)
ARMBIAN_ENV="/boot/armbianEnv.txt"
if [ -f "$ARMBIAN_ENV" ]; then
    echo "[DRM_KMS] 3. Configuring Armbian DRM/KMS boot parameters in $ARMBIAN_ENV..."
    
    # Backup original env
    cp "$ARMBIAN_ENV" "${ARMBIAN_ENV}.bak"
    
    # Ensure drm parameters exist
    if ! grep -q "extraargs=" "$ARMBIAN_ENV"; then
        echo "extraargs=video=HDMI-A-1:1024x600M@60 drm.vblankoffdelay=1" >> "$ARMBIAN_ENV"
    else
        sed -i 's/extraargs=\(.*\)/extraargs=\1 video=HDMI-A-1:1024x600M@60 drm.vblankoffdelay=1/' "$ARMBIAN_ENV" || true
    fi
fi

echo "[DRM_KMS] DRM/KMS Kernel & Display Configuration completed!"
