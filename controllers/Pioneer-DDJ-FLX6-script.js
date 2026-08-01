// Pioneer-DDJ-FLX6-script.js
// ****************************************************************************
// * Mixxx mapping script file for the Pioneer DDJ-FLX6.
// * Mostly adapted from the DDJ-FLX4 mapping script
// * Authors: Warker, nschloe, dj3730, jusko, Robert904, jaimearu, fixxJ
// ****************************************************************************
//
//  Implemented (as per manufacturer's manual):
//      * Mixer Section (Faders, EQ, Filter, Gain, Cue)
//      * Browsing and loading + Waveform zoom (shift)
//      * Jogwheels, Scratching, Bending, Loop adjust
//      * Cycle Temporange
//      * Beat Sync
//      * Hot Cue Mode
//      * Beat Loop Mode
//      * Beat Jump Mode
//      * Sampler Mode
//      * Merge Fx
//      * Pad Fx
//      * Keyboard Mode
//
//  Custom (Mixxx specific mappings):
//      * BeatFX: Assigned Effect Unit 1
//                v FX_SELECT Load next effect.
//                SHIFT + v FX_SELECT Load previous effect.
//                < LEFT Cycle effect focus leftward
//                > RIGHT Cycle effect focus rightward
//                ON/OFF toggles focused effect slot
//                SHIFT + ON/OFF disables all three effect slots.
//
//      * 32 beat jump forward & back (Shift + </> CUE/LOOP CALL arrows)
//      * Toggle quantize (Shift + channel cue)
//
//  Not implemented (after discussion and trial attempts):
//      * Loop Section:
//        * -4BEAT auto loop (hacky---prefer a clean way to set a 4 beat loop
//                            from a previous position on long press)
//
//        * CUE/LOOP CALL - memory & delete (complex and not useful. Hot cues are sufficient)
//
//      * Secondary pad modes (trial attempts complex and too experimental)
//
//  Not implemented yet (but might be in the future):
//      * Sample Scratch Mode

var PioneerDDJFLX6 = {};

PioneerDDJFLX6.lights = {
    beatFx: {
        status: 0x94,
        data1: 0x47,
    },
    shiftBeatFx: {
        status: 0x94,
        data1: 0x43,
    },
    deck1: {
        vuMeter: {
            status: 0xB0,
            data1: 0x02,
        },
        playPause: {
            status: 0x90,
            data1: 0x0B,
        },
        shiftPlayPause: {
            status: 0x90,
            data1: 0x47,
        },
        cue: {
            status: 0x90,
            data1: 0x0C,
        },
        shiftCue: {
            status: 0x90,
            data1: 0x48,
        },
        hotcueMode: {
            status: 0x90,
            data1: 0x1B,
        },
        keyboardMode: {
            status: 0x90,
            data1: 0x69,
        },
        padFX1Mode: {
            status: 0x90,
            data1: 0x1E,
        },
        padFX2Mode: {
            status: 0x90,
            data1: 0x6B,
        },
        beatJumpMode: {
            status: 0x90,
            data1: 0x20,
        },
        beatLoopMode: {
            status: 0x90,
            data1: 0x6D,
        },
        samplerMode: {
            status: 0x90,
            data1: 0x22,
        },
        keyShiftMode: {
            status: 0x90,
            data1: 0x6F,
        },
    },
    deck2: {
        vuMeter: {
            status: 0xB0,
            data1: 0x02,
        },
        playPause: {
            status: 0x91,
            data1: 0x0B,
        },
        shiftPlayPause: {
            status: 0x91,
            data1: 0x47,
        },
        cue: {
            status: 0x91,
            data1: 0x0C,
        },
        shiftCue: {
            status: 0x91,
            data1: 0x48,
        },
        hotcueMode: {
            status: 0x91,
            data1: 0x1B,
        },
        keyboardMode: {
            status: 0x91,
            data1: 0x69,
        },
        padFX1Mode: {
            status: 0x91,
            data1: 0x1E,
        },
        padFX2Mode: {
            status: 0x91,
            data1: 0x6B,
        },
        beatJumpMode: {
            status: 0x91,
            data1: 0x20,
        },
        beatLoopMode: {
            status: 0x91,
            data1: 0x6D,
        },
        samplerMode: {
            status: 0x91,
            data1: 0x22,
        },
        keyShiftMode: {
            status: 0x91,
            data1: 0x6F,
        },
    },
	deck3: {
        vuMeter: {
            status: 0xB2,
            data1: 0x02,
        },
        playPause: {
            status: 0xB2,
            data1: 0x0B,
        },
        shiftPlayPause: {
            status: 0xB2,
            data1: 0x47,
        },
        cue: {
            status: 0xB2,
            data1: 0x0C,
        },
        shiftCue: {
            status: 0xB2,
            data1: 0x48,
        },
        hotcueMode: {
            status: 0xB2,
            data1: 0x1B,
        },
        keyboardMode: {
            status: 0xB2,
            data1: 0x69,
        },
        padFX1Mode: {
            status: 0xB2,
            data1: 0x1E,
        },
        padFX2Mode: {
            status: 0xB2,
            data1: 0x6B,
        },
        beatJumpMode: {
            status: 0xB2,
            data1: 0x20,
        },
        beatLoopMode: {
            status: 0xB2,
            data1: 0x6D,
        },
        samplerMode: {
            status: 0xB2,
            data1: 0x22,
        },
        keyShiftMode: {
            status: 0xB2,
            data1: 0x6F,
        },
    },
    deck4: {
        vuMeter: {
            status: 0xB3,
            data1: 0x02,
        },
        playPause: {
            status: 0xB3,
            data1: 0x0B,
        },
        shiftPlayPause: {
            status: 0xB3,
            data1: 0x47,
        },
        cue: {
            status: 0xB3,
            data1: 0x0C,
        },
        shiftCue: {
            status: 0xB3,
            data1: 0x48,
        },
        hotcueMode: {
            status: 0xB3,
            data1: 0x1B,
        },
        keyboardMode: {
            status: 0xB3,
            data1: 0x69,
        },
        padFX1Mode: {
            status: 0xB3,
            data1: 0x1E,
        },
        padFX2Mode: {
            status: 0xB3,
            data1: 0x6B,
        },
        beatJumpMode: {
            status: 0xB3,
            data1: 0x20,
        },
        beatLoopMode: {
            status: 0xB3,
            data1: 0x6D,
        },
        samplerMode: {
            status: 0xB3,
            data1: 0x22,
        },
        keyShiftMode: {
            status: 0xB3,
            data1: 0x6F,
        },
    },
};

