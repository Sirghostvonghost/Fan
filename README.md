# Airscape Fan Temperature Controller

This C++ program automatically turns off an Airscape 3.5e fan when the temperature falls below a specified threshold, using the Airscape's official API.

## Features

- Monitors temperature using the fan's built-in temperature sensors
- Automatically turns off the fan when temperature falls below a set threshold
- Configurable temperature threshold and check interval
- Works with all Airscape Gen 2 Control fans (Sierra, 2.5e, 3.5e, etc.)
- Command-line configuration options for easy use

## Requirements

- C++ compiler with C++17 support
- CMake (3.10 or newer)
- libcurl
- nlohmann/json library (will be downloaded automatically if not found)

## Command-Line Options

The program accepts the following command-line parameters:

```
fan_controller [options]

Options:
  --ip=ADDRESS      IP address of the Airscape fan (default: 192.168.1.100)
  --temp=VALUE      Temperature threshold in °F (default: 75.0)
  --source=SOURCE   Temperature source to use (inside, attic, oa) (default: inside)
  --name=NAME       Name for the fan (default: "Airscape Fan")
  --interval=SECS   Check interval in seconds (default: 60)
  --help            Display help message
```

### Examples:

```
fan_controller --ip=192.168.1.100 --temp=72 --source=attic
fan_controller --ip=192.168.7.193 --name="Attic Fan" --temp=68
```

You can also use positional arguments in this order: IP, temperature, source, name:

```
fan_controller 192.168.1.100 72 attic "My Attic Fan"
```

## Building the Program

### Windows with Visual Studio

1. Install Visual Studio with C++ development tools
2. Install vcpkg (https://github.com/microsoft/vcpkg)
3. Install dependencies:
   ```
   vcpkg install curl:x64-windows nlohmann-json:x64-windows
   ```
4. Open a command prompt and run:
   ```
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake
   cmake --build . --config Release
   ```

### Linux

1. Install dependencies:
   ```
   sudo apt-get install build-essential cmake libcurl4-openssl-dev nlohmann-json-dev
   ```
2. Build the program:
   ```
   mkdir build
   cd build
   cmake ..
   make
   ```

## Usage

Run the compiled executable with your desired settings:

```
./fan_controller --ip=192.168.1.100 --temp=75 --source=inside
```

The program will:
1. Connect to your Airscape fan using its IP address
2. Check the fan's temperature sensor at regular intervals
3. Turn off the fan when the temperature falls below the threshold
4. Print status messages to the console

## API Information

This program uses the official Airscape Generation 2 Controls API. The API endpoints are:

- `/status.json.cgi` - Get fan status and temperature data in JSON format
- `/fanspd.cgi?dir=4` - Turn the fan off
- `/fanspd.cgi?dir=1` - Increase fan speed
- `/fanspd.cgi?dir=3` - Decrease fan speed
- `/fanspd.cgi?dir=2` - Add one hour to the timer

The fan reports three temperature values:
- `inside`: Inside/house temperature
- `attic`: Attic temperature
- `oa`: Outside air temperature

For more information about the API, see the documentation included with your fan or visit:
https://blog.airscapefans.com/archives/gen-2-controls-api