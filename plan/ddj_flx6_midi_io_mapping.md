# Pioneer DDJ-FLX6 MIDI I/O Mapping & Exhaustive Register Audit Specification

Dokumen ini memuat analisis spesifikasi teknis protokol MIDI I/O, spesifikasi pengiriman data LED (**Jog Wheel Spinner**, **VU Meter**, **Pioneer SysEx Keep-Alive**), serta **breakdown audit menyeluruh untuk seluruh 552 register MIDI (158 Terimplementasi & 394 Belum Terimplementasi)** pada kontroler **Pioneer DDJ-FLX6** di engine **XDJ-UNX-C**.

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
| **Mixer & EQ** | Faders 1-4, Crossfader, Gain Trim, EQ High/Mid/Low, Color FX, PFL Cue | 14-Bit Fader Fine LSB, EQ Kills, Secondary PFL | 22 Registers | 38 Registers | **37% Complete** |
| **Transport & Jog** | Play/Pause, Cue, Pitch MSB, Tempo Range, Sync, Key Lock, Vinyl, Scratch, Touch | Shift + Jog Scrub Seek, Nudge fine | 24 Registers | 32 Registers | **43% Complete** |
| **Browser & Nav** | Browse Knob, Browse Push (`enter`), Back Button (`back`), View Toggle, Load A/B | Waveform Zoom Step | 6 Registers | 10 Registers | **37% Complete** |
| **Loops & Cues** | Auto Loop 1-16, Loop In/Out/Exit, Halve/Double, Hot Cue 1-8 Set/Clear, Beat Jump | Loop In/Out Adjust, Reloop, Cue Call | 28 Registers | 40 Registers | **41% Complete** |
| **Beat FX & Rack** | FX On/Off, Dry/Wet, FX Select, Beat Left/Right/Tap, Channel Assign | Shift+FX Quick Reset, Rack Meta Knobs | 16 Registers | 64 Registers | **20% Complete** |
| **Merge FX** | - | Merge FX Knob Turn (L/R), Preset Select Buttons | 0 Registers | 8 Registers | **0% Complete** |
| **Performance Pads**| Hot Cue Mode, Beat Jump Mode | Sampler (16 Slots), Key Shift, Keyboard, Pad FX | 48 Registers | 148 Registers | **24% Complete** |
| **Output Driver** | SysEx Keep-Alive, Play, Cue, Vinyl, VU Meter, Jog Spinner Ring, Hot Cue LEDs | Pad Mode LEDs, Merge FX LEDs | 14 Registers | 16 Registers | **47% Complete** |

---

## 5. CATALOG REGISTER MIDI YANG SUDAH TERIMPLEMENTASI (Total: 158 Registers)

### 5.1. Mixer, EQ & Headphone Control Registers (Total: 22 Registers)

| Status Byte | Midino (CC/Note) | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- |
| `0xB0` | `0x04` | `[Channel1]` | `pregain` | TRIM - rotate |
| `0xB1` | `0x04` | `[Channel2]` | `pregain` | TRIM - rotate |
| `0xB2` | `0x04` | `[Channel3]` | `pregain` | TRIM - rotate |
| `0xB3` | `0x04` | `[Channel4]` | `pregain` | TRIM - rotate |
| `0xB0` | `0x13` | `[Channel1]` | `volume` | CHANNELFADER - slider |
| `0xB1` | `0x13` | `[Channel2]` | `volume` | CHANNELFADER - slider |
| `0xB2` | `0x13` | `[Channel3]` | `volume` | CHANNELFADER - slider |
| `0xB3` | `0x13` | `[Channel4]` | `volume` | CHANNELFADER - slider |
| `0xB6` | `0x1F` | `[Master]` | `crossfader` | CROSSFADER - slider |
| `0xB0` | `0x24` | `[Channel1]` | `pregain` | TRIM - rotate |
| `0xB1` | `0x24` | `[Channel2]` | `pregain` | TRIM - rotate |
| `0xB2` | `0x24` | `[Channel3]` | `pregain` | TRIM - rotate |
| `0xB3` | `0x24` | `[Channel4]` | `pregain` | TRIM - rotate |
| `0xB0` | `0x33` | `[Channel1]` | `volume` | CHANNELFADER - slider |
| `0xB1` | `0x33` | `[Channel2]` | `volume` | CHANNELFADER - slider |
| `0xB2` | `0x33` | `[Channel3]` | `volume` | CHANNELFADER - slider |
| `0xB3` | `0x33` | `[Channel4]` | `volume` | CHANNELFADER - slider |
| `0xB6` | `0x3F` | `[Master]` | `crossfader` | CROSSFADER - slider |
| `0x90` | `0x54` | `[Channel1]` | `pfl` | CUE Channel - press - toggle Headphone Cue |
| `0x91` | `0x54` | `[Channel2]` | `pfl` | CUE Channel - press - toggle Headphone Cue |
| `0x92` | `0x54` | `[Channel3]` | `pfl` | CUE Channel - press - toggle Headphone Cue |
| `0x93` | `0x54` | `[Channel4]` | `pfl` | CUE Channel - press - toggle Headphone Cue |


### 5.2. Deck Transport & Jogwheel Manipulation Registers (Total: 104 Registers)

