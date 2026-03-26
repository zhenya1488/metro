@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

:: ─── Метро СПб — Windows скрипт управления ───

set "REPO_URL=https://github.com/zhenya1488/metro.git"
set "DEFAULT_BRANCH=dev"
cd /d "%~dp0"

if "%~1"=="" goto :start
if "%~1"=="update" goto :update
if "%~1"=="switch" goto :switch
if "%~1"=="build" goto :build
if "%~1"=="run" goto :run
if "%~1"=="start" goto :start
if "%~1"=="deps" goto :deps
if "%~1"=="clean" goto :clean
if "%~1"=="status" goto :status
if "%~1"=="help" goto :help
if "%~1"=="-h" goto :help
echo [metro] Неизвестная команда: %~1
goto :help

:: ─── Установка зависимостей ───
:deps
echo [metro] Установка зависимостей для Windows...
where choco >nul 2>&1 && (
    choco install -y mingw openssl pkgconfiglite
    echo [metro] Для GTK-версии установи MSYS2: https://www.msys2.org
    echo [metro] В MSYS2: pacman -S mingw-w64-x86_64-gtk4 mingw-w64-x86_64-libadwaita
) || (
    where winget >nul 2>&1 && (
        winget install MSYS2.MSYS2
        echo [metro] В MSYS2 MINGW64 выполни:
        echo   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gtk4 mingw-w64-x86_64-libadwaita mingw-w64-x86_64-openssl
    ) || (
        echo [metro] Установи MSYS2 вручную: https://www.msys2.org
        echo [metro] Затем в MSYS2 MINGW64:
        echo   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gtk4 mingw-w64-x86_64-libadwaita mingw-w64-x86_64-openssl
    )
)
goto :eof

:: ─── Обновление ───
:update
if not exist ".git" (
    echo [metro] Клонирование репозитория...
    cd ..
    git clone -b %DEFAULT_BRANCH% %REPO_URL% metro
    cd metro
) else (
    for /f %%b in ('git branch --show-current') do set "BRANCH=%%b"
    echo [metro] Обновление ветки !BRANCH!...
    git pull origin !BRANCH! --ff-only || git pull origin !BRANCH! --rebase
)
echo [metro] Репозиторий обновлён.
goto :eof

:: ─── Переключение ветки ───
:switch
if "%~2"=="" (
    for /f %%b in ('git branch --show-current') do echo Текущая ветка: %%b
    echo Использование: %~nx0 switch ^<main^|dev^>
    goto :eof
)
echo [metro] Переключение на %~2...
git fetch origin
git checkout %~2 2>nul || git checkout -b %~2 origin/%~2
git pull origin %~2 --ff-only 2>nul
echo [metro] Ветка: %~2
goto :eof

:: ─── Сборка ───
:build
set "TARGET=%~2"
if "%TARGET%"=="" set "TARGET=cli"

where gcc >nul 2>&1 || (
    echo [metro] gcc не найден. Установи MinGW или MSYS2.
    goto :eof
)

if "%TARGET%"=="cli" (
    echo [metro] Сборка CLI...
    gcc -Wall -O2 -o metro-cli.exe metroapp/main.c metroapp/pathfinder.c -lssl -lcrypto
    echo [metro] Готово: metro-cli.exe
) else if "%TARGET%"=="gtk" (
    echo [metro] Сборка GTK ^(требуется MSYS2 MINGW64^)...
    make gtk
    echo [metro] Готово: metro-gtk.exe
) else if "%TARGET%"=="all" (
    call :build cli
    call :build gtk
) else (
    echo [metro] Неизвестная цель: %TARGET% ^(cli^|gtk^|all^)
)
goto :eof

:: ─── Запуск ───
:run
set "MODE=%~2"
if "%MODE%"=="" set "MODE=cli"

if "%MODE%"=="cli" (
    if not exist "metro-cli.exe" call :build cli
    echo [metro] Запуск CLI...
    metro-cli.exe
) else if "%MODE%"=="gtk" (
    if not exist "metro-gtk.exe" call :build gtk
    echo [metro] Запуск GTK...
    start "" metro-gtk.exe
) else (
    echo [metro] Неизвестный режим: %MODE% ^(cli^|gtk^)
)
goto :eof

:: ─── Полный цикл ───
:start
call :update
call :build cli
where pkg-config >nul 2>&1 && (
    pkg-config --exists libadwaita-1 >nul 2>&1 && call :build gtk
)
if exist "metro-gtk.exe" (
    start "" metro-gtk.exe
) else if exist "metro-cli.exe" (
    metro-cli.exe
) else (
    echo [metro] Сборка не удалась.
)
goto :eof

:: ─── Очистка ───
:clean
echo [metro] Очистка...
del /f metro-cli.exe metro-gtk.exe 2>nul
echo [metro] Готово.
goto :eof

:: ─── Статус ───
:status
echo OS:     Windows
for /f %%b in ('git branch --show-current 2^>nul') do echo Ветка:  %%b
for /f "delims=" %%c in ('git log --oneline -1 2^>nul') do echo Коммит: %%c
if exist "metro-cli.exe" (echo CLI:    собран) else (echo CLI:    не собран)
if exist "metro-gtk.exe" (echo GTK:    собран) else (echo GTK:    не собран)
goto :eof

:: ─── Help ───
:help
echo.
echo   Метро СПб — скрипт управления
echo.
echo   Использование:
echo     %~nx0                   Обновить + собрать + запустить
echo     %~nx0 update            Обновить из GitHub
echo     %~nx0 switch ^<ветка^>    Переключить ветку (main/dev)
echo     %~nx0 build [cli^|gtk]   Собрать проект
echo     %~nx0 run [cli^|gtk]     Запустить приложение
echo     %~nx0 deps              Установить зависимости
echo     %~nx0 clean             Удалить собранные файлы
echo     %~nx0 status            Показать состояние
echo     %~nx0 help              Эта справка
echo.
echo   Примеры:
echo     %~nx0                   полный цикл
echo     %~nx0 switch main       переключиться на main
echo     %~nx0 build cli         собрать CLI-версию
echo     %~nx0 run cli           запустить консольную версию
goto :eof
