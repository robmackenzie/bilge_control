#include <TaskScheduler.h>
#include <TaskSchedulerDeclarations.h>

#include <math.h>
#include <StandardCplusplus.h>
#include <bitset>



const int PUMP_PINS[] = {1, 2}; //This array defines pins to check, and also defines the number of pumps to keep track of.
// Number of pumps is limited by the number of pins available on controller AND BY MEMORY. Each additional pump needs an array of a full day to track it. each uses 10.8 kbytes of ram.
const int PUMP_CHECK_INTERVAL = 1000;
const int DATA_PUSH_INTERVAL = 500; // In seconds
const int DISPLAY_INTERVAL = 1000; //In ms


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

bool check_physical_pump(int pump_number) {
  //return true;
  return (int)random(2); //TODO: check actual pin
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
  lcd_dot = not(lcd_dot);
  for (int i = 0; i < NUM_PUMPS; i++) {
    Serial.print("Pump");
    Serial.print(i);
    Serial.print(" has been running for ");
    Serial.print(get_last_24_hours(i));
    Serial.print(" seconds in the last 24 hours");
    Serial.println();
    Serial.print("Pump");
    Serial.print(i);
    Serial.print(" has been running for ");
    Serial.print(get_last_n_days(7, i));
    Serial.print(" seconds in the last 7 days");
    Serial.println();
    Serial.print("Pump");
    Serial.print(i);
    Serial.print(" has been running for ");
    Serial.print(get_last_n_days(30, i));
    Serial.print(" seconds in the last 30 days");
    Serial.println();

  }
  Serial.print("Seconds since last day shift: ");
  Serial.println(seconds_since_day_shift);
  Serial.print("Current Day: ");
  Serial.println(current_day);
  Serial.print("number_of_days_for_average: ");
  Serial.println(number_of_days_for_average);

  Serial.print("Array seconds_on_by_day [");
  for (int i = 0; i < 30; i++) {
    Serial.print("[");
    for (int j = 0; j < NUM_PUMPS; j++) {
      Serial.print(seconds_on_by_day[i][j]);
      Serial.print(",");
    }
    Serial.print("]");
  }
  Serial.println("]");
  Serial.println("]");
  Serial.println();
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

// - INTURRUPT PIN CALLBACKS
void interrupt_push_enc1() {
}
void interrupt_push_enc2() {
}
void interrupt_turn_enc() {
}

// SETUP/LOOP STUFF
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(); // Clear junk on boot
  high_power_runner.init();
  low_power_runner.init();
  Serial.println("Initialized schedulers");
  low_power_runner.addTask(pump_thread);
  low_power_runner.addTask(data_push_thread);
  pump_thread.enable();
  data_push_thread.enable();
  high_power_runner.addTask(display_thread);
  display_thread.enable();
}
void loop() {
  //Always run low power thread
  low_power_runner.execute();
  if (HIGH_POWER) {
    high_power_runner.execute();
  }

}

