/*
       MIDI to CV with Autotune

       For RP2040

       Filesystem 1.5Mb/0.5Mb

      Version 1.1

      Copyright (C) 2020 Craig Barnes

      This program is free software: you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
      the Free Software Foundation, either version 3 of the License, or
      (at your option) any later version.

      This program is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      GNU General Public License <http://www.gnu.org/licenses/> for more details.
*/
#include <Arduino.h>
#include <iostream>
#include <cfloat>  // Include for DBL_MAX
#include <SPI.h>
#include <MIDI.h>
#include "Parameters.h"
#include "Hardware.h"
#include <Adafruit_NeoPixel.h>
#include <FS.h>
#include <LittleFS.h>

struct VoiceAndNote {
  int note;
  int velocity;
  long timeOn;
  bool sustained;  // Sustain flag
  bool keyDown;
  double noteFreq;  // Note frequency
  int position;
  bool noteOn;
};

// Struct to store note data with derivatives
struct NoteData {
  int value;
  double derivative;
};

struct VoiceAndNote voices[NO_OF_VOICES] = {
  { -1, -1, 0, false, false, 0, -1, false },
};

boolean voiceOn[NO_OF_VOICES] = { false };
bool notes[128] = { 0 }, initial_loop = 1;
int8_t noteOrder[40] = { 0 }, orderIndx = { 0 };
bool S1, S2;

// MIDI setup

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
const int channel = 1;

#define LED_PIN 23    // Define the pin for the built-in RGB LED
#define NUM_PIXELS 1  // Number of WS2812 LEDs

Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);

  while (!Serial)
    ;  // Wait for Serial to connect

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }

  Serial.println("LittleFS mounted successfully");

  pinMode(AUTO_INPUT, INPUT_PULLUP);

  SPI.setSCK(2);
  SPI.setRX(4);
  SPI.setTX(3);
  SPI.begin();

  delay(10);

  setupHardware();

  //MIDI 5 Pin DIN
  MIDI.begin(masterChan);
  MIDI.setHandleNoteOn(myNoteOn);
  MIDI.setHandleNoteOff(myNoteOff);
  MIDI.setHandlePitchBend(myPitchBend);
  MIDI.setHandleControlChange(myControlChange);
  MIDI.setHandleAfterTouchChannel(myAfterTouch);
  Serial.println("MIDI In DIN Listening");

  pixels.begin();
  delay(500);
  pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // GREEN
  pixels.show();

  loadTuningCorrectionsFromSD();
}

void readNoteCV() {
  // Step 1: Read the analog input (0-4095 for 12-bit ADC)
  int adcValue = analogRead(VOLT_OCT_INPUT);

  // Debug output for the ADC value
  Serial.print("ADC Value: ");
  Serial.println(adcValue);

  // Step 2: Convert the ADC value to voltage
  float voltage = (adcValue / (float)4096) * ADC_REF_VOLTAGE;

  // Step 3: Map voltage (1V/octave) to MIDI note (12 notes per octave)
  int midiNote = (int)(voltage * 12.0);

  // Debug output for calculated MIDI note
  Serial.print("Calculated MIDI Note: ");
  Serial.println(midiNote);

  // Step 4: Constrain the MIDI note to the valid range (0-127)
  midiNote = constrain(midiNote, 0, 127);

  // Step 5: Avoid retriggers for the same note
  if (midiNote != previousMidiNote) {
    if (midiNote > 0) {                     // Only trigger if the note is above 0
      myNoteOn(masterChan, midiNote, 127);  // Send MIDI Note On
      if (previousMidiNote >= 0) {
        myNoteOff(masterChan, previousMidiNote, 127);  // Turn off the previous note
      }
    }
    previousMidiNote = midiNote;  // Update the last MIDI note
  }
}