| Status Byte | Midino (CC/Note) | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- |
| `0x97` | `0x00` | `[Channel1]` | `hotcue_1_activate` | PAD 1 (DECK1) HOT CUE MODE - press - set hotcue |
| `0x98` | `0x00` | `[Channel1]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| `0x99` | `0x00` | `[Channel2]` | `hotcue_1_activate` | PAD 1 (DECK2) HOT CUE MODE - press - set hotcue |
| `0x9A` | `0x00` | `[Channel2]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| `0x9B` | `0x00` | `[Channel3]` | `hotcue_1_activate` | PAD 1 (DECK3) HOT CUE MODE - press - set hotcue |
| `0x9C` | `0x00` | `[Channel3]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| `0x9D` | `0x00` | `[Channel4]` | `hotcue_1_activate` | PAD 1 (DECK4) HOT CUE MODE - press - set hotcue |
| `0x9E` | `0x00` | `[Channel4]` | `hotcue_1_clear` | PAD 1 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| `0x97` | `0x01` | `[Channel1]` | `hotcue_2_activate` | PAD 2 (DECK1) HOT CUE MODE - press - set hotcue |
| `0x98` | `0x01` | `[Channel1]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| `0x99` | `0x01` | `[Channel2]` | `hotcue_2_activate` | PAD 2 (DECK2) HOT CUE MODE - press - set hotcue |
| `0x9A` | `0x01` | `[Channel2]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| `0x9B` | `0x01` | `[Channel3]` | `hotcue_2_activate` | PAD 2 (DECK3) HOT CUE MODE - press - set hotcue |
| `0x9C` | `0x01` | `[Channel3]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| `0x9D` | `0x01` | `[Channel4]` | `hotcue_2_activate` | PAD 2 (DECK4) HOT CUE MODE - press - set hotcue |
| `0x9E` | `0x01` | `[Channel4]` | `hotcue_2_clear` | PAD 2 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| `0x97` | `0x02` | `[Channel1]` | `hotcue_3_activate` | PAD 3 (DECK1) HOT CUE MODE - press - set hotcue |
| `0x98` | `0x02` | `[Channel1]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| `0x99` | `0x02` | `[Channel2]` | `hotcue_3_activate` | PAD 3 (DECK2) HOT CUE MODE - press - set hotcue |
| `0x9A` | `0x02` | `[Channel2]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| `0x9B` | `0x02` | `[Channel3]` | `hotcue_3_activate` | PAD 3 (DECK3) HOT CUE MODE - press - set hotcue |
| `0x9C` | `0x02` | `[Channel3]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| `0x9D` | `0x02` | `[Channel4]` | `hotcue_3_activate` | PAD 3 (DECK4) HOT CUE MODE - press - set hotcue |
| `0x9E` | `0x02` | `[Channel4]` | `hotcue_3_clear` | PAD 3 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| `0x97` | `0x03` | `[Channel1]` | `hotcue_4_activate` | PAD 4 (DECK1) HOT CUE MODE - press - set hotcue |
| `0x98` | `0x03` | `[Channel1]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| `0x99` | `0x03` | `[Channel2]` | `hotcue_4_activate` | PAD 4 (DECK2) HOT CUE MODE - press - set hotcue |
| `0x9A` | `0x03` | `[Channel2]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| `0x9B` | `0x03` | `[Channel3]` | `hotcue_4_activate` | PAD 4 (DECK3) HOT CUE MODE - press - set hotcue |
| `0x9C` | `0x03` | `[Channel3]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| `0x9D` | `0x03` | `[Channel4]` | `hotcue_4_activate` | PAD 4 (DECK4) HOT CUE MODE - press - set hotcue |
| `0x9E` | `0x03` | `[Channel4]` | `hotcue_4_clear` | PAD 4 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| `0x97` | `0x04` | `[Channel1]` | `hotcue_5_activate` | PAD 5(DECK1) HOT CUE MODE - press - set hotcue |
| `0x98` | `0x04` | `[Channel1]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| `0x99` | `0x04` | `[Channel2]` | `hotcue_5_activate` | PAD 5 (DECK2) HOT CUE MODE - press - set hotcue |
| `0x9A` | `0x04` | `[Channel2]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| `0x9B` | `0x04` | `[Channel3]` | `hotcue_5_activate` | PAD 5(DECK3) HOT CUE MODE - press - set hotcue |
| `0x9C` | `0x04` | `[Channel3]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| `0x9D` | `0x04` | `[Channel4]` | `hotcue_5_activate` | PAD 5 (DECK4) HOT CUE MODE - press - set hotcue |
| `0x9E` | `0x04` | `[Channel4]` | `hotcue_5_clear` | PAD 5 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| `0x97` | `0x05` | `[Channel1]` | `hotcue_6_activate` | PAD 6 (DECK1) HOT CUE MODE - press - set hotcue |
| `0x98` | `0x05` | `[Channel1]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| `0x99` | `0x05` | `[Channel2]` | `hotcue_6_activate` | PAD 6 (DECK2) HOT CUE MODE - press - set hotcue |
| `0x9A` | `0x05` | `[Channel2]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| `0x9B` | `0x05` | `[Channel3]` | `hotcue_6_activate` | PAD 6 (DECK3) HOT CUE MODE - press - set hotcue |
| `0x9C` | `0x05` | `[Channel3]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| `0x9D` | `0x05` | `[Channel4]` | `hotcue_6_activate` | PAD 6 (DECK4) HOT CUE MODE - press - set hotcue |
| `0x9E` | `0x05` | `[Channel4]` | `hotcue_6_clear` | PAD 6 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| `0x97` | `0x06` | `[Channel1]` | `hotcue_7_activate` | PAD 7 (DECK1) HOT CUE MODE - press - set hotcue |
| `0x98` | `0x06` | `[Channel1]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| `0x99` | `0x06` | `[Channel2]` | `hotcue_7_activate` | PAD 7 (DECK2) HOT CUE MODE - press - set hotcue |
| `0x9A` | `0x06` | `[Channel2]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| `0x9B` | `0x06` | `[Channel3]` | `hotcue_7_activate` | PAD 7 (DECK3) HOT CUE MODE - press - set hotcue |
| `0x9C` | `0x06` | `[Channel3]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| `0x9D` | `0x06` | `[Channel4]` | `hotcue_7_activate` | PAD 7 (DECK4) HOT CUE MODE - press - set hotcue |
| `0x9E` | `0x06` | `[Channel4]` | `hotcue_7_clear` | PAD 7 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| `0x97` | `0x07` | `[Channel1]` | `hotcue_8_activate` | PAD 8 (DECK1) HOT CUE MODE - press - set hotcue |
| `0x98` | `0x07` | `[Channel1]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK1) HOT CUE MODE - press - delete hotcue |
| `0x99` | `0x07` | `[Channel2]` | `hotcue_8_activate` | PAD 8 (DECK2) HOT CUE MODE - press - set hotcue |
| `0x9A` | `0x07` | `[Channel2]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK2) HOT CUE MODE - press - delete hotcue |
| `0x9B` | `0x07` | `[Channel3]` | `hotcue_8_activate` | PAD 8 (DECK3) HOT CUE MODE - press - set hotcue |
| `0x9C` | `0x07` | `[Channel3]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK3) HOT CUE MODE - press - delete hotcue |
| `0x9D` | `0x07` | `[Channel4]` | `hotcue_8_activate` | PAD 8 (DECK4) HOT CUE MODE - press - set hotcue |
| `0x9E` | `0x07` | `[Channel4]` | `hotcue_8_clear` | PAD 8 +SHIFT (DECK4) HOT CUE MODE - press - delete hotcue |
| `0x90` | `0x0B` | `[Channel1]` | `play` | PLAY/PAUSE (DECK1) - press - Play/Pause |
| `0x91` | `0x0B` | `[Channel2]` | `play` | PLAY/PAUSE (DECK2) - press - Play/Pause |
| `0x92` | `0x0B` | `[Channel3]` | `play` | PLAY/PAUSE (DECK3) - press - Play/Pause |
| `0x93` | `0x0B` | `[Channel4]` | `play` | PLAY/PAUSE (DECK4) - press - Play/Pause |
| `0x90` | `0x0C` | `[Channel1]` | `cue_default` | CUE (DECK1) - press - Set/Call Cue, Back Cue |
| `0x91` | `0x0C` | `[Channel2]` | `cue_default` | CUE (DECK2) - press - Set/Call Cue, Back Cue |
| `0x92` | `0x0C` | `[Channel3]` | `cue_default` | CUE (DECK3) - press - Set/Call Cue, Back Cue |
| `0x93` | `0x0C` | `[Channel4]` | `cue_default` | CUE (DECK4) - press - Set/Call Cue, Back Cue |
| `0xB0` | `0x21` | `[Channel1]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK1) - rotate - Pitch bend |
| `0xB1` | `0x21` | `[Channel2]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK2) - rotate - Pitch bend |
| `0xB2` | `0x21` | `[Channel3]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK3) - rotate - Pitch bend |
| `0xB3` | `0x21` | `[Channel4]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL SIDE (DECK4) - rotate - Pitch bend |
| `0xB0` | `0x22` | `[Channel1]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK1) - rotate - Scratch |
| `0xB1` | `0x22` | `[Channel2]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK2) - rotate - Scratch |
| `0xB2` | `0x22` | `[Channel3]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK3) - rotate - Scratch |
| `0xB3` | `0x22` | `[Channel4]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode On (DECK4) - rotate - Scratch |
| `0xB0` | `0x23` | `[Channel1]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK1) - rotate - Pitch bend |
| `0xB1` | `0x23` | `[Channel2]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK2) - rotate - Pitch bend |
| `0xB2` | `0x23` | `[Channel3]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK3) - rotate - Pitch bend |
| `0xB3` | `0x23` | `[Channel4]` | `PioneerDDJFLX6.jogTurn` | JOG DIAL PLATTER Vinyl mode Off (DECK4) - rotate - Pitch bend |
| `0xB0` | `0x29` | `[Channel1]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK1) - rotate - Search (Fast Pitch bend) |
| `0xB1` | `0x29` | `[Channel2]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK2) - rotate - Search (Fast Pitch bend) |
| `0xB2` | `0x29` | `[Channel3]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK3) - rotate - Search (Fast Pitch bend) |
| `0xB3` | `0x29` | `[Channel4]` | `PioneerDDJFLX6.jogSearch` | JOG DIAL PLATTER +SHIFT (DECK4) - rotate - Search (Fast Pitch bend) |
| `0x90` | `0x36` | `[Channel1]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK1) - touch - enable (on touch) / disable (on
                    release) Scratching/Pitch
                    bend
                 |
| `0x91` | `0x36` | `[Channel2]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK2) - touch - enable (on touch) / disable (on
                    release) Scratching/Pitch
                    bend
                 |
