#!/bin/bash
# ==============================================================================
# XDJ-UNX Systemd Sleep, Suspend & Hibernation Setup for Amlogic S905X
# Target OS: Armbian / Linux ARM64
# ==============================================================================

set -e

echo "[HIBERNATE_SETUP] Configuring Linux Suspend & Hibernation for Fast-Boot..."

# 1. Systemd Sleep Configuration
echo "[HIBERNATE_SETUP] 1. Creating /etc/systemd/sleep.conf.d/xdjunx-sleep.conf..."
mkdir -p /etc/systemd/sleep.conf.d/

cat << 'EOF' > /etc/systemd/sleep.conf.d/xdjunx-sleep.conf
[Sleep]
AllowSuspend=yes
AllowHibernation=yes
AllowSuspendThenHibernate=yes
AllowHybridSleep=yes
SuspendState=freeze
HibernateMode=platform shutdown
EOF

systemctl daemon-reload || true

# 2. Swap File Verification for Hibernation (Suspend to Disk)
echo "[HIBERNATE_SETUP] 2. Checking swap space for hibernation..."
SWAP_EXISTS=$(swapon --show --noheadings | wc -l)

if [ "$SWAP_EXISTS" -eq 0 ]; then
    echo "[HIBERNATE_SETUP] No swap active. Creating 512MB fast swapfile for hibernation..."
    if [ ! -f /swapfile_xdj ]; then
        dd if=/dev/zero of=/swapfile_xdj bs=1M count=512 status=progress || true
        chmod 600 /swapfile_xdj
        mkswap /swapfile_xdj
    fi
    swapon /swapfile_xdj || true
    if ! grep -q "/swapfile_xdj" /etc/fstab; then
        echo "/swapfile_xdj none swap defaults 0 0" >> /etc/fstab
    fi
else
    echo "[HIBERNATE_SETUP] Existing swap partition/file detected."
fi

# 3. Enable Systemd Suspend & Hibernate Targets
echo "[HIBERNATE_SETUP] 3. Verifying systemd suspend/hibernate targets..."
systemctl unmask suspend.target hibernate.target hybrid-sleep.target suspend-then-hibernate.target || true

echo "[HIBERNATE_SETUP] Hibernation & Deep Sleep configuration completed successfully!"
