# ESP-Sensors

ESP32-C6 устройство с LCD 1.47" дисплеем и веб-интерфейсом для мониторинга датчиков.

## Возможности

- **LCD 1.47" дисплей** (ST7789, 172×320) с LVGL v8 — отображение данных в реальном времени
- **Веб-сервер** — HTML-страница с живыми данными и JSON API
- **Демо-режим** — работает без внешних датчиков, с фиксированными значениями
- **Планируемые датчики**: SHT45 (температура/влажность I2C), BMP581 (давление SPI)

## Отображаемые данные

| Параметр | Единица | Описание |
|----------|---------|----------|
| T | °C | Температура |
| RH | % | Относительная влажность |
| AH | г/м³ | Абсолютная влажность (расчёт по формуле Magnus) |
| P | hPa | Атмосферное давление |

Обновление каждые 2 секунды.

## Веб-интерфейс

После подключения к Wi-Fi устройство доступно по IP-адресу:

- **`/`** — HTML-страница с карточками данных (автообновление каждые 2 сек)
- **`/api/sensors`** — JSON API: `{"temp":26.0,"rh":45.0,"ah":10.25,"pres":1013.25}`

## Оборудование

- **Плата**: Waveshare ESP32-C6-LCD-1.47 (ESP32-C6FH4, 4 MB Flash)
- **Дисплей**: ST7789, 172×320, 16-bit color (SPI)
- **Интерфейс**: USB-Serial/JTAG (консоль на COM7)

### Пины дисплея

| Пин LCD | ESP32-C6 |
|---------|----------|
| MOSI    | GPIO6    |
| SCLK    | GPIO7    |
| CS      | GPIO14   |
| DC      | GPIO15   |
| RST     | GPIO21   |
| BL      | GPIO22   |

## Архитектура

```
ESP-Sensors/
├── platformio.ini            # Конфигурация PlatformIO (ESP32-C6, ESP-IDF)
├── sdkconfig.defaults        # ESP-IDF: Wi-Fi включён, LVGL, USB-Serial/JTAG
├── partitions.csv            # Таблица партиций Flash (NVS + factory app)
├── CMakeLists.txt            # Точка входа CMake
├── .gitignore
├── src/
│   └── main.cpp              # app_main: инициализация дисплея, UI, веб-сервер
├── components/
│   ├── lvgl/                 # LVGL v8.3 (git submodule)
│   ├── ui_driver/            # Драйвер дисплея + порт LVGL для ESP-IDF
│   │   ├── display_driver.h/cpp   # ST7789 через SPI, LEDC backlight
│   │   └── lvgl_port.h/cpp        # LVGL display/touch port для ESP-IDF
│   └── web_server/           # HTTP сервер + Wi-Fi клиент
│       ├── web_server.h/cpp       # ESP-HTTP-Server, endpoints / и /api/sensors
│       └── CMakeLists.txt
└── dist/                     # Готовая прошивка (.bin)
```

### Компоненты

1. **ui_driver** — низкоуровневый драйвер ST7789 LCD через SPI + LEDC для подсветки. Адаптирован для Waveshare ESP32-C6-LCD-1.47 (смещение GRAM 172x320 из 240x320).

2. **web_server** — Wi-Fi STA + ESP-HTTP-Server. Соединяет общие данные сенсоров (`sensor_data_t`) с HTTP-эндпоинтами. Wi-Fi SSID/пароль задаются в `web_server.cpp` или через параметры `web_server_init()`.

3. **main.cpp** — объединяет дисплей и веб-сервер. LVGL timer handler работает в главном цикле, веб-сервер запускается после инициализации UI.

## Сборка и прошивка

### Требования
- PlatformIO IDE в VS Code (или CLI: `python -m platformio`)
- Фреймворк: ESP-IDF v5.5.5

### Сборка
```powershell
cd ESP-Sensors
python -m platformio run
```

### Прошивка через PlatformIO
```powershell
python -m platformio run --target upload
```

### Прошивка готовым .bin (без PlatformIO)
Файл `dist/ESP-Sensors-factory.bin` — полный образ (bootloader + partitions + app):

```powershell
python -m esptool --chip esp32c6 --port COM7 write_flash 0x0 dist/ESP-Sensors-factory.bin
```

Либо через **Flash Download Tool** от Espressif: ESP32-C6, `ESP-Sensors-factory.bin` со смещением `0x0`.

## Консоль

Вывод логов через USB-Serial/JTAG, 115200 бод:

```powershell
python -m platformio device monitor -e esp32_c6_lcd_1_47 --baud 115200
```

## Особенности

- **LVGL v8.3** — графическая библиотека (submodule)
- **Wi-Fi включён** — для веб-сервера (BLE отключён)
- **Framework**: ESP-IDF v5.5.5 через PlatformIO
- **Flash**: 4MB (Waveshare ESP32-C6-LCD-1.47)

## Планы

1. Подключить SHT45 (I2C) и BMP581 (SPI)
2. Заменить демо-данные на реальные показания сенсоров
3. Добавить управление через веб-интерфейс (настройки Wi-Fi, калибровка)

## Лицензия

MIT License