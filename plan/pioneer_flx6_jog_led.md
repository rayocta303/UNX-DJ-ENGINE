# Pioneer DDJ-FLX6 Jogwheel LED & Track Load Animation Protocol

Dokumen ini memuat analisa teknis dan spesifikasi protokol pengiriman data MIDI / SysEx untuk pengontrolan **LED Jogwheel Ring (Spinner)** dan **Animasi Track Load** pada kontroler **Pioneer DDJ-FLX6** (dan seri Pioneer DDJ sejenis).

---

## 1. Pioneer SysEx Keep-Alive & Full Feedback Mode

Pioneer DDJ-FLX6 membutuhkan sinyal handshake / Keep-Alive SysEx secara berkala (setiap 1000ms - 2000ms) agar kontroler tetap dalam mode feedback LED aktif penuh:

```c
// Sysex Keep-Alive Packet (12 Bytes)
const uint8_t PIONEER_SYSEX_KEEPALIVE[12] = {
    0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x04, 0x05, 0x00, 0x50, 0x02, 0xF7
};
```

---

## 2. Formatan Alamat & Data LED Jogwheel (Jog Ring / Spinner)

Posisi indikator LED melingkar pada jogwheel (*spinner*) dikirimkan melalui pesan **Control Change (CC)** khusus:

- **Status Byte**: `0xBB` (CC / MIDI Channel 12)
- **Data 1 (Control / Deck Index)**:
  - Deck 1 (Channel 1): `0x00`
  - Deck 2 (Channel 2): `0x01`
  - Deck 3 (Channel 3): `0x02`
  - Deck 4 (Channel 4): `0x03`
- **Data 2 (LED Position Value)**: `0x00` s/d `0x7F` (0 - 127)

### Rumus Kalkulasi Posisi Putaran LED (Playposition Spinner)

```c
// RPM standar: 33.3333 RPM (atau 45.0 RPM)
float revolutionsPerSecond = calibRPM / 60.0f;
float maxVal = 127.0f;
float speed = revolutionsPerSecond * maxVal;

// elapsedTime = posisi trek dalam detik
float elapsedTime = trackPositionPercent * trackTotalDurationSeconds;
uint8_t wheelPos = 1 + (uint8_t)fmodf(speed * elapsedTime, maxVal);

// Send MIDI Short Message
MIDI_SendShortMsg(0xBB, deckIdx, wheelPos);
```

---

## 3. Animasi Track Load (Track Loading LED Sequence)

Ketika lagu berhasil di-*load* ke Deck (misalnya melalui tombol `LOAD A` atau `LOAD B`):

### Urutan Sinyal Animasi Load Track:

1. **Light Sweep / Jog Chaser Animation**:
   - Kirimkan nilai `wheelPos` berurutan dari `0` hingga `127` dalam durasi ~300ms (atau putar 360° secara cepat 1-2 putaran).
   - Atau kirimkan pulsa `0x7F` (full outer ring light up) untuk mengonfirmasi lagu telah dimuat.

2. **Illuminate Deck & Mode LEDs**:
   - **Play/Pause LED**: `Status: 0x90 + deckIdx`, `Data1: 0x0B`, `Data2: 0x7F` (Nyalakan hijau saat ready/playing).
   - **Cue LED**: `Status: 0x90 + deckIdx`, `Data1: 0x0C`, `Data2: 0x7F` (Nyalakan orange/kuning jika Cue point diatur).
   - **Vinyl Mode LED**: `Status: 0x90 + deckIdx`, `Data1: 0x0E`, `Data2: 0x7F` (Nyalakan indikator vinyl mode aktif).

3. **Reset Posisi Jog Ring**:
   - Setelah animasi *sweep* selesai, set `wheelPos` kembali ke indeks `0x01` (posisi 12 o'clock awal trek).

---

## 4. Formatan Alamat & Handling VU Meter LED

Pioneer DDJ-FLX6 mengendalikan lampu indikator **Channel Level Meter (VU Meter)** secara terpisah untuk setiap deck melalui sinyal MIDI Control Change (CC):

### Sinyal MIDI Channel VU Meter:
- **Status Byte**: `0xB0 + deckIdx`
  - Deck 1 (Channel 1): `0xB0`
  - Deck 2 (Channel 2): `0xB1`
  - Deck 3 (Channel 3): `0xB2`
  - Deck 4 (Channel 4): `0xB3`
- **Data 1 (Control Number)**: `0x02` (Level Meter Indicator CC)
- **Data 2 (Value / Peak Level)**:
  - Range Normal: `0x00` s/d `0x76` (0 - 118 out of 127) -> Pemetaan linier ampitudo audio RMS (`0.0f` s/d `1.0f`).
  - Clip/Peak Indicator (LED Merah atas): Tambahkan `0x09` ke nilai (sehingga mencapai `0x7F` / 127) saat sinyal audio terdeteksi *clipping* (`peakLevel >= 1.0f`).

### Rumus Kalkulasi VU Meter:

```c
// amplitudeRMS: 0.0f s/d 1.0f
uint8_t meterVal = (uint8_t)(amplitudeRMS * 0x76); // Scale 0..118

// Jika audio mengalami clipping / peak, aktifkan LED merah (Peak indicator)
if (isPeakIndicatorActive) {
    meterVal += 0x09; // Nilai menjadi 127 (0x7F)
}

// Send MIDI Short Message ke Channel Deck yang bersangkutan
MIDI_SendShortMsg(0xB0 + deckIdx, 0x02, meterVal);
```

---

## 5. Rencana Integrasi ke Engine XDJ-UNX-C

1. Tambahkan fungsi `MIDI_UpdateOutputLeds(AudioEngine *engine)` pada `midi_handler.c` yang dipanggil setiap frame (~60 FPS).
2. Kalkulasikan nilai VU Meter dari buffer output audio per deck dan kirim pesan CC `0xB0 + deckIdx`, `0x02`, `val`.
3. Tambahkan variabel `float TrackLoadAnimTimer[4]` pada state deck untuk menangani animasi *spin/sweep* jogwheel selama 300ms setiap kali event `loadA` / `loadB` dipicu.
4. Kirimkan `PIONEER_SYSEX_KEEPALIVE` secara berkala via backend WinMIDI output.
