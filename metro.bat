@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

:: ─── Метро СПб — полностью автоматизированный скрипт (Windows) ───
:: Сам проверяет и устанавливает все зависимости через MSYS2.

set "REPO_URL=https://github.com/zhenya1488/metro.git"
set "DEFAULT_BRANCH=dev"
cd /d "%~dp0"

if "%~1"=="" goto :start
if "%~1"=="update" goto :update
if "%~1"=="switch" goto :switch
if "%~1"=="build" goto :build
if "%~1"=="run" goto :run
if "%~1"=="start" goto :start
if "%~1"=="deps" goto :ensure_deps
if "%~1"=="clean" goto :clean
if "%~1"=="status" goto :status
if "%~1"=="help" goto :help
if "%~1"=="-h" goto :help
echo [metro] Неизвестная команда: %~1
goto :help

:: ═══════════════════════════════════════════════════════════
:: Автоустановка зависимостей
:: ═══════════════════════════════════════════════════════════

:ensure_deps
echo [metro] Проверка зависимостей...

:: Git
where git >nul 2>&1 || (
    echo [metro] Git не найден. Устанавливаю...
    where winget >nul 2>&1 && (
        winget install --id Git.Git -e --source winget --accept-package-agreements --accept-source-agreements
    ) || (
        echo [metro] Установи Git вручную: https://git-scm.com/download/win
        exit /b 1
    )
)

:: MSYS2 (необходим для gcc, gtk4, libadwaita)
set "MSYS2_DIR="
if exist "C:\msys64\usr\bin\bash.exe" set "MSYS2_DIR=C:\msys64"
if exist "C:\tools\msys64\usr\bin\bash.exe" set "MSYS2_DIR=C:\tools\msys64"
if exist "%USERPROFILE%\msys64\usr\bin\bash.exe" set "MSYS2_DIR=%USERPROFILE%\msys64"

if "!MSYS2_DIR!"=="" (
    echo [metro] MSYS2 не найден. Устанавливаю...
    where winget >nul 2>&1 && (
        winget install --id MSYS2.MSYS2 -e --accept-package-agreements --accept-source-agreements
        echo [metro] MSYS2 установлен. Перезапусти скрипт.
        exit /b 0
    ) || (
        echo [metro] Установи MSYS2 вручную: https://www.msys2.org
        exit /b 1
    )
)

echo [metro] MSYS2 найден: !MSYS2_DIR!

:: Установка пакетов через pacman в MSYS2 MINGW64
set "BASH=!MSYS2_DIR!\usr\bin\bash.exe"
set "PACMAN_PACKAGES=mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-pkg-config mingw-w64-x86_64-openssl mingw-w64-x86_64-gtk4 mingw-w64-x86_64-libadwaita"

echo [metro] Проверка и установка пакетов MSYS2...
"!BASH!" --login -c "pacman -S --needed --noconfirm %PACMAN_PACKAGES%"

if errorlevel 1 (
    echo [metro] Ошибка установки пакетов MSYS2.
    exit /b 1
)

echo [metro] Все зависимости установлены.
goto :eof

:: ═══════════════════════════════════════════════════════════

:update
if not exist ".git" (
    echo [metro] Клонирование репозитория...
    cd ..
    git clone -b %DEFAULT_BRANCH% %REPO_URL% metro
    cd metro
) else (
    for /f %%b in ('git branch --show-current') do set "BRANCH=%%b"
    echo [metro] Обновление ветки !BRANCH!...
    git pull origin !BRANCH! --ff-only 2>nul || git pull origin !BRANCH! --rebase
)
echo [metro] Репозиторий обновлён.
goto :eof

:switch
if "%~2"=="" (
    for /f %%b in ('git branch --show-current') do echo Текущая: %%b
    echo Использование: %~nx0 switch ^<main^|dev^>
    goto :eof
)
echo [metro] Переключение на %~2...
git fetch origin
git checkout %~2 2>nul || git checkout -b %~2 origin/%~2
git pull origin %~2 --ff-only 2>nul
echo [metro] Ветка: %~2
goto :eof

:build
set "TARGET=%~2"
if "%TARGET%"=="" set "TARGET=all"