| `0x92` | `0x36` | `[Channel3]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK3) - touch - enable (on touch) / disable (on
                    release) Scratching/Pitch
                    bend
                 |
| `0x93` | `0x36` | `[Channel4]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER (DECK4) - touch - enable (on touch) / disable (on
                    release) Scratching/Pitch
                    bend
                 |
| `0x90` | `0x58` | `[Channel1]` | `sync_enabled` | MIDI Learned from 8 messages. |
| `0x91` | `0x58` | `[Channel2]` | `sync_enabled` | MIDI Learned from 8 messages. |
| `0x92` | `0x58` | `[Channel3]` | `sync_enabled` | MIDI Learned from 8 messages. |
| `0x93` | `0x58` | `[Channel4]` | `sync_enabled` | MIDI Learned from 8 messages. |
| `0x90` | `0x5C` | `[Channel1]` | `sync_leader` | MIDI Learned from 12 messages. |
| `0x91` | `0x5C` | `[Channel2]` | `sync_leader` | MIDI Learned from 8 messages. |
| `0x92` | `0x5C` | `[Channel3]` | `sync_leader` | MIDI Learned from 11 messages. |
| `0x93` | `0x5C` | `[Channel4]` | `sync_leader` | MIDI Learned from 9 messages. |
| `0x90` | `0x67` | `[Channel1]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK1) - touch - enable (on touch) / disable
                    (on release) highspeed
                    Pitch bend
                 |
| `0x91` | `0x67` | `[Channel2]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK2) - touch - enable (on touch) / disable
                    (on release) highspeed
                    Pitch bend
                 |
