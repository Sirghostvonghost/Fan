@echo off
echo Airscape Fan Controller Build Script
echo ===================================

REM Check if build directory exists, create if not
if not exist build mkdir build

REM Navigate to build directory and run CMake
cd build
echo Running CMake configuration...

REM Detect if vcpkg is available
if exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo Found vcpkg, using it for dependencies...
    cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
) else (
    echo vcpkg not found, attempting to build without it...
    echo You may need to install dependencies manually.
    cmake ..
)

REM Build the project
echo Building the project...
cmake --build . --config Release

REM Check if build was successful
if %ERRORLEVEL% NEQ 0 (
    echo Build failed. Please check the error messages above.
    exit /b %ERRORLEVEL%
)

echo Build successful!
echo.
echo Example usage:
echo.
echo - Default settings:
echo   build\Release\fan_controller.exe
echo.
echo - Specify fan IP address:
echo   build\Release\fan_controller.exe --ip=192.168.1.100
echo.
echo - Custom settings:
echo   build\Release\fan_controller.exe --ip=192.168.1.100 --temp=72 --source=attic --name="Attic Fan"
echo.

REM Ask if user wants to run the program
set /p RUNPROG=Do you want to run the program now? (y/n): 

if /i "%RUNPROG%"=="y" (
    set /p FAN_IP=Enter fan IP address (default: 192.168.1.100): 
    
    if "%FAN_IP%"=="" (
        echo Starting fan controller with default IP...
        Release\fan_controller.exe
    ) else (
        echo Starting fan controller with IP: %FAN_IP%
        Release\fan_controller.exe --ip=%FAN_IP%
    )
)

cd ..