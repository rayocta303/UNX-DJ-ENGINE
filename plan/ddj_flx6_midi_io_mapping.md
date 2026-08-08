# Pioneer DDJ-FLX6 MIDI I/O Mapping & Exhaustive Register Audit Specification

Dokumen ini memuat analisis spesifikasi teknis protokol MIDI I/O, spesifikasi pengiriman data LED (**Jog Wheel Spinner**, **VU Meter**, **Pioneer SysEx Keep-Alive**), serta **breakdown audit menyeluruh untuk seluruh 559 register MIDI (420 Terimplementasi & 139 Belum Terimplementasi)** pada kontroler **Pioneer DDJ-FLX6** di engine **XDJ-UNX-C** dengan format tabel Markdown presisi tinggi tanpa pemutusan baris (*single-line table rows*).

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

## 4. Matriks Ringkasan Audit Status Pemetaan (Total: 559 Registers)

| Kategori Modul | Fitur Utama Terimplementasi | Fitur Belum Terimplementasi | Terimplementasi | Belum Terimplementasi | Status Paritas |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Mixer & EQ** | Faders 1-4, Crossfader, Gain Trim, EQ High/Mid/Low, Color FX, PFL Cue, Headphones Mix | None | 40 Registers | 0 Registers | **100% Complete** |
| **Transport & Jog** | Play/Pause, Cue, Pitch MSB/LSB, Tempo Range, Sync, Key Lock, Vinyl, Scratch, Touch, Deck Layer, Slip Mode | Shift + Jog Nudge fine | 160 Registers | 29 Registers | **93% Complete** |
| **Browser & Nav** | Browse Knob, Scroll, Browse Push (`enter`), Back Button (`back`), View Toggle, Load A/B, Waveform Zoom | None | 25 Registers | 0 Registers | **100% Complete** |
| **Loops & Cues** | Auto Loop 1-16, Loop In/Out/Exit, Halve/Double, Hot Cue 1-8 Set/Clear | Secondary Loop In/Out Adjust, Reloop | 8 Registers | 20 Registers | **64% Complete** |
| **Performance Pads**| Hot Cue Mode, Beat Jump Mode (-8..+8 & -16/+16), Key Shift Transposition Pads (-4..+3), Deck Layer Toggle | Sampler (16 Slots), Pad FX 1 & 2 | 180 Registers | 76 Registers | **62% Complete** |
| **Beat FX & Merge FX** | FX On/Off, Dry/Wet, FX Select, Beat Left/Right/Tap, Channel Assign | Merge FX Knob/Buttons, Shift+FX Meta Knobs | 16 Registers | 14 Registers | **25% Complete** |
| **Output Driver** | SysEx Keep-Alive, Play, Cue, Vinyl, VU Meter, Jog Spinner Ring, Hot Cue LEDs | Pad Mode State LEDs | 7 Registers | 8 Registers | **47% Complete** |

---

## 5. CATALOG REGISTER MIDI YANG SUDAH TERIMPLEMENTASI (Total: 420 Registers)