void saveTuningCorrectionsToSD() {
  // Remove the file if it exists
  if (LittleFS.exists("tuning.txt")) {
    LittleFS.remove("tuning.txt");
  }

  // Open the file for writing
  File file = LittleFS.open("tuning.txt", "w");
  if (file) {
    for (int o = 0; o < (numOscillators + 1); o++) {
      for (int i = 0; i < 128; i++) {
        file.print(i);                       // Note
        file.print(",");                     // Delimiter
        file.print(o);                       // Oscillator
        file.print(",");                     // Delimiter
        file.println(autotune_value[i][o]);  // Value
      }
    }
    file.close();
    Serial.println("Autotune values saved to LittleFS.");
  } else {
    Serial.println("Error opening file for writing.");
  }
}

void loadTuningCorrectionsFromSD() {
  // Open the file for reading
  File file = LittleFS.open("tuning.txt", "r");
  if (file) {
    while (file.available()) {
      // Read a line from the file
      String line = file.readStringUntil('\n');

      // Parse the line
      int firstComma = line.indexOf(',');       // First delimiter
      int secondComma = line.lastIndexOf(',');  // Second delimiter

      // Extract note, oscillator, and value
      if (firstComma != -1 && secondComma != -1 && secondComma > firstComma) {
        int note = line.substring(0, firstComma).toInt();
        int osc = line.substring(firstComma + 1, secondComma).toInt();
        int value = line.substring(secondComma + 1).toInt();

        // Validate indices and populate the array
        if (note >= 0 && note < 128 && osc >= 0 && osc <= numOscillators) {
          autotune_value[note][osc] = value;
        } else {
          Serial.println("Invalid data in tuning.txt");
        }
      }
    }
    file.close();
    Serial.println("Tuning corrections loaded from LittleFS.");
  } else {
    Serial.println("Failed to open tuning.txt on LittleFS.");
  }
}

void startAutotune() {
  pixels.setPixelColor(0, pixels.Color(255, 0, 0));  // RED
  pixels.show();
  allNotesOff();
  autotuneStart = true;
  oscillator = 0;
  tuneNote = 0;
  setVCOStolowestA();
  delay(200);
  workingNote = A_NOTES[tuneNote];
  targetFrequency = midi_to_freqs[workingNote][1];
  digitalWrite(VCO_SEL, LOW);
  digitalWrite(MUX_ENABLE, LOW);
  delayMicroseconds(200);
}

void autotune() {
  int midiNote = A_NOTES[tuneNote];
  double targetFrequency = midi_to_freqs[midiNote][1];
  Serial.print("MIDI Note: ");
  Serial.println(midiNote);
  Serial.print("Target Frequency: ");
  Serial.println(targetFrequency);

  setOscillator(midiNote, oscillator);
  delay(10);  // Allow stabilization after setting the oscillator

  float initialFreq = directCount(targetFrequency);
  //double initialFreq = directCountAverage(3);  // Average initial measurement
  Serial.print("Initial Frequency: ");
  Serial.println(initialFreq);

  int minError = -1023;
  int maxError = 1023;
  int bestError = 0;
  double bestFreqDiff = DBL_MAX;

  while (minError <= maxError) {
    int midError = (minError + maxError) / 2;
    updateOscillator(midiNote, midError, oscillator);
    delay(10);  // Allow oscillator to stabilize

    // Take multiple frequency measurements and average them
    double measuredFreq = directCount(targetFrequency);
    double freqDiff = abs(measuredFreq - targetFrequency);

    Serial.print("Measured Frequency: ");
    Serial.print(measuredFreq);
    Serial.print(" | Frequency Difference: ");
    Serial.println(freqDiff);

    if (freqDiff < bestFreqDiff) {
      bestFreqDiff = freqDiff;
      bestError = midError;
    }

    // Adjust the binary search range
    if (measuredFreq < targetFrequency) {
      minError = midError + 1;  // Increase offset
    } else if (measuredFreq > targetFrequency) {
      maxError = midError - 1;  // Decrease offset
    } else {
      bestError = midError;  // Exact match found
      break;
    }
  }

  // Save the best error value
  autotune_value[midiNote][oscillator] = bestError;
  Serial.print("Oscillator: ");
  Serial.print(oscillator);
  Serial.print(" | Note: ");
  Serial.print(midiNote);
  Serial.print(" | Best Tune Offset: ");
  Serial.println(bestError);

  // Move to the next note/oscillator
  tuneNote++;
  if (tuneNote >= A_NOTES_COUNT) {
    tuneNote = 0;
    oscillator++;
    pixels.setPixelColor(0, pixels.Color(0, 0, 255));  // BLUE
    pixels.show();
    Serial.print("Oscillator: ");
    Serial.println(oscillator);
    if (oscillator > numOscillators) {
      autotuneStart = false;  // Autotune completed
      extrapolateNotes();
      Serial.println("Auto Tune Finished");
      pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // GREEN
      pixels.show();
      digitalWrite(MUX_ENABLE, HIGH);
    } else {
      // Switch to the next oscillator
      selectMuxInput();
      delay(10);  // Stabilize MUX switching
    }
  }
}

