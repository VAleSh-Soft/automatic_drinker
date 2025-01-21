#include <EEPROM.h>
#include <shButton.h>
#include <shTaskManager.h>

// ==== настройки ====================================

#define EEPROM_INDEX_FOR_CUR_MODE 10 // индекс в EEPROM для сохранения текущего режима (1 байт)

#define PUMP_OPERATING_TIME 300      // время работы помпы в секундах
#define PUMP_AUTOSTART_INTERVAL 1800 // интервал включения помпы по таймеру в секундах

#define LOG_ON 1 // включить вывод в сериал

#define USE_BUZZER_WHEN_STARTING_PUMP 1 // использовать пищалку при включении помпы

#define USE_H_LEWEL_SENSOR 1            // использовать датчик верхнего уровня воды

// ==== пины для подключения периферии ===============

#define BTN_PIN 2     // пин для подключения кнопки
#define BUZZER_PIN A0 // пин для подключения пищалки
#define PUMP_PIN 10   // пин для подключения помпы

#define L_LEVEL_SENSOR_PIN A2 // пин датчика низкого уровня воды
#define H_LEVEL_SENSOR_PIN A1 // пин датчика высокого уровня воды
#define PIR_SENSOR_PIN A3     // пин датчика движения

#define L_LEVEL_LED_PIN 6 // пин светодиода низкого уровня воды (красный)
#define H_LEVEL_LED_PIN 7 // пин светодиода высокого уровня воды (зеленый)

#define PWR_OFF_LED_PIN 8 // пин светодиода питания (красный)
#define PWR_ON_LED_PIN 9  // пин светодиода питания (зеленый)

// ==== управляющие уровни ===========================

// уровни срабатывания датчиков и управляющие уровни выходов;
// могут быть 1 (HIGN) или 0 (LOW)

#define PUMP_CONTROL_LEWLEL 1 // управляющий уровень помпы;

#define PIR_SENSOR_RESPONSE_LEWEL 1 // логический уровень при срабатывании pir-датчика;
#define L_SENSOR_RESPONSE_LEWEL 0   // логический уровень при срабатывании датчика низкого уровня воды (вода ниже датчика);
#define H_SENSOR_RESPONSE_LEWEL 0   // логический уровень при срабатывании датчика высокого уровня воды (вода ниже датчик;

// ===================================================

#if LOG_ON
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
  STANDBAY_MODE,  // спящий режим
  PUMP_STOP_MODE  // режим остановки помпы из-за низкого уровня воды
};

// ===================================================

shButton btn(BTN_PIN);

// датчик движения обрабатываем как обычную кнопку
#if PIR_SENSOR_RESPONSE_LEWEL == 0
shButton pir(PIR_SENSOR_PIN);
#else
shButton pir(PIR_SENSOR_PIN, PULL_DOWN);
#endif

shTaskManager tasks(5);

shHandle pump_starting;       // включение режима работы помпы на пять минут
shHandle pump_guard;          // собственно, отслеживание необходимости работы помпы
shHandle start_pump_by_timer; // периодическое включение помпы по таймеру
shHandle level_sensor_guard;  // отслеживание датчика низкого уровня воды
shHandle led_guard;           // управление светодиодами

SystemMode current_mode = DEFAULT_MODE;

// ===================================================

void setCurrentMode(SystemMode mode);
void restoreCurrentMode();
void btnCheck();
void pumpStaring();
void pumpGuard();
void startPumpByTimer();
void levelSensorGuard();
void ledGuard();
#if LOG_ON
void printCurrentMode();
#endif

// ===================================================

void setCurrentMode(SystemMode mode)
{
  current_mode = mode;
#if LOG_ON
  printCurrentMode();
#endif
  switch (current_mode)
  {
  case DEFAULT_MODE:
    tasks.startTask(start_pump_by_timer);
    tasks.stopTask(pump_starting);
    break;
  case CONTINOUS_MODE:
    AD_PRINTLN(F("Pump turns on constantly"));
#if USE_BUZZER_WHEN_STARTING_PUMP
    tone(BUZZER_PIN, 2500, 10);
#endif
    tasks.stopTask(start_pump_by_timer);
    break;
  case STANDBAY_MODE:
    tasks.stopTask(start_pump_by_timer);
    break;
  case PUMP_STOP_MODE:
    tasks.stopTask(start_pump_by_timer);
    break;
  }

  if (current_mode < (uint8_t)STANDBAY_MODE)
  {
    EEPROM.update(EEPROM_INDEX_FOR_CUR_MODE, current_mode);
  }
}

