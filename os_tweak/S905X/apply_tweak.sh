#!/bin/bash
# ==============================================================================
# XDJ-UNX OS & Kernel Performance Optimization & Fast-Boot Script
# Target OS: Armbian / Linux ARM64 (Linux DRM / Standalone)
# ==============================================================================

set -e

echo "[TWEAK] Starting XDJ-UNX Kernel & OS Boot Optimization..."

# 1. Apply Sysctl Kernel Tuning
echo "[TWEAK] 1. Applying kernel sysctl parameters..."
if [ -f /etc/sysctl.d/99-xdjunx-performance.conf ]; then
    sysctl -p /etc/sysctl.d/99-xdjunx-performance.conf || true
fi

# 2. Fast Systemd Shutdown / Timeout Configuration
echo "[TWEAK] 2. Configuring fast systemd shutdown timeouts..."
sed -i 's/#DefaultTimeoutStopSec=90s/DefaultTimeoutStopSec=3s/' /etc/systemd/system.conf || true
sed -i 's/DefaultTimeoutStopSec=90s/DefaultTimeoutStopSec=3s/' /etc/systemd/system.conf || true
sed -i 's/#DefaultTimeoutStartSec=90s/DefaultTimeoutStartSec=5s/' /etc/systemd/system.conf || true

# 3. Disable Slow / Non-Essential Services to Accelerate Boot Time
echo "[TWEAK] 3. Disabling unnecessary slow boot services..."
SERVICES_TO_DISABLE=(
    "systemd-networkd-wait-online.service"
    "NetworkManager-wait-online.service"
    "ModemManager.service"
    "apt-daily.service"
    "apt-daily-upgrade.service"
    "apt-daily.timer"
    "apt-daily-upgrade.timer"
    "man-db.timer"
)

for srv in "${SERVICES_TO_DISABLE[@]}"; do
    if systemctl is-enabled "$srv" &>/dev/null; then
        echo "  -> Disabling $srv..."
        systemctl disable "$srv" || true
        systemctl mask "$srv" || true
    fi
done

# 4. Set CPU Scaling Governor to Performance
echo "[TWEAK] 4. Setting CPU scaling governor to performance..."
for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    if [ -f "$g" ]; then
        echo performance > "$g" || true
    fi
done

# Enable CPU Governor Service
if [ -f /etc/systemd/system/xdjunx-governor.service ]; then
    systemctl daemon-reload
    systemctl enable xdjunx-governor.service || true
    systemctl start xdjunx-governor.service || true
fi

# 5. Optimize I/O Scheduler for Storage (eMMC/SD)
echo "[TWEAK] 5. Optimizing I/O Scheduler for eMMC/SD storage..."
for blk in /sys/block/mmcblk*/queue/scheduler; do
    if [ -f "$blk" ]; then
        echo mq-deadline > "$blk" 2>/dev/null || echo none > "$blk" 2>/dev/null || true
    fi
done

echo "[TWEAK] All OS & Kernel tweaks applied successfully!"
