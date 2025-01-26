## automatic_drinker v 1.0

Простой автомат управления поилкой-фонтанчиком для животных, построенный на Arduino и pir-датчике. Помещается даже в Atmega8/88.

- [Описание](#описание)
- [Управление](#управление)
- [Индикация](#индикация)
- [Настройки скетча](#настройки-скетча)
  - [Работа насоса](#работа-насоса)
  - [Датчики уровня воды](#датчики-уровня-воды)
  - [Звуковой излучатель (пищалка)](#звуковой-излучатель-пищалка)
  - [Вывод отладочной информации](#вывод-отладочной-информации)
- [Использованные сторонние библиотеки](#использованные-сторонние-библиотеки)


### Описание

Устройство предназначено для автоматического управления поилкой-фонтанчиком - насос поилки будет включаться при приближении животного к поилке. Дополнительные возможности устройства:

- кнопка для управления режимами работы;
- светодиодная индикация режимов работы;
- возможность изменения времени работы насоса после срабатывания датчика;
- возможность запуска насоса по таймеру, если не было срабатывания датчика движения, чтобы вода в емкости не застаивалась;
- возможность использования датчиков уровня воды:
  - датчик нижнего уровня; при его срабатывании насос будет отключаться, и будет включаться соответствующая сигнализация;
  - датчик верхнего (на самом деле среднего) уровня воды; при его срабатывании начинает мигать светодиод, подсказывая, что неплохо бы долить воды;
- возможность непрерывной работы насоса вне зависимости от датчика движения; 
- возможность использования звукового оповещения о работе устройства;


### Управление

Устройство управляется одной кнопкой, которая позволяет:

- Удержанием кнопки нажатой выключать устройство;
- Двойным кликом включать непрерывный режим работы насоса;
- Одиночным кликом включать устройство, если оно выключено или, если устройство включено, включать насос или выключать его вне зависимости от того, как он был перед этим включен;

После срабатывания датчика низкого уровня воды для восстановления работы устройства после долива воды нужно сбросить сигнал низкого уровня кликом кнопки.


### Индикация

Индикация работы устройства осуществляется одним или двумя двухцветными светодиодами с общим катодом.

Индикатор статуса показывает текущее состояние устройства:

- Светится красным цветом, если устройство выключено;
- Плавно разгорается и гаснет зеленым цветом, если устройство находится в автоматическом режиме;
- Мигает зеленым цветом с частотой около 1Гц, если включен режим непрерывной работы насоса;

Индикатор уровня воды используется, если используется датчик нижнего уровня (см. [здесь](#датчики-уровня-воды)); он дает информацию о наличии воды в емкости:

- Светится зеленым, если воды достаточно;
- Если используется датчик верхнего уровня воды, то мигает зеленым с частотой около 1Гц, если вода опустилась ниже датчика верхнего уровня, давая понять, что неплохо бы долить воды в емкость;
- Мигает красным с частотой около 1Гц, если вода опустилась ниже датчика нижнего уровня;


### Настройки скетча

Все настройки собраны в начале скетча.

#### Работа насоса

- `constexpr uint32_t PUMP_OPERATING_TIME = 300;` - время работы помпы после срабатывания датчика, запуска помпы вручную или по таймеру; задается в секундах;
- `#define USE_REGULAR_WATER_RECIRCULATION 1` - включает/выключает использование рециркуляции воды в емкости; при `1` насос будет регулярно включаться сам через заданные промежутки времени; если вам это не нужно, установите значение `0` или закомментируйте строку;
- `constexpr uint32_t PUMP_AUTOSTART_INTERVAL = 1800;` - интервал включения насоса для рециркуляции воды в емкости; задается в секундах; если задать `0` секунд, автовключение насоса использоваться не будет;

Отсчет интервала начинается после остановки насоса, т.е. рециркуляция будет включаться на ранее, чем через заданный (в данном случае 30 минут) интервал времени после последнего срабатывания насоса (т.е. последнего подхода животного к поилке);

#### Датчики уровня воды

В устройстве есть возможность использования до двух датчиков уровня воды.
- `#define USE_WATER_LEVEL_SENSOR 1` - при значении `1` появляется возможность использования датчиков уровня воды, а так же индикации состояния датчиков с помощью светодиодов и (если такая возможность включена) звуковая сигнализации низкого уровня воды в емкости; если вы не собираетесь использовать датчики уровня воды, задайте `0` или закомментируйте эту строку;
- `#define USE_H_LEVEL_SENSOR 1` - при значении `1` появляется возможность использования датчика верхнего (или среднего) уровня воды в емоксти; смысл этого датчика в предварительном оповещении (при его срабатывании светодиод уровня воды начинает мигать зеленым цветом) о необходимости долить воды в емкость до того, как сработает датчик нижнего уровня; размещать этот датчик следует так, чтобы он срабатывал при уменьшении количества воды в емкости примерно наполовину; если вы не собираетесь использовать этот датчик, задайте `0` или закомментируйте эту строку;

#### Звуковой излучатель (пищалка)

Устройство позволяет использовать звуковой излучатель (например, пьезоэлектрическую пищалку) для оповещения о событиях системы.

- `#define USE_BUZZER 1` - включает/выключает возможность использования пищалки; если вам это не нужно, задайте `0` или закомментируйте эту строку; нижеследующие опции имеют смысл только если `USE_BUZZER == 1`;
- `#define USE_BUZZER_WHEN_STARTING_PUMP 0` - при значении `1` каждое включение насоса будет сопровождаться коротким пиком;
- `#define USE_BUZZER_WHEN_BUTTON_CLICK 1` - при значении `1` коротким пиком будет сопровождаться каждый клик кнопкой;
- `#define USE_BUZZER_WHEN_LOW_WATER_LEVEL 1` - при значении `1` при низком уровне воды будет периодически выдаваться звуковой сигнал для привлечения внимания;
- `constexpr uint32_t LOW_LEVEL_BUZZER_TIMEOUT = 300;` - интервал срабатывания пищалки при низком уровне воды; задается в секундах; если задать `0`, будет использоваться интервал 300 секунд; 

#### Вывод отладочной информации

- `#define USE_DEBUG_OUT 0` - при значении `1` устройство будет выводить в монитор порта информацию о своей работе;

***

### Использованные сторонние библиотеки

**shButton.h** - https://github.com/VAleSh-Soft/shButton <br>
**shTaskManager.h** - https://github.com/VAleSh-Soft/shTaskManager <br>

***

Если возникнут вопросы, пишите на valesh-soft@yandex.ru 
