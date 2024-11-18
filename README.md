# Observatory Roof Controller

A fork of the original [INDI Aldiroof](https://github.com/dokeeffe/indi-aldiroof) project by Derek O'Keeffe, tailored to work with a simplified roof drive mechanism that operates with just two signals: **OPEN** and **CLOSE**.

This project includes an [INDI](http://indilib.org/) driver and Arduino firmware for controlling a roll-off roof in an astronomical observatory.  

![Observatory](./documentation/allsky-25.gif)

## Features

- Roof control using a motor and a sliding gate reducer.
- Control via two 30A relays connected to an Arduino microcontroller.
- Safety features, including limit switches for roof position and parking sensors (reed switches) for the telescope.
- Communication between the Arduino and the INDI driver using the Firmata protocol.

![Motor Controller](./documentation/motor-controller.jpg)

## Hardware Requirements

### Components
1. **Arduino UNO** (or any compatible microcontroller).
2. **2 x 30A relay modules** for Arduino.
3. A **Linux machine** running the INDI server.
4. *(Optional)* 3D-printed case for the hardware.

## Software Setup

### Flashing Firmware
1. Open the Arduino IDE.
2. Upload the firmware to your Arduino board.

### Building the INDI Driver

Follow these steps to install the necessary dependencies and build the driver:

1. **Install dependencies**  
   Ensure your Linux machine has the required tools and libraries:
   ```bash
   sudo apt-get update
   sudo apt-get install -y cmake libindi-dev libnova-dev
   ```

2. **Clone the repository**  
   Download the project from GitHub:
   ```bash
   git clone <repository-url>
   cd <repository-directory>
   ```

3. **Build and install**  
   Use the included script to build and install the driver:
   ```bash
   chmod +x ./indi-rollroof/install.sh
   ./indi-rollroof/install.sh
   ```

The script will:
- Create a `build` directory for the compiled files.
- Configure the project with `cmake`.
- Compile and install the driver using `make install`.

### Configuring INDI Web Manager
To add the driver to the INDI Web Manager, modify the driver list:

1. Edit the file `/usr/share/indi/drivers.xml`.
2. Add the following lines under the `<Domes>` section:
   ```xml
   <device label="Roll Roof" manufacturer="miksoft">
     <driver name="Roll Roof">indi_rollroof</driver>
     <version>1.0</version>
   </device>
   ```
### Arduino Controller Setup
![Arduino Controller](./documentation/arduino-controller.jpg)

## Acknowledgments

This project is based on the work of [Derek O'Keeffe](https://github.com/dokeeffe/indi-aldiroof) and integrates with the [INDI Library](http://indilib.org/). Special thanks to the open-source community for making projects like this possible.
