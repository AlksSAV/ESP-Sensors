# ESP-Sensors

Проект для Waveshare ESP32-C6-LCD-1.47. Отображает на встроенном LCD 1.47" (ST7789) температуру, относительную и абсолютную влажность и давление с использованием LVGL v8.

Сейчас работает в **демо-режиме** (без внешних сенсоров): температура — фиксированная (25°C), влажность и давление — демо-значения. Когда появятся датчики SHT45 и BMP581, код легко заменить на реальные показания.

## Оборудование

- **Плата**: Waveshare ESP32-C6-LCD-1.47 (ESP32-C6FH4, 4 MB Flash)
- **Дисплей**: ST7789, 172×320, 16-bit color (SPI)
- **Будущие датчики**:
  - SHT45 — температура и влажность (I2C)
  - BMP581 — давление и температура (SPI)

## Подключение дисплея (встроенного)

| Пин LCD | ESP32-C6 |
|---------|----------|
| MOSI    | GPIO6    |
| SCLK    | GPIO7    |
| CS      | GPIO14   |
| DC      | GPIO15   |
| RST     | GPIO21   |
| BL      | GPIO22   |

## Что отображается

- **T** — температура, °C (демо: 25.0)
- **RH** — относительная влажность, % (демо: 45.0)
- **AH** — абсолютная влажность, г/м³ (расчётная по формуле Magnus)
- **P** — давление, hPa (демо: 1013.25)

Данные обновляются каждые 2 секунды.

## Особенности

- **LVGL v8.3** — графическая библиотека (submodule)
- **Wi-Fi и Bluetooth отключены** через `sdkconfig.defaults`
- **Яркость дисплея 50%** — для предотвращения перегрева (рекомендация производителя)
- Framework: **ESP-IDF** (v5.5.5 через PlatformIO)

## Структура проекта

```
ESP-Sensors/
├── platformio.ini           # Конфигурация PlatformIO
├── sdkconfig.defaults       # Отключение Wi-Fi/BT, flash 4MB
├── .gitignore
├── src/
│   └── main.cpp             # Основное приложение + UI
├── components/
│   ├── lvgl/                # LVGL v8.3 (git submodule)
│   └── ui_driver/           # Драйвер ST7789 + порт LVGL для ESP-IDF
│       ├── display_driver.h/cpp
│       ├── lvgl_port.h/cpp
│       └── CMakeLists.txt
└── dist/                    # Готовая прошивка (.bin)
```

## Сборка и прошивка

### Требования
- PlatformIO IDE в VS Code (или CLI: `python -m platformio`)
- PlatformIO для ESP32-C6 (фреймворк `espressif32`, подтянется автоматически)

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
Файл `dist/ESP-Sensors-factory.bin` — полный образ (bootloader + partitions + app), прошивается через **esptool** или **Flash Download Tool**:

```powershell
# через esptool (уже установлен в PlatformIO):
python -m esptool --chip esp32c6 --port COM3 write_flash 0x0 dist/ESP-Sensors-factory.bin
```

Либо через **Flash Download Tool** от Espressif: выберите чип ESP32-C6, добавьте `ESP-Sensors-factory.bin` со смещением `0x0` и нажмите START.

## Следующие шаги (когда появятся сенсоры)

1. Подключить SHT45 (I2C) и BMP581 (SPI)
2. Добавить драйверы в `src/`
3. Заменить демо-значения на реальные чтения в `main.cpp`

## Лицензия

MIT License