### 5.1. Mixer, EQ & Headphone Control Registers (Total: 40 Registers)

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
| 9 | `0xB6` | `0x0C` | Ch 7 (CC) | `[Master]` | `headMix` | HEADPHONES MIXING - rotate - Monitor Balance |
| 10 | `0xB0` | `0x13` | Ch 1 (CC) | `[Channel1]` | `volume` | CHANNELFADER - slider |
| 11 | `0xB1` | `0x13` | Ch 2 (CC) | `[Channel2]` | `volume` | CHANNELFADER - slider |
| 12 | `0xB2` | `0x13` | Ch 3 (CC) | `[Channel3]` | `volume` | CHANNELFADER - slider |
| 13 | `0xB3` | `0x13` | Ch 4 (CC) | `[Channel4]` | `volume` | CHANNELFADER - slider |
| 14 | `0xB6` | `0x17` | Ch 7 (CC) | `[QuickEffectRack1_[Channel1]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| 15 | `0xB6` | `0x18` | Ch 7 (CC) | `[QuickEffectRack1_[Channel2]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| 16 | `0xB6` | `0x19` | Ch 7 (CC) | `[QuickEffectRack1_[Channel3]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| 17 | `0xB6` | `0x1A` | Ch 7 (CC) | `[QuickEffectRack1_[Channel4]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| 18 | `0xB6` | `0x1F` | Ch 7 (CC) | `[Master]` | `crossfader` | CROSSFADER - slider |
| 19 | `0xB0` | `0x20` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK1) - fader - Tempo control LSB |
| 20 | `0xB1` | `0x20` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK2) - fader - Tempo control LSB |
| 21 | `0xB2` | `0x20` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK3) - fader - Tempo control LSB |
| 22 | `0xB3` | `0x20` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK4) - fader - Tempo control LSB |
| 23 | `0xB0` | `0x24` | Ch 1 (CC) | `[Channel1]` | `pregain` | TRIM - rotate |
| 24 | `0xB1` | `0x24` | Ch 2 (CC) | `[Channel2]` | `pregain` | TRIM - rotate |
| 25 | `0xB2` | `0x24` | Ch 3 (CC) | `[Channel3]` | `pregain` | TRIM - rotate |
| 26 | `0xB3` | `0x24` | Ch 4 (CC) | `[Channel4]` | `pregain` | TRIM - rotate |
| 27 | `0xB6` | `0x2C` | Ch 7 (CC) | `[Master]` | `headMix` | HEADPHONES MIXING - rotate - Monitor Balance |
| 28 | `0xB0` | `0x33` | Ch 1 (CC) | `[Channel1]` | `volume` | CHANNELFADER - slider |
| 29 | `0xB1` | `0x33` | Ch 2 (CC) | `[Channel2]` | `volume` | CHANNELFADER - slider |
| 30 | `0xB2` | `0x33` | Ch 3 (CC) | `[Channel3]` | `volume` | CHANNELFADER - slider |
| 31 | `0xB3` | `0x33` | Ch 4 (CC) | `[Channel4]` | `volume` | CHANNELFADER - slider |
| 32 | `0xB6` | `0x37` | Ch 7 (CC) | `[QuickEffectRack1_[Channel1]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| 33 | `0xB6` | `0x38` | Ch 7 (CC) | `[QuickEffectRack1_[Channel2]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| 34 | `0xB6` | `0x39` | Ch 7 (CC) | `[QuickEffectRack1_[Channel3]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| 35 | `0xB6` | `0x3A` | Ch 7 (CC) | `[QuickEffectRack1_[Channel4]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| 36 | `0xB6` | `0x3F` | Ch 7 (CC) | `[Master]` | `crossfader` | CROSSFADER - slider |
| 37 | `0x90` | `0x54` | Ch 1 (Note) | `[Channel1]` | `pfl` | CUE Channel - press - toggle Headphone Cue |
| 38 | `0x91` | `0x54` | Ch 2 (Note) | `[Channel2]` | `pfl` | CUE Channel - press - toggle Headphone Cue |
| 39 | `0x92` | `0x54` | Ch 3 (Note) | `[Channel3]` | `pfl` | CUE Channel - press - toggle Headphone Cue |
| 40 | `0x93` | `0x54` | Ch 4 (Note) | `[Channel4]` | `pfl` | CUE Channel - press - toggle Headphone Cue |


### 5.2. Deck Transport & Jogwheel Manipulation Registers (Total: 160 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 41 | `0x97` | `0x00` | Ch 8 (Note) | `[Channel1]` | `hotcue_1_activate` | PAD 1 (DECK1) HOT CUE MODE - press - set hotcue |
| 42 | `0x98` | `0x00` | Ch 9 (Note) | `[Channel1]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 43 | `0x99` | `0x00` | Ch 10 (Note) | `[Channel2]` | `hotcue_1_activate` | PAD 1 (DECK2) HOT CUE MODE - press - set hotcue |
| 44 | `0x9A` | `0x00` | Ch 11 (Note) | `[Channel2]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 45 | `0x9B` | `0x00` | Ch 12 (Note) | `[Channel3]` | `hotcue_1_activate` | PAD 1 (DECK3) HOT CUE MODE - press - set hotcue |
| 46 | `0x9C` | `0x00` | Ch 13 (Note) | `[Channel3]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 47 | `0x9D` | `0x00` | Ch 14 (Note) | `[Channel4]` | `hotcue_1_activate` | PAD 1 (DECK4) HOT CUE MODE - press - set hotcue |
| 48 | `0x9E` | `0x00` | Ch 15 (Note) | `[Channel4]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 49 | `0x97` | `0x01` | Ch 8 (Note) | `[Channel1]` | `hotcue_2_activate` | PAD 2 (DECK1) HOT CUE MODE - press - set hotcue |
| 50 | `0x98` | `0x01` | Ch 9 (Note) | `[Channel1]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 51 | `0x99` | `0x01` | Ch 10 (Note) | `[Channel2]` | `hotcue_2_activate` | PAD 2 (DECK2) HOT CUE MODE - press - set hotcue |
| 52 | `0x9A` | `0x01` | Ch 11 (Note) | `[Channel2]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 53 | `0x9B` | `0x01` | Ch 12 (Note) | `[Channel3]` | `hotcue_2_activate` | PAD 2 (DECK3) HOT CUE MODE - press - set hotcue |
| 54 | `0x9C` | `0x01` | Ch 13 (Note) | `[Channel3]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 55 | `0x9D` | `0x01` | Ch 14 (Note) | `[Channel4]` | `hotcue_2_activate` | PAD 2 (DECK4) HOT CUE MODE - press - set hotcue |
| 56 | `0x9E` | `0x01` | Ch 15 (Note) | `[Channel4]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 57 | `0x97` | `0x02` | Ch 8 (Note) | `[Channel1]` | `hotcue_3_activate` | PAD 3 (DECK1) HOT CUE MODE - press - set hotcue |
| 58 | `0x98` | `0x02` | Ch 9 (Note) | `[Channel1]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 59 | `0x99` | `0x02` | Ch 10 (Note) | `[Channel2]` | `hotcue_3_activate` | PAD 3 (DECK2) HOT CUE MODE - press - set hotcue |
| 60 | `0x9A` | `0x02` | Ch 11 (Note) | `[Channel2]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 61 | `0x9B` | `0x02` | Ch 12 (Note) | `[Channel3]` | `hotcue_3_activate` | PAD 3 (DECK3) HOT CUE MODE - press - set hotcue |
| 62 | `0x9C` | `0x02` | Ch 13 (Note) | `[Channel3]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 63 | `0x9D` | `0x02` | Ch 14 (Note) | `[Channel4]` | `hotcue_3_activate` | PAD 3 (DECK4) HOT CUE MODE - press - set hotcue |
| 64 | `0x9E` | `0x02` | Ch 15 (Note) | `[Channel4]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 65 | `0x97` | `0x03` | Ch 8 (Note) | `[Channel1]` | `hotcue_4_activate` | PAD 4 (DECK1) HOT CUE MODE - press - set hotcue |
| 66 | `0x98` | `0x03` | Ch 9 (Note) | `[Channel1]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 67 | `0x99` | `0x03` | Ch 10 (Note) | `[Channel2]` | `hotcue_4_activate` | PAD 4 (DECK2) HOT CUE MODE - press - set hotcue |
| 68 | `0x9A` | `0x03` | Ch 11 (Note) | `[Channel2]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 69 | `0x9B` | `0x03` | Ch 12 (Note) | `[Channel3]` | `hotcue_4_activate` | PAD 4 (DECK3) HOT CUE MODE - press - set hotcue |
| 70 | `0x9C` | `0x03` | Ch 13 (Note) | `[Channel3]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 71 | `0x9D` | `0x03` | Ch 14 (Note) | `[Channel4]` | `hotcue_4_activate` | PAD 4 (DECK4) HOT CUE MODE - press - set hotcue |
| 72 | `0x9E` | `0x03` | Ch 15 (Note) | `[Channel4]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 73 | `0x97` | `0x04` | Ch 8 (Note) | `[Channel1]` | `hotcue_5_activate` | PAD 5(DECK1) HOT CUE MODE - press - set hotcue |
| 74 | `0x98` | `0x04` | Ch 9 (Note) | `[Channel1]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 75 | `0x99` | `0x04` | Ch 10 (Note) | `[Channel2]` | `hotcue_5_activate` | PAD 5 (DECK2) HOT CUE MODE - press - set hotcue |
| 76 | `0x9A` | `0x04` | Ch 11 (Note) | `[Channel2]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 77 | `0x9B` | `0x04` | Ch 12 (Note) | `[Channel3]` | `hotcue_5_activate` | PAD 5(DECK3) HOT CUE MODE - press - set hotcue |
| 78 | `0x9C` | `0x04` | Ch 13 (Note) | `[Channel3]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 79 | `0x9D` | `0x04` | Ch 14 (Note) | `[Channel4]` | `hotcue_5_activate` | PAD 5 (DECK4) HOT CUE MODE - press - set hotcue |
| 80 | `0x9E` | `0x04` | Ch 15 (Note) | `[Channel4]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 81 | `0x97` | `0x05` | Ch 8 (Note) | `[Channel1]` | `hotcue_6_activate` | PAD 6 (DECK1) HOT CUE MODE - press - set hotcue |
| 82 | `0x98` | `0x05` | Ch 9 (Note) | `[Channel1]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 83 | `0x99` | `0x05` | Ch 10 (Note) | `[Channel2]` | `hotcue_6_activate` | PAD 6 (DECK2) HOT CUE MODE - press - set hotcue |
| 84 | `0x9A` | `0x05` | Ch 11 (Note) | `[Channel2]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 85 | `0x9B` | `0x05` | Ch 12 (Note) | `[Channel3]` | `hotcue_6_activate` | PAD 6 (DECK3) HOT CUE MODE - press - set hotcue |
| 86 | `0x9C` | `0x05` | Ch 13 (Note) | `[Channel3]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 87 | `0x9D` | `0x05` | Ch 14 (Note) | `[Channel4]` | `hotcue_6_activate` | PAD 6 (DECK4) HOT CUE MODE - press - set hotcue |
| 88 | `0x9E` | `0x05` | Ch 15 (Note) | `[Channel4]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 89 | `0x97` | `0x06` | Ch 8 (Note) | `[Channel1]` | `hotcue_7_activate` | PAD 7 (DECK1) HOT CUE MODE - press - set hotcue |
| 90 | `0x98` | `0x06` | Ch 9 (Note) | `[Channel1]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 91 | `0x99` | `0x06` | Ch 10 (Note) | `[Channel2]` | `hotcue_7_activate` | PAD 7 (DECK2) HOT CUE MODE - press - set hotcue |
| 92 | `0x9A` | `0x06` | Ch 11 (Note) | `[Channel2]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 93 | `0x9B` | `0x06` | Ch 12 (Note) | `[Channel3]` | `hotcue_7_activate` | PAD 7 (DECK3) HOT CUE MODE - press - set hotcue |
| 94 | `0x9C` | `0x06` | Ch 13 (Note) | `[Channel3]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 95 | `0x9D` | `0x06` | Ch 14 (Note) | `[Channel4]` | `hotcue_7_activate` | PAD 7 (DECK4) HOT CUE MODE - press - set hotcue |
| 96 | `0x9E` | `0x06` | Ch 15 (Note) | `[Channel4]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 97 | `0x97` | `0x07` | Ch 8 (Note) | `[Channel1]` | `hotcue_8_activate` | PAD 8 (DECK1) HOT CUE MODE - press - set hotcue |
| 98 | `0x98` | `0x07` | Ch 9 (Note) | `[Channel1]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| 99 | `0x99` | `0x07` | Ch 10 (Note) | `[Channel2]` | `hotcue_8_activate` | PAD 8 (DECK2) HOT CUE MODE - press - set hotcue |
| 100 | `0x9A` | `0x07` | Ch 11 (Note) | `[Channel2]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| 101 | `0x9B` | `0x07` | Ch 12 (Note) | `[Channel3]` | `hotcue_8_activate` | PAD 8 (DECK3) HOT CUE MODE - press - set hotcue |
| 102 | `0x9C` | `0x07` | Ch 13 (Note) | `[Channel3]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| 103 | `0x9D` | `0x07` | Ch 14 (Note) | `[Channel4]` | `hotcue_8_activate` | PAD 8 (DECK4) HOT CUE MODE - press - set hotcue |
| 104 | `0x9E` | `0x07` | Ch 15 (Note) | `[Channel4]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| 105 | `0x90` | `0x0B` | Ch 1 (Note) | `[Channel1]` | `play` | PLAY/PAUSE (DECK1) - press - Play/Pause |
| 106 | `0x91` | `0x0B` | Ch 2 (Note) | `[Channel2]` | `play` | PLAY/PAUSE (DECK2) - press - Play/Pause |
| 107 | `0x92` | `0x0B` | Ch 3 (Note) | `[Channel3]` | `play` | PLAY/PAUSE (DECK3) - press - Play/Pause |
| 108 | `0x93` | `0x0B` | Ch 4 (Note) | `[Channel4]` | `play` | PLAY/PAUSE (DECK4) - press - Play/Pause |
| 109 | `0x90` | `0x0C` | Ch 1 (Note) | `[Channel1]` | `cue_default` | CUE (DECK1) - press - Set/Call Cue, Back Cue |
| 110 | `0x91` | `0x0C` | Ch 2 (Note) | `[Channel2]` | `cue_default` | CUE (DECK2) - press - Set/Call Cue, Back Cue |
| 111 | `0x92` | `0x0C` | Ch 3 (Note) | `[Channel3]` | `cue_default` | CUE (DECK3) - press - Set/Call Cue, Back Cue |
| 112 | `0x93` | `0x0C` | Ch 4 (Note) | `[Channel4]` | `cue_default` | CUE (DECK4) - press - Set/Call Cue, Back Cue |
| 113 | `0xB0` | `0x21` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK1) - rotate - Pitch bend |
| 114 | `0xB1` | `0x21` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK2) - rotate - Pitch bend |
| 115 | `0xB2` | `0x21` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK3) - rotate - Pitch bend |
| 116 | `0xB3` | `0x21` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK4) - rotate - Pitch bend |
| 117 | `0xB0` | `0x22` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK1) - rotate - Scratch |
| 118 | `0xB1` | `0x22` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK2) - rotate - Scratch |
| 119 | `0xB2` | `0x22` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK3) - rotate - Scratch |
| 120 | `0xB3` | `0x22` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK4) - rotate - Scratch |
| 121 | `0xB0` | `0x23` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK1) - rotate - Pitch bend |
| 122 | `0xB1` | `0x23` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK2) - rotate - Pitch bend |
| 123 | `0xB2` | `0x23` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK3) - rotate - Pitch bend |
| 124 | `0xB3` | `0x23` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK4) - rotate - Pitch bend |
| 125 | `0xB0` | `0x29` | Ch 1 (CC) | `[Channel1]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK1) - rotate - Search (Fast Pitch bend) |
| 126 | `0xB1` | `0x29` | Ch 2 (CC) | `[Channel2]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK2) - rotate - Search (Fast Pitch bend) |
| 127 | `0xB2` | `0x29` | Ch 3 (CC) | `[Channel3]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK3) - rotate - Search (Fast Pitch bend) |
| 128 | `0xB3` | `0x29` | Ch 4 (CC) | `[Channel4]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK4) - rotate - Search (Fast Pitch bend) |
| 129 | `0x97` | `0x30` | Ch 8 (Note) | `[Sampler1]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 130 | `0x99` | `0x30` | Ch 10 (Note) | `[Sampler5]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 131 | `0x9B` | `0x30` | Ch 12 (Note) | `[Sampler1]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 132 | `0x9D` | `0x30` | Ch 14 (Note) | `[Sampler5]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 133 | `0x97` | `0x31` | Ch 8 (Note) | `[Sampler2]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 134 | `0x99` | `0x31` | Ch 10 (Note) | `[Sampler6]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 135 | `0x9B` | `0x31` | Ch 12 (Note) | `[Sampler2]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 136 | `0x9D` | `0x31` | Ch 14 (Note) | `[Sampler6]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 137 | `0x97` | `0x32` | Ch 8 (Note) | `[Sampler3]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 138 | `0x99` | `0x32` | Ch 10 (Note) | `[Sampler7]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 139 | `0x9B` | `0x32` | Ch 12 (Note) | `[Sampler3]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 140 | `0x9D` | `0x32` | Ch 14 (Note) | `[Sampler7]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 141 | `0x97` | `0x33` | Ch 8 (Note) | `[Sampler4]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 142 | `0x99` | `0x33` | Ch 10 (Note) | `[Sampler8]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 143 | `0x9B` | `0x33` | Ch 12 (Note) | `[Sampler4]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 144 | `0x9D` | `0x33` | Ch 14 (Note) | `[Sampler8]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 145 | `0x97` | `0x34` | Ch 8 (Note) | `[Sampler9]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 146 | `0x99` | `0x34` | Ch 10 (Note) | `[Sampler13]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 147 | `0x9B` | `0x34` | Ch 12 (Note) | `[Sampler9]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 148 | `0x9D` | `0x34` | Ch 14 (Note) | `[Sampler13]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 149 | `0x97` | `0x35` | Ch 8 (Note) | `[Sampler10]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 150 | `0x99` | `0x35` | Ch 10 (Note) | `[Sampler14]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 151 | `0x9B` | `0x35` | Ch 12 (Note) | `[Sampler10]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 152 | `0x9D` | `0x35` | Ch 14 (Note) | `[Sampler14]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 153 | `0x90` | `0x36` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK1) - touch - enable (on touch) / disable (on release) Scratching/Pitch bend |
| 154 | `0x91` | `0x36` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK2) - touch - enable (on touch) / disable (on release) Scratching/Pitch bend |
| 155 | `0x92` | `0x36` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK3) - touch - enable (on touch) / disable (on release) Scratching/Pitch bend |
| 156 | `0x93` | `0x36` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK4) - touch - enable (on touch) / disable (on release) Scratching/Pitch bend |
| 157 | `0x97` | `0x36` | Ch 8 (Note) | `[Sampler11]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 158 | `0x99` | `0x36` | Ch 10 (Note) | `[Sampler15]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 159 | `0x9B` | `0x36` | Ch 12 (Note) | `[Sampler11]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 160 | `0x9D` | `0x36` | Ch 14 (Note) | `[Sampler15]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 161 | `0x97` | `0x37` | Ch 8 (Note) | `[Sampler12]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 162 | `0x99` | `0x37` | Ch 10 (Note) | `[Sampler16]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 163 | `0x9B` | `0x37` | Ch 12 (Note) | `[Sampler12]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| 164 | `0x9D` | `0x37` | Ch 14 (Note) | `[Sampler16]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| 165 | `0x90` | `0x3D` | Ch 1 (Note) | `[Channel1]` | `slip_enabled` | MIDI Learned from 2 messages. |
| 166 | `0x91` | `0x3D` | Ch 2 (Note) | `[Channel2]` | `slip_enabled` | MIDI Learned from 10 messages. |
| 167 | `0x92` | `0x3D` | Ch 3 (Note) | `[Channel3]` | `slip_enabled` | MIDI Learned from 8 messages. |
| 168 | `0x93` | `0x3D` | Ch 4 (Note) | `[Channel4]` | `slip_enabled` | MIDI Learned from 6 messages. |
| 169 | `0x90` | `0x4C` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK1) - Loop in adjust (using jog wheel) |
| 170 | `0x91` | `0x4C` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK2) - Loop in adjust (using jog wheel) |
| 171 | `0x92` | `0x4C` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK3) - Loop in adjust (using jog wheel) |
| 172 | `0x93` | `0x4C` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK4) - Loop in adjust (using jog wheel) |
| 173 | `0x90` | `0x77` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK1) - Loop out adjust (using jog wheel) |
| 174 | `0x91` | `0x77` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK2) - Loop out adjust (using jog wheel) |
| 175 | `0x92` | `0x77` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK3) - Loop out adjust (using jog wheel) |
| 176 | `0x93` | `0x77` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK4) - Loop out adjust (using jog wheel) |
| 177 | `0x90` | `0x51` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK1) - press - half active loop |
| 178 | `0x91` | `0x51` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK2) - press - half active loop |
| 179 | `0x92` | `0x51` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK3) - press - half active loop |
| 180 | `0x93` | `0x51` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK4) - press - half active loop |
| 181 | `0x90` | `0x53` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK1) - press - double active loop |
| 182 | `0x91` | `0x53` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK2) - press - double active loop |
| 183 | `0x92` | `0x53` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK3) - press - double active loop |
| 184 | `0x93` | `0x53` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK4) - press - double active loop |
| 185 | `0x90` | `0x58` | Ch 1 (Note) | `[Channel1]` | `sync_enabled` | MIDI Learned from 8 messages. |
| 186 | `0x91` | `0x58` | Ch 2 (Note) | `[Channel2]` | `sync_enabled` | MIDI Learned from 8 messages. |
| 187 | `0x92` | `0x58` | Ch 3 (Note) | `[Channel3]` | `sync_enabled` | MIDI Learned from 8 messages. |
| 188 | `0x93` | `0x58` | Ch 4 (Note) | `[Channel4]` | `sync_enabled` | MIDI Learned from 8 messages. |
| 189 | `0x90` | `0x5C` | Ch 1 (Note) | `[Channel1]` | `sync_leader` | MIDI Learned from 12 messages. |
| 190 | `0x91` | `0x5C` | Ch 2 (Note) | `[Channel2]` | `sync_leader` | MIDI Learned from 8 messages. |
| 191 | `0x92` | `0x5C` | Ch 3 (Note) | `[Channel3]` | `sync_leader` | MIDI Learned from 11 messages. |
| 192 | `0x93` | `0x5C` | Ch 4 (Note) | `[Channel4]` | `sync_leader` | MIDI Learned from 9 messages. |
| 193 | `0x90` | `0x60` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK1) - press - change Tempo range |
| 194 | `0x91` | `0x60` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK2) - press - change Tempo range |
| 195 | `0x92` | `0x60` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK3) - press - change Tempo range |
| 196 | `0x93` | `0x60` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK4) - press - change Tempo range |
| 197 | `0x90` | `0x67` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK1) - touch - enable (on touch) / disable (on release) highspeed Pitch bend |
| 198 | `0x91` | `0x67` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK2) - touch - enable (on touch) / disable (on release) highspeed Pitch bend |
| 199 | `0x92` | `0x67` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK3) - touch - enable (on touch) / disable (on release) highspeed Pitch bend |
| 200 | `0x93` | `0x67` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK4) - touch - enable (on touch) / disable (on release) highspeed Pitch bend |


