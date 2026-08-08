import paramiko
import os
import sys
import time

hostname = "192.168.5.249"
username = "root"
password = "Hanifbagus303"

script_dir = os.path.dirname(os.path.abspath(__file__))
local_base = os.path.abspath(os.path.join(script_dir, "..", ".."))
local_build_binary = os.path.join(local_base, "build", "linux_drm", "xdjunx")
remote_app = "/root/xdjunx"

if not os.path.exists(local_build_binary):
    print("[ERROR] Pre-compiled binary not found at %s. Please run build.bat linux drm first." % local_build_binary)
    sys.exit(1)

print("[DEPLOY] Connecting to SSH %s..." % hostname)
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

connected = False
for attempt in range(5):
    try:
        ssh.connect(hostname, username=username, password=password, timeout=15)
        print("[DEPLOY] SSH Connected.")
        connected = True
        break
    except Exception as e:
        print("[DEPLOY] Retry %d/5 failed (%s). Waiting 3s..." % (attempt + 1, e))
        time.sleep(3)

if not connected:
    print("[ERROR] Could not connect to SSH on %s" % hostname)
    sys.exit(1)

sftp = ssh.open_sftp()

def run_cmd(cmd):
    print("[SSH] Running: %s" % cmd)
    stdin, stdout, stderr = ssh.exec_command(cmd)
    out = stdout.read().decode('utf-8', errors='ignore')
    err = stderr.read().decode('utf-8', errors='ignore')
    if out.strip():
        print("--- STDOUT ---\n%s" % out.strip().encode('ascii', errors='replace').decode('ascii'))
    if err.strip():
        print("--- STDERR ---\n%s" % err.strip().encode('ascii', errors='replace').decode('ascii'))
    return out

# 1. Stop service
print("[DEPLOY] Stopping service...")
run_cmd("systemctl stop xdjunx_drm.service 2>/dev/null; killall -9 xdjunx 2>/dev/null || true")
run_cmd("mkdir -p %s" % remote_app)

# 2. Helper to upload directory
def upload_dir(local_path, remote_path):
    try:
        sftp.mkdir(remote_path)
    except IOError:
        pass
    for item in os.listdir(local_path):
        if item in ['.git', 'build', '.vscode', 'debug', 'raylib.zip']:
            continue
        l_item = os.path.join(local_path, item)
        r_item = remote_path + "/" + item
        if os.path.isdir(l_item):
            upload_dir(l_item, r_item)
        else:
            print("  -> Uploading %s to %s..." % (item, r_item))
            sftp.put(l_item, r_item)

# 3. Upload pre-compiled binary and updated controller scripts/assets
print("[DEPLOY] Uploading pre-compiled ARM64 binary to /root/xdjunx/xdjunx...")
sftp.put(local_build_binary, remote_app + "/xdjunx")
run_cmd("chmod +x %s/xdjunx" % remote_app)
print("[DEPLOY] Binary uploaded successfully.")

print("[DEPLOY] Cleaning and uploading controllers directory...")
run_cmd("rm -rf %s/controllers" % remote_app)
upload_dir(os.path.join(local_base, "controllers"), remote_app + "/controllers")

if os.path.exists(os.path.join(local_base, "assets")):
    print("[DEPLOY] Cleaning and uploading assets directory...")
    upload_dir(os.path.join(local_base, "assets"), remote_app + "/assets")

# 4. Configure udev rules for touchscreen
print("[DEPLOY] Setting up touchscreen udev rules...")
udev_rule = """# Permanent permissions for evdev input and USB touchscreen devices
KERNEL=="event*", SUBSYSTEM=="input", MODE="0666", GROUP="input"
SUBSYSTEM=="input", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="e2e3", MODE="0666", GROUP="input"
"""
with sftp.open("/etc/udev/rules.d/99-touchscreen.rules", "w") as f:
    f.write(udev_rule)

run_cmd("udevadm control --reload-rules && udevadm trigger")

# 5. Configure systemd service
print("[DEPLOY] Updating /etc/systemd/system/xdjunx_drm.service...")
service_file = """[Unit]
Description=XDJ-UNX Standalone Linux DRM Engine
After=sound.target local-fs.target
Wants=sound.target
Conflicts=getty@tty1.service

[Service]
Type=simple
User=root
Group=root
SupplementaryGroups=input video audio tty
WorkingDirectory=/root/xdjunx
ExecStart=/root/xdjunx/xdjunx
TTYPath=/dev/tty1
TTYReset=yes
TTYVHangup=yes
StandardInput=tty
StandardOutput=journal
StandardError=journal
Restart=always
RestartSec=1

[Install]
WantedBy=multi-user.target
"""
with sftp.open("/etc/systemd/system/xdjunx_drm.service", "w") as f:
    f.write(service_file)

# 5b. Configure Auto-Login for root on tty1
print("[DEPLOY] Configuring auto-login on tty1...")
run_cmd("mkdir -p /etc/systemd/system/getty@tty1.service.d")
autologin_override = """[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin root --noclear %I $TERM
"""
with sftp.open("/etc/systemd/system/getty@tty1.service.d/override.conf", "w") as f:
    f.write(autologin_override)

run_cmd("systemctl daemon-reload && systemctl enable xdjunx_drm.service")

# 6. Restart service and verify
print("[DEPLOY] Restarting xdjunx_drm.service...")
run_cmd("systemctl restart xdjunx_drm.service")

print("[DEPLOY] Checking service status...")
run_cmd("systemctl status xdjunx_drm.service --no-pager")
run_cmd("journalctl -u xdjunx_drm.service --no-pager -n 30")

sftp.close()
ssh.close()
print("[DEPLOY] COMPLETED SUCCESSFULLY!")
