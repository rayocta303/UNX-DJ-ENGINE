with open('src/core/midi/midi_scripts.c', 'r') as f:
    content = f.read()

import re

start_idx = content.find('void MIDI_ExecuteScript(MidiMapping *map, int actionId, uint8_t status,')
if start_idx == -1:
    start_idx = content.find('void MIDI_ExecuteScript(MidiMapping *map, const char *function, uint8_t status,')
end_idx = content.find('}', start_idx + 10)
while content[end_idx-1] != '\n' or content[end_idx-2] != '}':
    end_idx = content.find('\n}\n', end_idx + 1)
    if end_idx == -1: break

old_func = content[start_idx:end_idx+3]

new_func = old_func.replace('const char *function', 'int actionId')
new_func = new_func.replace('if (strstr(function, "shiftButton") || strstr(function, "shiftPressed")) {', 'switch(actionId) {\n  case SCRIPT_ACTION_SHIFT:')
new_func = new_func.replace('} else if (strstr(function, "jogTurn") || strstr(function, "jogSearch")) {', '  break;\n  case SCRIPT_ACTION_JOG_TURN:')
new_func = new_func.replace('} else if (strstr(function, "jogTouch") || strstr(function, "JogTouch")) {', '  break;\n  case SCRIPT_ACTION_JOG_TOUCH:')
new_func = new_func.replace('} else if (strstr(function, "beatTap") || strstr(function, "beatFxTap")) {', '  break;\n  case SCRIPT_ACTION_BEAT_TAP:')
new_func = new_func.replace('} else if (strstr(function, "beatFxSelect") ||\n             strstr(function, "beatFxNext")) {', '  break;\n  case SCRIPT_ACTION_BEATFX_NEXT:')
new_func = new_func.replace('} else if (strstr(function, "beatFxPrev")) {', '  break;\n  case SCRIPT_ACTION_BEATFX_PREV:')
new_func = new_func.replace('} else if (strstr(function, "beatFxLevelDepth") ||\n             strstr(function, "beatFxDepth") ||\n             strstr(function, "drywet") ||\n             strstr(function, "meta") ||\n             strstr(function, "super1")) {', '  break;\n  case SCRIPT_ACTION_BEATFX_DEPTH:')
new_func = new_func.replace('} else if (strstr(function, "fxEnabled") ||\n             strstr(function, "beatFxOnOffPressed") ||\n             strstr(function, "beatFxOnOff") ||\n             strstr(function, "super1_toggle")) {', '  break;\n  case SCRIPT_ACTION_BEATFX_TOGGLE:')
new_func = new_func.replace('} else if (strstr(function, "padMode") || strstr(function, "PadMode")) {', '  break;\n  case SCRIPT_ACTION_PAD_MODE:')
new_func = new_func.replace('} else if (strstr(function, "samplerPadPressed")) {', '  break;\n  case SCRIPT_ACTION_SAMPLER_PAD:')
new_func = new_func.replace('} else if (strstr(function, "toggleLoopAdjustIn")) {', '  break;\n  case SCRIPT_ACTION_LOOP_IN_ADJUST:')
new_func = new_func.replace('} else if (strstr(function, "toggleLoopAdjustOut")) {', '  break;\n  case SCRIPT_ACTION_LOOP_OUT_ADJUST:')
new_func = new_func.replace('} else if (strstr(function, "cueLoopCallLeft")) {', '  break;\n  case SCRIPT_ACTION_CUE_LOOP_LEFT:')
new_func = new_func.replace('} else if (strstr(function, "cueLoopCallRight")) {', '  break;\n  case SCRIPT_ACTION_CUE_LOOP_RIGHT:')
new_func = new_func.replace('} else if (strstr(function, "tempoSliderMSB")) {', '  break;\n  case SCRIPT_ACTION_TEMPO_MSB:')
new_func = new_func.replace('} else if (strstr(function, "tempoSliderLSB")) {', '  break;\n  case SCRIPT_ACTION_TEMPO_LSB:')
new_func = new_func.replace('} else if (strstr(function, "cycleTempoRange")) {', '  break;\n  case SCRIPT_ACTION_TEMPO_RANGE:')
new_func = new_func.replace('} else if (strstr(function, "syncPressed") ||\n             strstr(function, "syncLongPressed")) {', '  break;\n  case SCRIPT_ACTION_SYNC:')
new_func = new_func.replace('} else if (strstr(function, "quantizeToggle")) {', '  break;\n  case SCRIPT_ACTION_QUANTIZE:')
new_func = new_func.replace('} else if (strstr(function, "slipToggle")) {', '  break;\n  case SCRIPT_ACTION_SLIP:')
new_func = new_func.replace('} else if (strstr(function, "mergeFxTurn")) {', '  break;\n  case SCRIPT_ACTION_MERGE_FX_TURN:')
new_func = new_func.replace('} else if (strstr(function, "mergeFxPressed")) {', '  break;\n  case SCRIPT_ACTION_MERGE_FX_PRESS:')
new_func = new_func.replace('} else if (strstr(function, "loadSelectedTrack") ||\n             strstr(function, "LoadSelectedTrack")) {', '  break;\n  case SCRIPT_ACTION_LOAD_TRACK:')
new_func = new_func.replace('} else if (strstr(function, "browseClick") ||\n             strstr(function, "browsePush") ||\n             strstr(function, "SelectTrack") ||\n             strstr(function, "DirectoryPush") ||\n             strstr(function, "LibraryPush") || strstr(function, "knobClick")) {', '  break;\n  case SCRIPT_ACTION_BROWSE_CLICK:')
new_func = new_func.replace('} else if (strstr(function, "browseToggle")) {', '  break;\n  case SCRIPT_ACTION_BROWSE_TOGGLE:')
new_func = new_func.replace('} else if (strstr(function, "headMix") || strstr(function, "headphone_mix") ||\n             strstr(function, "headMixRotate")) {', '  break;\n  case SCRIPT_ACTION_HEAD_MIX:')
new_func = new_func.replace('} else if (strstr(function, "beatjumpPadPressed")) {', '  break;\n  case SCRIPT_ACTION_BEATJUMP_PAD:')
new_func = new_func.replace('} else if (strstr(function, "decreaseBeatjumpSizes")) {', '  break;\n  case SCRIPT_ACTION_BEATJUMP_DEC:')
new_func = new_func.replace('} else if (strstr(function, "increaseBeatjumpSizes")) {', '  break;\n  case SCRIPT_ACTION_BEATJUMP_INC:')
new_func = new_func.replace('} else if (strstr(function, "deckControlLPressed")) {', '  break;\n  case SCRIPT_ACTION_DECK_CONTROL_L:')
new_func = new_func.replace('} else if (strstr(function, "deckControlRPressed")) {', '  break;\n  case SCRIPT_ACTION_DECK_CONTROL_R:')
new_func = new_func.replace('} else if (strstr(function, "setGroupKeyValue") ||\n             strstr(function, "keyboardButtonPressed")) {', '  break;\n  case SCRIPT_ACTION_KEYBOARD_BTN:')
new_func = new_func.replace('} else if (strstr(function, "MoveVertical") ||\n             strstr(function, "scrollTrack")) {', '  break;\n  case SCRIPT_ACTION_BROWSE_SCROLL:')

# Close the switch block
new_func = new_func.rstrip() + "\n  break;\n  default: break;\n  }\n}\n"

with open('src/core/midi/midi_scripts.c', 'w') as f:
    f.write(content.replace(old_func, new_func))

print("Refactored midi_scripts.c successfully.")
