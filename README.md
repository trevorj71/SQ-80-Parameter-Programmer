# PG-80 Crosswave Parameter Programmer for Ensoniq SQ-80 and Ensoniq ESQ1

This repo contains open source hardware and software for the Ensoniq SQ-80 and ESQ-1 hybrid synthesizers, which I have named the "PG-80". It allows you to program up to 63 parameters in your synthesis engine using the 32 knobs on the front panel. It also supports hidden waves if your synth is equipped with an OS that supports them on the front panel. It also features a MIDI merge feature so you can run an input from your MIDI interface through the programmer to the synth, inserting the programmer in-line so it doesn't add unnecessarily to your workflow or MIDI rats nest.

I'm still tweaking the firmware to improve the knob layout and I have a new housing version in progress to accomodate a larger OLED display, but the current design is debugged and functional.

A BOM will be forthcoming when I have a few minutes to add it.

# Housing

My vision for this unit was always been a sidecar that can sit next to the synth on a stand designed for a 76 key workstation and looks like an extension of the synth to the extent possible.  Likely, you could design a smaller housing for desktop, as mine is not necessarily designed
to minimize its footprint or print time. 

The 3D models for the prototype are will be published here in both Fusion 360 and STL formats.

## Prototype Version (Adafruit 326 and PCB Mounted MIDI Connections)

![PG-80 Programmer prototype with Ensoniq SQ-80 and Ensonig ESQ-1](https://github.com/trevorj71/SQ-80-Parameter-Programmer/blob/main/images/PG-80_w_Synths.JPEG)

For my first build, I used PCB mounted MIDI connectors and a PCB that was optimized for production and shipping cost. This left me with limited space between the knob array and the back panel of the unit for an OLED, so I used an Adafruit 326 0.91mm display.  This was fine for prototyping but in practice in the studio it is really hard to read.  Otherwise the knob layout feels good and the overall geometry of the print meets the vision. 

I did have some slippage in my printer late in this job (which took 5 days) so there is a bit of a bondo job on this, but it works for a prototype.


## In Progress Updgraded Version (Adafruit ... and Panel Mounted MIDI Connecitons)

Build two will switch to a 2.42 inch Adafruit 2719 and panel mounted MIDI connections. Design for that enclosure is under way.

# Glitch Mode (Hidden Waveform) and Shift Switch

This momentary pushbutton switch, SW2, has two functions. 

A quick tap of this swtich enables glitch mode. This allows knobs 1-3 (oscillator waveforms) to transmit all 255 available parameters to the synth. If you have an OS for your synth that supports hidden waveforms, you will be able to scroll through all 255 paramteters. This only works with the modified OS versions that support hidden waves.

Pressing and holding this switch while also turning a knob makes it function as a shift key, letting you change the MIDI channel on your device or modify up to 63 parameters in your synthesis workflow. Failing to turn a knob while the switch is depressed reverts the programmer to Glitch Mode / Standard Mode.

# Arch Mode (SQ-80 / ESQ-1) Toggle

A panel mounted toggle switch, SW1, facilitates switching between the 32 wave ESQ-1 table and the 75 wave SQ-80 table. In standard mode, this aligns the values sent by the oscillator waveform knobs within the desired range for your model synth. In Glitch Mode, this is more a matter of the mask used for the OLED on parameters 33-75, since all 255 parameters can be transmitted to the synth.

# Potentiometer Layout

Currently, the programmer has been laid out with 32 Potentiometers. You can access up to 63 parameters plus MIDI channel by pressing and holding the Hidden Waveforms button like a shift key.  

The knob layout as of version 1.5 is as follows:

### GLITCH BUTTON IN THE UP POSITION - NOT PRESSED

| Oscillator 1 | Oscillator 2 | Oscillator 3 | DCA-Filter | MOD 1 | MOD 2 | ENV 1-3 | ENV 2-4 
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| WAVEFORM | WAVEFORM | WAVEFORM | DCA1 VOL | LFO1 WAVE | LFO2 FREQ | ENV1 ATK | ENV2 ATK 
| OSC1 FINE | OSC2 FINE | OSC3 FINE | DCA2 VOL | LFO1 FREQ | LFO3 WAVE | ENV1 DEC | ENV2 DEC 
| OSC1 COAR | OSC2 COAR | OSC3 COAR | DCA3 VOL | LFO1 DELY | LFO3 FREQ | ENV1 REL | ENV2 REL
| OSC1 OCT | OSC2 OCT | OSC3 OCT | ENV4 VOL | LFO2 WAVE | LFO3 DELY | ENV1 SUS | ENV2 SUS 

### GLITCH BUTTON HELD DOWN WHILE TURNING

| Oscillator 1 | Oscillator 2 | Oscillator 3 | DCA-Filter | MOD 1 | MOD 2 | ENV 1-3 | ENV 2-4 
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| MOD1 SRC | MOD1 SRC | MOD1 SRC | FILT CUT | LFO1 L1 | LFO2 LS | ENV3 ATK | ENV4 ATK 
| MOD1 LEV | MOD1 LEV | MOD1 LEV | FILT RES | LFO1 L2 | LFO3 L1` | ENV3 DEC | ENV4 DEC 
| MOD2 SRC | MOD2 SRC | MOD2 SRC | FILT MOD3 | LFO2 DELY | LFO3 L2 | ENV3 REL | ENV4 REL
| MOD2 LEV | MOD2 LEV | MOD2 LEV | VOICE PAN | LFO2 L1 | MIDI CHAN | ENV3 SUS | ENV4 SUS 