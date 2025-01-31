# 16bit-1-note-MIDI-to-CV with AS3340 VCOs

A spin off from my 8 note poly MIDI to CV, a standalone dual oscillator with autotune.

Using a YD-RP2040, the CV's, Pitchbend, CC, gates and triggers will all need some level conversion in hardware which I've covered in the schematic PDF. I've used matching 12k/36k resistors on the CV DAC level converters to give 4x conversion and this gives 1v/octave, Velocity uses 10k/10k for 2x conversion for 0-5v velocity range.

# The YD-RP2040 should be setup with a Flash Size of 1.5Mb/512Kb FS or something similar to allow storage.

* The triggers and gates are currently +3.3v.
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
* CC 23  Pulse Width VCO_A. (0-127)
* CC 24  PWM depth VCO_A. (0-127)
* CC 23  Pulse Width VCO_B. (0-127)
* CC 24  PWM depth VCO_B. (0-127) 
* CC 65  Portamento Switch (0/127) Off/On
* CC 121 Start Autotune Routine.  start 127
* CC 122 Clear Autotune values. start 127
* CC 123 All notes Off
* CC 127 Keyboard Mode  0-127, Mono Top, Bottom, Last (Default)

# Calibration

In normal operation the autotune LED will be unlit, the pixel LED on the YD-RP2040 will be green, when auto tune is active the autotune LED will illuminate and the pixel LED will change to red until the first oscillator has been tuned and then it will change to blue for the second oscillator, finally it will return to green when the auto tune is complete. Also the autotune LED will extinguish and you can play notes again on the keyboard.

After construction, using a midi keyboard tune the VCO's using the Tune trimmer to 27.5 Hz when note 33 is played, then retune with the scale pot to 880 Hz when playing note 93. Repeat the process for both oscillators until they are approximately in tune at both ends of the keyboard. Press the autotune button and measure with a scope the signal coming out of pin 4 of the 74HC14, its should be a clean pulse of about 10-15% duty cycle. Adjust the autotune pot to get the required pulse width. There might be nothing there until the trimmer is adjusted to trigger the input on pin 1 of the 74HC14. When you have a good signal on the output of the 74HC14 you can run auto tune again as it should be able to now pull in the notes.

FM input, with all modulation controls set to minimum, adjust the offset trimmer on the FM input to measure 1.65V on the FM input (pin 29) of the YD-RP2040. This allows the LFO signal to swing positive and negative.

PWM input, as with the FM input, adjust the trimmer for PWM to measure 1.65V on the PWM input (pin 26) of the YD-RP2040. This allows the modulation source of the PWM to swing positive and negative.
