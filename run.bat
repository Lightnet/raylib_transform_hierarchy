@echo off
setlocal
set APPNAME=raylib_luajit
set APPPATH=build\Debug\%APPNAME%.exe
set EXECUTABLE=%APPNAME%.exe
set DEMO_SRC=demo.lua
set DEMO_DEST=build\Debug\demo.lua

echo %DEMO_SRC%
echo %DEMO_DEST%

if not exist %APPPATH% (
    echo Executable not found! Please build the project first.
    exit /b 1
)
echo copying %DEMO_SRC%
copy %DEMO_SRC% %DEMO_DEST%

@REM @REM Check if demo.lua exists in the output directory, copy it if not
@REM if not exist demo.lua (
@REM     echo demo.lua not found in output directory, copying from source...
@REM     copy %DEMO_SRC% %DEMO_DEST%
@REM     if %ERRORLEVEL% NEQ 0 (
@REM         echo Failed to copy demo.lua!
@REM         exit /b %ERRORLEVEL%
@REM     )
@REM )

echo Running raylib_luajit...

cd build\Debug\

%EXECUTABLE% demo.lua

@REM if %ERRORLEVEL% NEQ 0 (
@REM     echo Program exited with error code %ERRORLEVEL%
@REM     exit /b %ERRORLEVEL%
@REM )

@REM echo Program ran successfully!
endlocal