| `0x92` | `0x67` | `[Channel3]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK3) - touch - enable (on touch) / disable
                    (on release) highspeed
                    Pitch bend
                 |
| `0x93` | `0x67` | `[Channel4]` | `PioneerDDJFLX6.jogTouch` | JOG DIAL PLATTER +SHIFT (DECK4) - touch - enable (on touch) / disable
                    (on release) highspeed
                    Pitch bend
                 |


### 5.3. Library & Navigation Registers (Total: 4 Registers)

| Status Byte | Midino (CC/Note) | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- |
| `0x96` | `0x65` | `[Library]` | `back` | BROWSER BACK BUTTON |
| `0x96` | `0x7A` | `[App]` | `browser_toggle` | BROWSER VIEW TOGGLE BUTTON |
| `0x96` | `0x41` | `[Library]` | `MoveFocusForward` | BROWSE - press - Move cursor between track list and tree view |
| `0x96` | `0x42` | `[Library]` | `MoveFocusBackward` | BROWSE +SHIFT - press - Move cursor between track list and tree view |


### 5.4. Looping & Hot Cue Performance Pad Registers (Total: 28 Registers)

| Status Byte | Midino (CC/Note) | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- |
| `0x90` | `0x10` | `[Channel1]` | `loop_in` | LOOP IN/4 BEAT (DECK1) - press - Set loop in |
| `0x91` | `0x10` | `[Channel2]` | `loop_in` | LOOP IN/4 BEAT (DECK2) - press - Set loop in |
| `0x92` | `0x10` | `[Channel3]` | `loop_in` | LOOP IN/4 BEAT (DECK3) - press - Set loop in |
| `0x93` | `0x10` | `[Channel4]` | `loop_in` | LOOP IN/4 BEAT (DECK4) - press - Set loop in |
| `0x90` | `0x11` | `[Channel1]` | `loop_out` | LOOP OUT (DECK1) - press - Set loop out |
| `0x91` | `0x11` | `[Channel2]` | `loop_out` | LOOP OUT (DECK2) - press - Set loop out |
| `0x92` | `0x11` | `[Channel3]` | `loop_out` | LOOP OUT (DECK3) - press - Set loop out |
| `0x93` | `0x11` | `[Channel4]` | `loop_out` | LOOP OUT (DECK4) - press - Set loop out |
| `0x97` | `0x62` | `[Channel1]` | `beatloop_1_toggle` | PAD 3 (DECK1) BEAT LOOP MODE - press - 1/1 Beatloop |
| `0x99` | `0x62` | `[Channel2]` | `beatloop_1_toggle` | PAD 3 (DECK2) BEAT LOOP MODE - press - 1/1 Beatloop |
| `0x9B` | `0x62` | `[Channel3]` | `beatloop_1_toggle` | PAD 3 (DECK3) BEAT LOOP MODE - press - 1/1 Beatloop |
| `0x9D` | `0x62` | `[Channel4]` | `beatloop_1_toggle` | PAD 3 (DECK4) BEAT LOOP MODE - press - 1/1 Beatloop |
| `0x97` | `0x63` | `[Channel1]` | `beatloop_2_toggle` | PAD 4 (DECK1) BEAT LOOP MODE - press - 2 Beatloop |
| `0x99` | `0x63` | `[Channel2]` | `beatloop_2_toggle` | PAD 4 (DECK2) BEAT LOOP MODE - press - 2 Beatloop |
| `0x9B` | `0x63` | `[Channel3]` | `beatloop_2_toggle` | PAD 4 (DECK3) BEAT LOOP MODE - press - 2 Beatloop |
| `0x9D` | `0x63` | `[Channel4]` | `beatloop_2_toggle` | PAD 4 (DECK4) BEAT LOOP MODE - press - 2 Beatloop |
| `0x97` | `0x64` | `[Channel1]` | `beatloop_4_toggle` | PAD 5 (DECK1) BEAT LOOP MODE - press - 4 Beatloop |
| `0x99` | `0x64` | `[Channel2]` | `beatloop_4_toggle` | PAD 5 (DECK2) BEAT LOOP MODE - press - 4 Beatloop |
| `0x9B` | `0x64` | `[Channel3]` | `beatloop_4_toggle` | PAD 5 (DECK3) BEAT LOOP MODE - press - 4 Beatloop |
| `0x9D` | `0x64` | `[Channel4]` | `beatloop_4_toggle` | PAD 5 (DECK4) BEAT LOOP MODE - press - 4 Beatloop |
| `0x97` | `0x65` | `[Channel1]` | `beatloop_8_toggle` | PAD 6 (DECK1) BEAT LOOP MODE - press - 8 Beatloop |
| `0x99` | `0x65` | `[Channel2]` | `beatloop_8_toggle` | PAD 6 (DECK2) BEAT LOOP MODE - press - 8 Beatloop |
| `0x9B` | `0x65` | `[Channel3]` | `beatloop_8_toggle` | PAD 6 (DECK3) BEAT LOOP MODE - press - 8 Beatloop |
| `0x9D` | `0x65` | `[Channel4]` | `beatloop_8_toggle` | PAD 6 (DECK4) BEAT LOOP MODE - press - 8 Beatloop |
| `0x97` | `0x66` | `[Channel1]` | `beatloop_16_toggle` | PAD 7 (DECK1) BEAT LOOP MODE - press - 16 Beatloop |
| `0x99` | `0x66` | `[Channel2]` | `beatloop_16_toggle` | PAD 7 (DECK2) BEAT LOOP MODE - press - 16 Beatloop |
| `0x9B` | `0x66` | `[Channel3]` | `beatloop_16_toggle` | PAD 7 (DECK3) BEAT LOOP MODE - press - 16 Beatloop |
| `0x9D` | `0x66` | `[Channel4]` | `beatloop_16_toggle` | PAD 7 (DECK4) BEAT LOOP MODE - press - 16 Beatloop |


### 5.5. Beat FX & Master Control Registers (Total: 0 Registers)

| Status Byte | Midino (CC/Note) | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- |


### 5.6. Hardware LED & Signal Output Feedback Registers (Total: 7 Registers)

| Status Byte | Midino (CC/Note) | Target Group | ControlObject / Key | Fungsi & Handler Engine |
| :--- | :--- | :--- | :--- | :--- |
| `0xF0` | `SysEx` | `[Master]` | `Pioneer Keep-Alive` | Handshake SysEx Packet (12 Bytes) |
| `0x90 + deck` | `0x0B` | `[Channel1..4]` | `play` | Play/Pause Button Green LED (0x7F = On, 0x00 = Off) |
| `0x90 + deck` | `0x0C` | `[Channel1..4]` | `cue` | Cue Button Amber LED (0x7F = On, 0x00 = Off) |
| `0x90 + deck` | `0x0E` | `[Channel1..4]` | `vinyl_mode` | Vinyl Mode LED Indicator (0x7F = On, 0x00 = Off) |
| `0xB0 + deck` | `0x02` | `[Channel1..4]` | `vu_meter` | Channel Level VU Meter CC (0..118 RMS + 127 Peak Clip) |
| `0xBB` | `0x00..0x03` | `[Channel1..4]` | `jog_spinner` | Jogwheel Outer LED Ring Playposition Spinner (1..72 steps) |
| `0x97 / 0x99` | `0x00..0x07` | `[Channel1..2]` | `hotcue_1..8` | Hot Cue Pad Active Marker LEDs (0x7F = Active Cue Present) |


---

## 6. CATALOG BREAKDOWN DETIL SELURUH REGISTER MIDI YANG BELUM TERIMPLEMENTASI (Total: 394 Registers)

### 6.1. Merge FX Control & Preset Selectors (Total: 8 Registers)

| Status Byte | Midino (CC/Note) | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- |
| `0xB4` | `0x08` | `L` | `PioneerDDJFLX6.mergeFxTurn` | MergeFX Turn |
| `0xB5` | `0x08` | `R` | `PioneerDDJFLX6.mergeFxTurn` | MergeFX Turn |
| `0x94` | `0x2E` | `L` | `PioneerDDJFLX6.mergeEffectButtonPressed` | Merge Effect L Button |
| `0x95` | `0x2E` | `R` | `PioneerDDJFLX6.mergeEffectButtonPressed` | Merge Effect R Button |
| `0x94` | `0x2F` | `L` | `PioneerDDJFLX6.mergeEffectSelectorPressed` | Merge Effect L Button |
| `0x95` | `0x2F` | `R` | `PioneerDDJFLX6.mergeEffectSelectorPressed` | Merge Effect R Button |
| `0x94` | `0x30` | `L` | `PioneerDDJFLX6.mergeEffectSelectorPressedReverse` | Merge Effect L Button shift |
| `0x95` | `0x30` | `R` | `PioneerDDJFLX6.mergeEffectSelectorPressedReverse` | Merge Effect R Button shift |


### 6.2. Sampler Slot Triggering & Bank Controls (Total: 68 Registers)

| Status Byte | Midino (CC/Note) | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- |
| `0x90` | `0x22` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | SAMPLER MODE (DECK1) - press - set sampler mode |
| `0x91` | `0x22` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | SAMPLER MODE (DECK2) - press - set sampler mode |
| `0x92` | `0x22` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | SAMPLER MODE (DECK3) - press - set sampler mode |
| `0x93` | `0x22` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | SAMPLER MODE (DECK4) - press - set sampler mode |
| `0x97` | `0x30` | `[Sampler1]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x98` | `0x30` | `[Sampler1]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 1 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x99` | `0x30` | `[Sampler5]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9A` | `0x30` | `[Sampler5]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 1 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9B` | `0x30` | `[Sampler1]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9C` | `0x30` | `[Sampler1]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 1 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9D` | `0x30` | `[Sampler5]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 1 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9E` | `0x30` | `[Sampler5]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 1 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x97` | `0x31` | `[Sampler2]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x98` | `0x31` | `[Sampler2]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 2 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x99` | `0x31` | `[Sampler6]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9A` | `0x31` | `[Sampler6]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 2 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9B` | `0x31` | `[Sampler2]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9C` | `0x31` | `[Sampler2]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 2 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9D` | `0x31` | `[Sampler6]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 2 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9E` | `0x31` | `[Sampler6]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 2 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x97` | `0x32` | `[Sampler3]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x98` | `0x32` | `[Sampler3]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 3 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x99` | `0x32` | `[Sampler7]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9A` | `0x32` | `[Sampler7]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 3 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9B` | `0x32` | `[Sampler3]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9C` | `0x32` | `[Sampler3]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 3 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9D` | `0x32` | `[Sampler7]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 3 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9E` | `0x32` | `[Sampler7]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 3 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x97` | `0x33` | `[Sampler4]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x98` | `0x33` | `[Sampler4]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 4 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x99` | `0x33` | `[Sampler8]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9A` | `0x33` | `[Sampler8]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 4 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9B` | `0x33` | `[Sampler4]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9C` | `0x33` | `[Sampler4]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 4 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9D` | `0x33` | `[Sampler8]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 4 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9E` | `0x33` | `[Sampler8]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 4 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x97` | `0x34` | `[Sampler9]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x98` | `0x34` | `[Sampler9]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 5 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x99` | `0x34` | `[Sampler13]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9A` | `0x34` | `[Sampler13]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 5 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9B` | `0x34` | `[Sampler9]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9C` | `0x34` | `[Sampler9]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 5 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9D` | `0x34` | `[Sampler13]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 5 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9E` | `0x34` | `[Sampler13]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 5 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x97` | `0x35` | `[Sampler10]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x98` | `0x35` | `[Sampler10]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 6 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x99` | `0x35` | `[Sampler14]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9A` | `0x35` | `[Sampler14]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 6 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9B` | `0x35` | `[Sampler10]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9C` | `0x35` | `[Sampler10]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 6 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9D` | `0x35` | `[Sampler14]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 6 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9E` | `0x35` | `[Sampler14]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 6 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x97` | `0x36` | `[Sampler11]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x98` | `0x36` | `[Sampler11]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 7 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x99` | `0x36` | `[Sampler15]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9A` | `0x36` | `[Sampler15]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 7 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9B` | `0x36` | `[Sampler11]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9C` | `0x36` | `[Sampler11]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 7 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9D` | `0x36` | `[Sampler15]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 7 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9E` | `0x36` | `[Sampler15]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 7 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x97` | `0x37` | `[Sampler12]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x98` | `0x37` | `[Sampler12]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 8 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x99` | `0x37` | `[Sampler16]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9A` | `0x37` | `[Sampler16]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 8 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9B` | `0x37` | `[Sampler12]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (LEFT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9C` | `0x37` | `[Sampler12]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 8 (LEFT)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |
| `0x9D` | `0x37` | `[Sampler16]` | `PioneerDDJFLX6.samplerPadPressed` | PAD 8 (RIGHT) SAMPLE MODE - press - Play Sample or Load Track |
| `0x9E` | `0x37` | `[Sampler16]` | `PioneerDDJFLX6.samplerPadShiftPressed` | PAD 8 (Right)+SHIFT SAMPLE MODE - press - Stop Playback or Eject Track |


### 6.3. Key Shift, Key Sync & Keyboard Pitch Transposition (Total: 40 Registers)

| Status Byte | Midino (CC/Note) | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- |
| `0x97` | `0x40` | `[Channel1];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x99` | `0x40` | `[Channel2];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9B` | `0x40` | `[Channel3];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9D` | `0x40` | `[Channel4];4` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x97` | `0x41` | `[Channel1];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x99` | `0x41` | `[Channel2];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9B` | `0x41` | `[Channel3];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9D` | `0x41` | `[Channel4];5` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x97` | `0x42` | `[Channel1];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x99` | `0x42` | `[Channel2];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9B` | `0x42` | `[Channel3];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9D` | `0x42` | `[Channel4];6` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x97` | `0x43` | `[Channel1];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x99` | `0x43` | `[Channel2];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9B` | `0x43` | `[Channel3];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9D` | `0x43` | `[Channel4];7` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x97` | `0x44` | `[Channel1];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x99` | `0x44` | `[Channel2];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9B` | `0x44` | `[Channel3];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9D` | `0x44` | `[Channel4];0` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x97` | `0x45` | `[Channel1];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x99` | `0x45` | `[Channel2];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9B` | `0x45` | `[Channel3];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9D` | `0x45` | `[Channel4];1` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x97` | `0x46` | `[Channel1];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x99` | `0x46` | `[Channel2];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9B` | `0x46` | `[Channel3];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9D` | `0x46` | `[Channel4];2` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x97` | `0x47` | `[Channel1];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x99` | `0x47` | `[Channel2];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9B` | `0x47` | `[Channel3];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x9D` | `0x47` | `[Channel4];3` | `PioneerDDJFLX6.keyboardButtonPressed` |  |
| `0x90` | `0x69` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK1) - press - set keyboard mode |
| `0x91` | `0x69` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK2) - press - set keyboard mode |
| `0x92` | `0x69` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK3) - press - set keyboard mode |
| `0x93` | `0x69` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEYBOARD MODE (DECK4) - press - set keyboard mode |
| `0x90` | `0x6F` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK1) - press - set key shift mode |
| `0x91` | `0x6F` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK2) - press - set key shift mode |
| `0x92` | `0x6F` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK3) - press - set key shift mode |
| `0x93` | `0x6F` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | KEY SHIFT MODE (DECK4) - press - set key shift mode |


### 6.4. Pad FX 1 & Pad FX 2 Performance Modes (Total: 40 Registers)

| Status Byte | Midino (CC/Note) | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- |
| `0x97` | `0x10` | `[Channel1];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x99` | `0x10` | `[Channel2];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9B` | `0x10` | `[Channel3];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9D` | `0x10` | `[Channel4];1` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x97` | `0x11` | `[Channel1];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x99` | `0x11` | `[Channel2];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9B` | `0x11` | `[Channel3];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9D` | `0x11` | `[Channel4];2` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x97` | `0x12` | `[Channel1];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x99` | `0x12` | `[Channel2];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9B` | `0x12` | `[Channel3];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9D` | `0x12` | `[Channel4];3` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x97` | `0x13` | `[Channel1];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x99` | `0x13` | `[Channel2];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9B` | `0x13` | `[Channel3];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9D` | `0x13` | `[Channel4];4` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x97` | `0x14` | `[Channel1];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x99` | `0x14` | `[Channel2];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9B` | `0x14` | `[Channel3];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9D` | `0x14` | `[Channel4];5` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x97` | `0x15` | `[Channel1];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x99` | `0x15` | `[Channel2];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9B` | `0x15` | `[Channel3];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9D` | `0x15` | `[Channel4];6` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x97` | `0x16` | `[Channel1];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x99` | `0x16` | `[Channel2];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9B` | `0x16` | `[Channel3];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9D` | `0x16` | `[Channel4];7` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x97` | `0x17` | `[Channel1];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x99` | `0x17` | `[Channel2];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9B` | `0x17` | `[Channel3];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x9D` | `0x17` | `[Channel4];8` | `PioneerDDJFLX6.padFxPressed` | Pressed PadFX button |
| `0x90` | `0x1E` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK1) - press - set pad fx1 mode |
| `0x91` | `0x1E` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK2) - press - set pad fx1 mode |
| `0x92` | `0x1E` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK3) - press - set pad fx1 mode |
| `0x93` | `0x1E` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX1 MODE (DECK4) - press - set pad fx1 mode |
| `0x90` | `0x6B` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK1) - press - set pad fx2 mode |
| `0x91` | `0x6B` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK2) - press - set pad fx2 mode |
| `0x92` | `0x6B` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK3) - press - set pad fx2 mode |
| `0x93` | `0x6B` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | PAD FX2 MODE (DECK4) - press - set pad fx2 mode |