// Store timer IDs
PioneerDDJFLX6.timers = {};

// Keep alive timer
PioneerDDJFLX6.sendKeepAlive = function() {
    midi.sendSysexMsg([0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x04, 0x05, 0x00, 0x50, 0x02, 0xf7], 12); // This was reverse engineered with Wireshark
};

// Jog wheel constants
PioneerDDJFLX6.vinylMode = true;
PioneerDDJFLX6.alpha = 1.0/8;
PioneerDDJFLX6.beta = PioneerDDJFLX6.alpha/32;

// Multiplier for fast seek through track using SHIFT+JOGWHEEL
PioneerDDJFLX6.fastSeekScale = 150;
PioneerDDJFLX6.bendScale = 0.4;
PioneerDDJFLX6.bendScaleMergeFxKnop = 0.2;

PioneerDDJFLX6.tempoRanges = [0.06, 0.10, 0.16, 0.25];

PioneerDDJFLX6.shiftButtonDown = [false, false,false,false];

// Jog wheel loop adjust
PioneerDDJFLX6.loopAdjustIn = [false, false,false,false];
PioneerDDJFLX6.loopAdjustOut = [false, false,false,false];
PioneerDDJFLX6.loopAdjustMultiply = 50;

// Beatjump pad (beatjump_size values)
PioneerDDJFLX6.beatjumpSizeForPad = {
    0x20: -1, // PAD 1
    0x21: 1,  // PAD 2
    0x22: -2, // PAD 3
    0x23: 2,  // PAD 4
    0x24: -4, // PAD 5
    0x25: 4,  // PAD 6
    0x26: -8, // PAD 7
    0x27: 8   // PAD 8
};

PioneerDDJFLX6.quickJumpSize = 32;

// Used for tempo slider
PioneerDDJFLX6.highResMSB = {
    "[Channel1]": {},
    "[Channel2]": {},
	"[Channel3]": {},
    "[Channel4]": {}
};

PioneerDDJFLX6.lastRotation = {
    "[Channel1]": 0,
    "[Channel2]": 0,
	"[Channel3]": 0,
    "[Channel4]": 0
};

PioneerDDJFLX6.mergeFxEnabled = {
    "[Channel1]": false,
    "[Channel2]": false,
	"[Channel3]": false,
    "[Channel4]": false
};

PioneerDDJFLX6.mergeFxBeforeEnabled = {
    "[Channel1]": false,
    "[Channel2]": false,
	"[Channel3]": false,
    "[Channel4]": false
};

PioneerDDJFLX6.mergeFxBeforeValue = {
    "[Channel1]": 0.5,
    "[Channel2]": 0.5,
	"[Channel3]": 0.5,
    "[Channel4]": 0.5
};

PioneerDDJFLX6.mergeFxbeforeLoadedChainPreset = {
    "[Channel1]": 0,
    "[Channel2]": 0,
	"[Channel3]": 0,
    "[Channel4]": 0
};

PioneerDDJFLX6.mergeFxChainPreset = {
    "L": 1,
    "R": 1
};


PioneerDDJFLX6.deckControlL = "[Channel1]";
PioneerDDJFLX6.deckControlR = "[Channel2]";

PioneerDDJFLX6.fxSelect = "";

PioneerDDJFLX6.trackLoadedLED = function(value, group, _control) {
    midi.sendShortMsg(
        0x9F,
        group.match(script.channelRegEx)[1] - 1,
        value > 0 ? 0x7F : 0x00
    );
};

PioneerDDJFLX6.toggleLight = function(midiIn, active) {
    midi.sendShortMsg(midiIn.status, midiIn.data1, active ? 0x7F : 0);
};

//
// Init
//

PioneerDDJFLX6.init = function() {
    engine.setValue("[EffectRack1_EffectUnit1]", "show_focus", 1);

    engine.makeConnection("[Channel1]", "vu_meter", PioneerDDJFLX6.vuMeterUpdate);
    engine.makeConnection("[Channel2]", "vu_meter", PioneerDDJFLX6.vuMeterUpdate);
	engine.makeConnection("[Channel3]", "vu_meter", PioneerDDJFLX6.vuMeterUpdate);
    engine.makeConnection("[Channel4]", "vu_meter", PioneerDDJFLX6.vuMeterUpdate);

    engine.makeConnection("[Channel1]", "playposition", PioneerDDJFLX6.playPositionUpdate);
    engine.makeConnection("[Channel2]", "playposition", PioneerDDJFLX6.playPositionUpdate);
	engine.makeConnection("[Channel3]", "playposition", PioneerDDJFLX6.playPositionUpdate);
    engine.makeConnection("[Channel4]", "playposition", PioneerDDJFLX6.playPositionUpdate);

    //

    PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.deck1.vuMeter, false);
    PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.deck2.vuMeter, false);
	PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.deck3.vuMeter, false);
    PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.deck4.vuMeter, false);

    engine.softTakeover("[Channel1]", "rate", true);
    engine.softTakeover("[Channel2]", "rate", true);
	engine.softTakeover("[Channel3]", "rate", true);
    engine.softTakeover("[Channel4]", "rate", true);
    engine.softTakeover("[EffectRack1_EffectUnit1_Effect1]", "meta", true);
    engine.softTakeover("[EffectRack1_EffectUnit1_Effect2]", "meta", true);
    engine.softTakeover("[EffectRack1_EffectUnit1_Effect3]", "meta", true);
    engine.softTakeover("[EffectRack1_EffectUnit1]", "mix", true);

    const samplerCount = 16;
    if (engine.getValue("[App]", "num_samplers") < samplerCount) {
        engine.setValue("[App]", "num_samplers", samplerCount);
    }
    for (let i = 1; i <= samplerCount; ++i) {
        engine.makeConnection("[Sampler" + i + "]", "play", PioneerDDJFLX6.samplerPlayOutputCallbackFunction);
    }

    engine.makeConnection("[Channel1]", "track_loaded", PioneerDDJFLX6.trackLoadedLED);
    engine.makeConnection("[Channel2]", "track_loaded", PioneerDDJFLX6.trackLoadedLED);
	engine.makeConnection("[Channel3]", "track_loaded", PioneerDDJFLX6.trackLoadedLED);
    engine.makeConnection("[Channel4]", "track_loaded", PioneerDDJFLX6.trackLoadedLED);


    // play the "track loaded" animation on both decks at startup
    midi.sendShortMsg(0x9F, 0x00, 0x7F);
    midi.sendShortMsg(0x9F, 0x01, 0x7F);

    PioneerDDJFLX6.setLoopButtonLights(0x90, 0x7F);
    PioneerDDJFLX6.setLoopButtonLights(0x91, 0x7F);
	PioneerDDJFLX6.setLoopButtonLights(0x93, 0x7F);
    PioneerDDJFLX6.setLoopButtonLights(0x94, 0x7F);

    engine.makeConnection("[Channel1]", "loop_enabled", PioneerDDJFLX6.loopToggle);
    engine.makeConnection("[Channel2]", "loop_enabled", PioneerDDJFLX6.loopToggle);
	engine.makeConnection("[Channel3]", "loop_enabled", PioneerDDJFLX6.loopToggle);
    engine.makeConnection("[Channel4]", "loop_enabled", PioneerDDJFLX6.loopToggle);

    
    for (i = 1; i <= 3; i++) {
        engine.makeConnection("[EffectRack1_EffectUnit1_Effect" + i +"]", "enabled", PioneerDDJFLX6.toggleFxLight);
        engine.makeConnection("[EffectRack1_EffectUnit2_Effect" + i +"]", "enabled", PioneerDDJFLX6.toggleFxLight);
    }
    
    //engine.makeConnection("[EffectRack1_EffectUnit1]", "focused_effect", PioneerDDJFLX6.toggleFxLight);
    

    //PioneerDDJFLX6.keepAliveTimer = engine.beginTimer(200, PioneerDDJFLX6.sendKeepAlive,true);

    // query the controller for current control positions on startup
    var ControllerStatusSysex = [0xF0, 0x00, 0x20, 0x7F, 0x03, 0x01, 0xF7];
    // After midi controller receive this Outbound Message request SysEx Message,
    // midi controller will send the status of every item on the
    // control surface. (Mixxx will be initialized with current values)
    midi.sendSysexMsg(ControllerStatusSysex, ControllerStatusSysex.length);
};