void setOscillator(int midiNote, int oscillator) {
  switch (oscillator) {
    case 0:
      setDAC(DAC_NOTE1, (midiNote - 36), 0, 0, oscillator1a);
      break;
    case 1:
      setDAC(DAC_NOTE1, (midiNote - 36), 0, 1, oscillator1b);
      break;
  }
}

void updateOscillator(int note, int error, int oscillator) {
  switch (oscillator) {
    case 0:
      setDAC(DAC_NOTE1, (note - 36), error, 0, oscillator1a);
      break;
    case 1:
      setDAC(DAC_NOTE1, (note - 36), error, 1, oscillator1b);
      break;
  }
}

void setDAC(int chipSelect, int note, int error, int oscillator, float SF) {
  switch (oscillator) {
    case 0:
      mV = (unsigned int)(((float)(note)*NOTE_SF * SF + 0.5) + (VOLTOFFSET + error));
      sample_data = (channel_a & 0xFFF0000F) | (((mV)&0xFFFF) << 4);
      outputDAC(chipSelect, sample_data);
      break;
    case 1:
      mV = (unsigned int)(((float)(note)*NOTE_SF * SF + 0.5) + (VOLTOFFSET + error));
      sample_data = (channel_b & 0xFFF0000F) | (((mV)&0xFFFF) << 4);
      outputDAC(chipSelect, sample_data);
      break;
  }
}

//////////////////////////////////////////////////////////////////////////////

void extrapolateNotes() {
  for (int o = 0; o <= numOscillators; o++) {
    // Extrapolate and interpolate across the full MIDI note range (0-127)
    for (int i = 0; i < 128; i++) {
      // Find the nearest lower and higher known notes in A_NOTES
      int lowerNote = -1;
      int higherNote = -1;

      for (int j = 0; j < A_NOTES_COUNT; j++) {
        if (A_NOTES[j] <= i) {
          lowerNote = A_NOTES[j];
        }
        if (A_NOTES[j] > i) {
          higherNote = A_NOTES[j];
          break;
        }
      }

      if (lowerNote != -1 && higherNote != -1) {
        // Interpolation between lowerNote and higherNote
        int lowerValue = autotune_value[lowerNote][o];
        int higherValue = autotune_value[higherNote][o];
        double t = static_cast<double>(i - lowerNote) / (higherNote - lowerNote);
        autotune_value[i][o] = constrain(
          static_cast<int>(lowerValue + t * (higherValue - lowerValue)),
          -32768, 32767);
      } else if (lowerNote != -1) {
        // Extrapolation below the first known note
        int lowerValue = autotune_value[lowerNote][o];
        autotune_value[i][o] = constrain(lowerValue, -32768, 32767);
      } else if (higherNote != -1) {
        // Extrapolation above the last known note
        int higherValue = autotune_value[higherNote][o];
        autotune_value[i][o] = constrain(higherValue, -32768, 32767);
      }
    }
  }

  saveTuningCorrectionsToSD();
}

//////////////////////////////////////////////////////////////////////

void loadSDCardNow() {
  loadTuningCorrectionsFromSD();
}

void debugFrequencyReading() {
  float frequency = directCount(targetFrequency);  // Measure frequency
  Serial.print("Frequency: ");
  Serial.print(frequency, 1);  // Print frequency with 0.1 Hz resolution
  Serial.println(" Hz");
}

