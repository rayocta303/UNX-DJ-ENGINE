# UNX-DJ Controller Debug Terminal

Utility CLI interaktif untuk merekam, memantau (*live monitoring*), menguji sinyal LED/SysEx, dan melakukan debugging pada DJ Controller (termasuk Pioneer DDJ-FLX6, DDJ-FLX4, dan MIDI Controller generik).

## Fitur Utama

- **Live MIDI Monitor**: Memantau secara real-time pesan MIDI inbound dari controller (Status, Channel, Note/Control, Raw Hex/Dec, Normalized float).
- **XML Mapping Inspector**: Otomatis mendeteksi dan mencocokkan pesan MIDI dengan pemetaan ControlObject UNX dari file preset XML (`Pioneer-DDJ-FLX6.midi.xml`).
- **Interactive Device Enumeration**: Menampilkan daftar seluruh port MIDI Input & Output yang terhubung di sistem Windows.
- **Short MIDI & SysEx Tester**: Mengirimkan perintah MIDI short message atau SysEx ke controller untuk menguji indikator LED / VU Meter / Jog Wheel Display.

## Cara Mengompilasi & Menjalankan

Jalankan perintah berikut pada terminal di folder `p:\XDJ-UNX-C\debug`:

```cmd
build_debug.bat
```

Untuk mengompilasi dan langsung menjalankan terminal debug:
```cmd
build_debug.bat run
```

## Daftar Perintah Terminal Debug

| Perintah | Deskripsi |
| :--- | :--- |
| `list` | Menampilkan daftar perangkat MIDI Input & Output yang terdeteksi |
| `connect <id>` | Terhubung ke port MIDI controller berdasarkan ID |
| `load <xml_path>` | Memuat file preset pemetaan XML (misal: `load ../controllers/Pioneer-DDJ-FLX6.midi.xml`) |
| `monitor` | Mengaktifkan/menonaktifkan log MIDI inbound secara real-time |
| `send <status_hex> <control_hex> <val_hex>` | Mengirim sinyal MIDI short message ke controller (misal: `send 90 0B 7F`) |
| `sysex_flx6_keepalive` | Menampilkan/mengirim format SysEx Keep-Alive untuk Pioneer DDJ-FLX6 |
| `clear` | Membersihkan layar terminal |
| `help` | Menampilkan menu bantuan perintah |
| `exit` | Keluar dari aplikasi terminal debug |