//
// Waveform zoom
//

PioneerDDJFLX6.waveformZoom = function(midichan, control, value, status, group) {
    if (value === 0x7f) {
        script.triggerControl(group, "waveform_zoom_up", 100);
    } else {
        script.triggerControl(group, "waveform_zoom_down", 100);
    }
};

//
// Channel level lights
//

PioneerDDJFLX6.vuMeterUpdate = function(value, group) {
    const newVal = value * 127;

    switch (group) {
    case "[Channel1]":
        midi.sendShortMsg(0xB0, 0x02, newVal);
        break;

    case "[Channel2]":
        midi.sendShortMsg(0xB1, 0x02, newVal);
        break;
	case "[Channel3]":
        midi.sendShortMsg(0xB2, 0x02, newVal);
        break;

    case "[Channel4]":
        midi.sendShortMsg(0xB3, 0x02, newVal);
        break;
    }
};

PioneerDDJFLX6.fxSelected = function(_channel, _control, value, status, group){
    console.log("fxSelected group: "+group+" value: "+value);
    if(value === 0x7f)
    {
        var oldValue = 0;
        if(PioneerDDJFLX6.fxSelect != "")
        {
            oldValue = engine.getValue(PioneerDDJFLX6.fxSelect,"enabled");
            engine.setValue(PioneerDDJFLX6.fxSelect,"enabled",0);
            
        }
        PioneerDDJFLX6.fxSelect = group;
        engine.setValue(group,"enabled",oldValue);
    }
    else
    {
        //engine.setValue(group,"mix",0);
        //engine.setValue(group,"enabled", 0);
    }
};

PioneerDDJFLX6.fxEnabled= function(_channel, _control, value, status, group){
    console.log("fxEnabled group: "+group+" value: "+value);
    var effects = [
        "[EffectRack1_EffectUnit1_Effect1]",
        "[EffectRack1_EffectUnit1_Effect2]",
        "[EffectRack1_EffectUnit1_Effect3]",
        "[EffectRack1_EffectUnit2_Effect1]",
        "[EffectRack1_EffectUnit2_Effect2]",
        "[EffectRack1_EffectUnit2_Effect3]"
    ];

    var isAnEffectEnabled = false;
    for(let i = 0; i<effects.length;i++)
    {
        if(engine.getValue(effects[i],"enabled") > 0)
        {
            isAnEffectEnabled = true;
            break;
        }
    }

    if(value === 0x7f)
    {
        if(isAnEffectEnabled)
        {
            for(let i = 0; i<effects.length;i++)
            {
                if(engine.getValue(effects[i],"enabled") > 0)
                    engine.setValue(effects[i],"enabled",0);
            }
            //PioneerDDJFLX6.fxSelect = "";
        }
        else
        {
            PioneerDDJFLX6.fxSelect = group;

            engine.setValue(group,"enabled",1);
            
        }
    }

};

PioneerDDJFLX6.keyboardButtonPressed = function(_channel, _control, value, _status, group){
    if(value == 0)
        return;
    const groupPitch = group.split(";");
    if(groupPitch.length < 2)
        return;
    engine.setValue(groupPitch[0], "cue_goto", 1);
    engine.setValue(groupPitch[0], "pitch", parseFloat(groupPitch[1]));
};

PioneerDDJFLX6.setGroupKey = function(_channel, _control, value, _status, group){
    var groupKey = group.split(";");
    if (groupKey.length < 2)
        return;
    engine.setValue(groupKey[0],groupKey[1],value > 0);
};

PioneerDDJFLX6.setGroupKeyValue = function(_channel, _control, _value, _status, group){
    var groupKeyValue = group.split(";");
    if (groupKeyValue.length < 3)
        return;
    engine.setValue(groupKeyValue[0],groupKeyValue[1],parseFloat(groupKeyValue[2]));
};

PioneerDDJFLX6.playPositionUpdate = function(value, group){
    var duration = engine.getValue(group,"duration");
    //duration = 60 * 2;
    var newVal = (value*duration*0x48*0.6075) % 0x48;
    newVal = newVal < 0 ? newVal + 0x48 : newVal;
    newVal += 1;

    var oldVal = PioneerDDJFLX6.lastRotation[group];
    if(oldVal == newVal)
        return;
    PioneerDDJFLX6.lastRotation[group] = newVal;

    switch (group) {
    case "[Channel1]":
        midi.sendShortMsg(0xBB, 0x00, newVal);
        break;

    case "[Channel2]":
        midi.sendShortMsg(0xBB, 0x01, newVal);
        break;
	case "[Channel3]":
        midi.sendShortMsg(0xBB, 0x02, newVal);
        break;

    case "[Channel4]":
        midi.sendShortMsg(0xBB, 0x03, newVal);
        break;
    }
};