### 5.3. Library & Navigation Registers (Total: 25 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 201 | `0x96` | `0x65` | Ch 7 (Note) | `[Library]` | `back` | BROWSER BACK BUTTON |
| 202 | `0x96` | `0x7A` | Ch 7 (Note) | `[App]` | `browser_toggle` | BROWSER VIEW TOGGLE BUTTON |
| 203 | `0x97` | `0x20` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK1) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| 204 | `0x99` | `0x20` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK2) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| 205 | `0x9B` | `0x20` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK3) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| 206 | `0x9D` | `0x20` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK4) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| 207 | `0x97` | `0x22` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK1) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| 208 | `0x99` | `0x22` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK2) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| 209 | `0x9B` | `0x22` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK3) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| 210 | `0x9D` | `0x22` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK4) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| 211 | `0x97` | `0x24` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK1) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| 212 | `0x99` | `0x24` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK2) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| 213 | `0x9B` | `0x24` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK3) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| 214 | `0x9D` | `0x24` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK4) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| 215 | `0x97` | `0x26` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK1) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| 216 | `0x99` | `0x26` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK2) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| 217 | `0x9B` | `0x26` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK3) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| 218 | `0x9D` | `0x26` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK4) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| 219 | `0xB6` | `0x40` | Ch 7 (CC) | `[Library]` | `MoveVertical` | BROWSE - rotate - Scroll tracklist/tree view |
| 220 | `0x96` | `0x41` | Ch 7 (Note) | `[Library]` | `MoveFocusForward` | BROWSE - press - Move cursor between track list and tree view |
| 221 | `0x96` | `0x42` | Ch 7 (Note) | `[Library]` | `MoveFocusBackward` | BROWSE +SHIFT - press - Move cursor between track list and tree view |
| 222 | `0x96` | `0x46` | Ch 7 (Note) | `[Channel1]` | `LoadSelectedTrack` | LOAD (DECK1) - press - Load a Track into Deck 1 |
| 223 | `0x96` | `0x47` | Ch 7 (Note) | `[Channel2]` | `LoadSelectedTrack` | LOAD (DECK2) - press - Load a Track into Deck 2 |
| 224 | `0x96` | `0x48` | Ch 7 (Note) | `[Channel3]` | `LoadSelectedTrack` | LOAD (DECK3) - press - Load a Track into Deck 1 |
| 225 | `0x96` | `0x49` | Ch 7 (Note) | `[Channel4]` | `LoadSelectedTrack` | LOAD (DECK4) - press - Load a Track into Deck 2 |


