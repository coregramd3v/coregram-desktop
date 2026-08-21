@echo off
setlocal enabledelayedexpansion
title CoreGram Desktop - avtosborka
chcp 65001 >nul

echo ==========================================================
echo   CoreGram Desktop - avtosborka (Windows x64)
echo ==========================================================
echo.

set "REPO=%~dp0"
if "%REPO:~-1%"=="\" set "REPO=%REPO:~0,-1%"
for %%I in ("%REPO%\..") do set "PARENT=%%~fI"

echo Repozitorij : %REPO%
echo Biblioteki  : %PARENT%\Libraries
echo.

set "API_ID=2040"
set "API_HASH=b18441a1ff607e10a989891a5462e627"
set /p "IN_ID=api_id (Enter = testovyj 2040): "
if not "%IN_ID%"=="" set "API_ID=%IN_ID%"
set /p "IN_HASH=api_hash (Enter = testovyj): "
if not "%IN_HASH%"=="" set "API_HASH=%IN_HASH%"
echo.

set "MISSING="
where git    >nul 2>nul || (echo [!] Git ne najden    & set "MISSING=1")
where python >nul 2>nul || (echo [!] Python ne najden & set "MISSING=1")
where cmake  >nul 2>nul || (echo [!] CMake ne najden  & set "MISSING=1")

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
)
if not defined VSPATH (echo [!] Visual Studio 2022 s C++ ne najdena & set "MISSING=1")

if defined MISSING (
  echo.
  echo Ne hvataet instrumentov. Ustanovit ih cherez winget? [Y/N]
  set /p "ANS="
  if /i "!ANS!"=="Y" (
    winget install --id Git.Git -e --accept-source-agreements --accept-package-agreements
    winget install --id Python.Python.3.10 -e --accept-package-agreements
    winget install --id Kitware.CMake -e --accept-package-agreements
    winget install --id NASM.NASM -e --accept-package-agreements
    winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-package-agreements --override "--wait --quiet --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended"
    echo.
    echo Ustanovka zapushena. Posle nee PEREZAPUSTI etot fajl.
    pause
    exit /b 0
  ) else (
    echo Bez etih instrumentov sborka nevozmozhna. Vyhod.
    pause
    exit /b 1
  )
)

echo.
echo === Inicializiruju okruzhenie Visual Studio (x64) ===
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (echo [!] Ne udalos podnyat okruzhenie VS & pause & exit /b 1)

if not exist "%PARENT%\Libraries\.coregram_prepared" (
  echo.
  echo === Gotovlju biblioteki (Qt / OpenSSL / FFmpeg / ...) ===
  echo     Eto DOLGO - pervyj raz chas-dva. Ne zakryvaj okno.
  echo.
  pushd "%PARENT%"
  call "%REPO%\Telegram\build\prepare\win.bat"
  if errorlevel 1 (echo [!] Podgotovka bibliotek upala. Smotri soobshenie vyshe. & popd & pause & exit /b 1)
  popd
  echo prepared > "%PARENT%\Libraries\.coregram_prepared"
) else (
  echo Biblioteki uzhe gotovy - propuskaju podgotovku.
)

echo.
echo === Konfiguriruju proekt ===
pushd "%REPO%\Telegram"
call configure.bat x64 -D TDESKTOP_API_ID=%API_ID% -D TDESKTOP_API_HASH=%API_HASH%
if errorlevel 1 (echo [!] configure upal & popd & pause & exit /b 1)
popd

echo.
echo === Sobiraju Release (eto tozhe dolgo) ===
cmake --build "%REPO%\out" --config Release
if errorlevel 1 (echo [!] Sborka upala. Tekst oshibki vyshe. & pause & exit /b 1)

echo.
echo ==========================================================
echo   GOTOVO. Telegram.exe (CoreGram) sobran.
echo ==========================================================
set "EXE=%REPO%\out\Release\Telegram.exe"
if exist "%EXE%" (
  echo Fajl: %EXE%
  start "" "%REPO%\out\Release"
  choice /c YN /m "Zapustit CoreGram sejchas"
  if not errorlevel 2 start "" "%EXE%"
) else (
  echo [!] Telegram.exe ne najden v out\Release - proveri log sborki.
)
pause
exit /b 0