//
// Effects
//

PioneerDDJFLX6.toggleFxLight = function(value, group, _control) {
    //const enabled = engine.getValue(PioneerDDJFLX6.focusedFxGroup(), "enabled");
    console.log("toggleFxLight group: "+group+" fxSelect: "+PioneerDDJFLX6.fxSelect);
    var lightData = null;
    switch(group)
    {
        case "[EffectRack1_EffectUnit1_Effect1]":
            lightData = {status: 0x94, data1: 0x47};
            break;
        case "[EffectRack1_EffectUnit1_Effect2]":
            lightData = {status: 0x94, data1: 0x48};
            break;
        case "[EffectRack1_EffectUnit1_Effect3]":
            lightData = {status: 0x94, data1: 0x49};
            break;
        case "[EffectRack1_EffectUnit2_Effect1]":
            lightData = {status: 0x95, data1: 0x47};
            break;
        case "[EffectRack1_EffectUnit2_Effect2]":
            lightData = {status: 0x95, data1: 0x48};
            break;
        case "[EffectRack1_EffectUnit2_Effect3]":
            lightData = {status: 0x95, data1: 0x49};
            break;
    }
    if(lightData == null)
        return; 
    var enabled = value >= 0.5;
    PioneerDDJFLX6.toggleLight(lightData, enabled);
    //PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.shiftBeatFx, enabled);
};

PioneerDDJFLX6.focusedFxGroup = function() {
    const focusedFx = engine.getValue("[EffectRack1_EffectUnit1]", "focused_effect");
    return "[EffectRack1_EffectUnit1_Effect" + focusedFx + "]";
};

PioneerDDJFLX6.beatFxLevelDepthRotate = function(_channel, _control, value) {
    if (PioneerDDJFLX6.shiftButtonDown[0] || PioneerDDJFLX6.shiftButtonDown[1] || PioneerDDJFLX6.shiftButtonDown[2] || PioneerDDJFLX6.shiftButtonDown[3]) {
        engine.softTakeoverIgnoreNextValue("[EffectRack1_EffectUnit1]", "mix");
        engine.setParameter(PioneerDDJFLX6.focusedFxGroup(), "meta", value / 0x7F);
    } else {
        engine.softTakeoverIgnoreNextValue(PioneerDDJFLX6.focusedFxGroup(), "meta");
        engine.setParameter("[EffectRack1_EffectUnit1]", "mix", value / 0x7F);
    }
};

PioneerDDJFLX6.changeFocusedEffectBy = function(numberOfSteps) {
    let focusedEffect = engine.getValue("[EffectRack1_EffectUnit1]", "focused_effect");

    // Convert to zero-based index
    focusedEffect -= 1;

    // Standard Euclidean modulo by use of two plain modulos
    const numberOfEffectsPerEffectUnit = 3;
    focusedEffect = (((focusedEffect + numberOfSteps) % numberOfEffectsPerEffectUnit) + numberOfEffectsPerEffectUnit) % numberOfEffectsPerEffectUnit;

    // Convert back to one-based index
    focusedEffect += 1;

    engine.setValue("[EffectRack1_EffectUnit1]", "focused_effect", focusedEffect);
};

PioneerDDJFLX6.beatFxSelectPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    engine.setValue(PioneerDDJFLX6.focusedFxGroup(), "next_effect", value);
};

PioneerDDJFLX6.beatFxSelectShiftPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    engine.setValue(PioneerDDJFLX6.focusedFxGroup(), "prev_effect", value);
};

PioneerDDJFLX6.beatFxLeftPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    //PioneerDDJFLX6.changeFocusedEffectBy(-1);
    if(this.fxSelect == "")
        return;

    engine.setValue(this.fxSelect,"effect_selector",-1);
};

PioneerDDJFLX6.beatFxRightPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    //PioneerDDJFLX6.changeFocusedEffectBy(1);
    if(this.fxSelect == "")
        return;
    console.log("beatFxRightPressed fxSelected: "+this.fxSelect);
    engine.setValue(this.fxSelect,"effect_selector",1);
};

PioneerDDJFLX6.beatFxOnOffPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    const toggleEnabled = !engine.getValue(PioneerDDJFLX6.focusedFxGroup(), "enabled");
    engine.setValue(PioneerDDJFLX6.focusedFxGroup(), "enabled", toggleEnabled);
};

PioneerDDJFLX6.beatFxOnOffShiftPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    engine.setParameter("[EffectRack1_EffectUnit1]", "mix", 0);
    engine.softTakeoverIgnoreNextValue("[EffectRack1_EffectUnit1]", "mix");

    for (let i = 1; i <= 3; i++) {
        engine.setValue("[EffectRack1_EffectUnit1_Effect" + i + "]", "enabled", 0);
    }
    PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.beatFx, false);
    PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.shiftBeatFx, false);
};

PioneerDDJFLX6.beatFxChannel1 = function(_channel, control, value, _status, group) {
    let enableChannel = 0;

    if (value === 0x7f) { enableChannel = 1; }

    engine.setValue(group, "group_[Channel1]_enable", enableChannel);
};

PioneerDDJFLX6.beatFxChannel2 = function(_channel, control, value, _status, group) {
    let enableChannel = 0;

    if (value === 0x7f) { enableChannel = 1; }

    engine.setValue(group, "group_[Channel2]_enable", enableChannel);
};

//
// Loop IN/OUT ADJUST
//

PioneerDDJFLX6.toggleLoopAdjustIn = function(channel, _control, value, _status, group) {
    if (value === 0 || engine.getValue(group, "loop_enabled") === 0) {
        return;
    }
    PioneerDDJFLX6.loopAdjustIn[channel] = !PioneerDDJFLX6.loopAdjustIn[channel];
    PioneerDDJFLX6.loopAdjustOut[channel] = false;
};

PioneerDDJFLX6.toggleLoopAdjustOut = function(channel, _control, value, _status, group) {
    if (value === 0 || engine.getValue(group, "loop_enabled") === 0) {
        return;
    }
    PioneerDDJFLX6.loopAdjustOut[channel] = !PioneerDDJFLX6.loopAdjustOut[channel];
    PioneerDDJFLX6.loopAdjustIn[channel] = false;
};