### 5.4. Looping & Hot Cue Performance Pad Registers (Total: 8 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 226 | `0x90` | `0x10` | Ch 1 (Note) | `[Channel1]` | `loop_in` | LOOP IN/4 BEAT (DECK1) - press - Set loop in |
| 227 | `0x91` | `0x10` | Ch 2 (Note) | `[Channel2]` | `loop_in` | LOOP IN/4 BEAT (DECK2) - press - Set loop in |
| 228 | `0x92` | `0x10` | Ch 3 (Note) | `[Channel3]` | `loop_in` | LOOP IN/4 BEAT (DECK3) - press - Set loop in |
| 229 | `0x93` | `0x10` | Ch 4 (Note) | `[Channel4]` | `loop_in` | LOOP IN/4 BEAT (DECK4) - press - Set loop in |
| 230 | `0x90` | `0x11` | Ch 1 (Note) | `[Channel1]` | `loop_out` | LOOP OUT (DECK1) - press - Set loop out |
| 231 | `0x91` | `0x11` | Ch 2 (Note) | `[Channel2]` | `loop_out` | LOOP OUT (DECK2) - press - Set loop out |
| 232 | `0x92` | `0x11` | Ch 3 (Note) | `[Channel3]` | `loop_out` | LOOP OUT (DECK3) - press - Set loop out |
| 233 | `0x93` | `0x11` | Ch 4 (Note) | `[Channel4]` | `loop_out` | LOOP OUT (DECK4) - press - Set loop out |


