#include <EEPROM.h>
#include <shButton.h>
#include <shTaskManager.h>

// ==== настройки ====================================

constexpr uint32_t PUMP_OPERATING_TIME = 300; // время работы помпы, секунд

#define USE_REGULAR_WATER_RECIRCULATION 0 // включать помпу по таймеру без срабатывания датчика

#define USE_DEBUG_OUT 0 // включить вывод в сериал

#define USE_WATER_LEVEL_SENSOR 0 // использовать датчики уровня воды

#if USE_WATER_LEVEL_SENSOR

#define USE_H_LEVEL_SENSOR 0 // использовать датчик верхнего уровня воды

#endif

#define USE_BUZZER 0 // использовать пищалку

#if USE_REGULAR_WATER_RECIRCULATION
constexpr uint32_t PUMP_AUTOSTART_INTERVAL = 1800; // интервал включения помпы по таймеру, секунд
#endif

#if USE_BUZZER
constexpr uint32_t LOW_LEVEL_BUZZER_TIMEOUT = 300; // интервал срабатывания пищалки при низком уровне воды, секунд
#define USE_BUZZER_WHEN_STARTING_PUMP 0            // обозначать включение помпы коротким пиком
#define USE_BUZZER_WHEN_BUTTON_CLICK 1             // обозначать коротким пиком клик кнопки
#endif

// ==== пины для подключения периферии ===============

constexpr uint8_t BTN_PIN = 2;         // пин для подключения кнопки
constexpr uint8_t PIR_SENSOR_PIN = A3; // пин датчика движения
constexpr uint8_t PUMP_PIN = 10;       // пин для подключения помпы
#if USE_BUZZER
constexpr uint8_t BUZZER_PIN = A0; // пин для подключения пищалки
#endif

#if USE_WATER_LEVEL_SENSOR
constexpr uint8_t L_LEVEL_SENSOR_PIN = A2; // пин датчика низкого уровня воды
constexpr uint8_t H_LEVEL_SENSOR_PIN = A1; // пин датчика высокого уровня воды

constexpr uint8_t L_LEVEL_LED_PIN = 6; // пин светодиода низкого уровня воды (красный)
constexpr uint8_t H_LEVEL_LED_PIN = 7; // пин светодиода высокого уровня воды (зеленый)
#endif

constexpr uint8_t PWR_OFF_LED_PIN = 8; // пин светодиода питания (красный)
constexpr uint8_t PWR_ON_LED_PIN = 9;  // пин светодиода питания (зеленый)

// ==== управляющие уровни ===========================

// уровни срабатывания датчиков и управляющие уровни выходов;
// могут быть 1 (HIGN) или 0 (LOW)

constexpr uint8_t PUMP_CONTROL_LEVEL = 1; // управляющий уровень помпы;

constexpr uint8_t PIR_SENSOR_RESPONSE_LEVEL = 1; // логический уровень при срабатывании pir-датчика;
#if USE_WATER_LEVEL_SENSOR
constexpr uint8_t L_SENSOR_RESPONSE_LEVEL = 0; // логический уровень при срабатывании датчика низкого уровня воды (вода ниже датчика);
#if USE_H_LEVEL_SENSOR
constexpr uint8_t H_SENSOR_RESPONSE_LEVEL = 0; // логический уровень при срабатывании датчика высокого уровня воды (вода ниже датчик;
#endif
#endif

// ==== EEPROM =======================================

constexpr int EEPROM_INDEX_FOR_CUR_MODE = 10; // индекс в EEPROM для сохранения текущего режима (1 байт)

// ===================================================

#if USE_DEBUG_OUT
#define AD_PRINTLN(x) Serial.println(x)
#define AD_PRINT(x) Serial.print(x)
#else
#define AD_PRINTLN(x)
#define AD_PRINT(x)
#endif

// ===================================================

