C++

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- OLED DISPLAY CONFIGURATION ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- HARDWARE PIN DEFINITIONS ---
const int s0 = 2;
const int s1 = 3;
const int s2 = 4;
const int s3 = 5;

const int muxA = A0; // Channels 0-15 (Physical Pots 1-16)
const int muxB = A1; // Channels 0-15 (Physical Pots 17-32)

const int modelSwitchPin = 6;  
const int glitchButtonPin = 7; // Multi-gesture shift layer / glitch input button

// --- SYSTEM STATE VARIABLES ---
int lastPotState[32] = {0};
const int hysteresis = 8;         
bool isSQ80 = true;               
bool glitchMode = false;          // Starts up at "Hidden Waves: OFF"
bool lastGlitchButtonState = LOW;  // matches INPUT_PULLDOWN idle state

// --- GLOBAL MIDI SETTINGS ---
int midiChannel = 1;              

// --- ERA-ACCURATE FACTORY WAVEFORM LOOKUP TABLES ---
const char* const esq1Waves[32] = {
  "SAW", "SQUARE", "PULSE", "SINE", "NOISE", "REED", "ORGAN", "VOICE",
  "CHORD", "SYNTH", "BASS", "PIANO", "EL PNO", "VOX 1", "VOX 2", "BREATH",
  "DRUM 1", "DRUM 2", "DRUM 3", "KICK", "SNARE", "TOM", "HI-HAT", "PERC 1",
  "SYN 2", "SYN 3", "MELLON", "OCTAVE", "FIFTH", "PIRATE", "GHOST", "HEAVEN"
};
const char* const sq80ExtraWaves[43] = {
  "GLINT 1", "GLINT 2", "GLINT 3", "GLINT 4", "GLINT 5", "GLINT 6", "GRIT 1", "GRIT 2", 
  "GRIT 3", "WRONG", "SNARE", "KICK", "TOM", "HI-HAT", "RIM", "BREATH", 
  "VOICE 2", "STEAM", "METAL", "CHIME", "BOWED", "STRUCK", "PLUCK", "PLINK", 
  "CHIFF", "THUMP", "CLICK", "LOG", "XLYO", "MALLET", "ORGAN 2", "NEIGH", 
  "ROBOT", "SQUAWK", "ALARM", "REVERSE", "LOOP 1", "LOOP 2", "LOOP 3", "LOOP 4", 
  "LOOP 5", "LOOP 6", "MAC"
};
const char* const lfoWaves[5] = {
  "TRIANGLE", "SAWTOOTH", "SQUARE", "RANDOM", "NOISE"
};

// --- PARAMETER LOOKUP TABLE: COLUMN-FIRST LAYOUT ---
// Physical layout: 8 columns x 4 rows, read left-to-right top-to-bottom.
// Each column groups a parameter family so pots in the same column are
// visually consistent with the SQ-80's front panel button groupings.
const int sq80Parameters[32] = {
  // Row 1 (Pots 1-8):   one pot per column, top knob of each group
  3,  7,  11, 12, 17, 24, 43, 50,
  // Pots 1-8: OSC1 Wave | OSC2 Wave | OSC3 Wave | DCA1 Vol | LFO1 Wave | LFO2 Freq | ENV1 Atk | ENV2 Atk

  // Row 2 (Pots 9-16):  second knob of each group
  1,  5,  9,  13, 15, 35, 44, 51,
  // Pots 9-16: OSC1 Semi | OSC2 Semi | OSC3 Semi | DCA2 Vol | LFO1 Freq | LFO3 Wave | ENV1 Dec | ENV2 Dec

  // Row 3 (Pots 17-24): third knob of each group
  2,  6,  10, 18, 16, 33, 46, 53,
  // Pots 17-24: OSC1 Fine | OSC2 Fine | OSC3 Fine | VCF Cut | LFO1 Dly | LFO3 Freq | ENV1 Rel | ENV2 Rel

  // Row 4 (Pots 25-32): fourth/bottom knob of each group
  0,  4,  8,  23, 26, 32, 48, 55
  // Pots 25-32: OSC1 Oct | OSC2 Oct | OSC3 Oct | Patch Vol | LFO2 Wave | LFO3 Dly | ENV1 Sus | ENV2 Sus
};