// Two signals are sent here so that the light stays lit/unlit in its shift state too
PioneerDDJFLX6.setReloopLight = function(status, value) {
    midi.sendShortMsg(status, 0x4D, value);
    midi.sendShortMsg(status, 0x50, value);
};


PioneerDDJFLX6.setLoopButtonLights = function(status, value) {
    [0x10, 0x11, 0x4E, 0x4C].forEach(function(control) {
        midi.sendShortMsg(status, control, value);
    });
};

PioneerDDJFLX6.startLoopLightsBlink = function(channel, control, status, group) {
    let blink = 0x7F;

    PioneerDDJFLX6.stopLoopLightsBlink(group, control, status);

    PioneerDDJFLX6.timers[group][control] = engine.beginTimer(500, () => {
        blink = 0x7F - blink;

        // When adjusting the loop out position, turn the loop in light off
        if (PioneerDDJFLX6.loopAdjustOut[channel]) {
            midi.sendShortMsg(status, 0x10, 0x00);
            midi.sendShortMsg(status, 0x4C, 0x00);
        } else {
            midi.sendShortMsg(status, 0x10, blink);
            midi.sendShortMsg(status, 0x4C, blink);
        }

        // When adjusting the loop in position, turn the loop out light off
        if (PioneerDDJFLX6.loopAdjustIn[channel]) {
            midi.sendShortMsg(status, 0x11, 0x00);
            midi.sendShortMsg(status, 0x4E, 0x00);
        } else {
            midi.sendShortMsg(status, 0x11, blink);
            midi.sendShortMsg(status, 0x4E, blink);
        }
    });

};

PioneerDDJFLX6.stopLoopLightsBlink = function(group, control, status) {
    PioneerDDJFLX6.timers[group] = PioneerDDJFLX6.timers[group] || {};

    if (PioneerDDJFLX6.timers[group][control] !== undefined) {
        engine.stopTimer(PioneerDDJFLX6.timers[group][control]);
    }
    PioneerDDJFLX6.timers[group][control] = undefined;
    PioneerDDJFLX6.setLoopButtonLights(status, 0x7F);
};

PioneerDDJFLX6.loopToggle = function(value, group, control) {
	
    let status;
    let channel;

    switch (group) {
        case "[Channel1]":
            status = 0x90;
            channel = 0;
            break;
        case "[Channel2]":
            status = 0x91;
            channel = 1;
            break;
        case "[Channel3]":
            status = 0x92;
            channel = 2;
            break;
        case "[Channel4]":
            status = 0x93;
            channel = 3;
            break;
        default:
            console.error("Unknown group: " + group);
            return; // Exit if group doesn't match any case
    }

    PioneerDDJFLX6.setReloopLight(status, value ? 0x7F : 0x00);

    if (value) {
        PioneerDDJFLX6.startLoopLightsBlink(channel, control, status, group);
    } else {
        PioneerDDJFLX6.stopLoopLightsBlink(group, control, status);
        PioneerDDJFLX6.loopAdjustIn[channel] = false;
        PioneerDDJFLX6.loopAdjustOut[channel] = false;
    }
};

//
// CUE/LOOP CALL
//

PioneerDDJFLX6.cueLoopCallLeft = function(_channel, _control, value, _status, group) {
    if (value) {
        engine.setValue(group, "loop_scale", 0.5);
    }
};

PioneerDDJFLX6.cueLoopCallRight = function(_channel, _control, value, _status, group) {
    if (value) {
        engine.setValue(group, "loop_scale", 2.0);
    }
};

//
// BEAT SYNC
//
// Note that the controller sends different signals for a short press and a long
// press of the same button.
//

PioneerDDJFLX6.syncPressed = function(channel, control, value, status, group) {
    if (engine.getValue(group, "sync_enabled") && value > 0) {
        engine.setValue(group, "sync_enabled", 0);
    } else {
        engine.setValue(group, "beatsync", value);
    }
};

PioneerDDJFLX6.syncLongPressed = function(channel, control, value, status, group) {
    if (value) {
        engine.setValue(group, "sync_enabled", 1);
    }
};

PioneerDDJFLX6.cycleTempoRange = function(_channel, _control, value, _status, group) {
    if (value === 0) { return; } // ignore release

    const currRange = engine.getValue(group, "rateRange");
    let idx = 0;

    for (let i = 0; i < this.tempoRanges.length; i++) {
        if (currRange === this.tempoRanges[i]) {
            // idx get the index of the value in tempoRanges following the currently configured one
            // or cycle back to 0 if the current is the last value of the list.
            idx = (i + 1) % this.tempoRanges.length;
            break;
        }
    }
    engine.setValue(group, "rateRange", this.tempoRanges[idx]);
};

//
// Jog wheels
//

PioneerDDJFLX6.jogTurn = function(channel, _control, value, _status, group) {
    const deckNum = channel + 1;
    // wheel center at 64; <64 rew >64 fwd
    let newVal = value - 64;

    // loop_in / out adjust
    const loopEnabled = engine.getValue(group, "loop_enabled");
    const minLoopRange = 1024;
    if (loopEnabled > 0) {
        if (PioneerDDJFLX6.loopAdjustIn[channel]) {
            const loopEndPosition = engine.getValue(group, "loop_end_position");
            newVal = newVal * PioneerDDJFLX6.loopAdjustMultiply + engine.getValue(group, "loop_start_position");
            if (newVal > loopEndPosition - minLoopRange)
                newVal = loopEndPosition - minLoopRange;
            engine.setValue(group, "loop_start_position", newVal);
            return;
        }
        if (PioneerDDJFLX6.loopAdjustOut[channel]) {
            const loopStartPosition = engine.getValue(group, "loop_start_position");
            newVal = newVal * PioneerDDJFLX6.loopAdjustMultiply + engine.getValue(group, "loop_end_position");
            if (newVal < loopStartPosition + minLoopRange)
                newVal = loopStartPosition + minLoopRange;
            engine.setValue(group, "loop_end_position", newVal);
            return;
        }
    }

    if (engine.isScratching(deckNum)) {
        engine.scratchTick(deckNum, newVal);
    } else { // fallback
        engine.setValue(group, "jog", newVal * this.bendScale);
    }
};