### 6.5. High-Precision Pitch Slider Fine Tuning - 14-Bit LSB (Total: 4 Registers)

| Status Byte | Midino (CC/Note) | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- |
| `0xB0` | `0x20` | `[Channel1]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK1) - fader - Tempo control LSB |
| `0xB1` | `0x20` | `[Channel2]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK2) - fader - Tempo control LSB |
| `0xB2` | `0x20` | `[Channel3]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK3) - fader - Tempo control LSB |
| `0xB3` | `0x20` | `[Channel4]` | `PioneerDDJFLX6.tempoSliderLSB` | TEMPO (DECK4) - fader - Tempo control LSB |


### 6.6. Secondary Loop Adjust, Reloop & Cue Call Navigation (Total: 40 Registers)

| Status Byte | Midino (CC/Note) | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- |
| `0x90` | `0x3E` | `[Channel1]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK1) - press - quick jump back |
| `0x91` | `0x3E` | `[Channel2]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK2) - press - quick jump back |
| `0x92` | `0x3E` | `[Channel3]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK3) - press - quick jump back |
| `0x93` | `0x3E` | `[Channel4]` | `PioneerDDJFLX6.quickJumpBack` | CUE/LOOP CALL LEFT + SHIFT (DECK4) - press - quick jump back |
| `0x90` | `0x4C` | `[Channel1]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK1) - Loop in adjust (using jog wheel) |
| `0x91` | `0x4C` | `[Channel2]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK2) - Loop in adjust (using jog wheel) |
| `0x92` | `0x4C` | `[Channel3]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK3) - Loop in adjust (using jog wheel) |
| `0x93` | `0x4C` | `[Channel4]` | `PioneerDDJFLX6.toggleLoopAdjustIn` | SHIFT + LOOP IN (DECK4) - Loop in adjust (using jog wheel) |
| `0x90` | `0x4D` | `[Channel1]` | `reloop_toggle` | RELOOP/EXIT (DECK1) - press - (loop off) Reloop, (loop on) Loop exit |
| `0x91` | `0x4D` | `[Channel2]` | `reloop_toggle` | RELOOP/EXIT (DECK2) - press - (loop off) Reloop, (loop on) Loop exit |
| `0x92` | `0x4D` | `[Channel3]` | `reloop_toggle` | RELOOP/EXIT (DECK3) - press - (loop off) Reloop, (loop on) Loop exit |
| `0x93` | `0x4D` | `[Channel4]` | `reloop_toggle` | RELOOP/EXIT (DECK4) - press - (loop off) Reloop, (loop on) Loop exit |
| `0x90` | `0x77` | `[Channel1]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK1) - Loop out adjust (using jog wheel) |
| `0x91` | `0x77` | `[Channel2]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK2) - Loop out adjust (using jog wheel) |
| `0x92` | `0x77` | `[Channel3]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK3) - Loop out adjust (using jog wheel) |
| `0x93` | `0x77` | `[Channel4]` | `PioneerDDJFLX6.toggleLoopAdjustOut` | SHIFT + LOOP OUT (DECK4) - Loop out adjust (using jog wheel) |
| `0x90` | `0x50` | `[Channel1]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK1) - press - Reloop and stop |
| `0x91` | `0x50` | `[Channel2]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK2) - press - Reloop and stop |
| `0x92` | `0x50` | `[Channel3]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK3) - press - Reloop and stop |
| `0x93` | `0x50` | `[Channel4]` | `reloop_andstop` | RELOOP/EXIT +SHIFT (DECK4) - press - Reloop and stop |
| `0x90` | `0x51` | `[Channel1]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK1) - press - half active loop |
| `0x91` | `0x51` | `[Channel2]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK2) - press - half active loop |
| `0x92` | `0x51` | `[Channel3]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK3) - press - half active loop |
| `0x93` | `0x51` | `[Channel4]` | `PioneerDDJFLX6.cueLoopCallLeft` | CUE/LOOP CALL LEFT (DECK4) - press - half active loop |
| `0x90` | `0x53` | `[Channel1]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK1) - press - double active loop |
| `0x91` | `0x53` | `[Channel2]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK2) - press - double active loop |
| `0x92` | `0x53` | `[Channel3]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK3) - press - double active loop |
| `0x93` | `0x53` | `[Channel4]` | `PioneerDDJFLX6.cueLoopCallRight` | CUE/LOOP CALL LEFT (DECK4) - press - double active loop |
| `0x97` | `0x60` | `[Channel1]` | `beatloop_0.25_toggle` | PAD 1 (DECK1) BEAT LOOP MODE - press - 1/4 Beatloop |
| `0x99` | `0x60` | `[Channel2]` | `beatloop_0.25_toggle` | PAD 1 (DECK2) BEAT LOOP MODE - press - 1/4 Beatloop |
| `0x9B` | `0x60` | `[Channel3]` | `beatloop_0.25_toggle` | PAD 1 (DECK3) BEAT LOOP MODE - press - 1/4 Beatloop |
| `0x9D` | `0x60` | `[Channel4]` | `beatloop_0.25_toggle` | PAD 1 (DECK4) BEAT LOOP MODE - press - 1/4 Beatloop |
| `0x97` | `0x61` | `[Channel1]` | `beatloop_0.5_toggle` | PAD 2 (DECK1) BEAT LOOP MODE - press - 1/2 Beatloop |
| `0x99` | `0x61` | `[Channel2]` | `beatloop_0.5_toggle` | PAD 2 (DECK2) BEAT LOOP MODE - press - 1/2 Beatloop |
| `0x9B` | `0x61` | `[Channel3]` | `beatloop_0.5_toggle` | PAD 2 (DECK3) BEAT LOOP MODE - press - 1/2 Beatloop |
| `0x9D` | `0x61` | `[Channel4]` | `beatloop_0.5_toggle` | PAD 2 (DECK4) BEAT LOOP MODE - press - 1/2 Beatloop |
| `0x97` | `0x67` | `[Channel1]` | `beatloop_32_toggle` | PAD 8 (DECK1) BEAT LOOP MODE - press - 32 Beatloop |
| `0x99` | `0x67` | `[Channel2]` | `beatloop_32_toggle` | PAD 8 (DECK2) BEAT LOOP MODE - press - 32 Beatloop |
| `0x9B` | `0x67` | `[Channel3]` | `beatloop_32_toggle` | PAD 8 (DECK3) BEAT LOOP MODE - press - 32 Beatloop |
| `0x9D` | `0x67` | `[Channel4]` | `beatloop_32_toggle` | PAD 8 (DECK4) BEAT LOOP MODE - press - 32 Beatloop |


### 6.7. Shift + Beat FX Meta Controls & Rack Parameter Routing (Total: 74 Registers)

| Status Byte | Midino (CC/Note) | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- |
| `0xB4` | `0x02` | `[EffectRack1_EffectUnit1_Effect1]` | `meta` | MIDI Learned from 322 messages. |
| `0xB5` | `0x02` | `[EffectRack1_EffectUnit2_Effect1]` | `meta` | MIDI Learned from 324 messages. |
| `0xB4` | `0x04` | `[EffectRack1_EffectUnit1_Effect2]` | `meta` | MIDI Learned from 236 messages. |
| `0xB5` | `0x04` | `[EffectRack1_EffectUnit2_Effect2]` | `meta` | MIDI Learned from 286 messages. |
| `0x94` | `0x06` | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxLeftPressed` | BEAT LEFT - press - select previous effect unit |
| `0x95` | `0x06` | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxLeftPressed` | BEAT LEFT - press - select previous effect unit |
| `0xB4` | `0x06` | `[EffectRack1_EffectUnit1_Effect3]` | `meta` | MIDI Learned from 224 messages. |
| `0xB5` | `0x06` | `[EffectRack1_EffectUnit2_Effect3]` | `meta` | MIDI Learned from 228 messages. |
| `0x94` | `0x07` | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxRightPressed` | BEAT RIGHT - press - select next effect unit |
| `0x95` | `0x07` | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxRightPressed` | BEAT RIGHT - press - select next effect unit |
| `0xB0` | `0x07` | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter3` | EQ HI - rotate |
| `0xB1` | `0x07` | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter3` | EQ HI - rotate |
| `0xB2` | `0x07` | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter3` | EQ HI - rotate |
| `0xB3` | `0x07` | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter3` | EQ HI - rotate |
| `0xB0` | `0x0B` | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter2` | EQ MID - rotate |
| `0xB1` | `0x0B` | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter2` | EQ MID - rotate |
| `0xB2` | `0x0B` | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter2` | EQ MID - rotate |
| `0xB3` | `0x0B` | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter2` | EQ MID - rotate |
| `0xB0` | `0x0F` | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter1` | EQ LOW - rotate |
| `0xB1` | `0x0F` | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter1` | EQ LOW - rotate |
| `0xB2` | `0x0F` | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter1` | EQ LOW - rotate |
| `0xB3` | `0x0F` | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter1` | EQ LOW - rotate |
| `0x94` | `0x10` | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxChannel1` | BEAT FX CH SELECT CH1 - slide - Select FX on DECK 1 |
| `0x95` | `0x11` | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxChannel2` | BEAT FX CH SELECT CH2 - slide - Select FX on DECK 2 |
| `0x94` | `0x14` | `[EffectRack1_EffectUnit1];group_[Master]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 MST |
| `0x95` | `0x14` | `[EffectRack1_EffectUnit2];group_[Master]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 MST |
| `0xB6` | `0x17` | `[QuickEffectRack1_[Channel1]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| `0xB6` | `0x18` | `[QuickEffectRack1_[Channel2]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| `0xB6` | `0x19` | `[QuickEffectRack1_[Channel3]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| `0xB6` | `0x1A` | `[QuickEffectRack1_[Channel4]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| `0x94` | `0x1C` | `[EffectRack1_EffectUnit1];group_[Channel1]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH1 |
| `0x95` | `0x1C` | `[EffectRack1_EffectUnit2];group_[Channel1]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH1 |
| `0x94` | `0x1D` | `[EffectRack1_EffectUnit1];group_[Channel2]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH2 |
| `0x95` | `0x1D` | `[EffectRack1_EffectUnit2];group_[Channel2]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH2 |
| `0x94` | `0x1E` | `[EffectRack1_EffectUnit1];group_[Channel3]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH3 |
| `0x95` | `0x1E` | `[EffectRack1_EffectUnit2];group_[Channel3]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH3 |
| `0x94` | `0x1F` | `[EffectRack1_EffectUnit1];group_[Channel4]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX1 CH4 |
| `0x95` | `0x1F` | `[EffectRack1_EffectUnit2];group_[Channel4]_enable` | `PioneerDDJFLX6.setGroupKey` | CH Select FX2 CH4 |
| `0xB4` | `0x22` | `[EffectRack1_EffectUnit1_Effect1]` | `meta` | MIDI Learned from 322 messages. |
| `0xB5` | `0x22` | `[EffectRack1_EffectUnit2_Effect1]` | `meta` | MIDI Learned from 324 messages. |
| `0xB4` | `0x24` | `[EffectRack1_EffectUnit1_Effect2]` | `meta` | MIDI Learned from 236 messages. |
| `0xB5` | `0x24` | `[EffectRack1_EffectUnit2_Effect2]` | `meta` | MIDI Learned from 286 messages. |
| `0xB4` | `0x26` | `[EffectRack1_EffectUnit1_Effect3]` | `meta` | MIDI Learned from 224 messages. |
| `0xB5` | `0x26` | `[EffectRack1_EffectUnit2_Effect3]` | `meta` | MIDI Learned from 228 messages. |
| `0xB0` | `0x27` | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter3` | EQ HI - rotate |
| `0xB1` | `0x27` | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter3` | EQ HI - rotate |
| `0xB2` | `0x27` | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter3` | EQ HI - rotate |
| `0xB3` | `0x27` | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter3` | EQ HI - rotate |
| `0xB0` | `0x2B` | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter2` | EQ MID - rotate |
| `0xB1` | `0x2B` | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter2` | EQ MID - rotate |
| `0xB2` | `0x2B` | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter2` | EQ MID - rotate |
| `0xB3` | `0x2B` | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter2` | EQ MID - rotate |
| `0xB0` | `0x2F` | `[EqualizerRack1_[Channel1]_Effect1]` | `parameter1` | EQ LOW - rotate |
| `0xB1` | `0x2F` | `[EqualizerRack1_[Channel2]_Effect1]` | `parameter1` | EQ LOW - rotate |
| `0xB2` | `0x2F` | `[EqualizerRack1_[Channel3]_Effect1]` | `parameter1` | EQ LOW - rotate |
| `0xB3` | `0x2F` | `[EqualizerRack1_[Channel4]_Effect1]` | `parameter1` | EQ LOW - rotate |
| `0xB6` | `0x37` | `[QuickEffectRack1_[Channel1]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| `0xB6` | `0x38` | `[QuickEffectRack1_[Channel2]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| `0xB6` | `0x39` | `[QuickEffectRack1_[Channel3]]` | `super1` | FILTER CH1 - rotate - Filter Effect Knob |
| `0xB6` | `0x3A` | `[QuickEffectRack1_[Channel4]]` | `super1` | FILTER CH2 - rotate - Filter Effect Knob |
| `0x94` | `0x47` | `[EffectRack1_EffectUnit1_Effect1]` | `PioneerDDJFLX6.fxEnabled` | BFX FX1-1 |
| `0x95` | `0x47` | `[EffectRack1_EffectUnit2_Effect1]` | `PioneerDDJFLX6.fxEnabled` | BFX FX2-1 |
| `0x94` | `0x48` | `[EffectRack1_EffectUnit1_Effect2]` | `PioneerDDJFLX6.fxEnabled` | BFX FX1-2 |
| `0x95` | `0x48` | `[EffectRack1_EffectUnit2_Effect2]` | `PioneerDDJFLX6.fxEnabled` | BFX FX2-2 |
| `0x94` | `0x49` | `[EffectRack1_EffectUnit1_Effect3]` | `PioneerDDJFLX6.fxEnabled` | BFX FX1-3 |
| `0x95` | `0x49` | `[EffectRack1_EffectUnit2_Effect3]` | `PioneerDDJFLX6.fxEnabled` | BFX FX2-3 |
| `0x94` | `0x63` | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxSelectPressed` | BEAT FX SELECT - press once - select next effect |
| `0x94` | `0x64` | `[EffectRack1_EffectUnit1]` | `PioneerDDJFLX6.beatFxSelectShiftPressed` | BEAT FX SELECT + shift - press once - select previous effect |
| `0x94` | `0x70` | `[EffectRack1_EffectUnit1_Effect1]` | `PioneerDDJFLX6.fxSelected` |  |
| `0x95` | `0x70` | `[EffectRack1_EffectUnit2_Effect1]` | `PioneerDDJFLX6.fxSelected` |  |
| `0x94` | `0x71` | `[EffectRack1_EffectUnit1_Effect2]` | `PioneerDDJFLX6.fxSelected` |  |
| `0x95` | `0x71` | `[EffectRack1_EffectUnit2_Effect2]` | `PioneerDDJFLX6.fxSelected` |  |
| `0x94` | `0x72` | `[EffectRack1_EffectUnit1_Effect3]` | `PioneerDDJFLX6.fxSelected` |  |
| `0x95` | `0x72` | `[EffectRack1_EffectUnit2_Effect3]` | `PioneerDDJFLX6.fxSelected` |  |


### 6.8. Shift + Transport & Pitch Nudge Special Controls (Total: 20 Registers)

| Status Byte | Midino (CC/Note) | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- |
| `0xB0` | `0x00` | `[Channel1]` | `PioneerDDJFLX6.tempoSliderMSB` | TEMPO (DECK1) - fader - Tempo control MSB |
| `0xB1` | `0x00` | `[Channel2]` | `PioneerDDJFLX6.tempoSliderMSB` | TEMPO (DECK2) - fader - Tempo control MSB |
| `0xB2` | `0x00` | `[Channel3]` | `PioneerDDJFLX6.tempoSliderMSB` | TEMPO (DECK3) - fader - Tempo control MSB |
| `0xB3` | `0x00` | `[Channel4]` | `PioneerDDJFLX6.tempoSliderMSB` | TEMPO (DECK4) - fader - Tempo control MSB |
| `0x90` | `0x1B` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK1) - press - set hotcue mode |
| `0x91` | `0x1B` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK2) - press - set hotcue mode |
| `0x92` | `0x1B` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK3) - press - set hotcue mode |
| `0x93` | `0x1B` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | HOT CUE MODE (DECK4) - press - set hotcue mode |
| `0x90` | `0x47` | `[Channel1]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK1) - press - Reverse playback in Slip Mode while
                    held (Censor) |
| `0x91` | `0x47` | `[Channel2]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK2) - press - Reverse playback in Slip Mode while
                    held (Censor) |
| `0x92` | `0x47` | `[Channel3]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK3) - press - Reverse playback in Slip Mode while
                    held (Censor) |
| `0x93` | `0x47` | `[Channel4]` | `reverseroll` | PLAY/PAUSE +SHIFT (DECK4) - press - Reverse playback in Slip Mode while
                    held (Censor) |
| `0x90` | `0x48` | `[Channel1]` | `start_stop` | CUE +SHIFT (DECK1) - press - Jump to track start |
| `0x91` | `0x48` | `[Channel2]` | `start_stop` | CUE +SHIFT (DECK2) - press - Jump to track start |
| `0x92` | `0x48` | `[Channel3]` | `start_stop` | CUE +SHIFT (DECK3) - press - Jump to track start |
| `0x93` | `0x48` | `[Channel4]` | `start_stop` | CUE +SHIFT (DECK4) - press - Jump to track start |
| `0x90` | `0x60` | `[Channel1]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK1) - press - change Tempo range |
| `0x91` | `0x60` | `[Channel2]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK2) - press - change Tempo range |
| `0x92` | `0x60` | `[Channel3]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK3) - press - change Tempo range |
| `0x93` | `0x60` | `[Channel4]` | `PioneerDDJFLX6.cycleTempoRange` | BEAT SYNC +SHIFT (DECK4) - press - change Tempo range |


### 6.9. Shift + Mixer, EQ Kill & Headphone Cue Secondary Controls (Total: 0 Registers)

| Status Byte | Midino (CC/Note) | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- |


### 6.10. Utility, Deck Layer Selectors & Miscellaneous Hardware Mappings (Total: 100 Registers)

| Status Byte | Midino (CC/Note) | Target Group | Mixxx Key / Function | Deskripsi Detail Trigger Hardware |
| :--- | :--- | :--- | :--- | :--- |
| `0xB6` | `0x0C` | `[Master]` | `headMix` | HEADPHONES MIXING - rotate - Monitor Balance |
| `0x90` | `0x20` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK1) - press - set beat jump mode |
| `0x91` | `0x20` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK2) - press - set beat jump mode |
| `0x92` | `0x20` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK3) - press - set beat jump mode |
| `0x93` | `0x20` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT JUMP MODE (DECK4) - press - set beat jump mode |
| `0x97` | `0x20` | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK1) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| `0x99` | `0x20` | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK2) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| `0x9B` | `0x20` | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK3) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| `0x9D` | `0x20` | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 1 (DECK4) BEAT JUMP MODE - press - Jump 1 Beat backwards |
| `0x97` | `0x21` | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK1) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| `0x99` | `0x21` | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK2) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| `0x9B` | `0x21` | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK3) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| `0x9D` | `0x21` | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 2 (DECK4) BEAT JUMP MODE - press - Jump 1 Beat forwards |
| `0x97` | `0x22` | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK1) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| `0x99` | `0x22` | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK2) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| `0x9B` | `0x22` | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK3) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| `0x9D` | `0x22` | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 3 (DECK4) BEAT JUMP MODE - press - Jump 2 Beats backwards |
| `0x97` | `0x23` | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK1) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| `0x99` | `0x23` | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK2) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| `0x9B` | `0x23` | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK3) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| `0x9D` | `0x23` | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 4 (DECK4) BEAT JUMP MODE - press - Jump 2 Beats forwards |
| `0x97` | `0x24` | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK1) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| `0x99` | `0x24` | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK2) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| `0x9B` | `0x24` | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK3) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| `0x9D` | `0x24` | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 5 (DECK4) BEAT JUMP MODE - press - Jump 4 Beats backwards |
| `0x97` | `0x25` | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK1) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| `0x99` | `0x25` | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK2) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| `0x9B` | `0x25` | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK3) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| `0x9D` | `0x25` | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 6 (DECK4) BEAT JUMP MODE - press - Jump 4 Beats forwards |
| `0x97` | `0x26` | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK1) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| `0x98` | `0x26` | `[Channel1]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK1) +SHift BEAT JUMP MODE - press - decrease Beatjump by a
                    factor of 16 |