void my_isr() {
  count++;  // Increment count on each rising edge
}

float directCount(float targetFreq) {
  count = 0;  // Reset count

  // Dynamically calculate measurement time in microseconds
  uint32_t measurementDuration;
  if (targetFreq <= 50.0) {
    measurementDuration = 2E6;  // 2 seconds for low frequencies (<= 50 Hz)
  } else if (targetFreq <= 100.0) {
    measurementDuration = 1E6;  // 1 second for moderate frequencies
  } else {
    measurementDuration = 5E5;  // 500 ms for higher frequencies
  }

  uint32_t startTime = micros();
  uint32_t targetTime = startTime + measurementDuration;

  // Enable interrupt for rising edge detection
  attachInterrupt(digitalPinToInterrupt(AUTO_INPUT), my_isr, RISING);

  // Wait for the measurement duration
  while (micros() - startTime < measurementDuration) {}

  // Disable interrupt to stop counting
  detachInterrupt(digitalPinToInterrupt(AUTO_INPUT));

  // Calculate frequency based on the measurement time
  float frequency = static_cast<float>(count) * (1E6 / measurementDuration);
  return frequency;
}

void loop() {

  if (autotuneStart) {
    autotune();
  } else {
    updateTimers();
    MIDI.read(masterChan);  //MIDI 5 Pin DIN
    //readNoteCV();
    mod_task();
    adjustInterval();
    updateVoice1();
  }
}

void setVCOStolowestA() {
  mV = (unsigned int)(((float)(33) * NOTE_SF * oscillator1a + 0.5) + (VOLTOFFSET + autotune_value[33][0]));
  sample_data1 = (channel_a & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data1);
  mV = (unsigned int)(((float)(33) * NOTE_SF * oscillator1b + 0.5) + (VOLTOFFSET + autotune_value[33][1]));
  sample_data1 = (channel_b & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data1);
}

void ResetAutoTuneValues() {
  for (int o = 0; o < (numOscillators + 1); o++) {
    for (int i = 0; i < 128; i++) {
      (autotune_value[i][o]) = 0;
    }
  }
  Serial.print("All Autotune Values are 0");
  Serial.println();
  Serial.print("Writing all 0 to SD card");
  Serial.println();
  saveTuningCorrectionsToSD();
}

void DisplayAutoTuneValues() {

  Serial.println("=== Autotune Values ===");
  if (displayvalues) {
    for (int o = 0; o < (numOscillators + 1); o++) {
      Serial.print("Oscillator ");
      Serial.print(o);
      Serial.println(":");
      for (int i = 0; i < 128; i++) {
        Serial.print("  Note ");
        Serial.print(i);
        Serial.print(" ");
        Serial.println(autotune_value[i][o]);
      }
    }
  }
}

void allowOsc1Through() {

  if (osc1Through) {
    digitalWrite(VCO_SEL, LOW);
    delayMicroseconds(100);
    digitalWrite(MUX_ENABLE, LOW);
  } else {
    digitalWrite(VCO_SEL, LOW);
    digitalWrite(MUX_ENABLE, HIGH);
  }
}

void myPitchBend(byte channel, int bend) {
  if ((channel == masterChan) || (masterChan == 0)) {
    switch (BEND_WHEEL) {
      case 12:
        bend_data = map(bend, -8192, 8191, -3235, 3235);
        break;

      case 11:
        bend_data = map(bend, -8192, 8191, -2970, 2969);
        break;

      case 10:
        bend_data = map(bend, -8192, 8191, -2700, 2699);
        break;

      case 9:
        bend_data = map(bend, -8192, 8191, -2430, 2429);
        break;

      case 8:
        bend_data = map(bend, -8192, 8191, -2160, 2159);
        break;

      case 7:
        bend_data = map(bend, -8192, 8191, -1890, 1889);
        break;

      case 6:
        bend_data = map(bend, -8192, 8191, -1620, 1619);
        break;

      case 5:
        bend_data = map(bend, -8192, 8191, -1350, 1349);
        break;

      case 4:
        bend_data = map(bend, -8192, 8191, -1080, 1079);
        break;

      case 3:
        bend_data = map(bend, -8192, 8191, -810, 809);
        break;

      case 2:
        bend_data = map(bend, -8192, 8191, -540, 539);
        break;

      case 1:
        bend_data = map(bend, -8192, 8191, -270, 270);
        break;

      case 0:
        bend_data = 0;
        break;
    }
    // sample_data = (channel_a & 0xFFF0000F) | (((int(bend * 0.395) + 13180) & 0xFFFF) << 4);
    // outputDAC(DAC_NOTE5, sample_data);
  }
}

