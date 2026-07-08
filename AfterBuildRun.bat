@echo off
setlocal

rem Code banking: Keil built-in OHX51 uses standard HEX and aborts.
rem Generate HEX-386 here instead (see T5L51.uvproj CreateHexFile=0).

if not exist obj\T5L51 (
  echo ERROR: obj\T5L51 not found - link step failed?
  exit /b 1
)

call tools\make_hex386.cmd obj\T5L51 obj\T5L51.hex
if errorlevel 1 (
  echo ERROR: make_hex386 failed
  exit /b 1
)

if not exist Download mkdir Download
copy /Y obj\T5L51.hex Download\ >nul
cd Download

if not exist dynamic_load_tool.exe (
    if exist ..\..\T5L51_new\Download\dynamic_load_tool.exe (
        copy /Y ..\..\T5L51_new\Download\dynamic_load_tool.exe . >nul
    )
)
if not exist 22_Config.bin (
    if exist ..\..\T5L51_new\Download\22_Config.bin (
        copy /Y ..\..\T5L51_new\Download\22_Config.bin . >nul
    )
)

if not exist dynamic_load_tool.exe (
    echo ERROR: dynamic_load_tool.exe not found in Download\
    echo Please copy from T5L51_new\Download\dynamic_load_tool.exe
    exit /b 1
)

del /Q *.bank 2>nul
dynamic_load_tool.exe 8 256 16384 0
if errorlevel 1 exit /b 1

cd ..
