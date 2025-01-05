#include <shButton.h>
#include <shTaskManager.h>

// ===================================================

#define BTN_PIN 2
#define BUZZER_PIN A0
#define MOTOR_PIN 10

#define L_LEVEL_SENSOR_PIN A2
#define H_LEVEL_SENSOR_PIN A1
#define PIR_SENSOR_PIN A3

#define L_LEVEL_LED_PIN 3
#define H_LEVEL_LED_PIN 5
#define PWR_ON_LED_PIN 9
#define PWR_OFF_LED_PIN 7

// ===================================================

shButton btn(BTN_PIN);
shTaskManager tasks(1);

shHandle pump_starting;

// ===================================================

void btnCheck();
void pumpStaring();

// ===================================================

void btnCheck()
{
  /*
   * - длинный клик выключает (или включает, если он выключен) модуль;
   * - одиночный клик
   *   - если модуль включен и в дефолтном режиме - включает помпу на пять
   *     минут;
   *   - если модуль выключен - включает его;
   *   - если модуль в режиме непрерывной работы помпы - выключает помпу и
   *     переводит модуль в дефолтный режим;
   * - двойной клик включает режим непрерывной работы помпы;
   */
  switch (btn.getButtonState())
  {
  case BTN_LONGCLICK:
    /* code */
    break;
  case BTN_ONECLICK:

    break;
  case BTN_DBLCLICK:
    /* code */
    break;
  }
}

void pumpStaring()
{
  if (!tasks.getTaskState(pump_starting))
  {
    tasks.startTask(pump_starting);
    digitalWrite(MOTOR_PIN, HIGH);
  }
  else
  {
    tasks.stopTask(pump_starting);
    digitalWrite(MOTOR_PIN, LOW);
  }
}

// ===================================================

void setup()
{
  btn.setVirtualClickOn();

// ===================================================

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(L_LEVEL_LED_PIN, OUTPUT);
  pinMode(H_LEVEL_LED_PIN, OUTPUT);
  pinMode(PWR_ON_LED_PIN, OUTPUT);
  pinMode(PWR_OFF_LED_PIN, OUTPUT);
  pinMode(L_LEVEL_SENSOR_PIN, INPUT);
  pinMode(H_LEVEL_SENSOR_PIN, INPUT);
  pinMode(PIR_SENSOR_PIN, INPUT);

// ===================================================

  pump_starting = tasks.addTask(300000, pumpStaring, false);
}

void loop()
{
  btnCheck();
  tasks.tick();
}