void adjustInterval() {
  if (INTERVAL_POT != INTERVAL) {
    Serial.print("Interval setting ");
    Serial.println(INTERVAL_POT);
  }
  switch (INTERVAL_POT) {
    case 12:
      INTERVAL = 12;
      break;

    case 11:
      INTERVAL = 11;
      break;

    case 10:
      INTERVAL = 10;
      break;

    case 9:
      INTERVAL = 9;
      break;

    case 8:
      INTERVAL = 8;
      break;

    case 7:
      INTERVAL = 7;
      break;

    case 6:
      INTERVAL = 6;
      break;

    case 5:
      INTERVAL = 5;
      break;

    case 4:
      INTERVAL = 4;
      break;

    case 3:
      INTERVAL = 3;
      break;

    case 2:
      INTERVAL = 2;
      break;

    case 1:
      INTERVAL = 1;
      break;

    case 0:
      INTERVAL = 0;
      break;
  }
}


void myControlChange(byte channel, byte number, byte value) {
  if ((channel == masterChan) || (masterChan == 0)) {
    switch (number) {

      case 1:
        FM_RANGE_UPPER = int(value * FM_MOD_WHEEL);
        FM_RANGE_LOWER = (FM_RANGE_UPPER - FM_RANGE_UPPER - FM_RANGE_UPPER);
        TM_RANGE_UPPER = int(value * TM_MOD_WHEEL);
        TM_RANGE_LOWER = (TM_RANGE_UPPER - TM_RANGE_UPPER - TM_RANGE_UPPER);
        break;

      case 5:                                           // Portamento time
        portamentoTime = map(value, 0, 127, 0, 10000);  // Map to a max of 2 seconds
        Serial.print("Portamento Time: ");
        Serial.println(portamentoTime);
        break;

      case 14:
        INTERVAL_POT = map(value, 0, 127, 0, 12);
        break;

      case 15:
        DETUNE = value;
        break;

      case 16:
        BEND_WHEEL = map(value, 0, 127, 0, 12);
        break;

      case 17:
        FM_MOD_WHEEL = map(value, 0, 127, 0, 16.2);
        break;

      case 18:
        TM_MOD_WHEEL = map(value, 0, 127, 0, 16.2);
        break;

      case 19:
        FM_AT_WHEEL = map(value, 0, 127, 0, 16.2);
        break;

      case 20:
        TM_AT_WHEEL = map(value, 0, 127, 0, 16.2);
        break;

      case 21:
        value = map(value, 0, 127, 0, 2);
        switch (value) {
          case 0:
            OCTAVE_A = -12;
            break;
          case 1:
            OCTAVE_A = 0;
            break;
          case 2:
            OCTAVE_A = 12;
            break;
        }
        break;

      case 22:
        value = map(value, 0, 127, 0, 2);
        switch (value) {
          case 0:
            OCTAVE_B = -12;
            break;
          case 1:
            OCTAVE_B = 0;
            break;
          case 2:
            OCTAVE_B = 12;
            break;
        }
        break;

      case 65:  // Portamento on/off
        switch (value) {
          case 127:
            portamentoOn = true;
            Serial.print("Portamento On: ");
            Serial.println(portamentoOn);
            break;
          case 0:
            isPortamentoActive = false;
            portamentoOn = false;
            Serial.print("Portamento Off: ");
            Serial.println(portamentoOn);
            break;
        }
        break;

      case 121:
        switch (value) {
          case 127:
            startAutotune();
            break;
        }
        break;

      case 122:
        switch (value) {
          case 127:
            ResetAutoTuneValues();
            break;
        }
        break;

      case 127:
        keyboardMode = map(value, 0, 127, 0, 2);
        if (keyboardMode > 0 && keyboardMode < 3) {
          allNotesOff();
        }
        break;
    }
  }
}

