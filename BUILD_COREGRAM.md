# Сборка CoreGram Desktop

Форк opengram (owpengram-desktop-client) с брендингом CoreGram и набором
клиентских фич («экстры»). Ниже — как собрать локально под Windows и Linux.
Всё это делается на **твоей машине**, не на сервере.

## Что уже в форке
- Брендинг CoreGram: иконки, имя приложения, метаданные Windows, протокол «CoreGram Link».
- Мультисервер (выбор сервера при входе), сервер по умолчанию `2.26.123.219`.
- Кошелёк/витрина CoreGram и домены `coregram.live`.
- Клиентские команды-экстры (`.snos` и др.) — модуль `Telegram/SourceFiles/coregram/coregram_extras.*`.

## Быстрый способ (Windows): build_coregram.bat

В корне репозитория лежит **`build_coregram.bat`** — двойной клик, и он сам:
- проверит инструменты (Git, Python, CMake, Visual Studio 2022 C++) и предложит
  доустановить недостающее через winget;
- спросит api_id / api_hash (Enter — тестовые, для локальной проверки хватает);
- поднимет окружение Visual Studio x64;
- соберёт зависимости (Qt/OpenSSL/FFmpeg) — первый раз час-два, потом пропускает;
- сконфигурирует и соберёт Release;
- откроет папку с готовым `Telegram.exe` и предложит запустить.

Библиотеки складываются в папку РЯДОМ с репозиторием (`..\Libraries`,
`..\ThirdParty`), поэтому клонируй репозиторий в отдельную папку, например
`D:\CoreGram\coregram-desktop`, и запускай .bat оттуда.

Если предпочитаешь вручную — шаги ниже.

## Windows (Visual Studio 2022)

Официальная инструкция tdesktop подходит один в один — меняется только имя.

1. Поставь: **Visual Studio 2022** (Desktop C++), **CMake 3.28+**, **Python 3**, **Git**, **NASM**, **Ninja**, **Yasm**, **MozillaBuild** (для perl/gperf).
2. Заведи каталог сборки, например `D:\CoreGram\`, и склонируй туда репозиторий:
   ```
   cd D:\CoreGram
   git clone https://github.com/coregramd3v/coregram-desktop.git tdesktop
   ```
3. Собери зависимости (один раз, долго — час-два):
   ```
   cd D:\CoreGram\tdesktop\Telegram\build
   prepare.py --arch x64
   ```
   Скрипт скачает и соберёт Qt, OpenSSL, FFmpeg и прочее в `D:\CoreGram\Libraries`.
4. Сгенерируй проект и собери Release:
   ```
   cd D:\CoreGram\tdesktop
   configure.bat x64 -D TDESKTOP_API_ID=<api_id> -D TDESKTOP_API_HASH=<api_hash>
   cmake --build out --config Release
   ```
   `api_id`/`api_hash` — твои с my.telegram.org (или тестовые 2040 / b18441a1ff607e10a989891a5462e627, для локального теста хватает).
5. Готовый `Telegram.exe` (с нашей иконкой и именем CoreGram) — в `out\Release`.

## Linux (Docker, как в оригинале)

Собирается в готовом образе, ничего ставить в систему не надо, кроме Docker.

```
git clone https://github.com/coregramd3v/coregram-desktop.git
cd coregram-desktop
docker build -t coregram-desktop:centos Telegram/build/docker/centos_env
Telegram/build/docker/centos_env/build.sh -D TDESKTOP_API_ID=<api_id> -D TDESKTOP_API_HASH=<api_hash>
```
Готовый бинарь `Telegram` (ELF) — в `out/Release`. Для отладочной сборки —
`build_debug.sh` вместо `build.sh`.

## macOS
`git clone`, затем по инструкции tdesktop для macOS (Xcode + `configure.sh`).
Иконка `.icns` и `.desktop`/бандл генерируются из наших PNG автоматически.

## Проверка после сборки
- В заголовке окна и в трее — **CoreGram**, иконка фиолетовая с самолётом.
- «Настройки → О программе» — CoreGram, не OwpenGram.
- Экстры: набери в любом чате `.snos @username` — откроется окно сноса (визуальное, на сервер ничего не уходит).

## Экстры (клиентские команды)
Живут в `Telegram/SourceFiles/coregram/coregram_extras.cpp`. Новая команда
добавляется одной записью в таблицу `Commands()` — см. комментарий в файле.
Все команды **локальные и визуальные**: перехватываются до отправки, на сервер
не уходят.
