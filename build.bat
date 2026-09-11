@echo off
setlocal

set BUILD_DIR=build

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake was not found in PATH.
    exit /b 1
)

cmake -S . -B %BUILD_DIR%
if errorlevel 1 exit /b 1

cmake --build %BUILD_DIR% --config Release
if errorlevel 1 exit /b 1

ctest --test-dir %BUILD_DIR% -C Release --output-on-failure
if errorlevel 1 exit /b 1

echo.
echo Build and tests completed successfully.
echo Headless executable is under %BUILD_DIR%\Release or %BUILD_DIR% depending on generator.
endlocal
