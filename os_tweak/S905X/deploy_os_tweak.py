import paramiko
import os
import sys
import time

hostname = "192.168.5.249"
username = "root"
password = "Hanifbagus303"

script_dir = os.path.dirname(os.path.abspath(__file__))

print("[DEPLOY_TWEAK] Connecting to SSH %s..." % hostname)
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

connected = False
for attempt in range(5):
    try:
        ssh.connect(hostname, username=username, password=password, timeout=15)
        print("[DEPLOY_TWEAK] SSH Connected.")
        connected = True
        break
    except Exception as e:
        print("[DEPLOY_TWEAK] Retry %d/5 failed (%s). Waiting 3s..." % (attempt + 1, e))
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
    return stdout.channel.recv_exit_status()

# 1. Upload sysctl conf
conf_local = os.path.join(script_dir, "99-xdjunx-performance.conf")
if os.path.exists(conf_local):
    print("[DEPLOY_TWEAK] Uploading 99-xdjunx-performance.conf to /etc/sysctl.d/...")
    sftp.put(conf_local, "/etc/sysctl.d/99-xdjunx-performance.conf")

# 2. Upload governor service
gov_local = os.path.join(script_dir, "xdjunx-governor.service")
if os.path.exists(gov_local):
    print("[DEPLOY_TWEAK] Uploading xdjunx-governor.service to /etc/systemd/system/...")
    sftp.put(gov_local, "/etc/systemd/system/xdjunx-governor.service")

# 3. Upload apply_tweak.sh
sh_local = os.path.join(script_dir, "apply_tweak.sh")
if os.path.exists(sh_local):
    print("[DEPLOY_TWEAK] Uploading apply_tweak.sh to /root/apply_tweak.sh...")
    sftp.put(sh_local, "/root/apply_tweak.sh")
    run_cmd("chmod +x /root/apply_tweak.sh")

# 4. Upload setup_drm_kms.sh (Passive configuration)
drm_local = os.path.join(script_dir, "setup_drm_kms.sh")
if os.path.exists(drm_local):
    print("[DEPLOY_TWEAK] Uploading setup_drm_kms.sh to /root/setup_drm_kms.sh...")
    sftp.put(drm_local, "/root/setup_drm_kms.sh")
    run_cmd("chmod +x /root/setup_drm_kms.sh")

# 5. Upload & Execute setup_hibernate.sh
hib_local = os.path.join(script_dir, "setup_hibernate.sh")
if os.path.exists(hib_local):
    print("[DEPLOY_TWEAK] Uploading setup_hibernate.sh to /root/setup_hibernate.sh...")
    sftp.put(hib_local, "/root/setup_hibernate.sh")
    run_cmd("chmod +x /root/setup_hibernate.sh")

# 6. Execute apply_tweak.sh on target
print("[DEPLOY_TWEAK] Executing /root/apply_tweak.sh...")
run_cmd("/root/apply_tweak.sh")

# 7. Execute setup_hibernate.sh on target
print("[DEPLOY_TWEAK] Executing /root/setup_hibernate.sh...")
run_cmd("/root/setup_hibernate.sh")

sftp.close()
ssh.close()
print("[DEPLOY_TWEAK] COMPLETED SUCCESSFULLY!")
