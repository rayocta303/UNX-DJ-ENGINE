const fs = require('fs');
let content = fs.readFileSync('src/core/midi/midi_scripts.c', 'utf8');

// Normalize to LF for easy regex
content = content.replace(/\r\n/g, '\n');

let oldProtoRegex = /void MIDI_ExecuteScript\(MidiMapping \*map, const char \*function, uint8_t status,\n\s*uint8_t midino, uint8_t value\) \{/;
let newProto = 'void MIDI_ExecuteScript(MidiMapping *map, int actionId, uint8_t status,\n                        uint8_t midino, uint8_t value) {';
content = content.replace(oldProtoRegex, newProto);

let searchMap = [
    [/if\s*\(\s*strstr\(\s*function,\s*"shiftButton"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"shiftPressed"\s*\)\s*\)\s*\{/, 'switch(actionId) {\n  case SCRIPT_ACTION_SHIFT:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"jogTurn"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"jogSearch"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_JOG_TURN:\n  case SCRIPT_ACTION_JOG_SEARCH:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"jogTouch"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"JogTouch"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_JOG_TOUCH:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"beatTap"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"beatFxTap"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BEAT_TAP:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"beatFxSelect"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"beatFxNext"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BEATFX_NEXT:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"beatFxPrev"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BEATFX_PREV:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"beatFxLevelDepth"\s*\)\s*\|\|[\s\S]*?midino == 0x0F\)\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BEATFX_DEPTH:\n    if (1) {'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"fxEnabled"\s*\)\s*\|\|[\s\S]*?midino == 0x47\)\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BEATFX_TOGGLE:\n    if (1) {'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"padMode"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"PadMode"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_PAD_MODE:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"samplerPadPressed"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_SAMPLER_PAD:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"toggleLoopAdjustIn"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_LOOP_IN_ADJUST:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"toggleLoopAdjustOut"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_LOOP_OUT_ADJUST:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"cueLoopCallLeft"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_CUE_LOOP_LEFT:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"cueLoopCallRight"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_CUE_LOOP_RIGHT:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"tempoSliderMSB"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_TEMPO_MSB:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"tempoSliderLSB"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_TEMPO_LSB:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"cycleTempoRange"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_TEMPO_RANGE:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"syncPressed"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"syncLongPressed"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_SYNC:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"quantizeToggle"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_QUANTIZE:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"slipToggle"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_SLIP:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"mergeFxTurn"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_MERGE_FX_TURN:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"mergeFxPressed"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_MERGE_FX_PRESS:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"loadSelectedTrack"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"LoadSelectedTrack"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_LOAD_TRACK:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"browseClick"\s*\)\s*\|\|[\s\S]*?strstr\(\s*function,\s*"knobClick"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BROWSE_CLICK:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"browseToggle"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BROWSE_TOGGLE:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"headMix"\s*\)\s*\|\|[\s\S]*?strstr\(\s*function,\s*"headMixRotate"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_HEAD_MIX:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"beatjumpPadPressed"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BEATJUMP_PAD:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"decreaseBeatjumpSizes"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BEATJUMP_DEC:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"increaseBeatjumpSizes"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BEATJUMP_INC:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"deckControlLPressed"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_DECK_CONTROL_L:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"deckControlRPressed"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_DECK_CONTROL_R:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"setGroupKeyValue"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"keyboardButtonPressed"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_KEYBOARD_BTN:'],
    [/\}\s*else if\s*\(\s*strstr\(\s*function,\s*"MoveVertical"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"scrollTrack"\s*\)\s*\)\s*\{/, '  break;\n  case SCRIPT_ACTION_BROWSE_SCROLL:'],
    
    // Fix inner strings in padMode
    [/\(\s*strstr\(\s*function,\s*"jogSearch"\s*\)\s*!=\s*NULL\s*\)/, '(actionId == SCRIPT_ACTION_JOG_SEARCH)'],
    [/if\s*\(\s*strstr\(\s*function,\s*"HotCue"\s*\)\s*\|\|\s*strstr\(\s*function,\s*"hotcueMode"\s*\)\s*\|\|/, 'if (1 /*hotcue*/ ||'],
    [/else if\s*\(\s*strstr\(\s*function,\s*"BeatLoop"\s*\)\s*\|\|[\s\S]*?strstr\(\s*function,\s*"beatLoopMode"\s*\)\s*\|\|/, 'else if (1 /*beatloop*/ ||'],
    [/else if\s*\(\s*strstr\(\s*function,\s*"PadFX"\s*\)\s*\|\|[\s\S]*?strstr\(\s*function,\s*"SlipLoop"\s*\)\s*\|\|/, 'else if (1 /*padfx*/ ||'],
    [/else if\s*\(\s*strstr\(\s*function,\s*"BeatJump"\s*\)\s*\|\|[\s\S]*?strstr\(\s*function,\s*"beatJumpMode"\s*\)\s*\|\|/, 'else if (1 /*beatjump*/ ||'],
    [/else if\s*\(\s*strstr\(\s*function,\s*"Sampler"\s*\)\s*\|\|[\s\S]*?strstr\(\s*function,\s*"GateCue"\s*\)\s*\|\|/, 'else if (1 /*sampler*/ ||'],
    [/else if\s*\(\s*strstr\(\s*function,\s*"ReleaseFX"\s*\)\s*\|\|[\s\S]*?strstr\(\s*function,\s*"keyboardMode"\s*\)\s*\|\|/, 'else if (1 /*releasefx*/ ||']
];

for (let [search, replace] of searchMap) {
    let before = content;
    content = content.replace(search, replace);
    if (before === content) {
        console.warn("Could not find regex: " + search);
    }
}

// Ensure the final block closes the switch
let findEnd = '    CO_AddValue("[Library]", "scroll", diff);\n  }\n}';
let replaceEnd = '    CO_AddValue("[Library]", "scroll", diff);\n  break;\n  default: break;\n  }\n}';
if (content.includes(findEnd)) {
    content = content.replace(findEnd, replaceEnd);
} else {
    console.warn("Could not find end block.");
}

// Convert back to original line endings if needed (let git handle it usually, but we keep LF)
fs.writeFileSync('src/core/midi/midi_scripts.c', content);
console.log('Refactored midi_scripts.c using regex successfully.');
