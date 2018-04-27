#define encoder0PinA  0
#define encoder0PinB  2
#define encoder0PinP  15
#define displayPinSDA 4
#define displayPinSCL 5
#define dispaly_is_bicolour true
#define enable_serial_out true
#define debug_mode true

// The PUMP_PINS array defines pins to check, and also defines the number of pumps to keep track of.
// Number of pumps is limited by the number of pins available on controller AND BY MEMORY.
// Each additional pump needs an array of a full day to track it. Each uses 10.8 kbytes of ram.
// Most ESPs only have enough for 3-4 pumps
const int PUMP_PINS[] = {14, 16};
const int PUMP_CHECK_INTERVAL = 1000; // In ms
const int DATA_PUSH_INTERVAL = 500; // In seconds
const int DISPLAY_INTERVAL = 200; //In ms

const String bodyText [] = {"Last 24 hours", "Last 7 days", "Last 30 days"};
// Stuff for Pump Calculations
#include <TaskScheduler.h>
#include <TaskSchedulerDeclarations.h>
#include <math.h>
//#include <StandardCplusplus.h>
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

void pumpFrame0(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  pumpFrame_template(display, state, x, y, 0);
}
void pumpFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  pumpFrame_template(display, state, x, y, 1);
}
void pumpFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  pumpFrame_template(display, state, x, y, 2);
}

void pumpFrame_template(OLEDDisplay * display, OLEDDisplayUiState * state, int16_t x, int16_t y, int frame_type) {
  int values[NUM_PUMPS];
  for (int i = 0; i < NUM_PUMPS; i++) {
    switch (frame_type) {
      case 0:
        values[i] = get_last_24_hours(i);
        break;
      case 1:
        values[i] = get_last_n_days(7, i);
        break;
      case 2:
        values[i] = get_last_n_days(30, i);
        break;
    }
  }
  // The coordinates define the center of the text
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawVerticalLine(64 + x, 16 + y, 32);
  display->setFont(ArialMT_Plain_10);
  display->drawString(32 + x, 14 + y, "Main");
  display->drawString(96 + x, 14 + y, "Backup");
  display->setFont(ArialMT_Plain_16);
  display->drawString(32 + x, 30 + y, String(values[0]));
  display->drawString(96 + x, 30 + y, String(values[1]));
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 52 + y, bodyText[frame_type]);

}

void frame_counter(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0, 0, String(state->currentFrame));
}

void alarmOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {

}

FrameCallback frames[] = {pumpFrame0, pumpFrame1, pumpFrame2};
int frameCount = 3;

OverlayCallback overlays[] = { alarmOverlay, frame_counter };
int overlaysCount = 2;



bool check_physical_pump(int pump_number) {
  return digitalRead(PUMP_PINS[pump_number]);
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
  int values[NUM_PUMPS];
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < NUM_PUMPS; i++) {
      switch (j) {
        case 0:
          values[i] = get_last_24_hours(i);
          break;
        case 1:
          values[i]=get_last_n_days(7,i);
          break;
        case 2:
          values[i]=get_last_n_days(30,i);
          break;
      }
    }
  //Serial.println(bodyText[j]);
  //Serial.print("0: ");
  //Serial.println(String(values[0]));
  //Serial.print("1: ");
  //Serial.println(String(values[1]));
  }
  //Serial.println();
  //Serial.println();
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
/* --- Start inturupts section --- */
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

/* --- End inturupts section --- */

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

  //ui.setTargetFPS(50);
  ui.setFrames(frames, frameCount);
  ui.setOverlays(overlays, overlaysCount);
  ui.disableAutoTransition();
  //ui.setTimePerTransition(100);
  ui.setFrameAnimation(SLIDE_UP);
  ui.setIndicatorPosition(RIGHT);

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
void changeFrame() {
  int desired_frame = encoder0Pos % frameCount;
  OLEDDisplayUiState * current_state = ui.getUiState();
  if (current_state->currentFrame != desired_frame && current_state->frameState == FIXED) {
    //ui.switchToFrame(desired_frame);
    ui.transitionToFrame(desired_frame);

  }
}
void loop() {
  //Always run low power thread
  low_power_runner.execute();
  if (HIGH_POWER) {
    // Run high power stuff.
    high_power_runner.execute();
    changeFrame();
    int remainingTimeBudget = ui.update();
    if (remainingTimeBudget > 0) {
      delay(remainingTimeBudget);//TODO: Delay this count for frame, or the next time we need to run a task. Figure that out
    }
  }
}