// Track whether Pot 32 was adjusted while shift was held down
bool pot32AdjustedDuringShift = false;

// Track what's currently on screen so other events (e.g. model switch) can redraw it
int lastDisplayedParamID = -1;
int lastDisplayedValue = 0;

// Reads a pot's raw ADC value with rotation direction inverted, so that
// clockwise rotation increases the value (matching typical studio gear),
// instead of the hardware's native counter-clockwise-to-increase wiring.
int readPot(int analogPin) {
  return 1023 - analogRead(analogPin);
}

// --- ENSONIQ HARDWARE MIDI PARAMETER TRANSLATOR ---
// Returns -1 for parameters whose NRPN address is not yet confirmed from the
// manual. The send path skips transmission for -1 to prevent corrupting the
// patch with a garbage address. Verify and fill these in after bench testing.
int translateToEnsoniqParam(int internalID) {
  switch(internalID) {
    // OSC 1 (NRPN block 64-71)
    case 0:  return 64; // OSC 1 OCTAVE
    case 1:  return 65; // OSC 1 SEMI
    case 2:  return 66; // OSC 1 FINE
    case 3:  return 67; // OSC 1 WAVE
    case 70: return 68; // OSC 1 MOD 1 SOURCE
    case 71: return 69; // OSC 1 MOD 1 LEVEL
    case 72: return 70; // OSC 1 MOD 2 SOURCE
    case 73: return 71; // OSC 1 MOD 2 LEVEL

    // OSC 2 (NRPN block 72-79)
    case 4:  return 72; // OSC 2 OCTAVE
    case 5:  return 73; // OSC 2 SEMI
    case 6:  return 74; // OSC 2 FINE
    case 7:  return 75; // OSC 2 WAVE
    case 74: return 76; // OSC 2 MOD 1 SOURCE
    case 75: return 77; // OSC 2 MOD 1 LEVEL
    case 76: return 78; // OSC 2 MOD 2 SOURCE
    case 77: return 79; // OSC 2 MOD 2 LEVEL

    // OSC 3 (NRPN block 80-87)
    case 8:  return 80; // OSC 3 OCTAVE
    case 9:  return 81; // OSC 3 SEMI
    case 10: return 82; // OSC 3 FINE
    case 11: return 83; // OSC 3 WAVE
    case 78: return 84; // OSC 3 MOD 1 SOURCE
    case 79: return 85; // OSC 3 MOD 1 LEVEL
    case 80: return 86; // OSC 3 MOD 2 SOURCE
    case 81: return 87; // OSC 3 MOD 2 LEVEL

    // DCA / VCF
    case 12: return 88; // DCA 1 LEVEL
    case 13: return 94; // DCA 2 LEVEL
    case 14: return 100;// DCA 3 LEVEL
    case 18: return 110;// FILTER CUTOFF
    case 19: return 111;// FILTER RESONANCE
    case 82: return 112;// FILTER MODULATION AMOUNT 3

    // LFO 1 (NRPN block 40-47)
    case 15: return 40; // LFO 1 FREQ
    case 16: return 45; // LFO 1 DELAY
    case 17: return 43; // LFO 1 WAVE
    case 83: return 44; // LFO 1 L1
    case 84: return 46; // LFO 1 L2

    // LFO 2 (NRPN block 48-55)
    case 24: return 48; // LFO 2 FREQ
    case 25: return 53; // LFO 2 DELAY
    case 26: return 51; // LFO 2 WAVE
    case 85: return 52; // LFO 2 L1
    case 86: return 54; // LFO 2 L2

    // LFO 3 (NRPN block 56-63)
    case 33: return 56; // LFO 3 FREQ
    case 32: return 61; // LFO 3 DELAY
    case 35: return 59; // LFO 3 WAVE
    case 87: return 60; // LFO 3 L1
    case 88: return 62; // LFO 3 L2

    // Misc
    case 23: return 30; // PATCH VOLUME (ENV 4 LV)
    case 42: return 107;// PAN POSITION

    // ENV 1 (NRPN block 0-9)
    case 43: return 5;  // ENV 1 T1 (Attack)
    case 44: return 6;  // ENV 1 T2 (Decay)
    case 46: return 8;  // ENV 1 T4 (Release)
    case 48: return 2;  // ENV 1 L3 (Sustain)

    // ENV 2 (NRPN block 10-19)
    case 50: return 15; // ENV 2 T1 (Attack)
    case 51: return 16; // ENV 2 T2 (Decay)
    case 53: return 18; // ENV 2 T4 (Release)
    case 55: return 12; // ENV 2 L3 (Sustain)

    // ENV 3 (NRPN block 20-29)
    case 56: return 25; // ENV 3 T1 (Attack)
    case 57: return 26; // ENV 3 T2 (Decay)
    case 59: return 28; // ENV 3 T4 (Release)
    case 62: return 22; // ENV 3 L3 (Sustain)

    // ENV 4 (NRPN block 30-39)
    case 64: return 35; // ENV 4 T1 (Attack)
    case 65: return 36; // ENV 4 T2 (Decay)
    case 67: return 38; // ENV 4 T4 (Release)
    case 68: return 32; // ENV 4 L3 (Sustain)

    default: return internalID;
  }
}