PioneerDDJFLX6.mergeFxTurn = function(channel, _control, value, _status, group) {
    var diff = 0;
    if(value > 0x41)
    {
        diff = value - 127;
    }
    else
    {
        diff = value;
    }
    diff *= 0.5;
    var newGroup = "";
    switch(group)
    {
        case "L":
            newGroup = PioneerDDJFLX6.deckControlL;
            break;
        case "R":
            newGroup = PioneerDDJFLX6.deckControlR;
            break;
    }
    if(newGroup == "")
        return;

    const mergeFxAsJogwheel = engine.getSetting('mergeFxAsJogwheel');

    if(mergeFxAsJogwheel)
    {
        switch(newGroup)
        {
            case "[Channel1]":
                newGroup = "[Channel3]";
                break;
            case "[Channel3]":
                newGroup = "[Channel1]";
                break;
            case "[Channel2]":
                newGroup = "[Channel4]";
                break;
            case "[Channel4]":
                newGroup = "[Channel2]";
                break;
        }

        engine.setValue(newGroup, "jog", diff * this.bendScaleMergeFxKnop);
    }
    else
    {
        //Use MergeFX knob rotation to rotate quick fx knob
        if(this.mergeFxEnabled[newGroup])
        {
            let superValue = engine.getValue("[QuickEffectRack1_"+newGroup+"]","super1");
            superValue += diff * 0.001;
            if(diff < 0)
                diff = 0;
            if(diff > 1)
                diff = 1;
            engine.setValue("[QuickEffectRack1_"+newGroup+"]","super1",superValue);
        }
    }
};

PioneerDDJFLX6.deckControlLPressed = function(channel, _control, value, _status, group) {
    if(value > 0)
        PioneerDDJFLX6.deckControlL = group;
};

PioneerDDJFLX6.deckControlRPressed = function(channel, _control, value, _status, group) {
    if(value > 0)
        PioneerDDJFLX6.deckControlR = group;
};

PioneerDDJFLX6.mergeEffectButtonPressed = function(channel, _control, value, _status, group) {
    if(value == 0)
        return;
    var newGroup = "";
    switch(group)
    {
        case "L":
            newGroup = PioneerDDJFLX6.deckControlL;
            break;
        case "R":
            newGroup = PioneerDDJFLX6.deckControlR;
            break;
    }
    if(newGroup == "")
        return;

    const mergeFxAsJogwheel = engine.getSetting('mergeFxAsJogwheel');
    if(mergeFxAsJogwheel)
    {
        switch(newGroup)
        {
            case "[Channel1]":
                newGroup = "[Channel3]";
                break;
            case "[Channel3]":
                newGroup = "[Channel1]";
                break;
            case "[Channel2]":
                newGroup = "[Channel4]";
                break;
            case "[Channel4]":
                newGroup = "[Channel2]";
                break;
        }
        engine.setValue(newGroup,"play",engine.getValue(newGroup,"play") >= 0.5 ? 0 : 1);
    }
    else
    {
        let enabled = this.mergeFxEnabled[newGroup];
        if(enabled)
        {
            this.stopMergeFx(group, newGroup);
        }
        else
        {
            this.startMergeFx(group, newGroup, this.mergeFxChainPreset[group]);
        }
    }
};

PioneerDDJFLX6.padFxPressed = function(channel, control, value, status, group){
    //if(value == 0)
    //    return;
    console.log("padFxPressed channel: "+channel+" control: "+control);
    let groupPreset = group.split(";");
    if(groupPreset.length < 2)
        return;
    let side = "";
    switch (groupPreset[0]) {
        case "[Channel1]":
            side = "L";
            break;
        case "[Channel2]":
            side = "R";
            break;
        case "[Channel3]":
            side = "L";
            break;
        case "[Channel4]":
            side = "R";
            break;
    }

    if(side == "")
        return;
    let enabled = this.mergeFxEnabled[groupPreset[0]];
    if(value > 0)
    {
        if(enabled)
        {
            //this.stopMergeFx(side, groupPreset[0]);
            engine.setValue("[QuickEffectRack1_"+groupPreset[0]+"]","loaded_chain_preset",groupPreset[1]);
        }
        else
        {
            this.startMergeFx(side, groupPreset[0], groupPreset[1]);
            engine.setValue("[QuickEffectRack1_"+groupPreset[0]+"]","super1",0.75);
        }
        this.startLEDBlink(status, control, 125);
    }
    else
    {
        if(enabled)
        {
            this.stopMergeFx(side,groupPreset[0]);
        }
        this.stopLEDBlink(status,control);
    }

}

PioneerDDJFLX6.startMergeFx = function(side, group, preset){
    this.mergeFxBeforeValue[group] = engine.getValue("[QuickEffectRack1_"+group+"]","super1");
    this.mergeFxbeforeLoadedChainPreset[group] = engine.getValue("[QuickEffectRack1_"+group+"]","loaded_chain_preset");
    this.mergeFxBeforeEnabled[group] = engine.getValue("[QuickEffectRack1_"+group+"]","enabled");

    engine.setValue("[QuickEffectRack1_"+group+"]","loaded_chain_preset",preset);
    engine.setValue("[QuickEffectRack1_"+group+"]","enabled",1);
    // midi.sendShortMsg(0xB4,0x10,0x7F);
    this.startLEDBlink(side == "L" ? 0xB4 : 0xB5, 0x10);
    this.mergeFxEnabled[group] = true;
}

PioneerDDJFLX6.stopMergeFx = function(side, group){
    engine.setValue("[QuickEffectRack1_"+group+"]","loaded_chain_preset",this.mergeFxbeforeLoadedChainPreset[group]);
    engine.setValue("[QuickEffectRack1_"+group+"]","super1",this.mergeFxBeforeValue[group]);
    engine.setValue("[QuickEffectRack1_"+group+"]","enabled",this.mergeFxBeforeEnabled[group]);
    this.stopLEDBlink(side == "L" ? 0xB4 : 0xB5,0x10);
    midi.sendShortMsg(side == "L" ? 0xB4 : 0xB5,0x10,0x7F);
    this.mergeFxEnabled[group] = false;
}

PioneerDDJFLX6.mergeEffectSelectorPressed = function(channel, _control, value, _status, group) {
    if(value == 0)
        return;
    var newGroup = "";
    switch(group)
    {
        case "L":
            newGroup = PioneerDDJFLX6.deckControlL;
            break;
        case "R":
            newGroup = PioneerDDJFLX6.deckControlR;
            break;
    }
    if(newGroup == "")
        return;
    let selector = this.mergeFxChainPreset[group];
    selector = selector + 1
    if(selector > 4.00001)
    {
        selector = 1;
    }
    this.mergeFxChainPreset[group] = selector;
    if(this.mergeFxEnabled[newGroup])
    {
        engine.setValue("[QuickEffectRack1_"+newGroup+"]","loaded_chain_preset",this.mergeFxChainPreset[group]);
    }
};

