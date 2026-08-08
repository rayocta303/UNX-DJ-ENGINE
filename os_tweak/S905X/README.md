# XDJ-UNX OS, Kernel & DRM/KMS Performance Tweaks (S905X)

Script & konfigurasi ini didesain khusus untuk mengoptimalkan performa kernel Linux (Armbian S905X ARM64) pada sistem standalone XDJ-UNX.

## Isi File dalam Folder `os_tweak/S905X/`:

1. `apply_tweak.sh`
   - Shell script utama yang mengeksekusi optimasi sysctl, menonaktifkan service lambat saat booting (`wait-online`, `apt-daily`, dsb.), mengunci governor CPU ke `performance`, serta menyesuaikan I/O scheduler storage.
2. `setup_drm_kms.sh`
   - Shell script untuk mengonfigurasi DRM/KMS mode pada Kernel Amlogic S905X (`/boot/armbianEnv.txt`, udev rules `/dev/dri/card0` & `renderD128`, serta `/etc/modprobe.d/meson-drm.conf`). **(TIDAK DI-EKSEKUSI OTOMATIS)**.
3. `setup_hibernate.sh`
   - Shell script untuk mengonfigurasi fitur Hibernasi / Suspend systemd dan swap file agar perangkat mendukung deep sleep / fast boot recovery.
4. `99-xdjunx-performance.conf`
   - Konfigurasi `sysctl` kernel untuk VM/RAM (`swappiness=10`, `vfs_cache_pressure=50`), real-time audio scheduling, serta menyenyapkan (quiet) log kernel saat boot.
5. `xdjunx-governor.service`
   - Unit service systemd untuk memastikan CPU dipaksa berjalan pada frekuensi maksimum (`performance governor`) setiap kali OS menyala.
6. `deploy_os_tweak.py`
   - Script otomatis untuk mengunggah dan mengeksekusi tweak OS/Kernel dasar di atas secara langsung ke perangkat Armbian melalui SSH.
7. `deploy_drm.py`
   - Script otomatis untuk mengunggah binary ARM64 hasil kompilasi (`build/linux_drm/xdjunx`), controller mappings, assets, udev rules touchscreen, serta mengonfigurasi systemd auto-login `xdjunx_drm.service` pada tty1 melalui SSH.

## Cara Menggunakan:

### Opsi 1: Otomatis Deployment Aplikasi & DRM via PC
Jalankan kompilasi ARM64 lalu deploy aplikasi ke perangkat target:
```cmd
build.bat linux drm
py os_tweak\S905X\deploy_drm.py
```

### Opsi 2: Otomatis Deployment OS & Kernel Tweak via PC
Jalankan script Python berikut dari folder proyek:
```cmd
py os_tweak\S905X\deploy_os_tweak.py
```

### Opsi 3: Mengaktifkan Konfigurasi DRM/KMS (Manual di Target)
Jika ingin menerapkan konfigurasi DRM/KMS pada kernel S905X:
```bash
chmod +x setup_drm_kms.sh
./setup_drm_kms.sh
```

### Opsi 4: Mengaktifkan Konfigurasi Hibernasi / Deep Sleep (Manual di Target)
Jika ingin mengaktifkan dukungan hibernasi & suspend-to-disk pada Armbian:
```bash
chmod +x setup_hibernate.sh
./setup_hibernate.sh
```
