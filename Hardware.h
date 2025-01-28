// #define NOTE_SF 137.2 // 1/2v octave
#define NOTE_SF 274.4 // 1V/Octave
#define VEL_SF 256.0

#define CV_MAX_VOLTAGE 5.0  // Maximum input voltage for 5 octaves
#define ADC_REF_VOLTAGE 3.3 // Reference voltage (adjust for your board)

#define FORMAT_LITTLEFS_IF_FAILED true

// Voices available
#define NO_OF_VOICES 1
#define trigTimeout 20

#define FM_INPUT 29
#define VOLT_OCT_INPUT 28

//Note DACS
#define DAC_NOTE1 5

//VCO 1 or 2
#define VCO_SEL 6

//Autotune input pin
#define AUTO_INPUT 7

//Autotune button
#define AUTO_BUTTON 8

//Trig output
#define TRIG_NOTE1 10

//Tune LED for front panel
#define TUNE_LED 11

// Mux enable
#define MUX_ENABLE 12

//Gate outputs
#define GATE_NOTE1 13

//Gate outputs
#define SYNC_PIN 14


void setupHardware() {

  analogReadResolution(12);

  pinMode(AUTO_BUTTON, INPUT_PULLUP);

  pinMode(GATE_NOTE1, OUTPUT);
  digitalWrite(GATE_NOTE1, LOW);

  pinMode(TRIG_NOTE1, OUTPUT);
  digitalWrite(TRIG_NOTE1, LOW);

  pinMode(VCO_SEL, OUTPUT);
  digitalWrite(VCO_SEL, LOW);

  pinMode(MUX_ENABLE, OUTPUT);
  digitalWrite(MUX_ENABLE, LOW);

  pinMode(SYNC_PIN, OUTPUT);
  digitalWrite(SYNC_PIN, LOW);

  pinMode(TUNE_LED, OUTPUT);
  digitalWrite(TUNE_LED, LOW);

  pinMode(DAC_NOTE1, OUTPUT);
  digitalWrite(DAC_NOTE1, HIGH);

  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE1));
  digitalWrite(DAC_NOTE1, LOW);
  SPI.transfer16(int_ref_on_flexible_mode >> 16);
  SPI.transfer16(int_ref_on_flexible_mode);
  delayMicroseconds(1);
  digitalWrite(DAC_NOTE1, HIGH);
  SPI.endTransaction();

}