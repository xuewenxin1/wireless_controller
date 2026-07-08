@echo off
rem Helper: OHX51 H386 (parentheses safe for AfterBuildRun.bat)
set "OBJ=%~1"
set "HEX=%~2"
if not defined OBJ set "OBJ=obj\T5L51"
if not defined HEX set "HEX=obj\T5L51.hex"
set "OHX51="
if exist "D:\Keil_v5\C51\C51\BIN\OHX51.exe" set "OHX51=D:\Keil_v5\C51\C51\BIN\OHX51.exe"
if not defined OHX51 if defined C51BIN if exist "%C51BIN%\OHX51.exe" set "OHX51=%C51BIN%\OHX51.exe"
if not defined OHX51 if exist "%KEIL%\C51\C51\BIN\OHX51.exe" set "OHX51=%KEIL%\C51\C51\BIN\OHX51.exe"
if not defined OHX51 (
  echo ERROR: OHX51.exe not found
  exit /b 1
)
if not exist "%OBJ%" (
  echo ERROR: linker output not found: %OBJ%
  exit /b 1
)
"%OHX51%" "%OBJ%" H386 HEXFILE (%HEX%)
exit /b %ERRORLEVEL%
