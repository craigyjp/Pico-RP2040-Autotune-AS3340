# 16bit-1-note-MIDI-to-CV with AS3340 VCOs

A spin off from my 8 note poly MIDI to CV, a standalone dual oscillator with autotune.

Using a Waveshare Zero Rp2040, the CV's, Pitchbend, CC, gates and triggers will all need some level conversion in hardware which I've covered in the schematic PDF. I've used matching 12k/36k resistors on the CV DAC level converters to give 4x conversion and this gives 1v/octave, Velocity uses 10k/10k for 2x conversion for 0-5v velocity range.

# The Waveshare should be setup with a Flash Size of 1.5Mb/512Kb FS

* The triggers and gates are currently +3.3v.
* 2 CV ouputs for osc1 & osc2.
* 1 filter CV outputs
* 1 velocity CV outputs
* 1 gate outputs
* 1 trigger outputs
* Pitchbend, channel aftertouch and CC outputs
* Oscillator 2 detune 
* Oscillator 2 interval settings 0-12 semitones 

# During the testing all the controls were done manually with pots and buttons, but to be integrated into a synth these functions need to be controlled over MIDI.

* MIDI CC numbers used to control functions

* CC 01  Modulation Wheel 0-12
* CC 05 Portamento Time
* CC 14  VCO_B Interval. 0-127 (0-12 semitones)
* CC 15  VCO_B Detune. 0-12
* CC 16  PitchBend Depth. 0-127 (0-12 seimitones)
* CC 17  FM Mod Wheel Depth. 0-127
* CC 18  TM Mod Wheel Depth. 0-127
* CC 19  FM Aftertouch Depth. 0-127
* CC 20  TM Aftertouch Depth. 0-127
* CC 21  VCO_A Octave switch. (0-127) -1, 0, +1
* CC 22  VCO_B Octave switch. (0-127) -1, 0, +1
* CC 65  Portamento Switch (0/127) Off/On
* CC 121 Start Autotune Routine.  start 127
* CC 122 Clear Autotune values. start 127
* CC 127 Keyboard Mode  0-127, Mono Top, Bottom, Last (Default)