enum SystemMode
{
  DEFAULT_MODE,   // автоматический режим работы
  CONTINOUS_MODE, // режим непрерывной работы помпы
  STANDBAY_MODE   // спящий режим
#if USE_WATER_LEVEL_SENSOR
  ,
  PUMP_STOP_MODE // режим остановки помпы из-за низкого уровня воды
#endif
};

// ===================================================

#if USE_BUZZER_WHEN_BUTTON_CLICK == 1

class adButton : public shButton
{
public:
  adButton(uint8_t _pin) : shButton(_pin)
  {
    shButton::setVirtualClickOn();
    shButton::setLongClickMode(LCM_ONLYONCE);
  }

  uint8_t getButtonState();
};

uint8_t adButton::getButtonState()
{
  uint8_t state = shButton::getButtonState();
  switch (state)
  {
  case BTN_DOWN:
  case BTN_DBLCLICK:
  case BTN_LONGCLICK:
    tone(BUZZER_PIN, 2500, 10);
    break;
  }
  return (state);
}

adButton btn(BTN_PIN);

#else

shButton btn(BTN_PIN);

#endif

// датчик движения обрабатываем как обычную кнопку
#if PIR_SENSOR_RESPONSE_LEVEL == 0
shButton pir(PIR_SENSOR_PIN);
#else
shButton pir(PIR_SENSOR_PIN, PULL_DOWN);
#endif

shTaskManager tasks;

shHandle pump_starting; // включение режима работы помпы на пять минут
shHandle pump_guard;    // собственно, отслеживание необходимости работы помпы
#if USE_REGULAR_WATER_RECIRCULATION
shHandle start_pump_by_timer; // периодическое включение помпы по таймеру
#endif
#if USE_WATER_LEVEL_SENSOR
shHandle level_sensor_guard; // отслеживание датчика низкого уровня воды
#endif
shHandle led_guard; // управление светодиодами
#if USE_BUZZER && USE_WATER_LEVEL_SENSOR
shHandle l_level_buzzer_on; // сигнал о низком уровне воды
#endif

SystemMode current_mode = DEFAULT_MODE;

// ===================================================

void setCurrentMode(SystemMode mode);
void restoreCurrentMode();
void btnCheck();
void pumpStaring();
void pumpGuard();
#if USE_REGULAR_WATER_RECIRCULATION
void startPumpByTimer();
#endif
#if USE_WATER_LEVEL_SENSOR
void levelSensorGuard();
#endif
void ledGuard();
#if USE_BUZZER && USE_WATER_LEVEL_SENSOR
void startLowLevelAlarm();
#endif
#if USE_DEBUG_OUT
void printCurrentMode();
#endif
#if USE_BUZZER_WHEN_STARTING_PUMP == 1
inline void beepPump();
#endif

// ===================================================

void setCurrentMode(SystemMode mode)
{
  current_mode = mode;
#if USE_DEBUG_OUT
  printCurrentMode();
#endif
  switch (current_mode)
  {
  case DEFAULT_MODE:
#if USE_REGULAR_WATER_RECIRCULATION
    tasks.startTask(start_pump_by_timer);
#endif
    tasks.stopTask(pump_starting);
    break;
  case CONTINOUS_MODE:
    AD_PRINTLN(F("Pump turns on constantly"));
#if USE_BUZZER_WHEN_STARTING_PUMP == 1
    beepPump();
#endif
#if USE_REGULAR_WATER_RECIRCULATION
    tasks.stopTask(start_pump_by_timer);
#endif
    break;
  case STANDBAY_MODE:
#if USE_REGULAR_WATER_RECIRCULATION
    tasks.stopTask(start_pump_by_timer);
#endif
    break;
#if USE_WATER_LEVEL_SENSOR
  case PUMP_STOP_MODE:
#if USE_REGULAR_WATER_RECIRCULATION
    tasks.stopTask(start_pump_by_timer);
#endif
    break;
#endif
  }

  if (current_mode < STANDBAY_MODE)
  {
    EEPROM.update(EEPROM_INDEX_FOR_CUR_MODE, (uint8_t)current_mode);
  }
}

