import sys
import time
import json
import os

try:
    import mido
except ImportError:
    print("Error: 'mido' and 'python-rtmidi' are required.")
    print("Please install them by running: pip install mido python-rtmidi")
    sys.exit(1)

SETTINGS_FILE = os.path.join(os.path.dirname(__file__), "..", "..", "build", "windows", "jog_config.json")

def find_midi_input():
    inputs = mido.get_input_names()
    if not inputs:
        print("No MIDI input devices found!")
        sys.exit(1)
        
    print("\n--- Available MIDI Input Devices ---")
    for i, name in enumerate(inputs):
        print(f"[{i}] {name}")
        
    try:
        idx = int(input("\nSelect the MIDI device for your controller (e.g. DDJ-FLX6) [0]: ") or "0")
        if 0 <= idx < len(inputs):
            return inputs[idx]
        else:
            print("Invalid selection.")
            sys.exit(1)
    except ValueError:
        print("Invalid selection.")
        sys.exit(1)

def main():
    print("========================================")
    print("  XDJ-UNX Jogwheel Calibration Tool")
    print("========================================\n")
    
    port_name = find_midi_input()
    print(f"\nConnecting to: {port_name}...")
    
    try:
        inport = mido.open_input(port_name)
    except Exception as e:
        print(f"Failed to open MIDI port: {e}")
        sys.exit(1)

    print("\n[STEP 1]")
    print("Let's calibrate the Jogwheel Ticks Per Revolution.")
    input("1. Place your finger on the jogwheel at the 12 o'clock position.\n2. Press ENTER when ready to start recording...")
    
    # Clear any pending messages
    for msg in inport.iter_pending():
        pass
        
    print("\n[RECORDING ACTIVE]")
    print("Rotate the jogwheel EXACTLY 1 FULL REVOLUTION CLOCKWISE (back to 12 o'clock).")
    input("Press ENTER when you are done rotating...")
    
    total_ticks = 0
    
    # Pioneer sends jog turn on CC 0x21, 0x22, 0x23 (Decimal: 33, 34, 35)
    # The value is usually 64 + delta. e.g. 65 is +1, 63 is -1
    for msg in inport.iter_pending():
        if msg.type == 'control_change':
            if msg.control in [33, 34, 35]: 
                delta = msg.value - 64
                total_ticks += delta
                
    inport.close()
    
    print(f"\n--- CALIBRATION RESULTS ---")
    print(f"Total Ticks Accumulated: {total_ticks}")
    
    if total_ticks == 0:
        print("Error: No jogwheel movement detected. Make sure you selected the right controller.")
        sys.exit(1)
        
    target_ticks = float(abs(total_ticks))
    print(f"\nCalculated TicksPerRev: {target_ticks}")
    print("This value dictates how fast the software waveform and LED Ring spin.")
    print("If your LED Ring is too slow, your TicksPerRev in settings is likely too high.")
    
    # Try to update jog_config.json if it exists
    if os.path.exists(SETTINGS_FILE):
        update = input(f"\nDo you want to automatically update jog_config.json with TicksPerRev = {target_ticks}? (y/n): ").strip().lower()
        if update == 'y':
            try:
                with open(SETTINGS_FILE, 'r') as f:
                    settings = json.load(f)
                    
                settings['TicksPerRev'] = target_ticks
                
                with open(SETTINGS_FILE, 'w') as f:
                    json.dump(settings, f, indent=4)
                print("\nSuccess! jog_config.json updated. Restart XDJ-UNX to apply.")
            except Exception as e:
                print(f"Failed to update jog_config.json: {e}")
    else:
        print(f"\nNote: {SETTINGS_FILE} not found.")
        print(f"Please manually set 'TicksPerRev': {target_ticks} in your jog_config.json file.")

if __name__ == "__main__":
    main()
