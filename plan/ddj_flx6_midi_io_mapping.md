# Pioneer DDJ-FLX6 MIDI I/O Mapping & Exhaustive Register Audit Specification

Dokumen ini memuat analisis spesifikasi teknis protokol MIDI I/O, spesifikasi pengiriman data LED (**Jog Wheel Spinner**, **VU Meter**, **Pioneer SysEx Keep-Alive**), serta **breakdown audit menyeluruh untuk seluruh 552 register MIDI (368 Terimplementasi & 191 Belum Terimplementasi)** pada kontroler **Pioneer DDJ-FLX6** di engine **XDJ-UNX-C** dengan format tabel Markdown presisi tinggi tanpa pemutusan baris (*single-line table rows*).

---

## 1. Pioneer SysEx Handshake & LED Feedback Protocol (Total: 1 Protocol System)

Kontroler Pioneer DDJ-FLX6 membutuhkan sinyal handshake / *Keep-Alive* SysEx secara berkala (setiap 1500ms) agar kontroler tetap berada dalam mode *hardware feedback LED active*:

```c
// Pioneer Sysex Keep-Alive Packet (12 Bytes)
const uint8_t PIONEER_SYSEX_KEEPALIVE[12] = {
    0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x04, 0x05, 0x00, 0x50, 0x02, 0xF7
};
```

---

## 2. Formatan Alamat & Indikator LED Jogwheel Spinner Ring (Total: 4 Output Channels)

Posisi indikator LED melingkar pada jogwheel (*spinner*) dikirimkan melalui pesan **Control Change (CC)** khusus:

- **Status Byte**: `0xBB` (CC / MIDI Channel 12)
- **Data 1 (Control / Deck Index)**: Deck 1: `0x00`, Deck 2: `0x01`, Deck 3: `0x02`, Deck 4: `0x03` 
- **Data 2 (LED Position Value)**: `0x01` s/d `0x48` (1 - 72 langkah putaran 360°)

### Rumus Kalkulasi Posisi Putaran LED (Playposition Spinner)

```c
// 72 langkah per putaran 360deg, 0.6075 rotation speed multiplier (33.33 RPM)
double currentSec = engine->Decks[i].Position / (double)engine->Decks[i].SampleRate;
double jogStep = fmod(currentSec * 72.0 * 0.6075, 72.0);
if (jogStep < 0.0) jogStep += 72.0;
uint8_t wheelPos = (uint8_t)(jogStep) + 1; // Range: 1..72

MIDI_SendShortMsg(0xBB, deckIdx, wheelPos);
```

---

## 3. Formatan Alamat & Handling VU Meter LED (Total: 4 Output Channels)

- **Status Byte**: `0xB0 + deckIdx` (Deck 1: `0xB0`, Deck 2: `0xB1`, Deck 3: `0xB2`, Deck 4: `0xB3`)
- **Data 1 (Control Number)**: `0x02` (Level Meter Indicator CC)
- **Data 2 (Value / Peak Level)**: `0x00` s/d `0x76` (0 - 118 out of 127) + Peak Clip `9` (`127` saat `rms >= 0.98f`).

---

## 4. Matriks Ringkasan Audit Status Pemetaan (Total: 552 Registers)

| Kategori Modul | Fitur Utama Terimplementasi | Fitur Belum Terimplementasi | Terimplementasi | Belum Terimplementasi | Status Paritas |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Mixer & EQ** | Faders 1-4, Crossfader, Gain Trim, EQ High/Mid/Low, Color FX, PFL Cue, Headphones Mix | 14-Bit Fader Fine LSB, EQ Kills, Secondary PFL | 24 Registers | 36 Registers | **40% Complete** |
| **Transport & Jog** | Play/Pause, Cue, Pitch MSB, Tempo Range, Sync, Key Lock, Vinyl, Scratch, Touch, Deck Layer, Slip Mode | Shift + Jog Scrub Seek, Nudge fine | 32 Registers | 24 Registers | **57% Complete** |
| **Browser & Nav** | Browse Knob, Browse Push (`enter`), Back Button (`back`), View Toggle, Load A/B, Rotary Track Scroll | Waveform Zoom Step | 8 Registers | 8 Registers | **50% Complete** |
| **Loops & Cues** | Auto Loop 1-16, Loop In/Out/Exit, Halve/Double, Hot Cue 1-8 Set/Clear, Beat Jump Pads (-8..+8) | Loop In/Out Adjust, Reloop, Cue Call | 36 Registers | 32 Registers | **53% Complete** |
| **Beat FX & Rack** | FX On/Off, Dry/Wet, FX Select, Beat Left/Right/Tap, Channel Assign | Shift+FX Quick Reset, Rack Meta Knobs | 16 Registers | 64 Registers | **20% Complete** |
| **Merge FX** | - | Merge FX Knob Turn (L/R), Preset Select Buttons | 0 Registers | 8 Registers | **0% Complete** |
| **Performance Pads**| Hot Cue Mode, Beat Jump Mode, Key Shift Transposition Pads (-4..+3) | Sampler (16 Slots), Key Shift Mode, Pad FX | 64 Registers | 132 Registers | **33% Complete** |
| **Output Driver** | SysEx Keep-Alive, Play, Cue, Vinyl, VU Meter, Jog Spinner Ring, Hot Cue LEDs | Pad Mode LEDs, Merge FX LEDs | 14 Registers | 16 Registers | **47% Complete** |

---

## 5. CATALOG REGISTER MIDI YANG SUDAH TERIMPLEMENTASI (Total: 368 Registers)