void myAfterTouch(byte channel, byte value) {
  if ((channel == masterChan) || (masterChan == 0)) {
    FM_AT_RANGE_UPPER = int(value * FM_AT_WHEEL);
    FM_AT_RANGE_LOWER = (FM_AT_RANGE_UPPER - FM_AT_RANGE_UPPER - FM_AT_RANGE_UPPER);
    TM_AT_RANGE_UPPER = int(value * TM_AT_WHEEL);
    TM_AT_RANGE_LOWER = (TM_AT_RANGE_UPPER - TM_AT_RANGE_UPPER - TM_AT_RANGE_UPPER);
  }
}

void mod_task() {

  MOD_VALUE = analogRead(FM_INPUT);
  FM_VALUE = map(MOD_VALUE, 0, 4095, FM_RANGE_LOWER, FM_RANGE_UPPER);
  FM_AT_VALUE = map(MOD_VALUE, 0, 4095, FM_AT_RANGE_LOWER, FM_AT_RANGE_UPPER);
  TM_VALUE = map(MOD_VALUE, 0, 4095, TM_RANGE_LOWER, TM_RANGE_UPPER);
  TM_AT_VALUE = map(MOD_VALUE, 0, 4095, TM_AT_RANGE_LOWER, TM_AT_RANGE_UPPER);
}

void commandTopNote() {
  int topNote = 0;
  bool noteActive = false;

  for (int i = 0; i < 128; i++) {
    if (notes[i]) {
      topNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNote(topNote);
  } else {  // All notes are off, turn off gate
    digitalWrite(GATE_NOTE1, LOW);
  }
}

void commandBottomNote() {
  int bottomNote = 0;
  bool noteActive = false;

  for (int i = 87; i >= 0; i--) {
    if (notes[i]) {
      bottomNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNote(bottomNote);
  } else {  // All notes are off, turn off gate
    digitalWrite(GATE_NOTE1, LOW);
  }
}

void commandLastNote() {

  int8_t noteIndx;

  for (int i = 0; i < 40; i++) {
    noteIndx = noteOrder[mod(orderIndx - i, 40)];
    if (notes[noteIndx]) {
      commandNote(noteIndx);
      return;
    }
  }
  digitalWrite(GATE_NOTE1, LOW);  // All notes are off
}

void commandNote(int noteMsg) {
  note1 = noteMsg;
  digitalWrite(GATE_NOTE1, HIGH);
  digitalWrite(TRIG_NOTE1, HIGH);
  noteTrig[0] = millis();
}

void commandTopNoteUni() {
  int topNote = 0;
  bool noteActive = false;

  for (int i = 0; i < 128; i++) {
    if (notes[i]) {
      topNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNoteUni(topNote);
  } else {  // All notes are off, turn off gate
    digitalWrite(GATE_NOTE1, LOW);
  }
}

void commandBottomNoteUni() {
  int bottomNote = 0;
  bool noteActive = false;

  for (int i = 87; i >= 0; i--) {
    if (notes[i]) {
      bottomNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNoteUni(bottomNote);
  } else {  // All notes are off, turn off gate
    digitalWrite(GATE_NOTE1, LOW);
  }
}

void commandLastNoteUni() {

  int8_t noteIndx;

  for (int i = 0; i < 40; i++) {
    noteIndx = noteOrder[mod(orderIndx - i, 40)];
    if (notes[noteIndx]) {
      commandNoteUni(noteIndx);
      return;
    }
  }
  digitalWrite(GATE_NOTE1, LOW);
}

void commandNoteUni(int noteMsg) {

  note1 = noteMsg;

  digitalWrite(TRIG_NOTE1, HIGH);
  noteTrig[0] = millis();
  digitalWrite(GATE_NOTE1, HIGH);
}

void myNoteOn(byte channel, byte note, byte velocity) {

  //Check for out of range notes
  if (note < 0 || note > 127) return;

  switch (keyboardMode) {

    case 0:
    case 1:
    case 2:
      if (keyboardMode == 0) {
        S1 = 1;
        S2 = 1;
      }
      if (keyboardMode == 1) {
        S1 = 0;
        S2 = 1;
      }
      if (keyboardMode == 2) {
        S1 = 0;
        S2 = 0;
      }
      noteMsg = note;

      if (velocity == 0) {
        notes[noteMsg] = false;
      } else {
        notes[noteMsg] = true;
      }
      voices[0].velocity = velocity;

      if (S1 && S2) {  // Highest note priority
        commandTopNote();
      } else if (!S1 && S2) {  // Lowest note priority
        commandBottomNote();
      } else {                 // Last note priority
        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx + 1) % 40;
          noteOrder[orderIndx] = noteMsg;
        }
        commandLastNote();
      }
      break;
  }
}

void myNoteOff(byte channel, byte note, byte velocity) {
  switch (keyboardMode) {
    case 0:
    case 1:
    case 2:
      if (keyboardMode == 0) {
        S1 = 1;
        S2 = 1;
      }
      if (keyboardMode == 1) {
        S1 = 0;
        S2 = 1;
      }
      if (keyboardMode == 2) {
        S1 = 0;
        S2 = 0;
      }
      noteMsg = note;

      notes[noteMsg] = false;

      if (S1 && S2) {  // Highest note priority
        commandTopNote();
      } else if (!S1 && S2) {  // Lowest note priority
        commandBottomNote();
      } else {                 // Last note priority
        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx + 1) % 40;
          noteOrder[orderIndx] = noteMsg;
        }
        commandLastNote();
      }
      break;
  }
}