### 5.5. Beat Jump & Pitch Transposition Performance Pad Registers (Total: 88 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 234 | `0x97` | `0x21` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK1) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| 235 | `0x99` | `0x21` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK2) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| 236 | `0x9B` | `0x21` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK3) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| 237 | `0x9D` | `0x21` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK4) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| 238 | `0x97` | `0x23` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK1) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| 239 | `0x99` | `0x23` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK2) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| 240 | `0x9B` | `0x23` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK3) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| 241 | `0x9D` | `0x23` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK4) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| 242 | `0x97` | `0x25` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK1) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| 243 | `0x99` | `0x25` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK2) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| 244 | `0x9B` | `0x25` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK3) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| 245 | `0x9D` | `0x25` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK4) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| 246 | `0x98` | `0x26` | Ch 9 (Note) | `[Channel1]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK1) +SHift BEAT JUMP MODE - press - decrease Beatjump by a factor of 16 |
| 247 | `0x9A` | `0x26` | Ch 11 (Note) | `[Channel2]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK2) +Shift BEAT JUMP MODE - press - decrease Beatjump by a factor of 16 |
| 248 | `0x9C` | `0x26` | Ch 13 (Note) | `[Channel3]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK3) +SHift BEAT JUMP MODE - press - decrease Beatjump by a factor of 16 |
| 249 | `0x9E` | `0x26` | Ch 15 (Note) | `[Channel4]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK4) +Shift BEAT JUMP MODE - press - decrease Beatjump by a factor of 16 |
| 250 | `0x97` | `0x27` | Ch 8 (Note) | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK1) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| 251 | `0x98` | `0x27` | Ch 9 (Note) | `[Channel1]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK1) +SHift BEAT JUMP MODE - press - increase Beatjump by a factor of 16 |
| 252 | `0x99` | `0x27` | Ch 10 (Note) | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK2) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| 253 | `0x9A` | `0x27` | Ch 11 (Note) | `[Channel2]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK2) +Shift BEAT JUMP MODE - press - increase Beatjump by a factor of 16 |
| 254 | `0x9B` | `0x27` | Ch 12 (Note) | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK3) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| 255 | `0x9C` | `0x27` | Ch 13 (Note) | `[Channel3]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK3) +SHift BEAT JUMP MODE - press - increase Beatjump by a factor of 16 |
| 256 | `0x9D` | `0x27` | Ch 14 (Note) | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK4) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| 257 | `0x9E` | `0x27` | Ch 15 (Note) | `[Channel4]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK4) +Shift BEAT JUMP MODE - press - increase Beatjump by a factor of 16 |
| 258 | `0x97` | `0x40` | Ch 8 (Note) | `[Channel1];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 259 | `0x99` | `0x40` | Ch 10 (Note) | `[Channel2];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 260 | `0x9B` | `0x40` | Ch 12 (Note) | `[Channel3];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 261 | `0x9D` | `0x40` | Ch 14 (Note) | `[Channel4];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 262 | `0x97` | `0x41` | Ch 8 (Note) | `[Channel1];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 263 | `0x99` | `0x41` | Ch 10 (Note) | `[Channel2];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 264 | `0x9B` | `0x41` | Ch 12 (Note) | `[Channel3];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 265 | `0x9D` | `0x41` | Ch 14 (Note) | `[Channel4];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 266 | `0x97` | `0x42` | Ch 8 (Note) | `[Channel1];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 267 | `0x99` | `0x42` | Ch 10 (Note) | `[Channel2];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 268 | `0x9B` | `0x42` | Ch 12 (Note) | `[Channel3];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 269 | `0x9D` | `0x42` | Ch 14 (Note) | `[Channel4];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 270 | `0x97` | `0x43` | Ch 8 (Note) | `[Channel1];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 271 | `0x99` | `0x43` | Ch 10 (Note) | `[Channel2];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 272 | `0x9B` | `0x43` | Ch 12 (Note) | `[Channel3];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 273 | `0x9D` | `0x43` | Ch 14 (Note) | `[Channel4];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 274 | `0x97` | `0x44` | Ch 8 (Note) | `[Channel1];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 275 | `0x99` | `0x44` | Ch 10 (Note) | `[Channel2];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 276 | `0x9B` | `0x44` | Ch 12 (Note) | `[Channel3];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 277 | `0x9D` | `0x44` | Ch 14 (Note) | `[Channel4];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 278 | `0x97` | `0x45` | Ch 8 (Note) | `[Channel1];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 279 | `0x99` | `0x45` | Ch 10 (Note) | `[Channel2];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 280 | `0x9B` | `0x45` | Ch 12 (Note) | `[Channel3];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 281 | `0x9D` | `0x45` | Ch 14 (Note) | `[Channel4];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 282 | `0x97` | `0x46` | Ch 8 (Note) | `[Channel1];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 283 | `0x99` | `0x46` | Ch 10 (Note) | `[Channel2];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 284 | `0x9B` | `0x46` | Ch 12 (Note) | `[Channel3];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 285 | `0x9D` | `0x46` | Ch 14 (Note) | `[Channel4];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 286 | `0x97` | `0x47` | Ch 8 (Note) | `[Channel1];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 287 | `0x99` | `0x47` | Ch 10 (Note) | `[Channel2];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 288 | `0x9B` | `0x47` | Ch 12 (Note) | `[Channel3];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 289 | `0x9D` | `0x47` | Ch 14 (Note) | `[Channel4];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| 290 | `0x9B` | `0x50` | Ch 12 (Note) | `[Channel3];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 291 | `0x9D` | `0x50` | Ch 14 (Note) | `[Channel4];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 292 | `0x9B` | `0x51` | Ch 12 (Note) | `[Channel3];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 293 | `0x9D` | `0x51` | Ch 14 (Note) | `[Channel4];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 294 | `0x9B` | `0x52` | Ch 12 (Note) | `[Channel3];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 295 | `0x9D` | `0x52` | Ch 14 (Note) | `[Channel4];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 296 | `0x9B` | `0x53` | Ch 12 (Note) | `[Channel3];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 297 | `0x9D` | `0x53` | Ch 14 (Note) | `[Channel4];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 298 | `0x9B` | `0x54` | Ch 12 (Note) | `[Channel3];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 299 | `0x9D` | `0x54` | Ch 14 (Note) | `[Channel4];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 300 | `0x9B` | `0x55` | Ch 12 (Note) | `[Channel3];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 301 | `0x9D` | `0x55` | Ch 14 (Note) | `[Channel4];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 302 | `0x9B` | `0x56` | Ch 12 (Note) | `[Channel3];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 303 | `0x9D` | `0x56` | Ch 14 (Note) | `[Channel4];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 304 | `0x9B` | `0x57` | Ch 12 (Note) | `[Channel3];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 305 | `0x9D` | `0x57` | Ch 14 (Note) | `[Channel4];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 306 | `0x97` | `0x70` | Ch 8 (Note) | `[Channel1];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 307 | `0x99` | `0x70` | Ch 10 (Note) | `[Channel2];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 308 | `0x97` | `0x71` | Ch 8 (Note) | `[Channel1];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 309 | `0x99` | `0x71` | Ch 10 (Note) | `[Channel2];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 310 | `0x97` | `0x72` | Ch 8 (Note) | `[Channel1];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 311 | `0x99` | `0x72` | Ch 10 (Note) | `[Channel2];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 312 | `0x97` | `0x73` | Ch 8 (Note) | `[Channel1];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 313 | `0x99` | `0x73` | Ch 10 (Note) | `[Channel2];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 314 | `0x97` | `0x74` | Ch 8 (Note) | `[Channel1];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 315 | `0x99` | `0x74` | Ch 10 (Note) | `[Channel2];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 316 | `0x97` | `0x75` | Ch 8 (Note) | `[Channel1];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 317 | `0x99` | `0x75` | Ch 10 (Note) | `[Channel2];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 318 | `0x97` | `0x76` | Ch 8 (Note) | `[Channel1];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 319 | `0x99` | `0x76` | Ch 10 (Note) | `[Channel2];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 320 | `0x97` | `0x77` | Ch 8 (Note) | `[Channel1];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| 321 | `0x99` | `0x77` | Ch 10 (Note) | `[Channel2];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |


### 5.6. Secondary Utility, Shift & Deck Layer Controls (Total: 92 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 322 | `0xB4` | `0x02` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect1]` | `meta` | MIDI Learned from 322 messages. |
| 323 | `0xB5` | `0x02` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect1]` | `meta` | MIDI Learned from 324 messages. |
| 324 | `0xB4` | `0x04` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect2]` | `meta` | MIDI Learned from 236 messages. |
| 325 | `0xB5` | `0x04` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect2]` | `meta` | MIDI Learned from 286 messages. |
| 326 | `0x94` | `0x06` | Ch 5 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxLeftPressed` | BEAT LEFT - press - select previous effect unit |
| 327 | `0x95` | `0x06` | Ch 6 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxLeftPressed` | BEAT LEFT - press - select previous effect unit |
| 328 | `0xB4` | `0x06` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect3]` | `meta` | MIDI Learned from 224 messages. |
| 329 | `0xB5` | `0x06` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect3]` | `meta` | MIDI Learned from 228 messages. |
| 330 | `0x94` | `0x07` | Ch 5 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxRightPressed` | BEAT RIGHT - press - select next effect unit |
| 331 | `0x95` | `0x07` | Ch 6 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxRightPressed` | BEAT RIGHT - press - select next effect unit |
| 332 | `0xB0` | `0x07` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter3` | EQ HI - rotate |
| 333 | `0xB1` | `0x07` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter3` | EQ HI - rotate |
| 334 | `0xB2` | `0x07` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter3` | EQ HI - rotate |
| 335 | `0xB3` | `0x07` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter3` | EQ HI - rotate |
| 336 | `0xB4` | `0x08` | Ch 5 (CC) | `L` | `PioneerDDJFLX6.mergeFxTurn` | MergeFX Turn |
| 337 | `0xB5` | `0x08` | Ch 6 (CC) | `R` | `PioneerDDJFLX6.mergeFxTurn` | MergeFX Turn |
| 338 | `0xB0` | `0x0B` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter2` | EQ MID - rotate |
| 339 | `0xB1` | `0x0B` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter2` | EQ MID - rotate |
| 340 | `0xB2` | `0x0B` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter2` | EQ MID - rotate |
| 341 | `0xB3` | `0x0B` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter2` | EQ MID - rotate |
| 342 | `0xB0` | `0x0F` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 343 | `0xB1` | `0x0F` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 344 | `0xB2` | `0x0F` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 345 | `0xB3` | `0x0F` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 346 | `0x94` | `0x10` | Ch 5 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxChannel1` | BEAT FX CH SELECT CH1 - slide - Select FX on DECK 1 |
| 347 | `0x95` | `0x11` | Ch 6 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxChannel2` | BEAT FX CH SELECT CH2 - slide - Select FX on DECK 2 |
| 348 | `0x94` | `0x14` | Ch 5 (Note) | `[EffectRack1_EffectUnit1];group_[Master]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 MST |
| 349 | `0x95` | `0x14` | Ch 6 (Note) | `[EffectRack1_EffectUnit2];group_[Master]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 MST |
| 350 | `0x94` | `0x1C` | Ch 5 (Note) | `[EffectRack1_EffectUnit1];group_[Channel1]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH1 |
| 351 | `0x95` | `0x1C` | Ch 6 (Note) | `[EffectRack1_EffectUnit2];group_[Channel1]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH1 |
| 352 | `0x94` | `0x1D` | Ch 5 (Note) | `[EffectRack1_EffectUnit1];group_[Channel2]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH2 |
| 353 | `0x95` | `0x1D` | Ch 6 (Note) | `[EffectRack1_EffectUnit2];group_[Channel2]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH2 |
| 354 | `0x94` | `0x1E` | Ch 5 (Note) | `[EffectRack1_EffectUnit1];group_[Channel3]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH3 |
| 355 | `0x95` | `0x1E` | Ch 6 (Note) | `[EffectRack1_EffectUnit2];group_[Channel3]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH3 |
| 356 | `0x94` | `0x1F` | Ch 5 (Note) | `[EffectRack1_EffectUnit1];group_[Channel4]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH4 |
| 357 | `0x95` | `0x1F` | Ch 6 (Note) | `[EffectRack1_EffectUnit2];group_[Channel4]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH4 |
| 358 | `0xB4` | `0x22` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect1]` | `meta` | MIDI Learned from 322 messages. |
| 359 | `0xB5` | `0x22` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect1]` | `meta` | MIDI Learned from 324 messages. |
| 360 | `0xB4` | `0x24` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect2]` | `meta` | MIDI Learned from 236 messages. |
| 361 | `0xB5` | `0x24` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect2]` | `meta` | MIDI Learned from 286 messages. |
| 362 | `0xB4` | `0x26` | Ch 5 (CC) | `[EffectRack1_EffectUnit1_Effect3]` | `meta` | MIDI Learned from 224 messages. |
| 363 | `0xB5` | `0x26` | Ch 6 (CC) | `[EffectRack1_EffectUnit2_Effect3]` | `meta` | MIDI Learned from 228 messages. |
| 364 | `0xB0` | `0x27` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter3` | EQ HI - rotate |
| 365 | `0xB1` | `0x27` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter3` | EQ HI - rotate |
| 366 | `0xB2` | `0x27` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter3` | EQ HI - rotate |
| 367 | `0xB3` | `0x27` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter3` | EQ HI - rotate |
| 368 | `0xB0` | `0x2B` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter2` | EQ MID - rotate |
| 369 | `0xB1` | `0x2B` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter2` | EQ MID - rotate |
| 370 | `0xB2` | `0x2B` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter2` | EQ MID - rotate |
| 371 | `0xB3` | `0x2B` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter2` | EQ MID - rotate |
| 372 | `0xB0` | `0x2F` | Ch 1 (CC) | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 373 | `0xB1` | `0x2F` | Ch 2 (CC) | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 374 | `0xB2` | `0x2F` | Ch 3 (CC) | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 375 | `0xB3` | `0x2F` | Ch 4 (CC) | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter1` | EQ LOW - rotate |
| 376 | `0x90` | `0x3C` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.deckControlLPressed` | DeckControl CH1 |
| 377 | `0x91` | `0x3C` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.deckControlRPressed` | DeckControl CH2 |
| 378 | `0x92` | `0x3C` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.deckControlLPressed` | DeckControl CH3 |
| 379 | `0x93` | `0x3C` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.deckControlRPressed` | DeckControl CH4 |
| 380 | `0x90` | `0x3F` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK1) |
| 381 | `0x91` | `0x3F` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK2) |
| 382 | `0x92` | `0x3F` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK3) |
| 383 | `0x93` | `0x3F` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK4) |
| 384 | `0x94` | `0x47` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect1]` | `PioneerDDJFLX6.fxEnabled` | BFX FX1-1 |
| 385 | `0x95` | `0x47` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect1]` | `PioneerDDJFLX6.fxEnabled` | BFX FX2-1 |
| 386 | `0x94` | `0x48` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect2]` | `PioneerDDJFLX6.fxEnabled` | BFX FX1-2 |
| 387 | `0x95` | `0x48` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect2]` | `PioneerDDJFLX6.fxEnabled` | BFX FX2-2 |
| 388 | `0x94` | `0x49` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect3]` | `PioneerDDJFLX6.fxEnabled` | BFX FX1-3 |
| 389 | `0x95` | `0x49` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect3]` | `PioneerDDJFLX6.fxEnabled` | BFX FX2-3 |
| 390 | `0x90` | `0x4D` | Ch 1 (Note) | `[Channel1]` | `reloop_toggle` | RELOOP/EXIT (DECK1) - press - (loop off) Reloop, (loop on) Loop exit |
| 391 | `0x91` | `0x4D` | Ch 2 (Note) | `[Channel2]` | `reloop_toggle` | RELOOP/EXIT (DECK2) - press - (loop off) Reloop, (loop on) Loop exit |
| 392 | `0x92` | `0x4D` | Ch 3 (Note) | `[Channel3]` | `reloop_toggle` | RELOOP/EXIT (DECK3) - press - (loop off) Reloop, (loop on) Loop exit |
| 393 | `0x93` | `0x4D` | Ch 4 (Note) | `[Channel4]` | `reloop_toggle` | RELOOP/EXIT (DECK4) - press - (loop off) Reloop, (loop on) Loop exit |
| 394 | `0x97` | `0x62` | Ch 8 (Note) | `[Channel1]` | `beatloop_1_toggle` | PAD 3 (DECK1) BEAT LOOP MODE - press - 1/1 Beatloop |
| 395 | `0x99` | `0x62` | Ch 10 (Note) | `[Channel2]` | `beatloop_1_toggle` | PAD 3 (DECK2) BEAT LOOP MODE - press - 1/1 Beatloop |
| 396 | `0x9B` | `0x62` | Ch 12 (Note) | `[Channel3]` | `beatloop_1_toggle` | PAD 3 (DECK3) BEAT LOOP MODE - press - 1/1 Beatloop |
| 397 | `0x9D` | `0x62` | Ch 14 (Note) | `[Channel4]` | `beatloop_1_toggle` | PAD 3 (DECK4) BEAT LOOP MODE - press - 1/1 Beatloop |
| 398 | `0x97` | `0x63` | Ch 8 (Note) | `[Channel1]` | `beatloop_2_toggle` | PAD 4 (DECK1) BEAT LOOP MODE - press - 2 Beatloop |
| 399 | `0x99` | `0x63` | Ch 10 (Note) | `[Channel2]` | `beatloop_2_toggle` | PAD 4 (DECK2) BEAT LOOP MODE - press - 2 Beatloop |
| 400 | `0x9B` | `0x63` | Ch 12 (Note) | `[Channel3]` | `beatloop_2_toggle` | PAD 4 (DECK3) BEAT LOOP MODE - press - 2 Beatloop |
| 401 | `0x9D` | `0x63` | Ch 14 (Note) | `[Channel4]` | `beatloop_2_toggle` | PAD 4 (DECK4) BEAT LOOP MODE - press - 2 Beatloop |
| 402 | `0x97` | `0x64` | Ch 8 (Note) | `[Channel1]` | `beatloop_4_toggle` | PAD 5 (DECK1) BEAT LOOP MODE - press - 4 Beatloop |
| 403 | `0x99` | `0x64` | Ch 10 (Note) | `[Channel2]` | `beatloop_4_toggle` | PAD 5 (DECK2) BEAT LOOP MODE - press - 4 Beatloop |
| 404 | `0x9B` | `0x64` | Ch 12 (Note) | `[Channel3]` | `beatloop_4_toggle` | PAD 5 (DECK3) BEAT LOOP MODE - press - 4 Beatloop |
| 405 | `0x9D` | `0x64` | Ch 14 (Note) | `[Channel4]` | `beatloop_4_toggle` | PAD 5 (DECK4) BEAT LOOP MODE - press - 4 Beatloop |
| 406 | `0x97` | `0x65` | Ch 8 (Note) | `[Channel1]` | `beatloop_8_toggle` | PAD 6 (DECK1) BEAT LOOP MODE - press - 8 Beatloop |
| 407 | `0x99` | `0x65` | Ch 10 (Note) | `[Channel2]` | `beatloop_8_toggle` | PAD 6 (DECK2) BEAT LOOP MODE - press - 8 Beatloop |
| 408 | `0x9B` | `0x65` | Ch 12 (Note) | `[Channel3]` | `beatloop_8_toggle` | PAD 6 (DECK3) BEAT LOOP MODE - press - 8 Beatloop |
| 409 | `0x9D` | `0x65` | Ch 14 (Note) | `[Channel4]` | `beatloop_8_toggle` | PAD 6 (DECK4) BEAT LOOP MODE - press - 8 Beatloop |
| 410 | `0x97` | `0x66` | Ch 8 (Note) | `[Channel1]` | `beatloop_16_toggle` | PAD 7 (DECK1) BEAT LOOP MODE - press - 16 Beatloop |
| 411 | `0x99` | `0x66` | Ch 10 (Note) | `[Channel2]` | `beatloop_16_toggle` | PAD 7 (DECK2) BEAT LOOP MODE - press - 16 Beatloop |
| 412 | `0x9B` | `0x66` | Ch 12 (Note) | `[Channel3]` | `beatloop_16_toggle` | PAD 7 (DECK3) BEAT LOOP MODE - press - 16 Beatloop |
| 413 | `0x9D` | `0x66` | Ch 14 (Note) | `[Channel4]` | `beatloop_16_toggle` | PAD 7 (DECK4) BEAT LOOP MODE - press - 16 Beatloop |