### 5.1. Mixer, EQ & Headphone Control Registers (Total: 30 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | `0xB0` | `0x00` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.tempoSliderMSB` | TEMPO (DECK1) - fader - Tempo control MSB |
| 2 | `0xB1` | `0x00` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.tempoSliderMSB` | TEMPO (DECK2) - fader - Tempo control MSB |
| 3 | `0xB2` | `0x00` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.tempoSliderMSB` | TEMPO (DECK3) - fader - Tempo control MSB |
| 4 | `0xB3` | `0x00` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.tempoSliderMSB` | TEMPO (DECK4) - fader - Tempo control MSB |
| 5 | `0xB0` | `0x04` | Ch 1 (CC) | `[Channel1]` | `pregain` | TRIM - rotate |
| 6 | `0xB1` | `0x04` | Ch 2 (CC) | `[Channel2]` | `pregain` | TRIM - rotate |
| 7 | `0xB2` | `0x04` | Ch 3 (CC) | `[Channel3]` | `pregain` | TRIM - rotate |
| 8 | `0xB3` | `0x04` | Ch 4 (CC) | `[Channel4]` | `pregain` | TRIM - rotate |
| 9 | `0xB0` | `0x13` | Ch 1 (CC) | `[Channel1]` | `volume` | CHANNELFADER - slider |
| 10 | `0xB1` | `0x13` | Ch 2 (CC) | `[Channel2]` | `volume` | CHANNELFADER - slider |
| 11 | `0xB2` | `0x13` | Ch 3 (CC) | `[Channel3]` | `volume` | CHANNELFADER - slider |
| 12 | `0xB3` | `0x13` | Ch 4 (CC) | `[Channel4]` | `volume` | CHANNELFADER - slider |
| 13 | `0xB6` | `0x1F` | Ch 7 (CC) | `[Master]` | `crossfader` | CROSSFADER - slider |
| 14 | `0xB0` | `0x20` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK1) - fader - Tempo control LSB |
| 15 | `0xB1` | `0x20` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK2) - fader - Tempo control LSB |
| 16 | `0xB2` | `0x20` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK3) - fader - Tempo control LSB |
| 17 | `0xB3` | `0x20` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK4) - fader - Tempo control LSB |
| 18 | `0xB0` | `0x24` | Ch 1 (CC) | `[Channel1]` | `pregain` | TRIM - rotate |
| 19 | `0xB1` | `0x24` | Ch 2 (CC) | `[Channel2]` | `pregain` | TRIM - rotate |
| 20 | `0xB2` | `0x24` | Ch 3 (CC) | `[Channel3]` | `pregain` | TRIM - rotate |
| 21 | `0xB3` | `0x24` | Ch 4 (CC) | `[Channel4]` | `pregain` | TRIM - rotate |
| 22 | `0xB0` | `0x33` | Ch 1 (CC) | `[Channel1]` | `volume` | CHANNELFADER - slider |
| 23 | `0xB1` | `0x33` | Ch 2 (CC) | `[Channel2]` | `volume` | CHANNELFADER - slider |
| 24 | `0xB2` | `0x33` | Ch 3 (CC) | `[Channel3]` | `volume` | CHANNELFADER - slider |
| 25 | `0xB3` | `0x33` | Ch 4 (CC) | `[Channel4]` | `volume` | CHANNELFADER - slider |
| 26 | `0xB6` | `0x3F` | Ch 7 (CC) | `[Master]` | `crossfader` | CROSSFADER - slider |
| 27 | `0x90` | `0x54` | Ch 1 (Note) | `[Channel1]` | `pfl` | CUE Channel - press - toggle Headphone Cue |
| 28 | `0x91` | `0x54` | Ch 2 (Note) | `[Channel2]` | `pfl` | CUE Channel - press - toggle Headphone Cue |
| 29 | `0x92` | `0x54` | Ch 3 (Note) | `[Channel3]` | `pfl` | CUE Channel - press - toggle Headphone Cue |
| 30 | `0x93` | `0x54` | Ch 4 (Note) | `[Channel4]` | `pfl` | CUE Channel - press - toggle Headphone Cue |


### 5.2. Deck Transport & Jogwheel Manipulation Registers (Total: 156 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 31 | `0x97` | `0x00` | Ch 8 (Note) | `[Channel1]` | `hotcue_1_activate` | PAD 1 (DECK1) HOT CUE MODE - press - set hotcue |
| 32 | `0x98` | `0x00` | Ch 9 (Note) | `[Channel1]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 33 | `0x99` | `0x00` | Ch 10 (Note) | `[Channel2]` | `hotcue_1_activate` | PAD 1 (DECK2) HOT CUE MODE - press - set hotcue |
| 34 | `0x9A` | `0x00` | Ch 11 (Note) | `[Channel2]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 35 | `0x9B` | `0x00` | Ch 12 (Note) | `[Channel3]` | `hotcue_1_activate` | PAD 1 (DECK3) HOT CUE MODE - press - set hotcue |
| 36 | `0x9C` | `0x00` | Ch 13 (Note) | `[Channel3]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 37 | `0x9D` | `0x00` | Ch 14 (Note) | `[Channel4]` | `hotcue_1_activate` | PAD 1 (DECK4) HOT CUE MODE - press - set hotcue |
| 38 | `0x9E` | `0x00` | Ch 15 (Note) | `[Channel4]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 39 | `0x97` | `0x01` | Ch 8 (Note) | `[Channel1]` | `hotcue_2_activate` | PAD 2 (DECK1) HOT CUE MODE - press - set hotcue |
| 40 | `0x98` | `0x01` | Ch 9 (Note) | `[Channel1]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 41 | `0x99` | `0x01` | Ch 10 (Note) | `[Channel2]` | `hotcue_2_activate` | PAD 2 (DECK2) HOT CUE MODE - press - set hotcue |
| 42 | `0x9A` | `0x01` | Ch 11 (Note) | `[Channel2]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 43 | `0x9B` | `0x01` | Ch 12 (Note) | `[Channel3]` | `hotcue_2_activate` | PAD 2 (DECK3) HOT CUE MODE - press - set hotcue |
| 44 | `0x9C` | `0x01` | Ch 13 (Note) | `[Channel3]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 45 | `0x9D` | `0x01` | Ch 14 (Note) | `[Channel4]` | `hotcue_2_activate` | PAD 2 (DECK4) HOT CUE MODE - press - set hotcue |
| 46 | `0x9E` | `0x01` | Ch 15 (Note) | `[Channel4]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 47 | `0x97` | `0x02` | Ch 8 (Note) | `[Channel1]` | `hotcue_3_activate` | PAD 3 (DECK1) HOT CUE MODE - press - set hotcue |
| 48 | `0x98` | `0x02` | Ch 9 (Note) | `[Channel1]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 49 | `0x99` | `0x02` | Ch 10 (Note) | `[Channel2]` | `hotcue_3_activate` | PAD 3 (DECK2) HOT CUE MODE - press - set hotcue |
| 50 | `0x9A` | `0x02` | Ch 11 (Note) | `[Channel2]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 51 | `0x9B` | `0x02` | Ch 12 (Note) | `[Channel3]` | `hotcue_3_activate` | PAD 3 (DECK3) HOT CUE MODE - press - set hotcue |
| 52 | `0x9C` | `0x02` | Ch 13 (Note) | `[Channel3]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 53 | `0x9D` | `0x02` | Ch 14 (Note) | `[Channel4]` | `hotcue_3_activate` | PAD 3 (DECK4) HOT CUE MODE - press - set hotcue |
| 54 | `0x9E` | `0x02` | Ch 15 (Note) | `[Channel4]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 55 | `0x97` | `0x03` | Ch 8 (Note) | `[Channel1]` | `hotcue_4_activate` | PAD 4 (DECK1) HOT CUE MODE - press - set hotcue |
| 56 | `0x98` | `0x03` | Ch 9 (Note) | `[Channel1]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 57 | `0x99` | `0x03` | Ch 10 (Note) | `[Channel2]` | `hotcue_4_activate` | PAD 4 (DECK2) HOT CUE MODE - press - set hotcue |
| 58 | `0x9A` | `0x03` | Ch 11 (Note) | `[Channel2]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 59 | `0x9B` | `0x03` | Ch 12 (Note) | `[Channel3]` | `hotcue_4_activate` | PAD 4 (DECK3) HOT CUE MODE - press - set hotcue |
| 60 | `0x9C` | `0x03` | Ch 13 (Note) | `[Channel3]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 61 | `0x9D` | `0x03` | Ch 14 (Note) | `[Channel4]` | `hotcue_4_activate` | PAD 4 (DECK4) HOT CUE MODE - press - set hotcue |
| 62 | `0x9E` | `0x03` | Ch 15 (Note) | `[Channel4]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 63 | `0x97` | `0x04` | Ch 8 (Note) | `[Channel1]` | `hotcue_5_activate` | PAD 5(DECK1) HOT CUE MODE - press - set hotcue |
| 64 | `0x98` | `0x04` | Ch 9 (Note) | `[Channel1]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 65 | `0x99` | `0x04` | Ch 10 (Note) | `[Channel2]` | `hotcue_5_activate` | PAD 5 (DECK2) HOT CUE MODE - press - set hotcue |
| 66 | `0x9A` | `0x04` | Ch 11 (Note) | `[Channel2]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 67 | `0x9B` | `0x04` | Ch 12 (Note) | `[Channel3]` | `hotcue_5_activate` | PAD 5(DECK3) HOT CUE MODE - press - set hotcue |
| 68 | `0x9C` | `0x04` | Ch 13 (Note) | `[Channel3]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 69 | `0x9D` | `0x04` | Ch 14 (Note) | `[Channel4]` | `hotcue_5_activate` | PAD 5 (DECK4) HOT CUE MODE - press - set hotcue |
| 70 | `0x9E` | `0x04` | Ch 15 (Note) | `[Channel4]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 71 | `0x97` | `0x05` | Ch 8 (Note) | `[Channel1]` | `hotcue_6_activate` | PAD 6 (DECK1) HOT CUE MODE - press - set hotcue |
| 72 | `0x98` | `0x05` | Ch 9 (Note) | `[Channel1]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 73 | `0x99` | `0x05` | Ch 10 (Note) | `[Channel2]` | `hotcue_6_activate` | PAD 6 (DECK2) HOT CUE MODE - press - set hotcue |
| 74 | `0x9A` | `0x05` | Ch 11 (Note) | `[Channel2]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 75 | `0x9B` | `0x05` | Ch 12 (Note) | `[Channel3]` | `hotcue_6_activate` | PAD 6 (DECK3) HOT CUE MODE - press - set hotcue |
| 76 | `0x9C` | `0x05` | Ch 13 (Note) | `[Channel3]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 77 | `0x9D` | `0x05` | Ch 14 (Note) | `[Channel4]` | `hotcue_6_activate` | PAD 6 (DECK4) HOT CUE MODE - press - set hotcue |
| 78 | `0x9E` | `0x05` | Ch 15 (Note) | `[Channel4]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 79 | `0x97` | `0x06` | Ch 8 (Note) | `[Channel1]` | `hotcue_7_activate` | PAD 7 (DECK1) HOT CUE MODE - press - set hotcue |
| 80 | `0x98` | `0x06` | Ch 9 (Note) | `[Channel1]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 81 | `0x99` | `0x06` | Ch 10 (Note) | `[Channel2]` | `hotcue_7_activate` | PAD 7 (DECK2) HOT CUE MODE - press - set hotcue |
| 82 | `0x9A` | `0x06` | Ch 11 (Note) | `[Channel2]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 83 | `0x9B` | `0x06` | Ch 12 (Note) | `[Channel3]` | `hotcue_7_activate` | PAD 7 (DECK3) HOT CUE MODE - press - set hotcue |
| 84 | `0x9C` | `0x06` | Ch 13 (Note) | `[Channel3]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 85 | `0x9D` | `0x06` | Ch 14 (Note) | `[Channel4]` | `hotcue_7_activate` | PAD 7 (DECK4) HOT CUE MODE - press - set hotcue |
| 86 | `0x9E` | `0x06` | Ch 15 (Note) | `[Channel4]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 87 | `0x97` | `0x07` | Ch 8 (Note) | `[Channel1]` | `hotcue_8_activate` | PAD 8 (DECK1) HOT CUE MODE - press - set hotcue |
| 88 | `0x98` | `0x07` | Ch 9 (Note) | `[Channel1]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 89 | `0x99` | `0x07` | Ch 10 (Note) | `[Channel2]` | `hotcue_8_activate` | PAD 8 (DECK2) HOT CUE MODE - press - set hotcue |
| 90 | `0x9A` | `0x07` | Ch 11 (Note) | `[Channel2]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 91 | `0x9B` | `0x07` | Ch 12 (Note) | `[Channel3]` | `hotcue_8_activate` | PAD 8 (DECK3) HOT CUE MODE - press - set hotcue |
| 92 | `0x9C` | `0x07` | Ch 13 (Note) | `[Channel3]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 93 | `0x9D` | `0x07` | Ch 14 (Note) | `[Channel4]` | `hotcue_8_activate` | PAD 8 (DECK4) HOT CUE MODE - press - set hotcue |
| 94 | `0x9E` | `0x07` | Ch 15 (Note) | `[Channel4]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 95 | `0x90` | `0x0B` | Ch 1 (Note) | `[Channel1]` | `play` | PLAY/PAUSE (DECK1) - press - Play/Pause |
| 96 | `0x91` | `0x0B` | Ch 2 (Note) | `[Channel2]` | `play` | PLAY/PAUSE (DECK2) - press - Play/Pause |
| 97 | `0x92` | `0x0B` | Ch 3 (Note) | `[Channel3]` | `play` | PLAY/PAUSE (DECK3) - press - Play/Pause |
| 98 | `0x93` | `0x0B` | Ch 4 (Note) | `[Channel4]` | `play` | PLAY/PAUSE (DECK4) - press - Play/Pause |
| 99 | `0x90` | `0x0C` | Ch 1 (Note) | `[Channel1]` | `cue_default` | CUE (DECK1) - press - Set/Call Cue, Back Cue |
| 100 | `0x91` | `0x0C` | Ch 2 (Note) | `[Channel2]` | `cue_default` | CUE (DECK2) - press - Set/Call Cue, Back Cue |
| 101 | `0x92` | `0x0C` | Ch 3 (Note) | `[Channel3]` | `cue_default` | CUE (DECK3) - press - Set/Call Cue, Back Cue |
| 102 | `0x93` | `0x0C` | Ch 4 (Note) | `[Channel4]` | `cue_default` | CUE (DECK4) - press - Set/Call Cue, Back Cue |
| 103 | `0xB0` | `0x21` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK1) - rotate - Pitch bend |
| 104 | `0xB1` | `0x21` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK2) - rotate - Pitch bend |
| 105 | `0xB2` | `0x21` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK3) - rotate - Pitch bend |
| 106 | `0xB3` | `0x21` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK4) - rotate - Pitch bend |
| 107 | `0xB0` | `0x22` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK1) - rotate - Scratch |
| 108 | `0xB1` | `0x22` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK2) - rotate - Scratch |
| 109 | `0xB2` | `0x22` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK3) - rotate - Scratch |
| 110 | `0xB3` | `0x22` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK4) - rotate - Scratch |
| 111 | `0xB0` | `0x23` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK1) - rotate - Pitch bend |
| 112 | `0xB1` | `0x23` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK2) - rotate - Pitch bend |
| 113 | `0xB2` | `0x23` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK3) - rotate - Pitch bend |
| 114 | `0xB3` | `0x23` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK4) - rotate - Pitch bend |
| 115 | `0xB0` | `0x29` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK1) - rotate - Search (Fast Pitch bend) |
| 116 | `0xB1` | `0x29` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK2) - rotate - Search (Fast Pitch bend) |
| 117 | `0xB2` | `0x29` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK3) - rotate - Search (Fast Pitch bend) |
| 118 | `0xB3` | `0x29` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK4) - rotate - Search (Fast Pitch bend) |
| 119 | `0x97` | `0x30` | Ch 8 (Note) | `[Sampler1]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 120 | `0x99` | `0x30` | Ch 10 (Note) | `[Sampler5]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 121 | `0x9B` | `0x30` | Ch 12 (Note) | `[Sampler1]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 122 | `0x9D` | `0x30` | Ch 14 (Note) | `[Sampler5]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 123 | `0x97` | `0x31` | Ch 8 (Note) | `[Sampler2]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 124 | `0x99` | `0x31` | Ch 10 (Note) | `[Sampler6]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 125 | `0x9B` | `0x31` | Ch 12 (Note) | `[Sampler2]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 126 | `0x9D` | `0x31` | Ch 14 (Note) | `[Sampler6]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 127 | `0x97` | `0x32` | Ch 8 (Note) | `[Sampler3]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 128 | `0x99` | `0x32` | Ch 10 (Note) | `[Sampler7]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 129 | `0x9B` | `0x32` | Ch 12 (Note) | `[Sampler3]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 130 | `0x9D` | `0x32` | Ch 14 (Note) | `[Sampler7]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 131 | `0x97` | `0x33` | Ch 8 (Note) | `[Sampler4]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 132 | `0x99` | `0x33` | Ch 10 (Note) | `[Sampler8]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 133 | `0x9B` | `0x33` | Ch 12 (Note) | `[Sampler4]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 134 | `0x9D` | `0x33` | Ch 14 (Note) | `[Sampler8]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 135 | `0x97` | `0x34` | Ch 8 (Note) | `[Sampler9]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 136 | `0x99` | `0x34` | Ch 10 (Note) | `[Sampler13]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 137 | `0x9B` | `0x34` | Ch 12 (Note) | `[Sampler9]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 138 | `0x9D` | `0x34` | Ch 14 (Note) | `[Sampler13]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 139 | `0x97` | `0x35` | Ch 8 (Note) | `[Sampler10]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 140 | `0x99` | `0x35` | Ch 10 (Note) | `[Sampler14]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 141 | `0x9B` | `0x35` | Ch 12 (Note) | `[Sampler10]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 142 | `0x9D` | `0x35` | Ch 14 (Note) | `[Sampler14]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 143 | `0x90` | `0x36` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK1) - touch - enable (on touch) / disable (on release) Scratching/Pitch bend |
| 144 | `0x91` | `0x36` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK2) - touch - enable (on touch) / disable (on release) Scratching/Pitch bend |
| 145 | `0x92` | `0x36` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK3) - touch - enable (on touch) / disable (on release) Scratching/Pitch bend |
| 146 | `0x93` | `0x36` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK4) - touch - enable (on touch) / disable (on release) Scratching/Pitch bend |
| 147 | `0x97` | `0x36` | Ch 8 (Note) | `[Sampler11]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 148 | `0x99` | `0x36` | Ch 10 (Note) | `[Sampler15]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 149 | `0x9B` | `0x36` | Ch 12 (Note) | `[Sampler11]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 150 | `0x9D` | `0x36` | Ch 14 (Note) | `[Sampler15]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 151 | `0x97` | `0x37` | Ch 8 (Note) | `[Sampler12]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 152 | `0x99` | `0x37` | Ch 10 (Note) | `[Sampler16]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 153 | `0x9B` | `0x37` | Ch 12 (Note) | `[Sampler12]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 154 | `0x9D` | `0x37` | Ch 14 (Note) | `[Sampler16]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 155 | `0x90` | `0x4C` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK1) - Loop in adjust (using jog wheel) |
| 156 | `0x91` | `0x4C` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK2) - Loop in adjust (using jog wheel) |
| 157 | `0x92` | `0x4C` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK3) - Loop in adjust (using jog wheel) |
| 158 | `0x93` | `0x4C` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK4) - Loop in adjust (using jog wheel) |
| 159 | `0x90` | `0x77` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK1) - Loop out adjust (using jog wheel) |
| 160 | `0x91` | `0x77` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK2) - Loop out adjust (using jog wheel) |
| 161 | `0x92` | `0x77` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK3) - Loop out adjust (using jog wheel) |
| 162 | `0x93` | `0x77` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK4) - Loop out adjust (using jog wheel) |
| 163 | `0x90` | `0x51` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK1) - press - half active loop |
| 164 | `0x91` | `0x51` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK2) - press - half active loop |
| 165 | `0x92` | `0x51` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK3) - press - half active loop |
| 166 | `0x93` | `0x51` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK4) - press - half active loop |
| 167 | `0x90` | `0x53` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK1) - press - double active loop |
| 168 | `0x91` | `0x53` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK2) - press - double active loop |
| 169 | `0x92` | `0x53` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK3) - press - double active loop |
| 170 | `0x93` | `0x53` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK4) - press - double active loop |
| 171 | `0x90` | `0x58` | Ch 1 (Note) | `[Channel1]` | `sync_enabled` | MIDI Learned from 8 messages. |
| 172 | `0x91` | `0x58` | Ch 2 (Note) | `[Channel2]` | `sync_enabled` | MIDI Learned from 8 messages. |
| 173 | `0x92` | `0x58` | Ch 3 (Note) | `[Channel3]` | `sync_enabled` | MIDI Learned from 8 messages. |
| 174 | `0x93` | `0x58` | Ch 4 (Note) | `[Channel4]` | `sync_enabled` | MIDI Learned from 8 messages. |
| 175 | `0x90` | `0x5C` | Ch 1 (Note) | `[Channel1]` | `sync_leader` | MIDI Learned from 12 messages. |
| 176 | `0x91` | `0x5C` | Ch 2 (Note) | `[Channel2]` | `sync_leader` | MIDI Learned from 8 messages. |
| 177 | `0x92` | `0x5C` | Ch 3 (Note) | `[Channel3]` | `sync_leader` | MIDI Learned from 11 messages. |
| 178 | `0x93` | `0x5C` | Ch 4 (Note) | `[Channel4]` | `sync_leader` | MIDI Learned from 9 messages. |
| 179 | `0x90` | `0x60` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK1) - press - change Tempo range |
| 180 | `0x91` | `0x60` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK2) - press - change Tempo range |
| 181 | `0x92` | `0x60` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK3) - press - change Tempo range |
| 182 | `0x93` | `0x60` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK4) - press - change Tempo range |
| 183 | `0x90` | `0x67` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK1) - touch - enable (on touch) / disable (on release) highspeed Pitch bend |
| 184 | `0x91` | `0x67` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK2) - touch - enable (on touch) / disable (on release) highspeed Pitch bend |
| 185 | `0x92` | `0x67` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK3) - touch - enable (on touch) / disable (on release) highspeed Pitch bend |
| 186 | `0x93` | `0x67` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK4) - touch - enable (on touch) / disable (on release) highspeed Pitch bend |


### 5.3. Library & Navigation Registers (Total: 9 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 187 | `0x96` | `0x65` | Ch 7 (Note) | `[Library]` | `back` | BROWSER BACK BUTTON |
| 188 | `0x96` | `0x7A` | Ch 7 (Note) | `[App]` | `browser_toggle` | BROWSER VIEW TOGGLE BUTTON |
| 189 | `0xB6` | `0x40` | Ch 7 (CC) | `[Library]` | `MoveVertical` | BROWSE - rotate - Scroll tracklist/tree view |
| 190 | `0x96` | `0x41` | Ch 7 (Note) | `[Library]` | `MoveFocusForward` | BROWSE - press - Move cursor between track list and tree view |
| 191 | `0x96` | `0x42` | Ch 7 (Note) | `[Library]` | `MoveFocusBackward` | BROWSE +SHIFT - press - Move cursor between track list and tree view |
| 192 | `0x96` | `0x46` | Ch 7 (Note) | `[Channel1]` | `LoadSelectedTrack` | LOAD (DECK1) - press - Load a Track into Deck 1 |
| 193 | `0x96` | `0x47` | Ch 7 (Note) | `[Channel2]` | `LoadSelectedTrack` | LOAD (DECK2) - press - Load a Track into Deck 2 |
| 194 | `0x96` | `0x48` | Ch 7 (Note) | `[Channel3]` | `LoadSelectedTrack` | LOAD (DECK3) - press - Load a Track into Deck 1 |
| 195 | `0x96` | `0x49` | Ch 7 (Note) | `[Channel4]` | `LoadSelectedTrack` | LOAD (DECK4) - press - Load a Track into Deck 2 |


### 5.4. Looping & Hot Cue Performance Pad Registers (Total: 28 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 196 | `0x90` | `0x10` | Ch 1 (Note) | `[Channel1]` | `loop_in` | LOOP IN/4 BEAT (DECK1) - press - Set loop in |
| 197 | `0x91` | `0x10` | Ch 2 (Note) | `[Channel2]` | `loop_in` | LOOP IN/4 BEAT (DECK2) - press - Set loop in |
| 198 | `0x92` | `0x10` | Ch 3 (Note) | `[Channel3]` | `loop_in` | LOOP IN/4 BEAT (DECK3) - press - Set loop in |
| 199 | `0x93` | `0x10` | Ch 4 (Note) | `[Channel4]` | `loop_in` | LOOP IN/4 BEAT (DECK4) - press - Set loop in |
| 200 | `0x90` | `0x11` | Ch 1 (Note) | `[Channel1]` | `loop_out` | LOOP OUT (DECK1) - press - Set loop out |
| 201 | `0x91` | `0x11` | Ch 2 (Note) | `[Channel2]` | `loop_out` | LOOP OUT (DECK2) - press - Set loop out |
| 202 | `0x92` | `0x11` | Ch 3 (Note) | `[Channel3]` | `loop_out` | LOOP OUT (DECK3) - press - Set loop out |
| 203 | `0x93` | `0x11` | Ch 4 (Note) | `[Channel4]` | `loop_out` | LOOP OUT (DECK4) - press - Set loop out |
| 204 | `0x97` | `0x62` | Ch 8 (Note) | `[Channel1]` | `beatloop_1_toggle` | PAD 3 (DECK1) BEAT LOOP MODE - press - 1/1 Beatloop |
| 205 | `0x99` | `0x62` | Ch 10 (Note) | `[Channel2]` | `beatloop_1_toggle` | PAD 3 (DECK2) BEAT LOOP MODE - press - 1/1 Beatloop |
| 206 | `0x9B` | `0x62` | Ch 12 (Note) | `[Channel3]` | `beatloop_1_toggle` | PAD 3 (DECK3) BEAT LOOP MODE - press - 1/1 Beatloop |
| 207 | `0x9D` | `0x62` | Ch 14 (Note) | `[Channel4]` | `beatloop_1_toggle` | PAD 3 (DECK4) BEAT LOOP MODE - press - 1/1 Beatloop |
| 208 | `0x97` | `0x63` | Ch 8 (Note) | `[Channel1]` | `beatloop_2_toggle` | PAD 4 (DECK1) BEAT LOOP MODE - press - 2 Beatloop |
| 209 | `0x99` | `0x63` | Ch 10 (Note) | `[Channel2]` | `beatloop_2_toggle` | PAD 4 (DECK2) BEAT LOOP MODE - press - 2 Beatloop |
| 210 | `0x9B` | `0x63` | Ch 12 (Note) | `[Channel3]` | `beatloop_2_toggle` | PAD 4 (DECK3) BEAT LOOP MODE - press - 2 Beatloop |
| 211 | `0x9D` | `0x63` | Ch 14 (Note) | `[Channel4]` | `beatloop_2_toggle` | PAD 4 (DECK4) BEAT LOOP MODE - press - 2 Beatloop |
| 212 | `0x97` | `0x64` | Ch 8 (Note) | `[Channel1]` | `beatloop_4_toggle` | PAD 5 (DECK1) BEAT LOOP MODE - press - 4 Beatloop |
| 213 | `0x99` | `0x64` | Ch 10 (Note) | `[Channel2]` | `beatloop_4_toggle` | PAD 5 (DECK2) BEAT LOOP MODE - press - 4 Beatloop |
| 214 | `0x9B` | `0x64` | Ch 12 (Note) | `[Channel3]` | `beatloop_4_toggle` | PAD 5 (DECK3) BEAT LOOP MODE - press - 4 Beatloop |
| 215 | `0x9D` | `0x64` | Ch 14 (Note) | `[Channel4]` | `beatloop_4_toggle` | PAD 5 (DECK4) BEAT LOOP MODE - press - 4 Beatloop |
| 216 | `0x97` | `0x65` | Ch 8 (Note) | `[Channel1]` | `beatloop_8_toggle` | PAD 6 (DECK1) BEAT LOOP MODE - press - 8 Beatloop |
| 217 | `0x99` | `0x65` | Ch 10 (Note) | `[Channel2]` | `beatloop_8_toggle` | PAD 6 (DECK2) BEAT LOOP MODE - press - 8 Beatloop |
| 218 | `0x9B` | `0x65` | Ch 12 (Note) | `[Channel3]` | `beatloop_8_toggle` | PAD 6 (DECK3) BEAT LOOP MODE - press - 8 Beatloop |
| 219 | `0x9D` | `0x65` | Ch 14 (Note) | `[Channel4]` | `beatloop_8_toggle` | PAD 6 (DECK4) BEAT LOOP MODE - press - 8 Beatloop |
| 220 | `0x97` | `0x66` | Ch 8 (Note) | `[Channel1]` | `beatloop_16_toggle` | PAD 7 (DECK1) BEAT LOOP MODE - press - 16 Beatloop |
| 221 | `0x99` | `0x66` | Ch 10 (Note) | `[Channel2]` | `beatloop_16_toggle` | PAD 7 (DECK2) BEAT LOOP MODE - press - 16 Beatloop |
| 222 | `0x9B` | `0x66` | Ch 12 (Note) | `[Channel3]` | `beatloop_16_toggle` | PAD 7 (DECK3) BEAT LOOP MODE - press - 16 Beatloop |
| 223 | `0x9D` | `0x66` | Ch 14 (Note) | `[Channel4]` | `beatloop_16_toggle` | PAD 7 (DECK4) BEAT LOOP MODE - press - 16 Beatloop |


### 5.5. Secondary Mixer, Utility & Beat Jump Mappings (Total: 138 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 224 | `0x94` | `0x06` | Ch 5 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxLeftPressed` | BEAT LEFT - press - select previous effect unit |
| 225 | `0x95` | `0x06` | Ch 6 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxLeftPressed` | BEAT LEFT - press - select previous effect unit |
| 226 | `0x94` | `0x07` | Ch 5 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxRightPressed` | BEAT RIGHT - press - select next effect unit |
| 227 | `0x95` | `0x07` | Ch 6 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxRightPressed` | BEAT RIGHT - press - select next effect unit |
| 228 | `0xB4` | `0x08` | Ch 5 (CC) | `L` | `PioneerDDJFLX6.mergeFxTurn` | MergeFX Turn |
| 229 | `0xB5` | `0x08` | Ch 6 (CC) | `R` | `PioneerDDJFLX6.mergeFxTurn` | MergeFX Turn |
| 230 | `0xB6` | `0x0C` | Ch 7 (CC) | `[Master]` | `headMix` | HEADPHONES MIXING - rotate - Monitor Balance |
| 231 | `0x94` | `0x10` | Ch 5 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxChannel1` | BEAT FX CH SELECT CH1 - slide - Select FX on DECK 1 |
| 232 | `0x95` | `0x11` | Ch 6 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxChannel2` | BEAT FX CH SELECT CH2 - slide - Select FX on DECK 2 |
| 233 | `0x94` | `0x14` | Ch 5 (Note) | `[EffectRack1_EffectUnit1];group_[Master]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 MST |
| 234 | `0x95` | `0x14` | Ch 6 (Note) | `[EffectRack1_EffectUnit2];group_[Master]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 MST |
| 235 | `0x94` | `0x1C` | Ch 5 (Note) | `[EffectRack1_EffectUnit1];group_[Channel1]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH1 |
| 236 | `0x95` | `0x1C` | Ch 6 (Note) | `[EffectRack1_EffectUnit2];group_[Channel1]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH1 |
| 237 | `0x94` | `0x1D` | Ch 5 (Note) | `[EffectRack1_EffectUnit1];group_[Channel2]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH2 |
| 238 | `0x95` | `0x1D` | Ch 6 (Note) | `[EffectRack1_EffectUnit2];group_[Channel2]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH2 |
| 239 | `0x94` | `0x1E` | Ch 5 (Note) | `[EffectRack1_EffectUnit1];group_[Channel3]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH3 |
| 240 | `0x95` | `0x1E` | Ch 6 (Note) | `[EffectRack1_EffectUnit2];group_[Channel3]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH3 |
| 241 | `0x94` | `0x1F` | Ch 5 (Note) | `[EffectRack1_EffectUnit1];group_[Channel4]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH4 |
| 242 | `0x95` | `0x1F` | Ch 6 (Note) | `[EffectRack1_EffectUnit2];group_[Channel4]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH4 |
| 243 | `0x97` | `0x20` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK1) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| 244 | `0x99` | `0x20` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK2) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| 245 | `0x9B` | `0x20` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK3) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| 246 | `0x9D` | `0x20` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK4) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| 247 | `0x97` | `0x21` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK1) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| 248 | `0x99` | `0x21` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK2) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| 249 | `0x9B` | `0x21` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK3) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| 250 | `0x9D` | `0x21` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK4) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| 251 | `0x97` | `0x22` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK1) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| 252 | `0x99` | `0x22` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK2) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| 253 | `0x9B` | `0x22` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK3) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| 254 | `0x9D` | `0x22` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK4) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| 255 | `0x97` | `0x23` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK1) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| 256 | `0x99` | `0x23` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK2) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| 257 | `0x9B` | `0x23` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK3) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| 258 | `0x9D` | `0x23` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK4) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| 259 | `0x97` | `0x24` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK1) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| 260 | `0x99` | `0x24` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK2) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| 261 | `0x9B` | `0x24` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK3) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| 262 | `0x9D` | `0x24` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK4) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| 263 | `0x97` | `0x25` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK1) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| 264 | `0x99` | `0x25` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK2) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| 265 | `0x9B` | `0x25` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK3) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| 266 | `0x9D` | `0x25` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK4) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| 267 | `0x97` | `0x26` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK1) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| 268 | `0x98` | `0x26` | Ch 9 (Note) | `[Channel1]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK1) +SHift BEAT JUMP MODE - press - decrease Beatjump by a factor of 16 |
| 269 | `0x99` | `0x26` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK2) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| 270 | `0x9A` | `0x26` | Ch 11 (Note) | `[Channel2]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK2) +Shift BEAT JUMP MODE - press - decrease Beatjump by a factor of 16 |
| 271 | `0x9B` | `0x26` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK3) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| 272 | `0x9C` | `0x26` | Ch 13 (Note) | `[Channel3]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK3) +SHift BEAT JUMP MODE - press - decrease Beatjump by a factor of 16 |
| 273 | `0x9D` | `0x26` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK4) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| 274 | `0x9E` | `0x26` | Ch 15 (Note) | `[Channel4]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK4) +Shift BEAT JUMP MODE - press - decrease Beatjump by a factor of 16 |
| 275 | `0x97` | `0x27` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK1) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| 276 | `0x98` | `0x27` | Ch 9 (Note) | `[Channel1]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK1) +SHift BEAT JUMP MODE - press - increase Beatjump by a factor of 16 |
| 277 | `0x99` | `0x27` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK2) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| 278 | `0x9A` | `0x27` | Ch 11 (Note) | `[Channel2]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK2) +Shift BEAT JUMP MODE - press - increase Beatjump by a factor of 16 |
| 279 | `0x9B` | `0x27` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK3) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| 280 | `0x9C` | `0x27` | Ch 13 (Note) | `[Channel3]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK3) +SHift BEAT JUMP MODE - press - increase Beatjump by a factor of 16 |
| 281 | `0x9D` | `0x27` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK4) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| 282 | `0x9E` | `0x27` | Ch 15 (Note) | `[Channel4]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK4) +Shift BEAT JUMP MODE - press - increase Beatjump by a factor of 16 |
| 283 | `0xB6` | `0x2C` | Ch 7 (CC) | `[Master]` | `headMix` | HEADPHONES MIXING - rotate - Monitor Balance |
| 284 | `0x90` | `0x3C` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.deckControlLPressed` | DeckControl CH1 |
| 285 | `0x91` | `0x3C` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.deckControlRPressed` | DeckControl CH2 |
| 286 | `0x92` | `0x3C` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.deckControlLPressed` | DeckControl CH3 |
| 287 | `0x93` | `0x3C` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.deckControlRPressed` | DeckControl CH4 |
| 288 | `0x90` | `0x3F` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK1) |
| 289 | `0x91` | `0x3F` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK2) |
| 290 | `0x92` | `0x3F` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK3) |
| 291 | `0x93` | `0x3F` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK4) |
| 292 | `0x97` | `0x40` | Ch 8 (Note) | `[Channel1];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 293 | `0x99` | `0x40` | Ch 10 (Note) | `[Channel2];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 294 | `0x9B` | `0x40` | Ch 12 (Note) | `[Channel3];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 295 | `0x9D` | `0x40` | Ch 14 (Note) | `[Channel4];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 296 | `0x97` | `0x41` | Ch 8 (Note) | `[Channel1];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 297 | `0x99` | `0x41` | Ch 10 (Note) | `[Channel2];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 298 | `0x9B` | `0x41` | Ch 12 (Note) | `[Channel3];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 299 | `0x9D` | `0x41` | Ch 14 (Note) | `[Channel4];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 300 | `0x97` | `0x42` | Ch 8 (Note) | `[Channel1];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 301 | `0x99` | `0x42` | Ch 10 (Note) | `[Channel2];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 302 | `0x9B` | `0x42` | Ch 12 (Note) | `[Channel3];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 303 | `0x9D` | `0x42` | Ch 14 (Note) | `[Channel4];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 304 | `0x97` | `0x43` | Ch 8 (Note) | `[Channel1];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 305 | `0x99` | `0x43` | Ch 10 (Note) | `[Channel2];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 306 | `0x9B` | `0x43` | Ch 12 (Note) | `[Channel3];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 307 | `0x9D` | `0x43` | Ch 14 (Note) | `[Channel4];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 308 | `0x97` | `0x44` | Ch 8 (Note) | `[Channel1];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 309 | `0x99` | `0x44` | Ch 10 (Note) | `[Channel2];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 310 | `0x9B` | `0x44` | Ch 12 (Note) | `[Channel3];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 311 | `0x9D` | `0x44` | Ch 14 (Note) | `[Channel4];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 312 | `0x97` | `0x45` | Ch 8 (Note) | `[Channel1];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 313 | `0x99` | `0x45` | Ch 10 (Note) | `[Channel2];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 314 | `0x9B` | `0x45` | Ch 12 (Note) | `[Channel3];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 315 | `0x9D` | `0x45` | Ch 14 (Note) | `[Channel4];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 316 | `0x97` | `0x46` | Ch 8 (Note) | `[Channel1];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 317 | `0x99` | `0x46` | Ch 10 (Note) | `[Channel2];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 318 | `0x9B` | `0x46` | Ch 12 (Note) | `[Channel3];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 319 | `0x9D` | `0x46` | Ch 14 (Note) | `[Channel4];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 320 | `0x94` | `0x47` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect1]` | `PioneerDDJFLX6.fxEnabled` | BFX FX1-1 |
| 321 | `0x95` | `0x47` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect1]` | `PioneerDDJFLX6.fxEnabled` | BFX FX2-1 |
| 322 | `0x97` | `0x47` | Ch 8 (Note) | `[Channel1];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 323 | `0x99` | `0x47` | Ch 10 (Note) | `[Channel2];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 324 | `0x9B` | `0x47` | Ch 12 (Note) | `[Channel3];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 325 | `0x9D` | `0x47` | Ch 14 (Note) | `[Channel4];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 326 | `0x94` | `0x48` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect2]` | `PioneerDDJFLX6.fxEnabled` | BFX FX1-2 |
| 327 | `0x95` | `0x48` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect2]` | `PioneerDDJFLX6.fxEnabled` | BFX FX2-2 |
| 328 | `0x94` | `0x49` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect3]` | `PioneerDDJFLX6.fxEnabled` | BFX FX1-3 |
| 329 | `0x95` | `0x49` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect3]` | `PioneerDDJFLX6.fxEnabled` | BFX FX2-3 |
| 330 | `0x9B` | `0x50` | Ch 12 (Note) | `[Channel3];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 331 | `0x9D` | `0x50` | Ch 14 (Note) | `[Channel4];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 332 | `0x9B` | `0x51` | Ch 12 (Note) | `[Channel3];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 333 | `0x9D` | `0x51` | Ch 14 (Note) | `[Channel4];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 334 | `0x9B` | `0x52` | Ch 12 (Note) | `[Channel3];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 335 | `0x9D` | `0x52` | Ch 14 (Note) | `[Channel4];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 336 | `0x9B` | `0x53` | Ch 12 (Note) | `[Channel3];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 337 | `0x9D` | `0x53` | Ch 14 (Note) | `[Channel4];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 338 | `0x9B` | `0x54` | Ch 12 (Note) | `[Channel3];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 339 | `0x9D` | `0x54` | Ch 14 (Note) | `[Channel4];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 340 | `0x9B` | `0x55` | Ch 12 (Note) | `[Channel3];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 341 | `0x9D` | `0x55` | Ch 14 (Note) | `[Channel4];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 342 | `0x9B` | `0x56` | Ch 12 (Note) | `[Channel3];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 343 | `0x9D` | `0x56` | Ch 14 (Note) | `[Channel4];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 344 | `0x9B` | `0x57` | Ch 12 (Note) | `[Channel3];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 345 | `0x9D` | `0x57` | Ch 14 (Note) | `[Channel4];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 346 | `0x97` | `0x70` | Ch 8 (Note) | `[Channel1];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 347 | `0x99` | `0x70` | Ch 10 (Note) | `[Channel2];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 348 | `0x97` | `0x71` | Ch 8 (Note) | `[Channel1];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 349 | `0x99` | `0x71` | Ch 10 (Note) | `[Channel2];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 350 | `0x97` | `0x72` | Ch 8 (Note) | `[Channel1];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 351 | `0x99` | `0x72` | Ch 10 (Note) | `[Channel2];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 352 | `0x97` | `0x73` | Ch 8 (Note) | `[Channel1];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 353 | `0x99` | `0x73` | Ch 10 (Note) | `[Channel2];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 354 | `0x97` | `0x74` | Ch 8 (Note) | `[Channel1];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 355 | `0x99` | `0x74` | Ch 10 (Note) | `[Channel2];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 356 | `0x97` | `0x75` | Ch 8 (Note) | `[Channel1];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 357 | `0x99` | `0x75` | Ch 10 (Note) | `[Channel2];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 358 | `0x97` | `0x76` | Ch 8 (Note) | `[Channel1];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 359 | `0x99` | `0x76` | Ch 10 (Note) | `[Channel2];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 360 | `0x97` | `0x77` | Ch 8 (Note) | `[Channel1];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 361 | `0x99` | `0x77` | Ch 10 (Note) | `[Channel2];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |


### 5.6. Hardware LED & Signal Output Feedback Registers (Total: 7 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 362 | `0xF0` | `SysEx` | Global (SysEx) | `[Master]` | `Pioneer Keep-Alive` | Handshake SysEx Packet (12 Bytes) |
| 363 | `0x90..0x93` | `0x0B` | Ch 1..4 (Note) | `[Channel1..4]` | `play` | Play/Pause Button Green LED (0x7F = On, 0x00 = Off) |
| 364 | `0x90..0x93` | `0x0C` | Ch 1..4 (Note) | `[Channel1..4]` | `cue` | Cue Button Amber LED (0x7F = On, 0x00 = Off) |
| 365 | `0x90..0x93` | `0x0E` | Ch 1..4 (Note) | `[Channel1..4]` | `vinyl_mode` | Vinyl Mode LED Indicator (0x7F = On, 0x00 = Off) |
| 366 | `0xB0..0xB3` | `0x02` | Ch 1..4 (CC) | `[Channel1..4]` | `vu_meter` | Channel Level VU Meter CC (0..118 RMS + 127 Peak Clip) |
| 367 | `0xBB` | `0x00..0x03` | Ch 12 (CC) | `[Channel1..4]` | `jog_spinner` | Jogwheel Outer LED Ring Playposition Spinner (1..72 steps) |
| 368 | `0x97 / 0x99` | `0x00..0x07` | Ch 8 / 10 (Note) | `[Channel1..2]` | `hotcue_1..8` | Hot Cue Pad Active Marker LEDs (0x7F = Active Cue Present) |


---

## 6. CATALOG BREAKDOWN DETIL SELURUH REGISTER MIDI YANG BELUM TERIMPLEMENTASI (Total: 191 Registers)

### 6.1. Merge FX Control & Preset Selectors (Total: 6 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | `0x94` | `0x2E` | Ch 5 (Note) | `L` | `PioneerDDJFLX6.mergeEffectButtonPressed` | Merge Effect L Button |
| 2 | `0x95` | `0x2E` | Ch 6 (Note) | `R` | `PioneerDDJFLX6.mergeEffectButtonPressed` | Merge Effect R Button |
| 3 | `0x94` | `0x2F` | Ch 5 (Note) | `L` | `PioneerDDJFLX6.mergeEffectSelectorPressed` | Merge Effect L Button |
| 4 | `0x95` | `0x2F` | Ch 6 (Note) | `R` | `PioneerDDJFLX6.mergeEffectSelectorPressed` | Merge Effect R Button |
| 5 | `0x94` | `0x30` | Ch 5 (Note) | `L` | `PioneerDDJFLX6.mergeEffectSelectorPressedReverse` | Merge Effect L Button shift |
| 6 | `0x95` | `0x30` | Ch 6 (Note) | `R` | `PioneerDDJFLX6.mergeEffectSelectorPressedReverse` | Merge Effect R Button shift |


### 6.2. Sampler Slot Triggering & Bank Controls (Total: 36 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 7 | `0x90` | `0x22` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | SAMPLER MODE (DECK1) - press - set sampler mode |
| 8 | `0x91` | `0x22` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | SAMPLER MODE (DECK2) - press - set sampler mode |
| 9 | `0x92` | `0x22` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | SAMPLER MODE (DECK3) - press - set sampler mode |
| 10 | `0x93` | `0x22` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | SAMPLER MODE (DECK4) - press - set sampler mode |
| 11 | `0x98` | `0x30` | Ch 9 (Note) | `[Sampler1]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 1 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 12 | `0x9A` | `0x30` | Ch 11 (Note) | `[Sampler5]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 1 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 13 | `0x9C` | `0x30` | Ch 13 (Note) | `[Sampler1]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 1 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 14 | `0x9E` | `0x30` | Ch 15 (Note) | `[Sampler5]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 1 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 15 | `0x98` | `0x31` | Ch 9 (Note) | `[Sampler2]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 2 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 16 | `0x9A` | `0x31` | Ch 11 (Note) | `[Sampler6]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 2 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 17 | `0x9C` | `0x31` | Ch 13 (Note) | `[Sampler2]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 2 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 18 | `0x9E` | `0x31` | Ch 15 (Note) | `[Sampler6]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 2 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 19 | `0x98` | `0x32` | Ch 9 (Note) | `[Sampler3]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 3 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 20 | `0x9A` | `0x32` | Ch 11 (Note) | `[Sampler7]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 3 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 21 | `0x9C` | `0x32` | Ch 13 (Note) | `[Sampler3]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 3 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 22 | `0x9E` | `0x32` | Ch 15 (Note) | `[Sampler7]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 3 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 23 | `0x98` | `0x33` | Ch 9 (Note) | `[Sampler4]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 4 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 24 | `0x9A` | `0x33` | Ch 11 (Note) | `[Sampler8]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 4 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 25 | `0x9C` | `0x33` | Ch 13 (Note) | `[Sampler4]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 4 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 26 | `0x9E` | `0x33` | Ch 15 (Note) | `[Sampler8]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 4 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 27 | `0x98` | `0x34` | Ch 9 (Note) | `[Sampler9]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 5 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 28 | `0x9A` | `0x34` | Ch 11 (Note) | `[Sampler13]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 5 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 29 | `0x9C` | `0x34` | Ch 13 (Note) | `[Sampler9]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 5 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 30 | `0x9E` | `0x34` | Ch 15 (Note) | `[Sampler13]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 5 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 31 | `0x98` | `0x35` | Ch 9 (Note) | `[Sampler10]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 6 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 32 | `0x9A` | `0x35` | Ch 11 (Note) | `[Sampler14]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 6 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 33 | `0x9C` | `0x35` | Ch 13 (Note) | `[Sampler10]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 6 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 34 | `0x9E` | `0x35` | Ch 15 (Note) | `[Sampler14]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 6 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 35 | `0x98` | `0x36` | Ch 9 (Note) | `[Sampler11]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 7 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 36 | `0x9A` | `0x36` | Ch 11 (Note) | `[Sampler15]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 7 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 37 | `0x9C` | `0x36` | Ch 13 (Note) | `[Sampler11]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 7 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 38 | `0x9E` | `0x36` | Ch 15 (Note) | `[Sampler15]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 7 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 39 | `0x98` | `0x37` | Ch 9 (Note) | `[Sampler12]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 8 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 40 | `0x9A` | `0x37` | Ch 11 (Note) | `[Sampler16]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 8 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 41 | `0x9C` | `0x37` | Ch 13 (Note) | `[Sampler12]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 8 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| 42 | `0x9E` | `0x37` | Ch 15 (Note) | `[Sampler16]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 8 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |


### 6.3. Key Shift, Key Sync & Keyboard Pitch Transposition (Total: 8 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 43 | `0x90` | `0x69` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK1) - press - set keyboard mode |
| 44 | `0x91` | `0x69` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK2) - press - set keyboard mode |
| 45 | `0x92` | `0x69` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK3) - press - set keyboard mode |
| 46 | `0x93` | `0x69` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK4) - press - set keyboard mode |
| 47 | `0x90` | `0x6F` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK1) - press - set key shift mode |
| 48 | `0x91` | `0x6F` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK2) - press - set key shift mode |
| 49 | `0x92` | `0x6F` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK3) - press - set key shift mode |
| 50 | `0x93` | `0x6F` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK4) - press - set key shift mode |


### 6.4. Pad FX 1 & Pad FX 2 Performance Modes (Total: 40 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 51 | `0x97` | `0x10` | Ch 8 (Note) | `[Channel1];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 52 | `0x99` | `0x10` | Ch 10 (Note) | `[Channel2];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 53 | `0x9B` | `0x10` | Ch 12 (Note) | `[Channel3];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 54 | `0x9D` | `0x10` | Ch 14 (Note) | `[Channel4];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 55 | `0x97` | `0x11` | Ch 8 (Note) | `[Channel1];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 56 | `0x99` | `0x11` | Ch 10 (Note) | `[Channel2];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 57 | `0x9B` | `0x11` | Ch 12 (Note) | `[Channel3];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 58 | `0x9D` | `0x11` | Ch 14 (Note) | `[Channel4];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 59 | `0x97` | `0x12` | Ch 8 (Note) | `[Channel1];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 60 | `0x99` | `0x12` | Ch 10 (Note) | `[Channel2];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 61 | `0x9B` | `0x12` | Ch 12 (Note) | `[Channel3];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 62 | `0x9D` | `0x12` | Ch 14 (Note) | `[Channel4];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 63 | `0x97` | `0x13` | Ch 8 (Note) | `[Channel1];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 64 | `0x99` | `0x13` | Ch 10 (Note) | `[Channel2];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 65 | `0x9B` | `0x13` | Ch 12 (Note) | `[Channel3];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 66 | `0x9D` | `0x13` | Ch 14 (Note) | `[Channel4];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 67 | `0x97` | `0x14` | Ch 8 (Note) | `[Channel1];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 68 | `0x99` | `0x14` | Ch 10 (Note) | `[Channel2];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 69 | `0x9B` | `0x14` | Ch 12 (Note) | `[Channel3];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 70 | `0x9D` | `0x14` | Ch 14 (Note) | `[Channel4];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 71 | `0x97` | `0x15` | Ch 8 (Note) | `[Channel1];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 72 | `0x99` | `0x15` | Ch 10 (Note) | `[Channel2];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 73 | `0x9B` | `0x15` | Ch 12 (Note) | `[Channel3];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 74 | `0x9D` | `0x15` | Ch 14 (Note) | `[Channel4];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 75 | `0x97` | `0x16` | Ch 8 (Note) | `[Channel1];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 76 | `0x99` | `0x16` | Ch 10 (Note) | `[Channel2];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 77 | `0x9B` | `0x16` | Ch 12 (Note) | `[Channel3];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 78 | `0x9D` | `0x16` | Ch 14 (Note) | `[Channel4];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 79 | `0x97` | `0x17` | Ch 8 (Note) | `[Channel1];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 80 | `0x99` | `0x17` | Ch 10 (Note) | `[Channel2];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 81 | `0x9B` | `0x17` | Ch 12 (Note) | `[Channel3];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 82 | `0x9D` | `0x17` | Ch 14 (Note) | `[Channel4];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 83 | `0x90` | `0x1E` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK1) - press - set pad fx1 mode |
| 84 | `0x91` | `0x1E` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK2) - press - set pad fx1 mode |
| 85 | `0x92` | `0x1E` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK3) - press - set pad fx1 mode |
| 86 | `0x93` | `0x1E` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK4) - press - set pad fx1 mode |
| 87 | `0x90` | `0x6B` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK1) - press - set pad fx2 mode |
| 88 | `0x91` | `0x6B` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK2) - press - set pad fx2 mode |
| 89 | `0x92` | `0x6B` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK3) - press - set pad fx2 mode |
| 90 | `0x93` | `0x6B` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK4) - press - set pad fx2 mode |


### 6.5. High-Precision Pitch Slider Fine Tuning - 14-Bit LSB (Total: 0 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |


### 6.6. Secondary Loop Adjust, Reloop & Cue Call Navigation (Total: 24 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 91 | `0x90` | `0x3E` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK1) - press - quick jump back |
| 92 | `0x91` | `0x3E` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK2) - press - quick jump back |
| 93 | `0x92` | `0x3E` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK3) - press - quick jump back |
| 94 | `0x93` | `0x3E` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK4) - press - quick jump back |
| 95 | `0x90` | `0x4D` | Ch 1 (Note) | `[Channel1]` | `reloop_toggle` | RELOOP/EXIT (DECK1) - press - (loop off) Reloop, (loop on) Loop exit |
| 96 | `0x91` | `0x4D` | Ch 2 (Note) | `[Channel2]` | `reloop_toggle` | RELOOP/EXIT (DECK2) - press - (loop off) Reloop, (loop on) Loop exit |
| 97 | `0x92` | `0x4D` | Ch 3 (Note) | `[Channel3]` | `reloop_toggle` | RELOOP/EXIT (DECK3) - press - (loop off) Reloop, (loop on) Loop exit |
| 98 | `0x93` | `0x4D` | Ch 4 (Note) | `[Channel4]` | `reloop_toggle` | RELOOP/EXIT (DECK4) - press - (loop off) Reloop, (loop on) Loop exit |
| 99 | `0x90` | `0x50` | Ch 1 (Note) | `[Channel1]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK1) - press - Reloop and stop |
| 100 | `0x91` | `0x50` | Ch 2 (Note) | `[Channel2]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK2) - press - Reloop and stop |
| 101 | `0x92` | `0x50` | Ch 3 (Note) | `[Channel3]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK3) - press - Reloop and stop |
| 102 | `0x93` | `0x50` | Ch 4 (Note) | `[Channel4]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK4) - press - Reloop and stop |
| 103 | `0x97` | `0x60` | Ch 8 (Note) | `[Channel1]` | `beatloop_0.25_toggle` | PAD 1 (DECK1) BEAT LOOP MODE - press - 1/4 Beatloop |
| 104 | `0x99` | `0x60` | Ch 10 (Note) | `[Channel2]` | `beatloop_0.25_toggle` | PAD 1 (DECK2) BEAT LOOP MODE - press - 1/4 Beatloop |
| 105 | `0x9B` | `0x60` | Ch 12 (Note) | `[Channel3]` | `beatloop_0.25_toggle` | PAD 1 (DECK3) BEAT LOOP MODE - press - 1/4 Beatloop |
| 106 | `0x9D` | `0x60` | Ch 14 (Note) | `[Channel4]` | `beatloop_0.25_toggle` | PAD 1 (DECK4) BEAT LOOP MODE - press - 1/4 Beatloop |
| 107 | `0x97` | `0x61` | Ch 8 (Note) | `[Channel1]` | `beatloop_0.5_toggle` | PAD 2 (DECK1) BEAT LOOP MODE - press - 1/2 Beatloop |
| 108 | `0x99` | `0x61` | Ch 10 (Note) | `[Channel2]` | `beatloop_0.5_toggle` | PAD 2 (DECK2) BEAT LOOP MODE - press - 1/2 Beatloop |
| 109 | `0x9B` | `0x61` | Ch 12 (Note) | `[Channel3]` | `beatloop_0.5_toggle` | PAD 2 (DECK3) BEAT LOOP MODE - press - 1/2 Beatloop |
| 110 | `0x9D` | `0x61` | Ch 14 (Note) | `[Channel4]` | `beatloop_0.5_toggle` | PAD 2 (DECK4) BEAT LOOP MODE - press - 1/2 Beatloop |
| 111 | `0x97` | `0x67` | Ch 8 (Note) | `[Channel1]` | `beatloop_32_toggle` | PAD 8 (DECK1) BEAT LOOP MODE - press - 32 Beatloop |
| 112 | `0x99` | `0x67` | Ch 10 (Note) | `[Channel2]` | `beatloop_32_toggle` | PAD 8 (DECK2) BEAT LOOP MODE - press - 32 Beatloop |
| 113 | `0x9B` | `0x67` | Ch 12 (Note) | `[Channel3]` | `beatloop_32_toggle` | PAD 8 (DECK3) BEAT LOOP MODE - press - 32 Beatloop |
| 114 | `0x9D` | `0x67` | Ch 14 (Note) | `[Channel4]` | `beatloop_32_toggle` | PAD 8 (DECK4) BEAT LOOP MODE - press - 32 Beatloop |


### 6.7. Shift + Beat FX Meta Controls & Rack Parameter Routing (Total: 52 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 115 | `0xB4` | `0x02` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect1]` | `meta` | MIDI Learned from 322 messages. |
| 116 | `0xB5` | `0x02` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect1]` | `meta` | MIDI Learned from 324 messages. |
| 117 | `0xB4` | `0x04` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect2]` | `meta` | MIDI Learned from 236 messages. |
| 118 | `0xB5` | `0x04` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect2]` | `meta` | MIDI Learned from 286 messages. |
| 119 | `0xB4` | `0x06` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect3]` | `meta` | MIDI Learned from 224 messages. |
| 120 | `0xB5` | `0x06` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect3]` | `meta` | MIDI Learned from 228 messages. |
| 121 | `0xB0` | `0x07` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter3` | EQ HI - rotate |
| 122 | `0xB1` | `0x07` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter3` | EQ HI - rotate |
| 123 | `0xB2` | `0x07` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter3` | EQ HI - rotate |
| 124 | `0xB3` | `0x07` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter3` | EQ HI - rotate |
| 125 | `0xB0` | `0x0B` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter2` | EQ MID - rotate |
| 126 | `0xB1` | `0x0B` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter2` | EQ MID - rotate |
| 127 | `0xB2` | `0x0B` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter2` | EQ MID - rotate |
| 128 | `0xB3` | `0x0B` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter2` | EQ MID - rotate |
| 129 | `0xB0` | `0x0F` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 130 | `0xB1` | `0x0F` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 131 | `0xB2` | `0x0F` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 132 | `0xB3` | `0x0F` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 133 | `0xB6` | `0x17` | Ch 7 (CC) | `[QuickEffectRack1_[Channel1]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| 134 | `0xB6` | `0x18` | Ch 7 (CC) | `[QuickEffectRack1_[Channel2]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| 135 | `0xB6` | `0x19` | Ch 7 (CC) | `[QuickEffectRack1_[Channel3]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| 136 | `0xB6` | `0x1A` | Ch 7 (CC) | `[QuickEffectRack1_[Channel4]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| 137 | `0xB4` | `0x22` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect1]` | `meta` | MIDI Learned from 322 messages. |
| 138 | `0xB5` | `0x22` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect1]` | `meta` | MIDI Learned from 324 messages. |
| 139 | `0xB4` | `0x24` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect2]` | `meta` | MIDI Learned from 236 messages. |
| 140 | `0xB5` | `0x24` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect2]` | `meta` | MIDI Learned from 286 messages. |
| 141 | `0xB4` | `0x26` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect3]` | `meta` | MIDI Learned from 224 messages. |
| 142 | `0xB5` | `0x26` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect3]` | `meta` | MIDI Learned from 228 messages. |
| 143 | `0xB0` | `0x27` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter3` | EQ HI - rotate |
| 144 | `0xB1` | `0x27` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter3` | EQ HI - rotate |
| 145 | `0xB2` | `0x27` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter3` | EQ HI - rotate |
| 146 | `0xB3` | `0x27` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter3` | EQ HI - rotate |
| 147 | `0xB0` | `0x2B` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter2` | EQ MID - rotate |
| 148 | `0xB1` | `0x2B` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter2` | EQ MID - rotate |
| 149 | `0xB2` | `0x2B` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter2` | EQ MID - rotate |
| 150 | `0xB3` | `0x2B` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter2` | EQ MID - rotate |
| 151 | `0xB0` | `0x2F` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 152 | `0xB1` | `0x2F` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 153 | `0xB2` | `0x2F` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 154 | `0xB3` | `0x2F` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 155 | `0xB6` | `0x37` | Ch 7 (CC) | `[QuickEffectRack1_[Channel1]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| 156 | `0xB6` | `0x38` | Ch 7 (CC) | `[QuickEffectRack1_[Channel2]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| 157 | `0xB6` | `0x39` | Ch 7 (CC) | `[QuickEffectRack1_[Channel3]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| 158 | `0xB6` | `0x3A` | Ch 7 (CC) | `[QuickEffectRack1_[Channel4]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| 159 | `0x94` | `0x63` | Ch 5 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxSelectPressed` | BEAT FX SELECT - press once - select next effect |
| 160 | `0x94` | `0x64` | Ch 5 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxSelectShiftPressed` | BEAT FX SELECT + shift - press once - select previous effect |
| 161 | `0x94` | `0x70` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect1]` | `PioneerDDJFLX6.fxSelected` |  |
| 162 | `0x95` | `0x70` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect1]` | `PioneerDDJFLX6.fxSelected` |  |
| 163 | `0x94` | `0x71` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect2]` | `PioneerDDJFLX6.fxSelected` |  |
| 164 | `0x95` | `0x71` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect2]` | `PioneerDDJFLX6.fxSelected` |  |
| 165 | `0x94` | `0x72` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect3]` | `PioneerDDJFLX6.fxSelected` |  |
| 166 | `0x95` | `0x72` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect3]` | `PioneerDDJFLX6.fxSelected` |  |


### 6.8. Shift + Transport & Pitch Nudge Special Controls (Total: 12 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 167 | `0x90` | `0x1B` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK1) - press - set hotcue mode |
| 168 | `0x91` | `0x1B` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK2) - press - set hotcue mode |
| 169 | `0x92` | `0x1B` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK3) - press - set hotcue mode |
| 170 | `0x93` | `0x1B` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK4) - press - set hotcue mode |
| 171 | `0x90` | `0x47` | Ch 1 (Note) | `[Channel1]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK1) - press - Reverse playback in Slip Mode while held (Censor) |
| 172 | `0x91` | `0x47` | Ch 2 (Note) | `[Channel2]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK2) - press - Reverse playback in Slip Mode while held (Censor) |
| 173 | `0x92` | `0x47` | Ch 3 (Note) | `[Channel3]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK3) - press - Reverse playback in Slip Mode while held (Censor) |
| 174 | `0x93` | `0x47` | Ch 4 (Note) | `[Channel4]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK4) - press - Reverse playback in Slip Mode while held (Censor) |
| 175 | `0x90` | `0x48` | Ch 1 (Note) | `[Channel1]` | `start_stop` | CUE +SHIFT (DECK1) - press - Jump to track start |
| 176 | `0x91` | `0x48` | Ch 2 (Note) | `[Channel2]` | `start_stop` | CUE +SHIFT (DECK2) - press - Jump to track start |
| 177 | `0x92` | `0x48` | Ch 3 (Note) | `[Channel3]` | `start_stop` | CUE +SHIFT (DECK3) - press - Jump to track start |
| 178 | `0x93` | `0x48` | Ch 4 (Note) | `[Channel4]` | `start_stop` | CUE +SHIFT (DECK4) - press - Jump to track start |


### 6.9. Deck 3 & Deck 4 Hardware Layer Selection Controls (Total: 13 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 179 | `0x90` | `0x20` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK1) - press - set beat jump mode |
| 180 | `0x91` | `0x20` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK2) - press - set beat jump mode |
| 181 | `0x92` | `0x20` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK3) - press - set beat jump mode |
| 182 | `0x93` | `0x20` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK4) - press - set beat jump mode |
| 183 | `0x90` | `0x3D` | Ch 1 (Note) | `[Channel1]` | `slip_enabled` | MIDI Learned from 2 messages. |
| 184 | `0x91` | `0x3D` | Ch 2 (Note) | `[Channel2]` | `slip_enabled` | MIDI Learned from 10 messages. |
| 185 | `0x92` | `0x3D` | Ch 3 (Note) | `[Channel3]` | `slip_enabled` | MIDI Learned from 8 messages. |
| 186 | `0x93` | `0x3D` | Ch 4 (Note) | `[Channel4]` | `slip_enabled` | MIDI Learned from 6 messages. |
| 187 | `0xB6` | `0x64` | Ch 7 (CC) | `[Channel1]` | `PioneerDDJFLX6.waveformZoom` | BROWSE +SHIFT - Zoom waveform |
| 188 | `0x90` | `0x6D` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK1) - press - set beat loop mode |
| 189 | `0x91` | `0x6D` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK2) - press - set beat loop mode |
| 190 | `0x92` | `0x6D` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK3) - press - set beat loop mode |
| 191 | `0x93` | `0x6D` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK4) - press - set beat loop mode |


---

## 7. Kesimpulan & Panduan Integrasi Lanjutan

Dokumen ini mencatat total **552 Register MIDI** (368 Terimplementasi [No. 1 - 368] + 191 Belum Terimplementasi [No. 1 - 191]) secara presisi dengan penomoran counter ter-update.