PioneerDDJFLX6.mergeEffectSelectorPressedReverse = function(channel, _control, value, _status, group) {
    if(value == 0)
        return;
    var newGroup = "";
    switch(group)
    {
        case "L":
            newGroup = PioneerDDJFLX6.deckControlL;
            break;
        case "R":
            newGroup = PioneerDDJFLX6.deckControlR;
            break;
    }
    if(newGroup == "")
        return;
    let selector = this.mergeFxChainPreset[group];
    selector = selector - 1
    if(selector <= 0.00001)
    {
        selector = 4;
    }
    this.mergeFxChainPreset[group] = selector;
    if(this.mergeFxEnabled[newGroup])
    {
        engine.setValue("[QuickEffectRack1_"+newGroup+"]","loaded_chain_preset",this.mergeFxChainPreset[group]);
    }
};

PioneerDDJFLX6.jogSearch = function(_channel, _control, value, _status, group) {
    const newVal = (value - 64) * PioneerDDJFLX6.fastSeekScale;
    engine.setValue(group, "jog", newVal);
};

PioneerDDJFLX6.jogTouch = function(channel, _control, value) {
    const deckNum = channel + 1;

    // skip while adjusting the loop points
    if (PioneerDDJFLX6.loopAdjustIn[channel] || PioneerDDJFLX6.loopAdjustOut[channel]) {
        return;
    }

    if (value !== 0 && this.vinylMode) {
        engine.scratchEnable(deckNum, 720*10, 33+1/3, this.alpha, this.beta);
    } else {
        engine.scratchDisable(deckNum);
    }
};

//
// Shift button
//

PioneerDDJFLX6.shiftPressed = function(channel, _control, value, _status, _group) {
    PioneerDDJFLX6.shiftButtonDown[channel] = value === 0x7F;
};


//
// Tempo sliders
//
// The tempo option in Mixxx's deck preferences determine whether down/up
// increases/decreases the rate. Therefore it must be inverted here so that the
// UI and the control sliders always move in the same direction.
//

PioneerDDJFLX6.tempoSliderMSB = function(channel, control, value, status, group) {
    PioneerDDJFLX6.highResMSB[group].tempoSlider = value;
};

PioneerDDJFLX6.tempoSliderLSB = function(channel, control, value, status, group) {
    const fullValue = (PioneerDDJFLX6.highResMSB[group].tempoSlider << 7) + value;

    engine.setValue(
        group,
        "rate",
        1 - (fullValue / 0x2000)
    );
};

//
// Beat Jump mode
//
// Note that when we increase/decrease the sizes on the pad buttons, we use the
// value of the first pad (0x21) as an upper/lower limit beyond which we don't
// allow further increasing/decreasing of all the values.
//

PioneerDDJFLX6.beatjumpPadPressed = function(_channel, control, value, _status, group) {
    if (value === 0) {
        return;
    }
    engine.setValue(group, "beatjump_size", Math.abs(PioneerDDJFLX6.beatjumpSizeForPad[control]));
    engine.setValue(group, "beatjump", PioneerDDJFLX6.beatjumpSizeForPad[control]);
};

PioneerDDJFLX6.increaseBeatjumpSizes = function(_channel, control, value, _status, group) {
    if (value === 0 || PioneerDDJFLX6.beatjumpSizeForPad[0x21] * 16 > 16) {
        return;
    }
    Object.keys(PioneerDDJFLX6.beatjumpSizeForPad).forEach(function(pad) {
        PioneerDDJFLX6.beatjumpSizeForPad[pad] = PioneerDDJFLX6.beatjumpSizeForPad[pad] * 16;
    });
    engine.setValue(group, "beatjump_size", PioneerDDJFLX6.beatjumpSizeForPad[0x21]);
};

PioneerDDJFLX6.decreaseBeatjumpSizes = function(_channel, control, value, _status, group) {
    if (value === 0 || PioneerDDJFLX6.beatjumpSizeForPad[0x21] / 16 < 1/16) {
        return;
    }
    Object.keys(PioneerDDJFLX6.beatjumpSizeForPad).forEach(function(pad) {
        PioneerDDJFLX6.beatjumpSizeForPad[pad] = PioneerDDJFLX6.beatjumpSizeForPad[pad] / 16;
    });
    engine.setValue(group, "beatjump_size", PioneerDDJFLX6.beatjumpSizeForPad[0x21]);
};

//
// Sampler mode
//

PioneerDDJFLX6.samplerPlayOutputCallbackFunction = function(value, group, _control) {
    if (value === 1) {
        const curPad = group.match(script.samplerRegEx)[1];
        let deckIndex = 0;
        let padIndex = 0;

        if (curPad >=1 && curPad <= 4) {
            deckIndex = 0;
            padIndex = curPad - 1;
        } else if (curPad >=5 && curPad <= 8) {
            deckIndex = 2;
            padIndex = curPad - 5;
        } else if (curPad >=9 && curPad <= 12) {
            deckIndex = 0;
            padIndex = curPad - 5;
        } else if (curPad >=13 && curPad <= 16) {
            deckIndex = 2;
            padIndex = curPad - 9;
        }

        PioneerDDJFLX6.startSamplerBlink(
            0x97 + deckIndex,
            0x30 + padIndex,
            group);
    }
};

PioneerDDJFLX6.padModeKeyPressed = function(_channel, _control, value, _status, _group) {
    let deck;

	switch (_status) {
		case 0x90:
			deck = PioneerDDJFLX6.lights.deck1;
			break;
		case 0x91:
			deck = PioneerDDJFLX6.lights.deck2;
			break;
		case 0x92:
			deck = PioneerDDJFLX6.lights.deck3;
			break;
		case 0x93:
			deck = PioneerDDJFLX6.lights.deck4;
			break;
		default:
			console.error("Unknown status: " + _status);
			return;
	}

    if (_control === 0x1B) {
        PioneerDDJFLX6.toggleLight(deck.hotcueMode, true);
    } else if (_control === 0x69) {
        PioneerDDJFLX6.toggleLight(deck.keyboardMode, true);
    } else if (_control === 0x1E) {
        PioneerDDJFLX6.toggleLight(deck.padFX1Mode, true);
    } else if (_control === 0x6B) {
        PioneerDDJFLX6.toggleLight(deck.padFX2Mode, true);
    } else if (_control === 0x20) {
        PioneerDDJFLX6.toggleLight(deck.beatJumpMode, true);
    } else if (_control === 0x6D) {
        PioneerDDJFLX6.toggleLight(deck.beatLoopMode, true);
    } else if (_control === 0x22) {
        PioneerDDJFLX6.toggleLight(deck.samplerMode, true);
    } else if (_control === 0x6F) {
        PioneerDDJFLX6.toggleLight(deck.keyShiftMode, true);
    }
};