void restoreCurrentMode()
{
  current_mode = (SystemMode)EEPROM.read(EEPROM_INDEX_FOR_CUR_MODE);
  if (current_mode > STANDBAY_MODE)
  {
    setCurrentMode(DEFAULT_MODE);
  }
#if USE_DEBUG_OUT
  printCurrentMode();
#endif
}

void btnCheck()
{
  switch (btn.getButtonState())
  {
  case BTN_LONGCLICK:
    AD_PRINTLN(F("Button - long click"));
    // длинный клик - переключает в спящий режимы
    if (current_mode != STANDBAY_MODE)
    {
      setCurrentMode(STANDBAY_MODE);
    }
    else
    // или, если модуль был в спящем режиме - восстанавливает прежний режим
    {
      restoreCurrentMode();
    }
    break;
  case BTN_ONECLICK:
    AD_PRINTLN(F("Button - one click"));
    // одиночный клик
    switch (current_mode)
    {
    case DEFAULT_MODE:
      // в автоматическом режиме включает помпу на пять минут, если она выключена, и наоборот
      pumpStaring();
      break;
#if USE_WATER_LEVEL_SENSOR
    case PUMP_STOP_MODE:
      // если помпа была остановлена по датчику низкого уровня воды, то восстановить прежний режим работы
      if (digitalRead(L_LEVEL_SENSOR_PIN) != L_SENSOR_RESPONSE_LEVEL)
      {
        tasks.stopTask(l_level_buzzer_on);
        restoreCurrentMode();
      }
      break;
#endif
    case STANDBAY_MODE:
      // при выходе из спящего режима восстановить прежний режим работы
      restoreCurrentMode();
      break;
    default:
      // во всех остальных случаях переводит его в автоматический режим
      setCurrentMode(DEFAULT_MODE);
      break;
    }
    break;
  case BTN_DBLCLICK:
    AD_PRINTLN(F("Button - double click"));
    // двойной клик в автоматическом режиме включает непрерывный режим
    if (current_mode == DEFAULT_MODE)
    {
      setCurrentMode(CONTINOUS_MODE);
    }
    else
    {
      setCurrentMode(DEFAULT_MODE);
    }
    break;
  }

  switch (pir.getButtonState())
  {
  case BTN_DOWN:
    AD_PRINTLN(F("Pir sensor triggered"));
    if (current_mode == DEFAULT_MODE && !tasks.getTaskState(pump_starting))
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
    AD_PRINTLN(F("Pump starting"));
#if USE_BUZZER_WHEN_STARTING_PUMP == 1
    beepPump();
#endif
    tasks.startTask(pump_starting);
#if USE_REGULAR_WATER_RECIRCULATION
    tasks.stopTask(start_pump_by_timer);
#endif
  }
  else
  {
    AD_PRINTLN(F("Pump stopped"));
    tasks.stopTask(pump_starting);
    // перезапускаем таймер периодического включения помпы
#if USE_REGULAR_WATER_RECIRCULATION
    tasks.startTask(start_pump_by_timer);
#endif
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
    pump_state = HIGH;
    break;
  default:
    pump_state = LOW;
    break;
  }
  // TODO при низком уровне воды кроме того включать пищалку
  if (!PUMP_CONTROL_LEVEL)
  {
    pump_state = !pump_state;
  }

  digitalWrite(PUMP_PIN, pump_state);
}

#if USE_REGULAR_WATER_RECIRCULATION
void startPumpByTimer()
{
  if (current_mode == DEFAULT_MODE && !tasks.getTaskState(pump_starting))
  {
    AD_PRINTLN(F("Pump starting on a timer"));
    pumpStaring();
  }
}
#endif

