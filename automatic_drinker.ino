#include <EEPROM.h>
#include <shButton.h>
#include <shTaskManager.h>

// ===================================================

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

// уровни срабатывания датчиков и управляющие уровни выходов; могут быть 1 (HIGN) или 0 (LOW)
#define PIR_SENSOR_RESPONSE_LEWEL 1 // уровень при срабатывании pir-датчика;
#define PUMP_CONTROL_LEWLEL 1       // управляющий уровень помпы;
#define L_SENSOR_RESPONSE_LEWEL 1   // уровень при срабатывании датчика низкого уровня воды (вода ниже датчика);
#define H_SENSOR_RESPONSE_LEWEL 1   // уровень при срабатывании датчика высокого уровня воды (вода ниже датчик;

#define EEPROM_INDEX_FOR_CUR_MODE 10 // индекс в EEPROM для сохранения текущего режима (1 байт)

// ===================================================

enum SystemMode
{
  DEFAULT_MODE,   // дефолтный режим работы
  CONTINOUS_MODE, // режим непрерывной работы помпы
  STANDBAY_MODE,  // спящий режим
  PUMP_STOP_MODE  // режим остановки помпы из-за низкого уровня воды
};

// ===================================================

shButton btn(BTN_PIN);

#if PIR_SENSOR_RESPONSE_LEWEL
// датчик движения обрабатываем как обычную кнопку
shButton pir(PIR_SENSOR_PIN, PULL_DOWN);
#else
// датчик движения обрабатываем как обычную кнопку
shButton pir(PIR_SENSOR_PIN);
#endif

shTaskManager tasks(5);

shHandle pump_starting;       // включение режима работы помпы на пять минут
shHandle pump_guard;          // собственно, отслеживание необходимости работы помпы
shHandle start_pump_by_timer; // периодическое включение режима работы помпы на пять минут
shHandle level_sensor_guard;  // отслеживание датчика низкого уровня воды
shHandle led_guard;           // управление светодиодами

SystemMode current_mode = DEFAULT_MODE;

// ===================================================

void setCurrentMode(SystemMode mode);
void btnCheck();
void pumpStaring();
void pumpGuard();
void startPumpByTimer();
void levelSensorGuard();
void ledGuard();

// ===================================================

void setCurrentMode(SystemMode mode)
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

  if (current_mode != PUMP_STOP_MODE)
  {
    EEPROM.update(EEPROM_INDEX_FOR_CUR_MODE, current_mode);
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
    pump_state = HIGH;
    break;
  default:
    pump_state = LOW;
    break;
  }

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
    tasks.startTask(pump_starting);
  }
}

void levelSensorGuard()
{
  if (digitalRead(L_LEVEL_SENSOR_PIN) == L_SENSOR_RESPONSE_LEWEL)
  {
    setCurrentMode(PUMP_STOP_MODE);
  }
  else
  {
    setCurrentMode(DEFAULT_MODE);
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

  static uint8_t pwr_on_num = 0;
  static bool to_up = true;
  /* в остальных случаях он либо светится зеленым непрерывно (режим постоянно
     включенной помпы), либо плавно мигает зеленым */
  if (current_mode != STANDBAY_MODE)
  {
    if (current_mode == CONTINOUS_MODE)
    {
      digitalWrite(PWR_ON_LED_PIN, HIGH);
    }
    else
    {
      pwr_on_num += (to_up) ? 5 : -5;
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
    static uint8_t lew_num = 0;
    if (current_mode == PUMP_STOP_MODE)
    {
      digitalWrite(H_LEVEL_LED_PIN, LOW);
      digitalWrite(L_LEVEL_LED_PIN, (lew_num < 10));
      check_num(lew_num);
    }
    else
    {
      // если датчик нижнего уровня молчит, смотрим состояние датчика среднего уровня
      digitalWrite(L_LEVEL_LED_PIN, LOW);
      // если датчик среднего уровня сработал, светодиод уровня мигает зеленым с частотой 1Гц
      if (digitalRead(H_LEVEL_SENSOR_PIN) == H_SENSOR_RESPONSE_LEWEL)
      {
        digitalWrite(H_LEVEL_LED_PIN, (lew_num < 10));
        check_num(lew_num);
      }
      else
      {
        // иначе светодиод уровня горит зеленым
        digitalWrite(H_LEVEL_LED_PIN, HIGH);
        lew_num = 0;
      }
    }
  }
}

// ===================================================

void setup()
{
  btn.setVirtualClickOn();
  pir.setTimeoutOfDebounce(0);

  // ===================================================

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(L_LEVEL_LED_PIN, OUTPUT);
  pinMode(H_LEVEL_LED_PIN, OUTPUT);
  pinMode(PWR_ON_LED_PIN, OUTPUT);
  pinMode(PWR_OFF_LED_PIN, OUTPUT);

#if L_SENSOR_RESPONSE_LEWEL
  // желательно обеспечить подтяжку к GND
  pinMode(L_LEVEL_SENSOR_PIN, INPUT);
#else
  pinMode(L_LEVEL_SENSOR_PIN, INPUT_PULLUP);
#endif
#if L_SENSOR_RESPONSE_LEWEL
  // желательно обеспечить подтяжку к GND
  pinMode(H_LEVEL_SENSOR_PIN, INPUT);
#else
  pinMode(H_LEVEL_SENSOR_PIN, INPUT_PULLUP);
#endif

  // ===================================================

  current_mode = (SystemMode)EEPROM.read(EEPROM_INDEX_FOR_CUR_MODE);
  if (current_mode > STANDBAY_MODE)
  {
    setCurrentMode(DEFAULT_MODE);
  }

  // ===================================================

  pump_starting = tasks.addTask(300000ul, pumpStaring, false);
  level_sensor_guard = tasks.addTask(5ul, levelSensorGuard);
  pump_guard = tasks.addTask(5ul, pumpGuard);
  start_pump_by_timer = tasks.addTask(1800000ul, startPumpByTimer);
  led_guard = tasks.addTask(50ul, ledGuard);
}

void loop()
{
  btnCheck();
  tasks.tick();
}
