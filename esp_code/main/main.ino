// User settable values
#define encoder0PinA  0
#define encoder0PinB  2
#define encoder0PinP  15
#define displayPinSDA 4
#define displayPinSCL 5
// The PUMP_PINS array defines pins to check, and also defines the number of pumps to keep track of.
// Number of pumps is limited by the number of pins available on controller AND BY MEMORY.
// Each additional pump needs an array of a full day to track it. Each uses 10.8 kbytes of ram.
// Most ESPs only have enough for 3-4 pumps
const int PUMP_PINS[] = {14, 16};
const int PUMP_CHECK_INTERVAL = 1000; // In ms
const int DATA_PUSH_INTERVAL = 500; // In seconds
const int DISPLAY_INTERVAL = 100; //In ms

// Stuff for Pump Calculations
#include <TaskScheduler.h>
#include <TaskSchedulerDeclarations.h>
#include <math.h>
#include <StandardCplusplus.h>
#include <bitset>
// Stuff for display
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
#include "OLEDDisplayUi.h"
#//include "images.h" //TODO: Use this when you draw your boat


// Initialize the OLED display
SSD1306  display(0x3c, displayPinSDA, displayPinSCL);
//Add a UI using above display
OLEDDisplayUi ui     ( &display );
// Volatile variable for tracking the encoder's position
volatile unsigned int encoder0Pos = 0;

const int NUM_PUMPS = (sizeof(PUMP_PINS) / sizeof(PUMP_PINS[0])); //C++ lame way of finding the size of an array
const int SECONDS_IN_MINUTE = 60;
const int MINUTES_IN_HOUR = 60;
const int HOURS_IN_DAY = 24;
const int DAYS_IN_WEEK = 7;
const int DAYS_IN_MONTH = 30;

const int SECONDS_IN_DAY = (SECONDS_IN_MINUTE * MINUTES_IN_HOUR * HOURS_IN_DAY);


std::bitset<SECONDS_IN_DAY> pump_arrays[NUM_PUMPS];
int seconds_on_by_day[30][NUM_PUMPS] = {0};
int seconds_since_day_shift = 0;
//int seconds_on_since_day_shift[NUM_PUMPS] = {0};
int number_of_days_for_average = {0};
int current_day = 0;
bool lcd_dot = false;

void check_pumps();
void update_displays();
void push_data();


Task pump_thread(PUMP_CHECK_INTERVAL, TASK_FOREVER, &check_pumps);
Task display_thread(DISPLAY_INTERVAL, TASK_FOREVER, &update_displays);
Task data_push_thread(DATA_PUSH_INTERVAL, TASK_FOREVER, &push_data);

Scheduler high_power_runner;
Scheduler low_power_runner;

bool HIGH_POWER = true;

void alarmOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, String(millis()));
}


void pumpFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_16);

  // The coordinates define the center of the text
  display->setTextAlignment(TEXT_ALIGN_CENTER);

  display->drawString(64 - x, 18, String(get_last_24_hours(0)));
  display->drawString(64 - x, 36, String(encoder0Pos));
}

void imageFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // Text alignment demo
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 - x, 32, String(encoder0Pos)); // The coordinates define the center of the text
}

FrameCallback frames[] = { pumpFrame, pumpFrame, pumpFrame, pumpFrame, pumpFrame, pumpFrame, imageFrame };
int frameCount = 7;

OverlayCallback overlays[] = { alarmOverlay };
int overlaysCount = 1;



bool check_physical_pump(int pump_number) {
  return digitalRead(PUMP_PINS[pump_number]);
  //return (int)random(2); //TODO: check actual pin
}

int get_last_n_days(int n, int pump_number) { // This whole bit I THINK is right... stupid arrays
  int total = 0;
  for (int i = 0; i <= n; i++) {
    int index;
    if (current_day - i < 0) { //If we're looping back over begining of array
      index = (sizeof(seconds_on_by_day) / sizeof(seconds_on_by_day[0])) + (current_day - i); // We look from the end of the array.
    } else {
      index = current_day - i; // Otherwise it's just the i count here here
    }
    total += seconds_on_by_day[index][pump_number]; // Pull from array using calculated index
  }
  //total += seconds_on_since_day_shift[pump_number];
  if (number_of_days_for_average == 0) {
    return total;
  } else if (n > number_of_days_for_average) {
    return total / number_of_days_for_average;
  } else return total / n;
}
int get_last_24_hours(int pump_number) {
  return pump_arrays[pump_number].count();
}

