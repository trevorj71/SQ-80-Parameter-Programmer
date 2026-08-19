# PG-80 Crosswave Parameter Programmer for Ensoniq SQ-80 and Ensoniq ESQ1

This repo contains open source hardware and software for the Ensoniq SQ-80 and ESQ-1 hybrid synthesizers.

I do have further revisions in mind for the firmware to add access to more programming parameters, but the current version is fully functional for up to 42.  

A BOM wiill be forthcoming when I have a few minutes to add it.

# Housing

My vision for this unit was always been a sidecar that can sit next to the synth on a stand designed for a 76 key workstation and looks like an extension of the synth to the extent possible.
These 3D models will be published here in both raw Fusion 360 and STL formats once they are completed. Likely, you could design a smaller housing for desktop, as mine is not necessarily designed
to minimize its footprint or print time.

# Glitch Mode (Hidden Waveforme)

To be added

# Arch Mode (SQ-80 / ESQ-1) Toggle

To be added

# Potentiometer Layout

Currently, the programmer has been laid out with 32 Potentiometers. You can access up to 64 parameters by pressing and holding the Hidden Waveforms button like a shift key.  The knob layout as of version 1.5 is as follows:


| Oscillator 1 | Oscillator 2 | Oscillator 3 | DCA-Filter | Modulation 1 | Modulation 2 | Envelope 1-3 | Envelope 2-4 
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| WAVEFORM | WAVEFORM | WAVEFORM | DCA1 VOL | LFO1 WAVE | LFO2 FREQ | ENV1 ATK | ENV2 ATK 
| OSC1 FINE | OSC2 FINE | OSC3 FINE | DCA2 VOL | LFO1 FREQ | LFO3 WAVE | ENV1 DEC | ENV2 DEC 
