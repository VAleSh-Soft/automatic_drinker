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
  DEFAULT_MODE,   // дефолтный режим работы
  CONTINOUS_MODE, // режима непрерывной работы помпы
  STANDBAY_MODE,  // спящий режима
  PUMP_STOP_MODE  // режим остновки помпы из-за низкого уровня воды
};

// ===================================================

shButton btn(BTN_PIN);
shButton pir(PIR_SENSOR_PIN); // датчик движения обрабатываем как обычную кнопку

shTaskManager tasks(4);

shHandle pump_starting;       // включение режима работы помпы на пять минут
shHandle pump_guard;          // собственно, отслеживание необхоимости работы помпы
shHandle start_pump_by_timer; // периодическое включение режима работы помпы на пять минут
shHandle level_sensor_guard;  // отслеживание датчика низкого уровня воды

ModuleState current_mode = DEFAULT_MODE;

// ===================================================

void setCurrentMode(ModuleState mode);
void btnCheck();
void pumpStaring();
void pumpGuard();
void startPumpByTimer();
void levelSensorGuard();

// ===================================================

void setCurrentMode(ModuleState mode)
{
  current_mode = mode;
  switch (current_mode)
  {
  case DEFAULT_MODE:
    tasks.startTask(start_pump_by_timer);
    tasks.stopTask(pump_starting);
    break;
  case CONTINOUS_MODE:
    tasks.stopTask(start_pump_by_timer);
    break;
  case STANDBAY_MODE:
    tasks.stopTask(start_pump_by_timer);
    break;
  case PUMP_STOP_MODE:
    tasks.stopTask(start_pump_by_timer);
    break;
  }
}

void btnCheck()
{
  switch (btn.getButtonState())
  {
  case BTN_LONGCLICK:
    // длинный клик - переключает дефолтный и спящий режимы
    (current_mode != STANDBAY_MODE) ? setCurrentMode(STANDBAY_MODE)
                                    : setCurrentMode(DEFAULT_MODE);
    break;
  case BTN_ONECLICK:
    // одиночный клик
    switch (current_mode)
    {
    case DEFAULT_MODE:
      // в дефолтном режиме включает помпу на пять минут, если она выключена, и наоборот
      pumpStaring();
      break;
    default:
      // во всех остальных случаях переводит его в дефолтный режим
      setCurrentMode(DEFAULT_MODE);
      break;
    }
    break;
  case BTN_DBLCLICK:
    // двойной клик в дефолтном режиме включает непрерывный режим
    if (current_mode == DEFAULT_MODE)
    {
      setCurrentMode(CONTINOUS_MODE);
    }
    break;
  }

  switch (pir.getButtonState())
  {
  case BTN_DOWN:
    if (current_mode == DEFAULT_MODE)
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
    tasks.startTask(start_pump_by_timer);
  }
}

void pumpGuard()
{
  uint8_t pump_state = LOW;
  switch (current_mode)
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

void startPumpByTimer()
{
  if (current_mode == DEFAULT_MODE && !tasks.getTaskState(pump_starting))
  {
    tasks.startTask(pump_starting);
  }
}

void levelSensorGuard()
{
  if (digitalRead(L_LEVEL_SENSOR_PIN) == HIGH)
  {
    setCurrentMode(PUMP_STOP_MODE);
  }
  else
  {
    setCurrentMode(DEFAULT_MODE);
  }
}

// ===================================================

void setup()
{
  btn.setVirtualClickOn();
  pir.setTimeoutOfDebounce(0);

  // ===================================================

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(L_LEVEL_LED_PIN, OUTPUT);
  pinMode(H_LEVEL_LED_PIN, OUTPUT);
  pinMode(PWR_ON_LED_PIN, OUTPUT);
  pinMode(PWR_OFF_LED_PIN, OUTPUT);
  pinMode(L_LEVEL_SENSOR_PIN, INPUT);
  pinMode(H_LEVEL_SENSOR_PIN, INPUT);

  // ===================================================

  pump_starting = tasks.addTask(300000ul, pumpStaring, false);
  level_sensor_guard = tasks.addTask(5ul, levelSensorGuard);
  pump_guard = tasks.addTask(5ul, pumpGuard);
  start_pump_by_timer = tasks.addTask(1800000ul, startPumpByTimer);
}

void loop()
{
  btnCheck();
  tasks.tick();
}