| `0x99` | `0x26` | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK2) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| `0x9A` | `0x26` | `[Channel2]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK2) +Shift BEAT JUMP MODE - press - decrease Beatjump by a
                    factor of 16 |
| `0x9B` | `0x26` | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK3) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| `0x9C` | `0x26` | `[Channel3]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK3) +SHift BEAT JUMP MODE - press - decrease Beatjump by a
                    factor of 16 |
| `0x9D` | `0x26` | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 7 (DECK4) BEAT JUMP MODE - press - Jump 8 Beats backwards |
| `0x9E` | `0x26` | `[Channel4]` | `PioneerDDJFLX6.decreaseBeatjumpSizes` | PAD 7 (DECK4) +Shift BEAT JUMP MODE - press - decrease Beatjump by a
                    factor of 16 |
| `0x97` | `0x27` | `[Channel1]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK1) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| `0x98` | `0x27` | `[Channel1]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK1) +SHift BEAT JUMP MODE - press - increase Beatjump by a
                    factor of 16 |
| `0x99` | `0x27` | `[Channel2]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK2) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| `0x9A` | `0x27` | `[Channel2]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK2) +Shift BEAT JUMP MODE - press - increase Beatjump by a
                    factor of 16 |
| `0x9B` | `0x27` | `[Channel3]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK3) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| `0x9C` | `0x27` | `[Channel3]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK3) +SHift BEAT JUMP MODE - press - increase Beatjump by a
                    factor of 16 |