void restoreCurrentMode()
{
  current_mode = (SystemMode)EEPROM.read(EEPROM_INDEX_FOR_CUR_MODE);
  if (current_mode > (uint8_t)STANDBAY_MODE)
  {
    setCurrentMode(DEFAULT_MODE);
  }
#if LOG_ON
  printCurrentMode();
#endif
}

void btnCheck()
{
  switch (btn.getButtonState())
  {
  case BTN_LONGCLICK:
    AD_PRINTLN(F("Button - long click"));
    // длинный клик - включает спящий режимы
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
    case PUMP_STOP_MODE:
      // если помпа была остановлена по датчику низкого уровня воды, то восстановить прежний режим работы
      if (digitalRead(L_LEVEL_SENSOR_PIN) != L_SENSOR_RESPONSE_LEWEL)
      {
        restoreCurrentMode();
      }
      break;
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
#if USE_BUZZER_WHEN_STARTING_PUMP
    tone(BUZZER_PIN, 2500, 10);
#endif
    tasks.startTask(pump_starting);
    tasks.stopTask(start_pump_by_timer);
  }
  else
  {
    AD_PRINTLN(F("Pump stopped"));
    tasks.stopTask(pump_starting);
    // перезапускаем таймер периодического включения помпы
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
    pump_state = HIGH;
    break;
  default:
    pump_state = LOW;
    break;
  }
  // TODO при низком уровне воды кроме того включать пищалку
  if (!PUMP_CONTROL_LEWLEL)
  {
    pump_state = !pump_state;
  }

  digitalWrite(PUMP_PIN, pump_state);
}

void startPumpByTimer()
{
  if (current_mode == DEFAULT_MODE && !tasks.getTaskState(pump_starting))
  {
    AD_PRINTLN(F("Pump starting on a timer"));
    pumpStaring();
  }
}

void levelSensorGuard()
{
  if (digitalRead(L_LEVEL_SENSOR_PIN) == L_SENSOR_RESPONSE_LEWEL)
  {
    AD_PRINTLN(F("Level sensor triggered"));
    setCurrentMode(PUMP_STOP_MODE);
  }
}

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
#if USE_H_LEWEL_SENSOR
      // если датчик нижнего уровня молчит, смотрим состояние датчика верхнего уровня
      // если он сработал, светодиод уровня мигает зеленым с частотой 1Гц
      if (digitalRead(H_LEVEL_SENSOR_PIN) == H_SENSOR_RESPONSE_LEWEL)
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
}

#if LOG_ON

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

// ===================================================

void setup()
{
#if LOG_ON
  Serial.begin(115200);
#endif

  btn.setVirtualClickOn();
  btn.setLongClickMode(LCM_ONLYONCE);
  pir.setTimeoutOfDebounce(0);

  // ===================================================

  digitalWrite(PUMP_PIN, !PUMP_CONTROL_LEWLEL);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(L_LEVEL_LED_PIN, OUTPUT);
  pinMode(H_LEVEL_LED_PIN, OUTPUT);
  pinMode(PWR_ON_LED_PIN, OUTPUT);
  pinMode(PWR_OFF_LED_PIN, OUTPUT);

#if L_SENSOR_RESPONSE_LEWEL == 0
  pinMode(L_LEVEL_SENSOR_PIN, INPUT_PULLUP);
#else
  // нужно обеспечить подтяжку пина к GND
  pinMode(L_LEVEL_SENSOR_PIN, INPUT);
#endif
#if USE_H_LEWEL_SENSOR
#if H_SENSOR_RESPONSE_LEWEL == 0
  pinMode(H_LEVEL_SENSOR_PIN, INPUT_PULLUP);
#else
  // нужно обеспечить подтяжку пина к GND
  pinMode(H_LEVEL_SENSOR_PIN, INPUT);
#endif
#endif

  // ===================================================

  restoreCurrentMode();

  // ===================================================

  pump_starting = tasks.addTask(PUMP_OPERATING_TIME * 1000ul, pumpStaring, false);
  level_sensor_guard = tasks.addTask(5ul, levelSensorGuard);
  pump_guard = tasks.addTask(5ul, pumpGuard);
  start_pump_by_timer = tasks.addTask(PUMP_AUTOSTART_INTERVAL * 1000ul, startPumpByTimer);
  led_guard = tasks.addTask(50ul, ledGuard);
}

void loop()
{
  btnCheck();
  tasks.tick();
}