:: Автоустановка зависимостей
call :ensure_deps

:: Определяем MSYS2 MINGW64 окружение для сборки
set "MSYS2_DIR="
if exist "C:\msys64" set "MSYS2_DIR=C:\msys64"
if exist "C:\tools\msys64" set "MSYS2_DIR=C:\tools\msys64"
if exist "%USERPROFILE%\msys64" set "MSYS2_DIR=%USERPROFILE%\msys64"

set "BASH=!MSYS2_DIR!\usr\bin\bash.exe"
set "BUILDCMD=cd '%cd:\=/%' && export PATH=/mingw64/bin:$PATH"

if "%TARGET%"=="cli" (
    echo [metro] Сборка CLI...
    "!BASH!" --login -c "!BUILDCMD! && make cli"
) else if "%TARGET%"=="gtk" (
    echo [metro] Сборка GTK...
    "!BASH!" --login -c "!BUILDCMD! && make gtk"
) else if "%TARGET%"=="all" (
    echo [metro] Сборка CLI + GTK...
    "!BASH!" --login -c "!BUILDCMD! && make all"
) else (
    echo [metro] Неизвестная цель: %TARGET% ^(cli^|gtk^|all^)
    goto :eof
)

if errorlevel 1 (
    echo [metro] Ошибка сборки.
    exit /b 1
)
echo [metro] Сборка завершена.
goto :eof

:run
set "MODE=%~2"
if "%MODE%"=="" set "MODE=gtk"

if "%MODE%"=="cli" (
    if not exist "metro-cli.exe" if not exist "metro-cli" call :build cli
    echo [metro] Запуск CLI...
    if exist "metro-cli.exe" (metro-cli.exe) else (metro-cli)
) else if "%MODE%"=="gtk" (
    if not exist "metro-gtk.exe" if not exist "metro-gtk" call :build gtk
    echo [metro] Запуск GTK...
    if exist "metro-gtk.exe" (start "" metro-gtk.exe) else (start "" metro-gtk)
) else (
    echo [metro] Неизвестный режим: %MODE% ^(cli^|gtk^)
)
goto :eof

:start
call :update
call :build all
call :run gtk
goto :eof

:clean
echo [metro] Очистка...
del /f metro-cli.exe metro-gtk.exe metro-cli metro-gtk 2>nul
echo [metro] Готово.
goto :eof

:status
echo OS:      Windows
for /f %%b in ('git branch --show-current 2^>nul') do echo Ветка:   %%b
for /f "delims=" %%c in ('git log --oneline -1 2^>nul') do echo Коммит:  %%c
if exist "metro-cli.exe" (echo CLI:     собран) else if exist "metro-cli" (echo CLI:     собран) else (echo CLI:     не собран)
if exist "metro-gtk.exe" (echo GTK:     собран) else if exist "metro-gtk" (echo GTK:     собран) else (echo GTK:     не собран)
set "MSYS2_DIR="
if exist "C:\msys64" set "MSYS2_DIR=C:\msys64"
if exist "C:\tools\msys64" set "MSYS2_DIR=C:\tools\msys64"
if "!MSYS2_DIR!"=="" (echo MSYS2:   не установлен) else (echo MSYS2:   !MSYS2_DIR!)
goto :eof

:help
echo.
echo   Метро СПб — автоматизированный скрипт (Windows)
echo.
echo   Использование:
echo     %~nx0                   Обновить + установить всё + собрать + запустить
echo     %~nx0 update            Обновить из GitHub
echo     %~nx0 switch ^<ветка^>    Переключить ветку (main/dev)
echo     %~nx0 build [cli^|gtk]   Собрать (автоустановка MSYS2 + пакетов)
echo     %~nx0 run [cli^|gtk]     Запустить (автосборка если нужно)
echo     %~nx0 deps              Установить зависимости
echo     %~nx0 clean             Удалить бинарники
echo     %~nx0 status            Показать состояние
echo     %~nx0 help              Эта справка
echo.
echo   Зависимости (устанавливаются автоматически):
echo     Git          — через winget
echo     MSYS2        — через winget
echo     gcc, gtk4, libadwaita, openssl — через MSYS2 pacman
goto :eof
