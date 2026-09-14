# WeatherInformer

A desktop weather application built with **Qt 6 / C++17** that shows a 7-day forecast and hourly breakdown for any city, using live data from the [Open-Meteo](https://open-meteo.com/) API.

## Features

- **City search** — type a city name and press Enter; the app geocodes it to coordinates via the Open-Meteo Geocoding API.
- **7-day forecast** — a table with daily min/max temperature, "feels like" temperature, and a weather-condition icon per day.
- **Hourly drill-down** — click any day to open an hourly view (24 columns) with per-hour temperature and condition icons.
- **Day/night aware icons** — 16 custom weather icons (sunny, cloudy, foggy, rainy, heavy rain, snowy, stormy) with separate day and night variants, chosen using the API's `is_day` flag and the city's local time (via `utc_offset_seconds`).
- **Custom UI polish** — a clear ("×") button drawn at runtime with `QPainter` inside the search field, and the whole app is skinned with a custom Qt stylesheet (`style.qss`).

## Tech stack

| Area | Technology |
|---|---|
| Language | C++17 |
| UI framework | Qt 6 Widgets (`QMainWindow`, `QStackedWidget`, `QTableWidget`) |
| Networking | Qt Network (`QNetworkAccessManager`, async requests via lambda slots) |
| Data format | JSON (`QJsonDocument` / `QJsonObject` / `QJsonArray`) |
| Styling | Qt Style Sheets (QSS), resources bundled via `.qrc` |
| External APIs | [Open-Meteo Geocoding API](https://open-meteo.com/en/docs/geocoding-api), [Open-Meteo Forecast API](https://open-meteo.com/en/docs) |
| Build system | qmake (`WeatherInformer.pro`) |

## How it works

1. `geocodeCity()` sends an async GET to the Open-Meteo geocoding endpoint and resolves a city name to latitude/longitude.
2. `fetchWeather()` requests daily (`temperature_2m_max/min`, `apparent_temperature_max`, `weathercode`) and hourly (`temperature_2m`, `weathercode`, `is_day`) data for those coordinates, with `timezone=auto` so the API returns the city's own UTC offset.
3. Responses are parsed from JSON into `QVector`s and rendered into two `QTableWidget`s: a 7-day overview and an hourly view, navigated through a `QStackedWidget`.
4. `weatherCodeToIcon()` maps [WMO weather codes](https://open-meteo.com/en/docs) to one of 16 bundled icons, picking the day or night variant based on `is_day` / the city's local hour.

## Project structure

```
WeatherInformer/
├── main.cpp            # application entry point
├── mainwindow.h/.cpp    # UI logic, networking, JSON parsing, weather-code mapping
├── mainwindow.ui        # Qt Designer layout (search bar, week/day table pages)
├── style.qss            # application stylesheet
├── resource.qrc         # Qt resource bundle (stylesheet + icons)
├── img/                 # weather condition icons (day/night variants)
└── WeatherInformer.pro  # qmake project file
```

## Build & run

Requirements: Qt 6 (Widgets + Network modules) and a C++17 compiler.

**Qt Creator:** open `WeatherInformer.pro` and press Run.

**Command line:**

```bash
qmake WeatherInformer.pro
make
./WeatherInformer
```

## Roadmap ideas

- Persist the last searched city between sessions.
- Graceful UI feedback for "city not found" / network errors (currently logged to console only).
- Unit conversion (°C / °F).
