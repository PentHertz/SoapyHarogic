# 📡 SoapyHarogic

A SoapySDR module for Harogic HTRA series spectrum analysis devices. 🎯

This module allows software that supports the SoapySDR API (like GQRX, GNU Radio, CubicSDR, rtl_433, etc.) to use Harogic devices as a general-purpose SDR receiver. 🔧

**👨‍💻 Author:** Sébastien Dudek / [penthertz.com](https://penthertz.com)  
**📄 License:** LGPL v2.1

## ✨ Features

- 🔍 Auto-discovery of connected Harogic devices
- 📊 Supports IQ streaming in complex float 32-bit format (CF32)
- ⚙️ Configurable sample rates based on device capabilities
- 📻 Full control over RF frequency
- 🎛️ Comprehensive gain control through standard SoapySDR APIs:
    - 📏 Reference Level (REF)
    - 🔊 Preamplifier (PREAMP)
    - 🤖 IF AGC (IF_AGC)
- 🛠️ Support for device-specific settings:
    - 🎯 Gain Strategy (Low Noise / High Linearity)
    - 🔄 LO Optimization Mode

## 📦 Dependencies

Before building, you must have the following installed on your system:

- **🧩 SoapySDR Development Libraries**: libsoapysdr-dev
- **📚 Harogic HTRA API**: The libhtra_api.so library and the htra_api.h header file must be installed in a system path (e.g., /usr/local/lib/, /usr/local/include/, or in /opt/htraapi/***)
- **🔨 CMake**: cmake
- **⚡ A C++ Compiler**: g++

On a Debian/Ubuntu-based system, you can install the dependencies with:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake libsoapysdr-dev
```

🔧 **Important**: Calibration data must be placed in `/usr/bin/CalFile` directory in order to work.

## 🚀 Installation

### 1. 🏗️ Build the Module

Clone the repository and build the module using CMake:

```bash
git clone https://github.com/your-username/SoapyHarogic.git
cd SoapyHarogic
mkdir build
cd build
cmake ..
make
```

### 2. 📥 Install the Module

Install the compiled module to a system-wide SoapySDR path:

```bash
sudo make install
```

This will typically copy `libsoapyharogic.so` to `/usr/local/lib/SoapySDR/modules0.8/`. 📂

### 3. 🔐 USB Device Permissions (udev rule)

For non-root users to access the device, you must install a udev rule. 👤

1. 🔍 Find your device's Vendor and Product ID by running `lsusb`. Look for your Harogic device in the list.
2. 📝 Create a new udev rule file. A pre-made rule is included in this repository (99-harogic.rules). Copy it to the system directory:
  
  ```bash
  sudo cp ../99-harogic.rules /etc/udev/rules.d/
  ```
    
  💡 (If you don't use the provided file, make sure your custom rule looks like this, replacing the IDs with your device's actual IDs: `SUBSYSTEM=="usb", ATTRS{idVendor}=="1234", ATTRS{idProduct}=="5678", MODE="0666"`)
    
3. 🔄 Reload the udev rules and trigger them:
    ```bash
    sudo udevadm control --reload-rules
    sudo udevadm trigger
    ```
    
4. 🔌 Unplug and re-plug your Harogic device for the new rule to take effect.

### 4. ✅ Verify Installation

Check that SoapySDR can see your device and driver:
```bash
SoapySDRUtil --find="driver=harogic"
```

You should see output similar to this:

```
Found 1 device(s)
  0: driver=Harogic, label=Harogic 313**********, serial=3132***********
```

## ⚠️ Important Usage Notes

### 🎛️ Gain Control and Error -12 (IF Overflow)

The primary gain setting for this device is the **Reference Level**. This is a critical parameter that must be set correctly to avoid errors. 🎯

- ⬇️ If the Reference Level is set **too low** (e.g., -90 dBm, -100 dBm), the device's internal amplifiers may saturate. This will cause the device API to return an **IF Overflow** error (error code -12). In applications like GQRX or GNU Radio, this may appear as a STREAM_ERROR and the stream will stop. **If you see error -12, you must lower the Reference Level.**
   ⚠️ If the Reference Level is set **too high** (e.g., 0 dBm, 7 dBm), you may not see any signals, and the FFT display will look like a flat noise floor.

💡 **Pro Tip**: A good starting point for the Reference Level is typically between **-30 dBm and -50 dBm**. You will need to adjust it based on your antenna and the strength of the signals in your environment.

### 🔢 Sample Format (8-bit vs. 16-bit)

The device automatically uses different ADC sample resolutions depending on the selected sample rate to optimize performance. This driver handles the conversion automatically. 🤖

- ⚡ For sample rates **greater than 64 MS/s**, the device will stream in **8-bit** format (CS8)
- 🎯 For sample rates **at or below 64 MS/s**, the device will stream in **16-bit** format (CS16)

The driver always converts the native samples to 32-bit complex floats (CF32) for the application, so this is handled transparently. ✨

## SoapyHarogic Driver Options

This document outlines all the configurable options for the SoapyHarogic driver, for use in applications like GQRX, SDR++, GNU Radio, or via the command line with tools like SoapySDRUtil.

### Device Arguments

These arguments are used to find and select a specific Harogic device. They are entered in the "Device Arguments" field in GRC, GQRX, or as part of the device string.

|   |   |   |
|---|---|---|
|Argument Key|Example Value|Description|
|driver|harogic|**(Required)** This tells SoapySDR to load the Harogic driver.|
|serial|313251180036001A|**(Optional)** The unique serial number of the device. Use this if you have multiple Harogic devices connected to your system and need to select a specific one. If omitted, the first device found will be used.|
|label|Harogic 3132...|**(Read-Only)** A human-readable name for the device, automatically generated by the driver. You can use this to identify the device but cannot set it as an argument.|

**Example Device String:** driver=harogic,serial=313251180036001A

---

### Stream Arguments

These arguments configure the data stream itself and are typically set once before the stream is activated. They should be placed in the **"Stream Arguments"** field in GRC or SDR++.

|   |   |   |   |
|---|---|---|---|
|Argument Key|Example Value|Default|Description|
|force_8bit|true|false|If true, forces the device to use the 8-bit sample format (CS8) regardless of the sample rate. By default, the driver uses 8-bit mode only for rates above 64 MS/s. This can be useful for reducing USB bandwidth at lower rates.|

**Example Stream Arguments String:** force_8bit=true

---

### Channel Settings (RF and Gain)

These settings control the RF front-end and can usually be adjusted while the stream is active. They appear as sliders, dropdowns, or text boxes in SDR applications.

#### Antennas

The active antenna port can be selected from a dropdown menu.

- **Available Options:** External, Internal, ANT, T/R, SWR, INT
    

#### Gain Elements

The Harogic device uses a multi-stage gain system controlled by three separate elements.

|   |   |   |   |
|---|---|---|---|
|Gain Element|Type|Range|Description|
|REF|Integer|-100 to 7 [dBm]|**Reference Level.** This is the primary gain control. It sets the target power level for the top of the ADC's range. **Use a high value for strong signals to prevent overflow.**|
|PREAMP|Boolean|On/Off|Toggles the front-end low-noise preamplifier. On (1.0) enables it for better sensitivity on weak signals. Off (0.0) disables it.|
|IF_AGC|Boolean|On/Off|Toggles the Intermediate Frequency (IF) Automatic Gain Control.|

#### Other RF Settings

|   |   |   |   |
|---|---|---|---|
|Setting Key|Type|Options / Range|Description|
|gain_strategy|String|Low Noise, High Linearity|Optimizes the internal gain distribution. Low Noise is best for weak signals. High Linearity is better for environments with strong signals to prevent intermodulation.|
|lo_mode|String|Auto, Speed, Spurs, Phase Noise|Controls the Local Oscillator (LO) optimization strategy. Auto is recommended for general use. Other modes trade between tuning speed, spurious signal rejection, and phase noise performance.|

---

### Example Configuration in GNU Radio Companion (GRC)

To configure the Soapy Source block for a Harogic device:

1. **Device Arguments:** driver=harogic
2. **Stream Arguments:** force_8bit=true (if needed)
3. **Antenna:** Select from the dropdown (e.g., ANT)
4. **Gain Settings:**
    - Set the **"REF"** gain slider to an appropriate value (e.g., -50).
    - Set the **"PREAMP"** gain slider to 1 (On) or 0 (Off).
    - Set the **"IF_AGC"** gain slider to 1 (On) or 0 (Off).
5. **Device Settings:**
    - In the "Settings" field, you can specify gain_strategy=High Linearity or lo_mode=Spurs.

## 🎮 Usage

### 📊 GQRX

GQRX provides the most intuitive interface for controlling the device. 🖥️

1. 🚀 Open GQRX
2. ⚙️ Click the "Configure I/O devices" button (or go to File -> I/O devices)
3. 📋 In the "Device" dropdown, select your Harogic device. It should appear with its label. The "Device String" field should automatically populate with `driver=harogic,serial=...`
4. 🎛️ Go to the **"Input Controls"** tab on the right-hand side. You will find:
    - 📏 A slider for **REF** (Reference Level)
    - 🔊 A checkbox for **PREAMP**
    - 🤖 A checkbox for **IF_AGC**
    - 🎯 A dropdown for "Gain mode" (maps to our "Gain Strategy")
        
5. 🔧 Other device-specific settings like **"LO Mode"** can be found in the main GQRX toolbar under Device -> Device settings

### 🔬 GNU Radio Companion

Use the **Soapy Custom Source** block for full control. 🎛️

1. ➕ Add a "Soapy Custom Source" block to your flowgraph
2. 🖱️ Double-click the block to open its properties
3. 📋 In the **General** tab:
    - **Device Arguments**: Set this to `driver=harogic`
    - **Driver**: Leave this field **blank**
4. 📻 In the **RF Options** tab:
    - **Gain (dB)**: This controls the main **REF** (Reference Level) gain
    - **Settings**: This field is used to control the other named gains and settings as a comma-separated list of key=value pairs:
        - 🔊 To enable the Preamp: `PREAMP=1`
        - 🤖 To enable IF AGC: `IF_AGC=1`
        - 🔄 To set LO Mode to "Spurs": `lo_mode=Spurs`
        - 🎯 **Combined Example:** `PREAMP=1, IF_AGC=1, lo_mode=Spurs`

---

🎉 **Happy SDR-ing!** If you encounter any issues, please check the troubleshooting section or open an issue on GitHub. 🐛➡️🔧
