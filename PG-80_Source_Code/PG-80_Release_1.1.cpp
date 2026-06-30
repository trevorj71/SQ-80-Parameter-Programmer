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

// --- PERFECTLY ALIGNED PARAMETER LOOKUP TABLE ---
const int sq80Parameters[32] = {
  3,  1,  2,  0,   // Pots 1-4:   OSC 1 Wave, Semi, Fine, Oct
  7,  5,  6,  4,   // Pots 5-8:   OSC 2 Wave, Semi, Fine, Oct
  11, 9,  10, 8,   // Pots 9-12:  OSC 3 Wave, Semi, Fine, Oct
  12, 13, 18, 23,  // Pots 13-16: DCA 1 Vol, DCA 2 Vol, VCF Cutoff, Patch Vol

  17, 15, 16, 26,  // Pots 17-20: LFO 1 Wave, LFO 1 Freq, LFO 1 DELAY, LFO 2 Wave
  24, 25, 35, 33,  // Pots 21-24: LFO 2 Freq, LFO 2 DELAY, LFO 3 Wave, LFO 3 Freq
  43, 44, 46, 48,  // Pots 25-28: ENV 1 Attack, Decay, Release, Sustain
  50, 51, 53, 55   // Pots 29-32: ENV 2 Attack, Decay, Release, Sustain
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
int translateToEnsoniqParam(int internalID) {
  switch(internalID) {
    case 0:  return 64; // OSC 1 OCTAVE
    case 1:  return 65; // OSC 1 SEMI
    case 2:  return 66; // OSC 1 FINE
    case 3:  return 67; // OSC 1 WAVE
    case 4:  return 72; // OSC 2 OCTAVE
    case 5:  return 73; // OSC 2 SEMI
    case 6:  return 74; // OSC 2 FINE
    case 7:  return 75; // OSC 2 WAVE
    case 8:  return 80; // OSC 3 OCTAVE
    case 9:  return 81; // OSC 3 SEMI
    case 10: return 82; // OSC 3 FINE
    case 11: return 83; // OSC 3 WAVE
    case 12: return 88; // DCA 1 LEVEL
    case 13: return 94; // DCA 2 LEVEL
    case 14: return 100;// DCA 3 LEVEL
    case 15: return 40; // LFO 1 FREQ
    case 16: return 45; // LFO 1 DELAY
    case 17: return 43; // LFO 1 MOD WAVE
    case 18: return 110;// FILTER CUTOFF
    case 19: return 111;// FILTER RESONANCE
    case 23: return 30; // FINAL VOLUME
    case 24: return 48; // LFO 2 FREQ
    case 25: return 53; // LFO 2 DELAY
    case 26: return 51; // LFO 2 MOD WAVE
    case 32: return 61; // LFO 3 DELAY
    case 33: return 56; // LFO 3 FREQ
    case 35: return 59; // LFO 3 MOD WAVE
    case 42: return 107;// PAN POSITION
    case 43: return 5;  // ENV 1 T1 (Attack)
    case 44: return 6;  // ENV 1 T2 (Decay)
    case 46: return 8;  // ENV 1 T4 (Release)
    case 48: return 2;  // ENV 1 L3 (Sustain)
    case 50: return 15; // ENV 2 T1 (Attack)
    case 51: return 16; // ENV 2 T2 (Decay)
    case 53: return 18; // ENV 2 T4 (Release)
    case 55: return 12; // ENV 2 L3 (Sustain)
    case 56: return 25; // ENV 3 T1 (Attack)
    case 57: return 26; // ENV 3 T2 (Decay)
    case 59: return 28; // ENV 3 T4 (Release)
    case 62: return 22; // ENV 3 L3 (Sustain)
    case 64: return 35; // ENV 4 T1 (Attack)
    case 65: return 36; // ENV 4 T2 (Decay)
    case 67: return 38; // ENV 4 T4 (Release)
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
  if (isShifted) {
    int currentPot32Val = readPot(muxB); // Pot 32 lives on Mux B Channel 15
    if (abs(currentPot32Val - lastPotState[31]) > hysteresis) {
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

    // INTERCEPT 1: Absolute Global System MIDI Channel Assignment (Pot 32 Shift Layer)
    if (potID == 31 && shiftActive) {
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
    if (shiftActive) {
      switch(potID) {
        case 13: paramID = 14; break; // Pot 14: DCA 2 Vol -> DCA 3 Vol
        case 14: paramID = 19; break; // Pot 15: VCF Cutoff -> VCF Res
        case 15: paramID = 42; break; // Pot 16: Patch Vol -> Voice Pan
        case 23: paramID = 32; break; // Pot 24: LFO 3 Freq -> LFO 3 DELAY
        case 24: paramID = 56; break; // Pot 25 (ENV 1 T1) -> ENV 3 T1
        case 25: paramID = 57; break; // Pot 26 (ENV 1 T2) -> ENV 3 T2
        case 26: paramID = 59; break; // Pot 27 (ENV 1 T4) -> ENV 3 T4
        case 27: paramID = 62; break; // Pot 28 (ENV 1 L3) -> ENV 3 L3
        case 28: paramID = 64; break; // Pot 29 (ENV 2 T1) -> ENV 4 T1
        case 29: paramID = 65; break; // Pot 30 (ENV 2 T2) -> ENV 4 T2
        case 30: paramID = 67; break; // Pot 31 (ENV 2 T4) -> ENV 4 T4
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

    // Convert our internal layout code ID to Ensoniq's true NRPN parameter target index
    int realMIDIParameter = translateToEnsoniqParam(paramID);
    sendEnsoniqNRPN(realMIDIParameter, ensoniqValue);
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