void updateTimers() {

  if (millis() > noteTrig[0] + trigTimeout) {
    digitalWrite(TRIG_NOTE1, LOW);  // Set trigger low after 20 msec
  }
}

void updateVoice1() {
  oscanote1 = note1 + OCTAVE_A;
  mV = (unsigned int)(((float)(oscanote1 + transpose + realoctave) * NOTE_SF * oscillator1a + 0.5) + (VOLTOFFSET + bend_data + FM_VALUE + FM_AT_VALUE) + (autotune_value[oscanote1][0]));
  sample_data1 = (channel_a & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data1);
  oscbnote1 = note1 + INTERVAL + OCTAVE_B;
  mV = (unsigned int)(((float)(oscbnote1 + transpose + realoctave) * NOTE_SF * oscillator1b + 0.5) + (VOLTOFFSET + bend_data + FM_VALUE + FM_AT_VALUE + DETUNE) + (autotune_value[oscbnote1][1]));
  sample_data1 = (channel_b & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data1);
  mV = (unsigned int)(((float)(note1 + transpose + realoctave) * NOTE_SF * 1.00 + 0.5) + (TM_VALUE));
  sample_data1 = (channel_c & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data1);
  velmV = ((unsigned int)((float)voices[0].velocity) * VEL_SF);
  vel_data1 = (channel_d & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, vel_data1);
}

void allNotesOff() {
  digitalWrite(GATE_NOTE1, LOW);

  voices[0].note = -1;

  voiceOn[0] = false;
}

void outputDAC(int CHIP_SELECT, uint32_t sample_data) {
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE1));
  digitalWrite(CHIP_SELECT, LOW);
  delayMicroseconds(1);
  SPI.transfer16(sample_data >> 16);
  SPI.transfer16(sample_data);
  delayMicroseconds(3);  // Settling time delay
  digitalWrite(CHIP_SELECT, HIGH);
  SPI.endTransaction();
}

int mod(int a, int b) {
  int r = a % b;
  return r < 0 ? r + b : r;
}

void selectMuxInput() {
  switch (oscillator) {

    // Board 1 A
    case 0:
      digitalWrite(VCO_SEL, LOW);
      break;

    // Board 1 B
    case 1:
      digitalWrite(VCO_SEL, HIGH);
      break;
  }
}