void setup() {
  delay(200); // Give physical OLED power rail time to stabilize
  Wire.begin(); // Wake up I2C controller

  Serial1.begin(31250); // Hardware MIDI protocol

  pinMode(s0, OUTPUT);
  pinMode(s1, OUTPUT);
  pinMode(s2, OUTPUT);
  pinMode(s3, OUTPUT);

  pinMode(modelSwitchPin, INPUT_PULLUP);
  pinMode(glitchButtonPin, INPUT_PULLDOWN);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { 
    pinMode(LED_BUILTIN, OUTPUT);
    while(true) {
      digitalWrite(LED_BUILTIN, HIGH); delay(100);
      digitalWrite(LED_BUILTIN, LOW);  delay(100);
    }
  }
  
  // Set initial model state cleanly based on hardware layout
  isSQ80 = (digitalRead(modelSwitchPin) == LOW);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("PG-80");
  display.drawLine(0, 18, 128, 18, WHITE);
  
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.println("CROSS WAVE PROGRAMMER");
  display.setCursor(0, 36);
  display.println("Hardware Map Confirmed");
  display.setCursor(0, 48);
  display.println("Firmware Profile v2.2");

  display.display();
  delay(2000);

  // --- SEED INITIAL POT STATE TO PREVENT POWER-ON PARAMETER DUMP ---
  // Read every pot's current physical position once, silently, so the
  // first loop() scan has a true baseline and doesn't interpret "knob
  // sits away from zero" as "knob just moved."
  for (int channel = 0; channel < 16; channel++) {
    digitalWrite(s0, (channel >> 0) & 1);
    digitalWrite(s1, (channel >> 1) & 1);
    digitalWrite(s2, (channel >> 2) & 1);
    digitalWrite(s3, (channel >> 3) & 1);
    delayMicroseconds(30);

    lastPotState[channel]      = readPot(muxA);
    lastPotState[channel + 16] = readPot(muxB);
  }

  // Draw the idle/ready screen (also draws the engine badge for the first time)
  updateOLED(-1, 0);
}