PioneerDDJFLX6.samplerPadPressed = function(_channel, _control, value, _status, group) {
    if (engine.getValue(group, "track_loaded")) {
        engine.setValue(group, "cue_gotoandplay", value);
    } else {
        engine.setValue(group, "LoadSelectedTrack", value);
    }
};

PioneerDDJFLX6.samplerPadShiftPressed = function(_channel, _control, value, _status, group) {
    if (engine.getValue(group, "play")) {
        engine.setValue(group, "cue_gotoandstop", value);
    } else if (engine.getValue(group, "track_loaded")) {
        engine.setValue(group, "eject", value);
    }
};

PioneerDDJFLX6.startSamplerBlink = function(channel, control, group) {
    let val = 0x7f;

    PioneerDDJFLX6.stopSamplerBlink(channel, control);
    PioneerDDJFLX6.timers[channel][control] = engine.beginTimer(250, () => {
        val = 0x7f - val;

        // blink the appropriate pad
        midi.sendShortMsg(channel, control, val);
        // also blink the pad while SHIFT is pressed
        midi.sendShortMsg((channel+1), control, val);

        const isPlaying = engine.getValue(group, "play") === 1;

        if (!isPlaying) {
            // kill timer
            PioneerDDJFLX6.stopSamplerBlink(channel, control);
            // set the pad LED to ON
            midi.sendShortMsg(channel, control, 0x7f);
            // set the pad LED to ON while SHIFT is pressed
            midi.sendShortMsg((channel+1), control, 0x7f);
        }
    });
};

PioneerDDJFLX6.stopSamplerBlink = function(channel, control) {
    this.stopLEDBlink(channel, control);
};

PioneerDDJFLX6.startLEDBlink = function(channel, control, interval = 250) {
    let val = 0x00;

    PioneerDDJFLX6.stopLEDBlink(channel, control);
    PioneerDDJFLX6.timers[channel][control] = engine.beginTimer(interval, () => {
        val = 0x7f - val;

        // blink the appropriate LED
        midi.sendShortMsg(channel, control, val);
    });
};

PioneerDDJFLX6.stopLEDBlink = function(channel, control) {
    PioneerDDJFLX6.timers[channel] = PioneerDDJFLX6.timers[channel] || {};

    if (PioneerDDJFLX6.timers[channel][control] !== undefined) {
        engine.stopTimer(PioneerDDJFLX6.timers[channel][control]);
        PioneerDDJFLX6.timers[channel][control] = undefined;
    }
    midi.sendShortMsg(channel, control, 0x00);
};


PioneerDDJFLX6.toggleQuantize = function(_channel, _control, value, _status, group) {
    if (value) {
        script.toggleControl(group, "quantize");
    }
};

PioneerDDJFLX6.quickJumpForward = function(_channel, _control, value, _status, group) {
    if (value) {
        engine.setValue(group, "beatjump", PioneerDDJFLX6.quickJumpSize);
    }
};

PioneerDDJFLX6.quickJumpBack = function(_channel, _control, value, _status, group) {
    if (value) {
        engine.setValue(group, "beatjump", -PioneerDDJFLX6.quickJumpSize);
    }
};

//
// Shutdown
//

PioneerDDJFLX6.shutdown = function() {
    // reset vumeter
    PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.deck1.vuMeter, false);
    PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.deck2.vuMeter, false);
	PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.deck3.vuMeter, false);
    PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.deck4.vuMeter, false);

    // housekeeping
    // turn off all Sampler LEDs
    for (var i = 0; i <= 7; ++i) {
        midi.sendShortMsg(0x97, 0x30 + i, 0x00);    // Deck 1 pads
        midi.sendShortMsg(0x98, 0x30 + i, 0x00);    // Deck 1 pads with SHIFT
        midi.sendShortMsg(0x99, 0x30 + i, 0x00);    // Deck 2 pads
        midi.sendShortMsg(0x9A, 0x30 + i, 0x00);    // Deck 2 pads with SHIFT
    }
    // turn off all Hotcue LEDs
    for (i = 0; i <= 7; ++i) {
        midi.sendShortMsg(0x97, 0x00 + i, 0x00);    // Deck 1 pads
        midi.sendShortMsg(0x98, 0x00 + i, 0x00);    // Deck 1 pads with SHIFT
        midi.sendShortMsg(0x99, 0x00 + i, 0x00);    // Deck 2 pads
        midi.sendShortMsg(0x9A, 0x00 + i, 0x00);    // Deck 2 pads with SHIFT
		midi.sendShortMsg(0x9B, 0x00 + i, 0x00);    // Deck 3 pads
        midi.sendShortMsg(0x9C, 0x00 + i, 0x00);    // Deck 3 pads with SHIFT
        midi.sendShortMsg(0x9D, 0x00 + i, 0x00);    // Deck 4 pads
        midi.sendShortMsg(0x9E, 0x00 + i, 0x00);    // Deck 4 pads with SHIFT
    }

    // turn off loop in and out lights
    PioneerDDJFLX6.setLoopButtonLights(0x90, 0x00);
    PioneerDDJFLX6.setLoopButtonLights(0x91, 0x00);
    PioneerDDJFLX6.setLoopButtonLights(0x92, 0x00);
    PioneerDDJFLX6.setLoopButtonLights(0x93, 0x00);
    // turn off reloop lights
    PioneerDDJFLX6.setReloopLight(0x90, 0x00);
    PioneerDDJFLX6.setReloopLight(0x91, 0x00);
	PioneerDDJFLX6.setReloopLight(0x92, 0x00);
    PioneerDDJFLX6.setReloopLight(0x93, 0x00);

    // turn off jogwheel lights
    midi.sendShortMsg(0xBB, 0x00, 0x00);
    midi.sendShortMsg(0xBB, 0x01, 0x00);
    midi.sendShortMsg(0xBB, 0x02, 0x00);
    midi.sendShortMsg(0xBB, 0x03, 0x00);

    // stop any flashing lights
    PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.beatFx, false);
    PioneerDDJFLX6.toggleLight(PioneerDDJFLX6.lights.shiftBeatFx, false);

    // stop the keepalive timer
    engine.stopTimer(PioneerDDJFLX6.keepAliveTimer);
};