| `0x9D` | `0x27` | `[Channel4]` | `PioneerDDJFLX6.beatjumpPadPressed` | PAD 8 (DECK4) BEAT JUMP MODE - press - Jump 8 Beats forwards |
| `0x9E` | `0x27` | `[Channel4]` | `PioneerDDJFLX6.increaseBeatjumpSizes` | PAD 8 (DECK4) +Shift BEAT JUMP MODE - press - increase Beatjump by a
                    factor of 16 |
| `0xB6` | `0x2C` | `[Master]` | `headMix` | HEADPHONES MIXING - rotate - Monitor Balance |
| `0x90` | `0x3C` | `[Channel1]` | `PioneerDDJFLX6.deckControlLPressed` | DeckControl CH1 |
| `0x91` | `0x3C` | `[Channel2]` | `PioneerDDJFLX6.deckControlRPressed` | DeckControl CH2 |
| `0x92` | `0x3C` | `[Channel3]` | `PioneerDDJFLX6.deckControlLPressed` | DeckControl CH3 |
| `0x93` | `0x3C` | `[Channel4]` | `PioneerDDJFLX6.deckControlRPressed` | DeckControl CH4 |
| `0x90` | `0x3D` | `[Channel1]` | `slip_enabled` | MIDI Learned from 2 messages. |
| `0x91` | `0x3D` | `[Channel2]` | `slip_enabled` | MIDI Learned from 10 messages. |
| `0x92` | `0x3D` | `[Channel3]` | `slip_enabled` | MIDI Learned from 8 messages. |
| `0x93` | `0x3D` | `[Channel4]` | `slip_enabled` | MIDI Learned from 6 messages. |
| `0x90` | `0x3F` | `[Channel1]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK1) |
| `0x91` | `0x3F` | `[Channel2]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK2) |
| `0x92` | `0x3F` | `[Channel3]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK3) |
| `0x93` | `0x3F` | `[Channel4]` | `PioneerDDJFLX6.shiftPressed` | Shift (DECK4) |
| `0xB6` | `0x40` | `[Library]` | `MoveVertical` | BROWSE - rotate - Scroll tracklist/tree view |
| `0x96` | `0x46` | `[Channel1]` | `LoadSelectedTrack` | LOAD (DECK1) - press - Load a Track into Deck 1 |
| `0x96` | `0x47` | `[Channel2]` | `LoadSelectedTrack` | LOAD (DECK2) - press - Load a Track into Deck 2 |
| `0x96` | `0x48` | `[Channel3]` | `LoadSelectedTrack` | LOAD (DECK3) - press - Load a Track into Deck 1 |
| `0x96` | `0x49` | `[Channel4]` | `LoadSelectedTrack` | LOAD (DECK4) - press - Load a Track into Deck 2 |
| `0x9B` | `0x50` | `[Channel3];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9D` | `0x50` | `[Channel4];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9B` | `0x51` | `[Channel3];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9D` | `0x51` | `[Channel4];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9B` | `0x52` | `[Channel3];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9D` | `0x52` | `[Channel4];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9B` | `0x53` | `[Channel3];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9D` | `0x53` | `[Channel4];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9B` | `0x54` | `[Channel3];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9D` | `0x54` | `[Channel4];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9B` | `0x55` | `[Channel3];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9D` | `0x55` | `[Channel4];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9B` | `0x56` | `[Channel3];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9D` | `0x56` | `[Channel4];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9B` | `0x57` | `[Channel3];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x9D` | `0x57` | `[Channel4];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0xB6` | `0x64` | `[Channel1]` | `PioneerDDJFLX6.waveformZoom` | BROWSE +SHIFT - Zoom waveform |
| `0x90` | `0x6D` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK1) - press - set beat loop mode |
| `0x91` | `0x6D` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK2) - press - set beat loop mode |
| `0x92` | `0x6D` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK3) - press - set beat loop mode |
| `0x93` | `0x6D` | `[PadMode]` | `PioneerDDJFLX6.padModeKeyPressed` | BEAT LOOP MODE (DECK4) - press - set beat loop mode |
| `0x97` | `0x70` | `[Channel1];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x99` | `0x70` | `[Channel2];pitch;4` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x97` | `0x71` | `[Channel1];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x99` | `0x71` | `[Channel2];pitch;5` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x97` | `0x72` | `[Channel1];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x99` | `0x72` | `[Channel2];pitch;6` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x97` | `0x73` | `[Channel1];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x99` | `0x73` | `[Channel2];pitch;7` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x97` | `0x74` | `[Channel1];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x99` | `0x74` | `[Channel2];pitch;0` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x97` | `0x75` | `[Channel1];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x99` | `0x75` | `[Channel2];pitch;1` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x97` | `0x76` | `[Channel1];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x99` | `0x76` | `[Channel2];pitch;2` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x97` | `0x77` | `[Channel1];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |
| `0x99` | `0x77` | `[Channel2];pitch;3` | `PioneerDDJFLX6.setGroupKeyValue` |  |


---

## 7. Kesimpulan & Panduan Integrasi Lanjutan

Dokumen ini mencatat total **552 Register MIDI** (158 Terimplementasi + 394 Belum Terimplementasi) secara presisi dengan jumlah register yang tercantum eksplisit pada tiap judul poin.