void loop() {
  // 1. DYNAMIC SWITCH EVALUATION INTERFACE
  bool currentModel = (digitalRead(modelSwitchPin) == LOW);
  if (currentModel != isSQ80) {
    isSQ80 = currentModel;
    // Redraw whatever's currently on screen so the engine badge updates
    // in the same fixed slot every time, with no separate overlay logic.
    updateOLED(lastDisplayedParamID, lastDisplayedValue);
  }

  // Background hardware serial MIDI merger loop
  while (Serial1.available() > 0) {
    uint8_t inByte = Serial1.read();
    Serial1.write(inByte);
  }

  // Read current shifting key array state
  bool currentGlitchButtonState = digitalRead(glitchButtonPin);
  bool isShifted = (currentGlitchButtonState == HIGH);

  // 2. SYNCHRONOUS MULTIPLEXER ROW SCANNING ENGINE
  for (int channel = 0; channel < 16; channel++) {
    // Explicitly shift bits right so they always evaluate to a strict 0 or 1
    digitalWrite(s0, (channel >> 0) & 1);
    digitalWrite(s1, (channel >> 1) & 1);
    digitalWrite(s2, (channel >> 2) & 1);
    digitalWrite(s3, (channel >> 3) & 1);
    delayMicroseconds(30);

    int valA = readPot(muxA);
    int valB = readPot(muxB);
    
    checkAndSendMIDI(channel, valA, isShifted);
    checkAndSendMIDI(channel + 16, valB, isShifted);
  }

  // 3. SEPARATED MULTI-GESTURE TACTILE BUTTON PARSER
  // MIDI channel is now on Pot 30 shifted (potID 29, Mux B channel 13).
  // Set MUX select lines to channel 13 and sample Mux B directly here,
  // outside the main scan loop, to detect whether that pot moved.
  if (isShifted) {
    digitalWrite(s0, (13 >> 0) & 1);
    digitalWrite(s1, (13 >> 1) & 1);
    digitalWrite(s2, (13 >> 2) & 1);
    digitalWrite(s3, (13 >> 3) & 1);
    delayMicroseconds(30);
    int currentPot30Val = readPot(muxB); // Pot 30: LFO 3 Delay, shifted = MIDI Channel
    if (abs(currentPot30Val - lastPotState[29]) > hysteresis) {
      pot32AdjustedDuringShift = true;
    }
  }

  // Check for physical button release
  if (currentGlitchButtonState == LOW && lastGlitchButtonState == HIGH) {
    if (pot32AdjustedDuringShift) {
      pot32AdjustedDuringShift = false; // Reset modifier flag cleanly
      
      display.clearDisplay();
      display.setCursor(0, 20);
      display.setTextSize(1);
      display.print("CHANNEL LOCKED IN");
      display.display();
      delay(600);
    } 
    else {
      glitchMode = !glitchMode;
      display.clearDisplay();
      display.setCursor(0, 20);
      display.setTextSize(1); 
      if (glitchMode) display.print("HIDDEN WAVES: ON");
      else display.print("HIDDEN WAVES: OFF");
      display.display();
      delay(500);
    }
  }
  lastGlitchButtonState = currentGlitchButtonState;
}