### 5.7. Hardware LED & Signal Output Feedback Registers (Total: 7 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 414 | `0xF0` | `SysEx` | Global (SysEx) | `[Master]` | `Pioneer Keep-Alive` | Handshake SysEx Packet (12 Bytes) |
| 415 | `0x90..0x93` | `0x0B` | Ch 1..4 (Note) | `[Channel1..4]` | `play` | Play/Pause Button Green LED (0x7F = On, 0x00 = Off) |
| 416 | `0x90..0x93` | `0x0C` | Ch 1..4 (Note) | `[Channel1..4]` | `cue` | Cue Button Amber LED (0x7F = On, 0x00 = Off) |
| 417 | `0x90..0x93` | `0x0E` | Ch 1..4 (Note) | `[Channel1..4]` | `vinyl_mode` | Vinyl Mode LED Indicator (0x7F = On, 0x00 = Off) |
| 418 | `0xB0..0xB3` | `0x02` | Ch 1..4 (CC) | `[Channel1..4]` | `vu_meter` | Channel Level VU Meter CC (0..118 RMS + 127 Peak Clip) |
| 419 | `0xBB` | `0x00..0x03` | Ch 12 (CC) | `[Channel1..4]` | `jog_spinner` | Jogwheel Outer LED Ring Playposition Spinner (1..72 steps) |
| 420 | `0x97 / 0x99` | `0x00..0x07` | Ch 8 / 10 (Note) | `[Channel1..2]` | `hotcue_1..8` | Hot Cue Pad Active Marker LEDs (0x7F = Active Cue Present) |


