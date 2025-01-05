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

enum ModuleState
{
  DEFAULT_MODE,
  CONTINOUS_MODE,
  STANDBAY_MODE
};

// ===================================================

shButton btn(BTN_PIN);
shButton pir(PIR_SENSOR_PIN); // датчик движения обрабатываем как обычную кнопку

shTaskManager tasks(2);

shHandle pump_starting;
shHandle pump_guard;

ModuleState module_state = DEFAULT_MODE;

// ===================================================

void btnCheck();
void pumpStaring();
void pumpGuard();

// ===================================================

void btnCheck()
{
  switch (btn.getButtonState())
  {
  case BTN_LONGCLICK:
    // длинный клик - переключает дефолтный и спящий режимы
    module_state = (module_state != STANDBAY_MODE) ? STANDBAY_MODE
                                                   : DEFAULT_MODE;
    break;
  case BTN_ONECLICK:
    // одиночный клик
    switch (module_state)
    {
    case DEFAULT_MODE:
      // в дефолтном режиме включает помпу на пять минут, если она выключена, и наоборот
      pumpStaring();
      break;
    case STANDBAY_MODE:
    case CONTINOUS_MODE:
      // если модуль в спящем или непрерывном режимах, переводит его в дефолтный режим
      module_state = DEFAULT_MODE;
      break;
    }
    break;
  case BTN_DBLCLICK:
    // двойной клик в дефолтном режиме включает непрерывный режим
    if (module_state == DEFAULT_MODE)
    {
      module_state = CONTINOUS_MODE;
    }
    break;
  }

  switch (pir.getButtonState())
  {
  case BTN_DOWN:
    if (module_state == DEFAULT_MODE)
    {
      pumpStaring();
    }
    break;
  }
}

void pumpStaring()
{
  if (!tasks.getTaskState(pump_starting))
  {
    tasks.startTask(pump_starting);
  }
  else
  {
    tasks.stopTask(pump_starting);
  }
}

void pumpGuard()
{
  uint8_t pump_state = LOW;
  switch (module_state)
  {
  case DEFAULT_MODE:
    pump_state = tasks.getTaskState(pump_starting);
    break;
  case CONTINOUS_MODE:
    pump_state = true;
    break;
  default:
    pump_state = false;
    break;
  }
  digitalWrite(MOTOR_PIN, pump_state);
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

  pump_starting = tasks.addTask(300000ul, pumpStaring, false);
  pump_guard = tasks.addTask(5ul, pumpGuard);
}

void loop()
{
  btnCheck();
  tasks.tick();
}