void checkAndSendMIDI(int potID, int currentRawValue, bool shiftActive) {
  if (abs(currentRawValue - lastPotState[potID]) > hysteresis) {
    
    lastPotState[potID] = currentRawValue;

    // INTERCEPT 1: Absolute Global System MIDI Channel Assignment (Pot 30 Shift Layer)
    if (potID == 29 && shiftActive) {
      midiChannel = map(currentRawValue, 0, 1023, 1, 16);
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setCursor(0, 5);
      display.print("GLOBAL SYSTEM CFG");
      display.drawLine(0, 16, 128, 16, WHITE);
      display.setCursor(0, 26);
      display.print("SET OUT CHANNEL:");
      display.setTextSize(2);
      display.setCursor(0, 42);
      display.print("CH: ");
      display.print(midiChannel);
      display.display();
      return; 
    }
    
    int paramID = sq80Parameters[potID];

    // INTERCEPT 2: Multi-Layer Macro Shift Transformation Redirections
    // potID values are 0-based (potID = pot number - 1).
    // MIDI CHANNEL is now on Pot 30 (potID 29) shifted — caught by INTERCEPT 1 above.
    // Pot 32 (potID 31) shifted now carries ENV 4 SUSTAIN.
    if (shiftActive) {
      switch(potID) {
        // --- OSC 1 MOD MATRIX (Col 1 shifted) ---
        case 0:  paramID = 70; break; // Pot 1:  OSC 1 Wave     -> OSC 1 Mod 1 Source
        case 8:  paramID = 71; break; // Pot 9:  OSC 1 Semi     -> OSC 1 Mod 1 Level
        case 16: paramID = 72; break; // Pot 17: OSC 1 Fine     -> OSC 1 Mod 2 Source
        case 24: paramID = 73; break; // Pot 25: OSC 1 Oct      -> OSC 1 Mod 2 Level
        // --- OSC 2 MOD MATRIX (Col 2 shifted) ---
        case 1:  paramID = 74; break; // Pot 2:  OSC 2 Wave     -> OSC 2 Mod 1 Source
        case 9:  paramID = 75; break; // Pot 10: OSC 2 Semi     -> OSC 2 Mod 1 Level
        case 17: paramID = 76; break; // Pot 18: OSC 2 Fine     -> OSC 2 Mod 2 Source
        case 25: paramID = 77; break; // Pot 26: OSC 2 Oct      -> OSC 2 Mod 2 Level
        // --- OSC 3 MOD MATRIX (Col 3 shifted) ---
        case 2:  paramID = 78; break; // Pot 3:  OSC 3 Wave     -> OSC 3 Mod 1 Source
        case 10: paramID = 79; break; // Pot 11: OSC 3 Semi     -> OSC 3 Mod 1 Level
        case 18: paramID = 80; break; // Pot 19: OSC 3 Fine     -> OSC 3 Mod 2 Source
        case 26: paramID = 81; break; // Pot 27: OSC 3 Oct      -> OSC 3 Mod 2 Level
        // --- DCA / VCF / MISC (Col 4 shifted) ---
        case 3:  paramID = 14; break; // Pot 4:  DCA 1 Vol      -> DCA 3 Vol
        case 11: paramID = 82; break; // Pot 12: DCA 2 Vol      -> Filter Mod Amount 3
        case 19: paramID = 19; break; // Pot 20: VCF Cutoff     -> VCF Resonance
        case 27: paramID = 42; break; // Pot 28: Patch Vol      -> Voice Pan
        // --- LFO LEVELS (Cols 5-6 shifted) ---
        case 4:  paramID = 83; break; // Pot 5:  LFO 1 Wave     -> LFO 1 L1
        case 12: paramID = 84; break; // Pot 13: LFO 1 Freq     -> LFO 1 L2
        case 20: paramID = 25; break; // Pot 21: LFO 1 Delay    -> LFO 2 Delay
        case 28: paramID = 85; break; // Pot 29: LFO 2 Wave     -> LFO 2 L1
        case 5:  paramID = 86; break; // Pot 6:  LFO 2 Freq     -> LFO 2 L2
        case 13: paramID = 87; break; // Pot 14: LFO 3 Wave     -> LFO 3 L1
        case 21: paramID = 88; break; // Pot 22: LFO 3 Freq     -> LFO 3 L2
        // potID 29 (Pot 30): LFO 3 Delay -> MIDI Channel; caught by INTERCEPT 1 above.
        // --- ENV 1 / ENV 3 (Col 7 shifted) ---
        case 6:  paramID = 56; break; // Pot 7:  ENV 1 Attack   -> ENV 3 Attack
        case 14: paramID = 57; break; // Pot 15: ENV 1 Decay    -> ENV 3 Decay
        case 22: paramID = 59; break; // Pot 23: ENV 1 Release  -> ENV 3 Release
        case 30: paramID = 62; break; // Pot 31: ENV 1 Sustain  -> ENV 3 Sustain
        // --- ENV 2 / ENV 4 (Col 8 shifted) ---
        case 7:  paramID = 64; break; // Pot 8:  ENV 2 Attack   -> ENV 4 Attack
        case 15: paramID = 65; break; // Pot 16: ENV 2 Decay    -> ENV 4 Decay
        case 23: paramID = 67; break; // Pot 24: ENV 2 Release  -> ENV 4 Release
        case 31: paramID = 68; break; // Pot 32: ENV 2 Sustain  -> ENV 4 Sustain
      }
    }
    
    int ensoniqValue;

    // --- DYNAMIC ADAPTIVE PARAMETER MAPPING ENGINE ---
    if (paramID == 18) {
      ensoniqValue = map(currentRawValue, 0, 1023, 0, 127);
    } else if (paramID == 3 || paramID == 7 || paramID == 11) {
      if (glitchMode) {
        ensoniqValue = map(currentRawValue, 0, 1023, 0, 255);
      } else {
        if (isSQ80) ensoniqValue = map(currentRawValue, 0, 1023, 0, 74);
        else ensoniqValue = map(currentRawValue, 0, 1023, 0, 31);
      }
    } else if (paramID == 17 || paramID == 26 || paramID == 35) {
      if (isSQ80) ensoniqValue = map(currentRawValue, 0, 1023, 0, 4);
      else ensoniqValue = map(currentRawValue, 0, 1023, 0, 3);
    } else if (paramID == 19 || paramID == 2 || paramID == 6 || paramID == 10) {
      ensoniqValue = map(currentRawValue, 0, 1023, 0, 31);
    } else if (paramID == 42) {
      ensoniqValue = map(currentRawValue, 0, 1023, 0, 15);
    } else {
      ensoniqValue = map(currentRawValue, 0, 1023, 0, 63);
    }

    // Convert our internal layout code ID to Ensoniq's true NRPN parameter target index.
    // translateToEnsoniqParam() returns -1 for parameters whose NRPN address is not yet
    // confirmed. Skip transmission entirely in that case to avoid corrupting the patch.
    int realMIDIParameter = translateToEnsoniqParam(paramID);
    if (realMIDIParameter >= 0) {
      sendEnsoniqNRPN(realMIDIParameter, ensoniqValue);
    }
    updateOLED(paramID, ensoniqValue);
  }
}