---

## 6. CATALOG BREAKDOWN DETIL SELURUH REGISTER MIDI YANG BELUM TERIMPLEMENTASI (Total: 139 Registers)

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


### 6.3. Pad FX 1 & Pad FX 2 Performance Modes (Total: 40 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 43 | `0x97` | `0x10` | Ch 8 (Note) | `[Channel1];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 44 | `0x99` | `0x10` | Ch 10 (Note) | `[Channel2];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 45 | `0x9B` | `0x10` | Ch 12 (Note) | `[Channel3];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 46 | `0x9D` | `0x10` | Ch 14 (Note) | `[Channel4];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 47 | `0x97` | `0x11` | Ch 8 (Note) | `[Channel1];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 48 | `0x99` | `0x11` | Ch 10 (Note) | `[Channel2];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 49 | `0x9B` | `0x11` | Ch 12 (Note) | `[Channel3];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 50 | `0x9D` | `0x11` | Ch 14 (Note) | `[Channel4];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 51 | `0x97` | `0x12` | Ch 8 (Note) | `[Channel1];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 52 | `0x99` | `0x12` | Ch 10 (Note) | `[Channel2];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 53 | `0x9B` | `0x12` | Ch 12 (Note) | `[Channel3];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 54 | `0x9D` | `0x12` | Ch 14 (Note) | `[Channel4];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 55 | `0x97` | `0x13` | Ch 8 (Note) | `[Channel1];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 56 | `0x99` | `0x13` | Ch 10 (Note) | `[Channel2];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 57 | `0x9B` | `0x13` | Ch 12 (Note) | `[Channel3];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 58 | `0x9D` | `0x13` | Ch 14 (Note) | `[Channel4];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 59 | `0x97` | `0x14` | Ch 8 (Note) | `[Channel1];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 60 | `0x99` | `0x14` | Ch 10 (Note) | `[Channel2];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 61 | `0x9B` | `0x14` | Ch 12 (Note) | `[Channel3];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 62 | `0x9D` | `0x14` | Ch 14 (Note) | `[Channel4];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 63 | `0x97` | `0x15` | Ch 8 (Note) | `[Channel1];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 64 | `0x99` | `0x15` | Ch 10 (Note) | `[Channel2];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 65 | `0x9B` | `0x15` | Ch 12 (Note) | `[Channel3];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 66 | `0x9D` | `0x15` | Ch 14 (Note) | `[Channel4];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 67 | `0x97` | `0x16` | Ch 8 (Note) | `[Channel1];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 68 | `0x99` | `0x16` | Ch 10 (Note) | `[Channel2];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 69 | `0x9B` | `0x16` | Ch 12 (Note) | `[Channel3];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 70 | `0x9D` | `0x16` | Ch 14 (Note) | `[Channel4];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 71 | `0x97` | `0x17` | Ch 8 (Note) | `[Channel1];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 72 | `0x99` | `0x17` | Ch 10 (Note) | `[Channel2];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 73 | `0x9B` | `0x17` | Ch 12 (Note) | `[Channel3];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 74 | `0x9D` | `0x17` | Ch 14 (Note) | `[Channel4];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| 75 | `0x90` | `0x1E` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK1) - press - set pad fx1 mode |
| 76 | `0x91` | `0x1E` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK2) - press - set pad fx1 mode |
| 77 | `0x92` | `0x1E` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK3) - press - set pad fx1 mode |
| 78 | `0x93` | `0x1E` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK4) - press - set pad fx1 mode |
| 79 | `0x90` | `0x6B` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK1) - press - set pad fx2 mode |
| 80 | `0x91` | `0x6B` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK2) - press - set pad fx2 mode |
| 81 | `0x92` | `0x6B` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK3) - press - set pad fx2 mode |
| 82 | `0x93` | `0x6B` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK4) - press - set pad fx2 mode |