#if USE_WATER_LEVEL_SENSOR
void levelSensorGuard()
{
  if (digitalRead(L_LEVEL_SENSOR_PIN) == L_SENSOR_RESPONSE_LEVEL)
  {
    AD_PRINTLN(F("Low level sensor triggered"));
    setCurrentMode(PUMP_STOP_MODE);
    startLowLevelAlarm();
  }
}
#endif

void check_num(uint8_t &_num)
{
  if (++_num >= 20)
  {
    _num = 0;
  }
}

void ledGuard()
{
  // светодиод питания светится красным только в спящем режиме
  digitalWrite(PWR_OFF_LED_PIN, (current_mode == STANDBAY_MODE));

  static uint8_t blink_num = 0;
  static uint8_t pwr_on_num = 0;
  static bool to_up = true;

  /* в остальных случаях он светится зеленым
     - либо мигает с частотой 1Гц (режим постоянно включенной помпы)
     - либо мигает плавно в остальных режимах */
  if (current_mode != STANDBAY_MODE)
  {
    if (current_mode == CONTINOUS_MODE)
    {
      digitalWrite(PWR_ON_LED_PIN, (blink_num >= 10));
      check_num(blink_num);
    }
    else
    {
      pwr_on_num += (to_up) ? 10 : -10;
      analogWrite(PWR_ON_LED_PIN, pwr_on_num);
      to_up = (pwr_on_num == 250) ? false
                                  : ((pwr_on_num == 0) ? true : to_up);
    }
  }
  else
  {
    pwr_on_num = 0;
    to_up = true;
    digitalWrite(PWR_ON_LED_PIN, LOW);
  }

#if USE_WATER_LEVEL_SENSOR
  if (current_mode != STANDBAY_MODE)
  {
    // если сработал датчик нижнего уровня, светодиод уровня мигает красным с частотой 1Гц
    if (current_mode == PUMP_STOP_MODE)
    {
      digitalWrite(H_LEVEL_LED_PIN, LOW);
      digitalWrite(L_LEVEL_LED_PIN, (blink_num < 10));
      check_num(blink_num);
    }
    else
    {
      digitalWrite(L_LEVEL_LED_PIN, LOW);
#if USE_H_LEVEL_SENSOR
      // если датчик нижнего уровня молчит, смотрим состояние датчика верхнего уровня
      // если он сработал, светодиод уровня мигает зеленым с частотой 1Гц
      if (digitalRead(H_LEVEL_SENSOR_PIN) == H_SENSOR_RESPONSE_LEVEL)
      {
        digitalWrite(H_LEVEL_LED_PIN, (blink_num < 10));
        check_num(blink_num);
      }
      else
      {
        // иначе светодиод уровня горит зеленым
        digitalWrite(H_LEVEL_LED_PIN, HIGH);
      }
#else
      // иначе светодиод уровня горит зеленым
      digitalWrite(H_LEVEL_LED_PIN, HIGH);
#endif
    }
  }
  else
  {
    digitalWrite(H_LEVEL_LED_PIN, LOW);
    digitalWrite(L_LEVEL_LED_PIN, LOW);
  }
#endif
}

#if USE_BUZZER && USE_WATER_LEVEL_SENSOR
void startLowLevelAlarm()
{
  static const PROGMEM uint32_t pick[2][12] = {
      {2000, 0, 2000, 0, 2000, 0, 2000, 0, 2000, 0, 2000, 0},
      {50, 100, 50, 500, 50, 100, 50, 500, 50, 100, 50, LOW_LEVEL_BUZZER_TIMEOUT * 1000ul}};

  static uint8_t n = 0;

  if (!tasks.getTaskState(l_level_buzzer_on))
  {
    tasks.startTask(l_level_buzzer_on);
    n = 0;
  }
  else
  {
    if (digitalRead(L_LEVEL_SENSOR_PIN != L_SENSOR_RESPONSE_LEVEL))
    {
      tasks.stopTask(l_level_buzzer_on);
      restoreCurrentMode();
      n = 0;
    }
  }

  tone(BUZZER_PIN, pgm_read_dword(&pick[0][n]), pgm_read_dword(&pick[1][n]));

  tasks.setTaskInterval(l_level_buzzer_on, pgm_read_dword(&pick[1][n]), true);

  if (++n >= 12)
  {
    n = 0;
  }
}
#endif

