# План: libadwaita + исправление цветов + pizza-transfer

## Контекст

Три проблемы: (1) линии без цвета из-за `\r` в .dat файлах, (2) устаревший дизайн — нужна libadwaita, (3) пересадочные станции должны быть "pizza-slice" с цветами всех веток.

## 1. Исправление цветов линий (корневая причина)

**Файл:** `metroapp/pathfinder.c`

Баг: файлы данных имеют `\r\n` окончания строк. `sscanf("%[^\n]")` читает `"red\r"` вместо `"red"`. `parseLineColor(strcmp("red\r","red"))` возвращает серый.

Исправление: в `get_lines()` заменить `%[^\n]` на `%[^\r\n]`. Аналогично проверить `get_stations()` и `get_edges()`.

## 2. libadwaita — современный UI

**Установка:** `brew install libadwaita`

**Файлы:** `gtkapp/main.c`, `gtkapp/app-window.c`, `gtkapp/app-window.h`, `Makefile`

Изменения:
- `#include <adwaita.h>` вместо `<gtk/gtk.h>`
- `adw_application_new()` вместо `gtk_application_new()`
- `AdwApplicationWindow` с `AdwHeaderBar` (встроенный title bar)
- `AdwToolbarView` для layout
- `adw_style_manager_set_color_scheme(ADW_COLOR_SCHEME_FORCE_DARK)` — тёмная тема
- Makefile: добавить `pkg-config --cflags --libs libadwaita-1`
- CSS упростить — libadwaita даёт хороший дефолтный стиль

## 3. Pizza-slice для пересадочных станций

**Файл:** `gtkapp/metro-map.c`

Для каждой пересадочной станции:
1. Собрать все уникальные line_id: собственный + от transfer-соседей
2. Нарисовать круг разделённый на секторы (cairo_arc), каждый в цвете своей ветки
3. Белый ободок поверх

Новые функции:
- `colorForLineId(unsigned char lineId)` → Color
- `collectTransferColors(int stationIdx, Color* colors, int* count)`
- `drawPizzaStation(cairo_t*, double x, double y, double r, int stationIdx, double alpha)`

## Файлы и изменения

| Файл | Изменение |
|---|---|
| `metroapp/pathfinder.c` | Fix `\r` в sscanf |
| `gtkapp/main.c` | adw_application_new, include adwaita.h |
| `gtkapp/app-window.h` | include adwaita.h |
| `gtkapp/app-window.c` | AdwApplicationWindow, AdwHeaderBar, AdwToolbarView, убрать inline CSS (libadw даёт стиль) |
| `gtkapp/metro-map.c` | pizza-slice transfer stations, colorForLineId |
| `gtkapp/metro-map.h` | include adwaita.h |
| `gtkapp/route-panel.h` | include adwaita.h |
| `Makefile` | libadwaita-1 в pkg-config |

## Порядок

1. `brew install libadwaita`
2. Fix `\r` в pathfinder.c
3. Обновить headers (adwaita.h)
4. metro-map.c: pizza-slice + colorForLineId
5. app-window.c: AdwApplicationWindow + AdwHeaderBar
6. main.c: adw_application_new
7. Makefile: libadwaita-1
8. Сборка и тест

## Верификация

1. `make gtk` без ошибок
2. Линии ЦВЕТНЫЕ (красная, синяя, зелёная, оранжевая, фиолетовая, коричневая)
3. Пересадочные станции — pizza-slice с цветами веток
4. Современный вид: AdwHeaderBar, тёмная тема через libadwaita
5. Zoom/pan работают