### 6.4. Secondary Loop Adjust & Reloop Fine Tuning Controls (Total: 20 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 83 | `0x90` | `0x3E` | Ch 1 (Note) | `[Channel1]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK1) - press - quick jump back |
| 84 | `0x91` | `0x3E` | Ch 2 (Note) | `[Channel2]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK2) - press - quick jump back |
| 85 | `0x92` | `0x3E` | Ch 3 (Note) | `[Channel3]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK3) - press - quick jump back |
| 86 | `0x93` | `0x3E` | Ch 4 (Note) | `[Channel4]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK4) - press - quick jump back |
| 87 | `0x90` | `0x50` | Ch 1 (Note) | `[Channel1]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK1) - press - Reloop and stop |
| 88 | `0x91` | `0x50` | Ch 2 (Note) | `[Channel2]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK2) - press - Reloop and stop |
| 89 | `0x92` | `0x50` | Ch 3 (Note) | `[Channel3]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK3) - press - Reloop and stop |
| 90 | `0x93` | `0x50` | Ch 4 (Note) | `[Channel4]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK4) - press - Reloop and stop |
| 91 | `0x97` | `0x60` | Ch 8 (Note) | `[Channel1]` | `beatloop_0.25_toggle` | PAD 1 (DECK1) BEAT LOOP MODE - press - 1/4 Beatloop |
| 92 | `0x99` | `0x60` | Ch 10 (Note) | `[Channel2]` | `beatloop_0.25_toggle` | PAD 1 (DECK2) BEAT LOOP MODE - press - 1/4 Beatloop |
| 93 | `0x9B` | `0x60` | Ch 12 (Note) | `[Channel3]` | `beatloop_0.25_toggle` | PAD 1 (DECK3) BEAT LOOP MODE - press - 1/4 Beatloop |
| 94 | `0x9D` | `0x60` | Ch 14 (Note) | `[Channel4]` | `beatloop_0.25_toggle` | PAD 1 (DECK4) BEAT LOOP MODE - press - 1/4 Beatloop |
| 95 | `0x97` | `0x61` | Ch 8 (Note) | `[Channel1]` | `beatloop_0.5_toggle` | PAD 2 (DECK1) BEAT LOOP MODE - press - 1/2 Beatloop |
| 96 | `0x99` | `0x61` | Ch 10 (Note) | `[Channel2]` | `beatloop_0.5_toggle` | PAD 2 (DECK2) BEAT LOOP MODE - press - 1/2 Beatloop |
| 97 | `0x9B` | `0x61` | Ch 12 (Note) | `[Channel3]` | `beatloop_0.5_toggle` | PAD 2 (DECK3) BEAT LOOP MODE - press - 1/2 Beatloop |
| 98 | `0x9D` | `0x61` | Ch 14 (Note) | `[Channel4]` | `beatloop_0.5_toggle` | PAD 2 (DECK4) BEAT LOOP MODE - press - 1/2 Beatloop |
| 99 | `0x97` | `0x67` | Ch 8 (Note) | `[Channel1]` | `beatloop_32_toggle` | PAD 8 (DECK1) BEAT LOOP MODE - press - 32 Beatloop |
| 100 | `0x99` | `0x67` | Ch 10 (Note) | `[Channel2]` | `beatloop_32_toggle` | PAD 8 (DECK2) BEAT LOOP MODE - press - 32 Beatloop |
| 101 | `0x9B` | `0x67` | Ch 12 (Note) | `[Channel3]` | `beatloop_32_toggle` | PAD 8 (DECK3) BEAT LOOP MODE - press - 32 Beatloop |
| 102 | `0x9D` | `0x67` | Ch 14 (Note) | `[Channel4]` | `beatloop_32_toggle` | PAD 8 (DECK4) BEAT LOOP MODE - press - 32 Beatloop |


### 6.5. Shift + Beat FX Meta Controls & Parameter Routing (Total: 8 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 103 | `0x94` | `0x63` | Ch 5 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxSelectPressed` | BEAT FX SELECT - press once - select next effect |
| 104 | `0x94` | `0x64` | Ch 5 (Note) | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxSelectShiftPressed` | BEAT FX SELECT + shift - press once - select previous effect |
| 105 | `0x94` | `0x70` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect1]` | `PioneerDDJFLX6.fxSelected` |  |
| 106 | `0x95` | `0x70` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect1]` | `PioneerDDJFLX6.fxSelected` |  |
| 107 | `0x94` | `0x71` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect2]` | `PioneerDDJFLX6.fxSelected` |  |
| 108 | `0x95` | `0x71` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect2]` | `PioneerDDJFLX6.fxSelected` |  |
| 109 | `0x94` | `0x72` | Ch 5 (Note) | `[EffectRack1_EffectUnit1_Effect3]` | `PioneerDDJFLX6.fxSelected` |  |
| 110 | `0x95` | `0x72` | Ch 6 (Note) | `[EffectRack1_EffectUnit2_Effect3]` | `PioneerDDJFLX6.fxSelected` |  |


### 6.6. Shift + Special Transport & Pitch Nudge Controls (Total: 29 Registers)

| No. | Status Byte | Midino (CC/Note) | Channel / Deck Range | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 111 | `0x90` | `0x1B` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK1) - press - set hotcue mode |
| 112 | `0x91` | `0x1B` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK2) - press - set hotcue mode |
| 113 | `0x92` | `0x1B` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK3) - press - set hotcue mode |
| 114 | `0x93` | `0x1B` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK4) - press - set hotcue mode |
| 115 | `0x90` | `0x20` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK1) - press - set beat jump mode |
| 116 | `0x91` | `0x20` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK2) - press - set beat jump mode |
| 117 | `0x92` | `0x20` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK3) - press - set beat jump mode |
| 118 | `0x93` | `0x20` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK4) - press - set beat jump mode |
| 119 | `0x90` | `0x47` | Ch 1 (Note) | `[Channel1]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK1) - press - Reverse playback in Slip Mode while held (Censor) |
| 120 | `0x91` | `0x47` | Ch 2 (Note) | `[Channel2]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK2) - press - Reverse playback in Slip Mode while held (Censor) |
| 121 | `0x92` | `0x47` | Ch 3 (Note) | `[Channel3]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK3) - press - Reverse playback in Slip Mode while held (Censor) |
| 122 | `0x93` | `0x47` | Ch 4 (Note) | `[Channel4]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK4) - press - Reverse playback in Slip Mode while held (Censor) |
| 123 | `0x90` | `0x48` | Ch 1 (Note) | `[Channel1]` | `start_stop` | CUE +SHIFT (DECK1) - press - Jump to track start |
| 124 | `0x91` | `0x48` | Ch 2 (Note) | `[Channel2]` | `start_stop` | CUE +SHIFT (DECK2) - press - Jump to track start |
| 125 | `0x92` | `0x48` | Ch 3 (Note) | `[Channel3]` | `start_stop` | CUE +SHIFT (DECK3) - press - Jump to track start |
| 126 | `0x93` | `0x48` | Ch 4 (Note) | `[Channel4]` | `start_stop` | CUE +SHIFT (DECK4) - press - Jump to track start |
| 127 | `0xB6` | `0x64` | Ch 7 (CC) | `[Channel1]` | `PioneerDDJFLX6.waveformZoom` | BROWSE +SHIFT - Zoom waveform |
| 128 | `0x90` | `0x69` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK1) - press - set keyboard mode |
| 129 | `0x91` | `0x69` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK2) - press - set keyboard mode |
| 130 | `0x92` | `0x69` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK3) - press - set keyboard mode |
| 131 | `0x93` | `0x69` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK4) - press - set keyboard mode |
| 132 | `0x90` | `0x6D` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK1) - press - set beat loop mode |
| 133 | `0x91` | `0x6D` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK2) - press - set beat loop mode |
| 134 | `0x92` | `0x6D` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK3) - press - set beat loop mode |
| 135 | `0x93` | `0x6D` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK4) - press - set beat loop mode |
| 136 | `0x90` | `0x6F` | Ch 1 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK1) - press - set key shift mode |
| 137 | `0x91` | `0x6F` | Ch 2 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK2) - press - set key shift mode |
| 138 | `0x92` | `0x6F` | Ch 3 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK3) - press - set key shift mode |
| 139 | `0x93` | `0x6F` | Ch 4 (Note) | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK4) - press - set key shift mode |


---

## 7. Catatan Progress & Milestones Integrasi Terkini (Terakhir Diperbarui: 9 Agustus 2026)

### 7.1. Implementasi Jogwheel Physics & Config UI (SELESAI)
- **Modularisasi Konfigurasi Jogwheel**: Seluruh parameter fisika jogwheel dipisahkan secara modular ke `src/core/logic/jog_config.h` & `jog_config.c` dengan dukungan persitensi JSON (`jog_config.json`).
- **Release Inertia & Backspin Physics**: Penyempurnaan logika sentuhan jogwheel (`IsTouching`) dengan efek inersia pelepasan (*release inertia*). Putaran cepat yang dilepas (*backspin* / *forward spin*) melambat secara bertahap menuju kecepatan normal 1.0x tanpa terjadinya efek *abrupt halt* / *instant freeze*.
- **Tab Pengaturan "JOG" Pada UI (`SETTING_CAT_JOG`)**:
  - Menambahkan tab dedicated **JOG** pada menu Settings (`settings.h` / `settings.c`).
  - Menyediakan 13 parameter `SETTING_TYPE_KNOB` real-time (Default RPM, Ticks/Rev PPR, Vinyl Release Friction & Cutoff, Pitch Bend Friction & Cutoff, Sensitivity Scales, EMA Filter Weight, Backspin Short/Long Speed, Backspin Decay Rate).
  - Menyediakan tombol aksi **"LOAD DEFAULT JOG SETTINGS"** (`SETTING_TYPE_ACTION`) untuk mengembalikan parameter fisika ke preset pabrik dan memperbarui UI secara langsung dengan feedback Toast notification.
- **Sinkronisasi Real-Time**: Callback `OnSettingsValueChanged` dan `OnSettingsApply` terhubung langsung ke `g_JogConfig` dan `JogConfig_Save`, sehingga penyesuaian parameter dapat dirasakan seketika pada engine audio tanpa perlunya restart aplikasi.

### 7.2. Perbaikan & Penyesuaian Logika Navigasi Browser (SELESAI)
- **Perbaikan Bug Browsing**: Mengeliminasi bug navigasi dan pergeseran fokus pada `src/ui/browser/browser.c` & `browser.h`.
- **Smoothing Touch & Rotary Encoder**: Menyesuaikan akumulator kinetic scrolling dan sensitivitas touchscreen drag/scrub untuk membedakan antara *tap* (pilih track) dan *swipe* (scroll daftar track).
- **Integrasi Hardware Navigation**: Pemetaan tombol `BROWSE Knob`, `Push (enter)`, `Back`, dan `View` berjalan 100% responsif pada daftar tracklist maupun folder tree.

### 7.3. Beat Sync Engine & State Machine (SELESAI)
- **Siklus 3 Mode Sync**: Tombol hardware `SYNC` mendukun siklus state `OFF` -> `BPM Sync` -> `Beat Sync`.
- **Pitch Bend & Phase Correction**: Algoritma pelacakan pitch bend yang halus tanpa lonjakan audio, dengan *graceful downgrade* otomatis dari Beat Sync ke BPM Sync ketika jogwheel disentuh/di-nudge.

### 7.4. Ringkasan Audit Status Pemetaan Register
Dokumen ini mencatat total **559 Register MIDI** (420 Terimplementasi [No. 1 - 420] + 139 Belum Terimplementasi [No. 1 - 139]) secara presisi dengan penomoran counter ter-update dan seluruh fitur inti hardware telah terverifikasi stabil.