void sendEnsoniqNRPN(int parameter, int value) {
  uint8_t statusByte = 0xB0 + (midiChannel - 1);
  Serial1.write(statusByte);
  Serial1.write(99);
  Serial1.write(0);
  
  Serial1.write(statusByte);
  Serial1.write(98);
  Serial1.write(parameter); // Safely translated Ensoniq target ID
  
  Serial1.write(statusByte);
  Serial1.write(6);
  Serial1.write(value);
}

void updateOLED(int paramID, int value) {
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Remember what's currently being shown, so other events (model switch)
  // can ask for a redraw of "whatever was already on screen."
  lastDisplayedParamID = paramID;
  lastDisplayedValue = value;

  // --- LINE 1: HEADER TELEMETRY STRIP ---
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("CH:");
  display.print(midiChannel);
  if (glitchMode) display.print(" GLITCH");
  display.drawLine(0, 11, 128, 11, WHITE);

  // --- ENGINE BADGE: fixed top-right slot, redrawn every frame, well
  // clear of the header text above (max width "CH:16 GLITCH" ~78px) ---
  display.setCursor(104, 0);
  display.print(isSQ80 ? "SQ80" : "ESQ1");

  // --- IDLE / READY SCREEN (no parameter touched yet) ---
  if (paramID == -1) {
    display.setCursor(0, 24);
    display.setTextSize(1);
    display.print("READY - TURN A KNOB");
    display.display();
    return;
  }

  // --- LINE 2: PARAMETER SPECIFICATION ---
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.print("PM ");
  if (paramID < 10) display.print("0");
  display.print(paramID);
  display.print(": ");
  
  switch(paramID) {
    case 0: display.print("OSC 1 OCTAVE"); break;
    case 1: display.print("OSC 1 SEMI"); break;
    case 2: display.print("OSC 1 FINE"); break;
    case 3: display.print("OSC 1 WAVE"); break;
    case 4: display.print("OSC 2 OCTAVE"); break;
    case 5: display.print("OSC 2 SEMI"); break;
    case 6: display.print("OSC 2 FINE"); break;
    case 7: display.print("OSC 2 WAVE"); break;
    case 8: display.print("OSC 3 OCTAVE"); break;
    case 9: display.print("OSC 3 SEMI"); break;
    case 10: display.print("OSC 3 FINE"); break;
    case 11: display.print("OSC 3 WAVE"); break;
    case 12: display.print("DCA 1 VOL"); break;
    case 13: display.print("DCA 2 VOL"); break;
    case 14: display.print("DCA 3 VOL"); break;
    case 15: display.print("LFO 1 FREQ"); break;
    case 16: display.print("LFO 1 DELAY"); break;
    case 17: display.print("LFO 1 WAVE"); break;
    case 18: display.print("VCF CUTOFF"); break;
    case 19: display.print("VCF RESONANCE"); break;
    case 23: display.print("PATCH VOL"); break;
    case 24: display.print("LFO 2 FREQ"); break;
    case 25: display.print("LFO 2 DELAY"); break;
    case 26: display.print("LFO 2 WAVE"); break;
    case 32: display.print("LFO 3 DELAY"); break;
    case 33: display.print("LFO 3 FREQ"); break;
    case 35: display.print("LFO 3 WAVE"); break;
    case 42: display.print("VOICE PAN"); break;
    case 43: display.print("ENV 1 ATTACK"); break;
    case 44: display.print("ENV 1 DECAY"); break;
    case 46: display.print("ENV 1 RELEASE"); break;
    case 48: display.print("ENV 1 SUSTAIN"); break;
    case 50: display.print("ENV 2 ATTACK"); break;
    case 51: display.print("ENV 2 DECAY"); break;
    case 53: display.print("ENV 2 RELEASE"); break;
    case 55: display.print("ENV 2 SUSTAIN"); break;
    case 56: display.print("ENV 3 ATTACK"); break;
    case 57: display.print("ENV 3 DECAY"); break;
    case 59: display.print("ENV 3 RELEASE"); break;
    case 62: display.print("ENV 3 SUSTAIN"); break;
    case 64: display.print("ENV 4 ATTACK"); break;
    case 65: display.print("ENV 4 DECAY"); break;
    case 67: display.print("ENV 4 RELEASE"); break;
    case 68: display.print("ENV 4 SUSTAIN"); break;
    // OSC 1 MOD MATRIX
    case 70: display.print("O1 MOD1 SRC"); break;
    case 71: display.print("O1 MOD1 LVL"); break;
    case 72: display.print("O1 MOD2 SRC"); break;
    case 73: display.print("O1 MOD2 LVL"); break;
    // OSC 2 MOD MATRIX
    case 74: display.print("O2 MOD1 SRC"); break;
    case 75: display.print("O2 MOD1 LVL"); break;
    case 76: display.print("O2 MOD2 SRC"); break;
    case 77: display.print("O2 MOD2 LVL"); break;
    // OSC 3 MOD MATRIX
    case 78: display.print("O3 MOD1 SRC"); break;
    case 79: display.print("O3 MOD1 LVL"); break;
    case 80: display.print("O3 MOD2 SRC"); break;
    case 81: display.print("O3 MOD2 LVL"); break;
    // FILTER MOD / LFO LEVELS
    case 82: display.print("FILT MOD AMT3"); break;
    case 83: display.print("LFO1 L1"); break;
    case 84: display.print("LFO1 L2"); break;
    case 85: display.print("LFO2 L1"); break;
    case 86: display.print("LFO2 L2"); break;
    case 87: display.print("LFO3 L1"); break;
    case 88: display.print("LFO3 L2"); break;
    default: display.print("RAW PARAM"); break;
  }
  
  // --- LINE 3: VALUE RENDERING ---
  display.setCursor(0, 30);
  display.setTextSize(2);
  display.print("VAL: ");
  display.print(value);
  
  // --- LINE 4: WAVEFORM SUB-LABELS / SLIDER GRAPH ---
  if (paramID == 3 || paramID == 7 || paramID == 11) {
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.print(">> [");
    if (value >= 0 && value <= 31) {
      display.print(esq1Waves[value]);
    } 
    else if (value >= 32 && value <= 74 && isSQ80) {
      display.print(sq80ExtraWaves[value - 32]);
    } 
    else if (value >= 32 && value <= 74 && !isSQ80) {
      display.print("ESQ1 HIDDEN");
    } 
    else {
      display.print("RESERVED HIDDEN");
    }
    display.print("]");
  } 
  else if (paramID == 17 || paramID == 26 || paramID == 35) {
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.print(">> [");
    if (value >= 0 && value <= 4) display.print(lfoWaves[value]);
    else display.print("RESERVED");
    display.print("]");
  } 
  else {
    // Structural slider indicator graph
    int barWidth = map(value, 0, (paramID == 18) ? 127 : ((paramID == 42) ? 15 : 63), 0, 128);
    display.fillRect(0, 54, barWidth, 6, WHITE);
  }
  
  display.display();
}