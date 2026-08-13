const fs = require('fs');
let content = fs.readFileSync('src/core/midi/midi_scripts.c', 'utf8');

let oldProto = 'void MIDI_ExecuteScript(MidiMapping *map, const char *function, uint8_t status,\n                        uint8_t midino, uint8_t value) {';
let newProto = 'void MIDI_ExecuteScript(MidiMapping *map, int actionId, uint8_t status,\n                        uint8_t midino, uint8_t value) {';

if (content.includes(oldProto)) {
    content = content.replace(oldProto, newProto);
}

let searchMap = [
    ['if (strstr(function, "shiftButton") || strstr(function, "shiftPressed")) {', 'switch(actionId) {\n  case SCRIPT_ACTION_SHIFT:'],
    ['} else if (strstr(function, "jogTurn") || strstr(function, "jogSearch")) {', '  break;\n  case SCRIPT_ACTION_JOG_TURN:\n  case SCRIPT_ACTION_JOG_SEARCH:'],
    ['} else if (strstr(function, "jogTouch") || strstr(function, "JogTouch")) {', '  break;\n  case SCRIPT_ACTION_JOG_TOUCH:'],
    ['} else if (strstr(function, "beatTap") || strstr(function, "beatFxTap")) {', '  break;\n  case SCRIPT_ACTION_BEAT_TAP:'],
    ['} else if (strstr(function, "beatFxSelect") ||\n             strstr(function, "beatFxNext")) {', '  break;\n  case SCRIPT_ACTION_BEATFX_NEXT:'],
    ['} else if (strstr(function, "beatFxPrev")) {', '  break;\n  case SCRIPT_ACTION_BEATFX_PREV:'],
    ['} else if (strstr(function, "beatFxLevelDepth") ||\n             strstr(function, "beatFxDepth") ||\n             strstr(function, "drywet") ||\n             strstr(function, "meta") ||\n             strstr(function, "super1") ||', '  break;\n  case SCRIPT_ACTION_BEATFX_DEPTH:\n    if (1 ||'],
    ['} else if (strstr(function, "fxEnabled") ||\n             strstr(function, "beatFxOnOffPressed") ||\n             strstr(function, "beatFxOnOff") ||\n             strstr(function, "super1_toggle") ||', '  break;\n  case SCRIPT_ACTION_BEATFX_TOGGLE:\n    if (1 ||'],
    ['} else if (strstr(function, "padMode") || strstr(function, "PadMode")) {', '  break;\n  case SCRIPT_ACTION_PAD_MODE:'],
    ['} else if (strstr(function, "samplerPadPressed")) {', '  break;\n  case SCRIPT_ACTION_SAMPLER_PAD:'],
    ['} else if (strstr(function, "toggleLoopAdjustIn")) {', '  break;\n  case SCRIPT_ACTION_LOOP_IN_ADJUST:'],
    ['} else if (strstr(function, "toggleLoopAdjustOut")) {', '  break;\n  case SCRIPT_ACTION_LOOP_OUT_ADJUST:'],
    ['} else if (strstr(function, "cueLoopCallLeft")) {', '  break;\n  case SCRIPT_ACTION_CUE_LOOP_LEFT:'],
    ['} else if (strstr(function, "cueLoopCallRight")) {', '  break;\n  case SCRIPT_ACTION_CUE_LOOP_RIGHT:'],
    ['} else if (strstr(function, "tempoSliderMSB")) {', '  break;\n  case SCRIPT_ACTION_TEMPO_MSB:'],
    ['} else if (strstr(function, "tempoSliderLSB")) {', '  break;\n  case SCRIPT_ACTION_TEMPO_LSB:'],
    ['} else if (strstr(function, "cycleTempoRange")) {', '  break;\n  case SCRIPT_ACTION_TEMPO_RANGE:'],
    ['} else if (strstr(function, "syncPressed") ||\n             strstr(function, "syncLongPressed")) {', '  break;\n  case SCRIPT_ACTION_SYNC:'],
    ['} else if (strstr(function, "quantizeToggle")) {', '  break;\n  case SCRIPT_ACTION_QUANTIZE:'],
    ['} else if (strstr(function, "slipToggle")) {', '  break;\n  case SCRIPT_ACTION_SLIP:'],
    ['} else if (strstr(function, "mergeFxTurn")) {', '  break;\n  case SCRIPT_ACTION_MERGE_FX_TURN:'],
    ['} else if (strstr(function, "mergeFxPressed")) {', '  break;\n  case SCRIPT_ACTION_MERGE_FX_PRESS:'],
    ['} else if (strstr(function, "loadSelectedTrack") ||\n             strstr(function, "LoadSelectedTrack")) {', '  break;\n  case SCRIPT_ACTION_LOAD_TRACK:'],
    ['} else if (strstr(function, "browseClick") ||\n             strstr(function, "browsePush") ||\n             strstr(function, "SelectTrack") ||\n             strstr(function, "DirectoryPush") ||\n             strstr(function, "LibraryPush") || strstr(function, "knobClick")) {', '  break;\n  case SCRIPT_ACTION_BROWSE_CLICK:'],
    ['} else if (strstr(function, "browseToggle")) {', '  break;\n  case SCRIPT_ACTION_BROWSE_TOGGLE:'],
    ['} else if (strstr(function, "headMix") || strstr(function, "headphone_mix") ||\n             strstr(function, "headMixRotate")) {', '  break;\n  case SCRIPT_ACTION_HEAD_MIX:'],
    ['} else if (strstr(function, "beatjumpPadPressed")) {', '  break;\n  case SCRIPT_ACTION_BEATJUMP_PAD:'],
    ['} else if (strstr(function, "decreaseBeatjumpSizes")) {', '  break;\n  case SCRIPT_ACTION_BEATJUMP_DEC:'],
    ['} else if (strstr(function, "increaseBeatjumpSizes")) {', '  break;\n  case SCRIPT_ACTION_BEATJUMP_INC:'],
    ['} else if (strstr(function, "deckControlLPressed")) {', '  break;\n  case SCRIPT_ACTION_DECK_CONTROL_L:'],
    ['} else if (strstr(function, "deckControlRPressed")) {', '  break;\n  case SCRIPT_ACTION_DECK_CONTROL_R:'],
    ['} else if (strstr(function, "setGroupKeyValue") ||\n             strstr(function, "keyboardButtonPressed")) {', '  break;\n  case SCRIPT_ACTION_KEYBOARD_BTN:'],
    ['} else if (strstr(function, "MoveVertical") ||\n             strstr(function, "scrollTrack")) {', '  break;\n  case SCRIPT_ACTION_BROWSE_SCROLL:'],
    ['(strstr(function, "jogSearch") != NULL)', '(actionId == SCRIPT_ACTION_JOG_SEARCH)'],
    ['if (strstr(function, "HotCue") || strstr(function, "hotcueMode") ||', 'if (actionId == SCRIPT_ACTION_PAD_MODE ||'],
    ['strstr(function, "HotCue") || strstr(function, "hotcueMode") ||', '1 ||'],
    ['strstr(function, "BeatLoop") ||\n               strstr(function, "beatLoopMode") ||', '0 ||'],
    ['strstr(function, "PadFX") || strstr(function, "padFX") ||\n               strstr(function, "SlipLoop") ||', '0 ||'],
    ['strstr(function, "BeatJump") ||\n               strstr(function, "beatJumpMode") ||', '0 ||'],
    ['strstr(function, "Sampler") || strstr(function, "samplerMode") ||\n               strstr(function, "GateCue") ||', '0 ||'],
    ['strstr(function, "ReleaseFX") ||\n               strstr(function, "keyShiftMode") ||\n               strstr(function, "keyboardMode") ||', '0 ||']
];

for (let [search, replace] of searchMap) {
    if (content.includes(search)) {
        content = content.replace(search, replace);
    } else {
        console.warn("Could not find: " + search);
    }
}

// Ensure the final block closes the switch
content = content.replace('    CO_AddValue("[Library]", "scroll", diff);\n  }\n}', '    CO_AddValue("[Library]", "scroll", diff);\n  break;\n  default: break;\n  }\n}');

fs.writeFileSync('src/core/midi/midi_scripts.c', content);
console.log('Refactored midi_scripts.c using Node.js successfully.');
