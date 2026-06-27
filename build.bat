@echo off
setlocal enabledelayedexpansion
title EchoBox II — сборка

:: ── Настрой свой путь к Qt здесь ─────────────────────────────────────────────
set QT_DIR=D:\Qt\6.11.1\mingw_64
set CMAKE_EXE=D:\Qt\Tools\CMake_64\bin\cmake.exe
set NINJA_EXE=D:\Qt\Tools\Ninja\ninja.exe
set MINGW_DIR=D:\Qt\Tools\mingw1310_64\bin
:: ─────────────────────────────────────────────────────────────────────────────

set BUILD_DIR=build\release
set DIST_DIR=dist\EchoBox-II

echo.
echo  ================================================
echo   EchoBox II — Release Build
echo  ================================================
echo.

:: Добавляем MinGW и Qt в PATH
set PATH=%MINGW_DIR%;%QT_DIR%\bin;%PATH%

:: Проверка наличия Qt
if not exist "%QT_DIR%\bin\qmake.exe" (
    echo  [ОШИБКА] Qt не найден по пути: %QT_DIR%
    echo  Отредактируй QT_DIR в начале этого файла.
    pause & exit /b 1
)

:: ── Шаг 1: CMake конфигурация ─────────────────────────────────────────────────
echo  [1/3] Конфигурация CMake...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

"%CMAKE_EXE%" -S . -B "%BUILD_DIR%" ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%"

if errorlevel 1 (
    echo  [ОШИБКА] CMake конфигурация провалилась
    pause & exit /b 1
)

:: ── Шаг 2: Сборка ─────────────────────────────────────────────────────────────
echo.
echo  [2/3] Сборка...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release -j4

if errorlevel 1 (
    echo  [ОШИБКА] Сборка провалилась
    pause & exit /b 1
)

:: ── Шаг 3: Деплой (копирование DLL) ──────────────────────────────────────────
echo.
echo  [3/3] Деплой (копирование Qt DLL)...

if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"

:: Копируем .exe
copy "%BUILD_DIR%\EchoBoxII.exe" "%DIST_DIR%\EchoBoxII.exe" >nul

:: windeployqt копирует все нужные Qt DLL автоматически
"%QT_DIR%\bin\windeployqt.exe" "%DIST_DIR%\EchoBoxII.exe" ^
    --release ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw

if errorlevel 1 (
    echo  [ОШИБКА] windeployqt провалился
    pause & exit /b 1
)

:: Копируем MinGW runtime DLL (нужны для запуска без установленного Qt)
echo  Копирование MinGW runtime...
for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%MINGW_DIR%\%%D" (
        copy "%MINGW_DIR%\%%D" "%DIST_DIR%\%%D" >nul
        echo    + %%D
    )
)

:: Копируем OpenSSL DLL (нужны для HTTPS-запросов)
echo  Копирование OpenSSL...
set OPENSSL_DIR=%MINGW_DIR%\..\opt\bin
for %%D in (libssl-1_1-x64.dll libcrypto-1_1-x64.dll) do (
    if exist "%OPENSSL_DIR%\%%D" (
        copy "%OPENSSL_DIR%\%%D" "%DIST_DIR%\%%D" >nul
        echo    + %%D
    )
)

:: ── Готово ────────────────────────────────────────────────────────────────────
echo.
echo  ================================================
echo   Готово! Папка для распространения:
echo   %CD%\%DIST_DIR%
echo  ================================================
echo.
echo  Можно заархивировать папку dist\EchoBox-II и отдать кому угодно.
echo  Qt устанавливать не нужно.
echo.

:: Открыть папку в проводнике
explorer "%DIST_DIR%"

pause