#if USE_DEBUG_OUT
void printCurrentMode()
{
  AD_PRINT(F("Current mode = "));
  switch (current_mode)
  {
  case DEFAULT_MODE:
    AD_PRINTLN(F("default"));
    break;
  case CONTINOUS_MODE:
    AD_PRINTLN(F("continuous"));
    break;
  case STANDBAY_MODE:
    AD_PRINTLN(F("standby"));
    break;
  case PUMP_STOP_MODE:
    AD_PRINTLN(F("pump stopped"));
    break;
  }
}
#endif

#if USE_BUZZER_WHEN_STARTING_PUMP == 1
inline void beepPump()
{
  tone(BUZZER_PIN, 2500, 10);
}
#endif

// ===================================================

void setup()
{
#if USE_DEBUG_OUT
  Serial.begin(115200);
#endif

  pir.setTimeoutOfDebounce(0);

  // ===================================================

  digitalWrite(PUMP_PIN, !PUMP_CONTROL_LEVEL);
  pinMode(PUMP_PIN, OUTPUT);
#if USE_WATER_LEVEL_SENSOR
  pinMode(L_LEVEL_LED_PIN, OUTPUT);
#if USE_H_LEVEL_SENSOR
  pinMode(H_LEVEL_LED_PIN, OUTPUT);
#endif
#endif
  pinMode(PWR_ON_LED_PIN, OUTPUT);
  pinMode(PWR_OFF_LED_PIN, OUTPUT);

#if USE_WATER_LEVEL_SENSOR
#if L_SENSOR_RESPONSE_LEVEL == 0
  pinMode(L_LEVEL_SENSOR_PIN, INPUT_PULLUP);
#else
  // нужно обеспечить подтяжку пина к GND
  pinMode(L_LEVEL_SENSOR_PIN, INPUT);
#endif
#if USE_H_LEVEL_SENSOR
#if H_SENSOR_RESPONSE_LEVEL == 0
  pinMode(H_LEVEL_SENSOR_PIN, INPUT_PULLUP);
#else
  // нужно обеспечить подтяжку пина к GND
  pinMode(H_LEVEL_SENSOR_PIN, INPUT);
#endif
#endif
#endif

  // ===================================================

  restoreCurrentMode();

  // ===================================================
  uint8_t task_num = 3; // базовое количество задач

#if USE_REGULAR_WATER_RECIRCULATION
  task_num++;
#endif
#if USE_WATER_LEVEL_SENSOR
  task_num++;
#endif
#if USE_BUZZER && USE_WATER_LEVEL_SENSOR
  task_num++;
#endif
  tasks.init(task_num);

  pump_starting = tasks.addTask(PUMP_OPERATING_TIME * 1000ul, pumpStaring, false);
#if USE_WATER_LEVEL_SENSOR
  level_sensor_guard = tasks.addTask(5ul, levelSensorGuard);
#endif
  pump_guard = tasks.addTask(5ul, pumpGuard);
#if USE_REGULAR_WATER_RECIRCULATION
  start_pump_by_timer = tasks.addTask(PUMP_AUTOSTART_INTERVAL * 1000ul, startPumpByTimer);
#endif
  led_guard = tasks.addTask(50ul, ledGuard);
#if USE_BUZZER && USE_WATER_LEVEL_SENSOR
  l_level_buzzer_on = tasks.addTask(50ul, startLowLevelAlarm, false);
#endif
}

void loop()
{
  btnCheck();
  tasks.tick();
}