// - CALLBACKS FOR SCHEDULED TASKS
void update_displays() {
  int desired_frame = encoder0Pos % 7; //TODO: Set this 7 to the number of frames you lazy cunt
  OLEDDisplayUiState * current_state = ui.getUiState();
  if (current_state->currentFrame != desired_frame && current_state->frameState == FIXED) {
    ui.transitionToFrame(desired_frame);
  }

}
void check_pumps() {
  for (int i = 0; i < NUM_PUMPS; i++) {
    int immediate_pump_status = check_physical_pump(i);
    pump_arrays[i] <<= 1; // Shift array to the left, emptying out 24 hours ago data
    pump_arrays[i].set(0, immediate_pump_status); // Set rightmost bit (weird right, with the 0, you'd think it'd be on the left. That's why we shift left above) to the pump status
    //seconds_on_since_day_shift[i] += immediate_pump_status;
  }
  if (seconds_since_day_shift++ == SECONDS_IN_DAY) { //End of day
    //memset(seconds_on_since_day_shift, 0, sizeof(seconds_on_since_day_shift)); // Reset counters
    seconds_since_day_shift = 0;
    for (int i = 0; i < NUM_PUMPS; i++) {
      seconds_on_by_day[current_day][i] = pump_arrays[i].count();
    }
    if (current_day < 30) {
      current_day += 1;
    } else {
      current_day = 0;
    }
    if (number_of_days_for_average < 30) {
      number_of_days_for_average += 1;
    }
  }
}
void push_data() {
}

void doEncoderA() {
  // look for a low-to-high on channel A
  if (digitalRead(encoder0PinA) == HIGH) {

    // check channel B to see which way encoder is turning
    if (digitalRead(encoder0PinB) == LOW) {
      //encoder0Pos = encoder0Pos + 1;         // CW
    }
    else {
      encoder0Pos = encoder0Pos - 1;         // CCW
    }
  }

  else   // must be a high-to-low edge on channel A
  {
    // check channel B to see which way encoder is turning
    if (digitalRead(encoder0PinB) == HIGH) {
      encoder0Pos = encoder0Pos + 1;          // CW
    }
    else {
      //encoder0Pos = encoder0Pos - 1;          // CCW
    }
  }
}

void doEncoderB() {
  // look for a low-to-high on channel B
  if (digitalRead(encoder0PinB) == HIGH) {

    // check channel A to see which way encoder is turning
    if (digitalRead(encoder0PinA) == HIGH) {
      encoder0Pos = encoder0Pos + 1;         // CW
    }
    else {
      //encoder0Pos = encoder0Pos - 1;         // CCW
    }
  }

  // Look for a high-to-low on channel B

  else {
    // check channel B to see which way encoder is turning
    if (digitalRead(encoder0PinA) == LOW) {
      //encoder0Pos = encoder0Pos + 1;          // CW
    }
    else {
      encoder0Pos = encoder0Pos - 1;          // CCW
    }
  }
}


// SETUP/LOOP STUFF
void setup() {
  Serial.begin(115200);
  delay(20);
  Serial.println(); // Clear junk on boot
  Serial.println("Setup Start");
  Serial.println();

  high_power_runner.init();
  low_power_runner.init();
  Serial.println("Initialized schedulers");

  low_power_runner.addTask(pump_thread);
  low_power_runner.addTask(data_push_thread);
  pump_thread.enable();
  data_push_thread.enable();

  high_power_runner.addTask(display_thread);
  display_thread.enable();

  ui.setTargetFPS(30);
  ui.setFrames(frames, frameCount);
  ui.setOverlays(overlays, overlaysCount);
  ui.disableAutoTransition();
  ui.setTimePerTransition(200);
  ui.init();
  display.flipScreenVertically();

  pinMode(encoder0PinA, INPUT);
  pinMode(encoder0PinB, INPUT);
  pinMode(encoder0PinP, INPUT);
  attachInterrupt(0, doEncoderA, CHANGE);
  attachInterrupt(1, doEncoderB, CHANGE);

  delay(20);
  Serial.println("Setup Complete");
}
void loop() {
  //Always run low power thread
  low_power_runner.execute();
  if (HIGH_POWER) {
    high_power_runner.execute();
    int remainingTimeBudget = ui.update();
    if (remainingTimeBudget > 0) {
      delay(remainingTimeBudget);
    }
  }
}

