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
sudo apt-get install build-essential cmake libsoapysdr-dev soapysdr-tools
```

If you have another distribution, you can follow [the steps described here for Soapy installation](https://github.com/pothosware/PothosCore/wiki/BuildGuide).

🔧 **Important**: Calibration data must be placed in `/usr/bin/CalFile` directory in order to work.

## 🚀 Installation

### 1. 🔨 Install the SDK

To install the SDK, you can take the `Linux_Example/Install_HTRA_SDK` path of your USB stick provided by Harogic, or you can use the [unofficial GitHub repository](https://github.com/PentHertz/rfswift_harogic_install/releases) and download the `Install_HTRA_SDK.zip` you will unzip.

Then enterring in the `Install_HTRA_SDK` folder you can install everything as follows:

```bash
chmod +x install_htraapi_lib.sh
./install_htraapi_lib.sh
```

If you have installed Soapy library and the SDK, you are now ready to compile the SoapyHarogic module.

### 2. 🏗️ Build the Module

Clone the repository and build the module using CMake:

```bash
git clone https://github.com/PentHertz/SoapyHarogic.git
cd SoapyHarogic
mkdir build
cd build
cmake ..
make && make install
```

This will typically build and install `libsoapyharogic.so` to `/usr/local/lib/SoapySDR/modules0.8/`. 📂

### 3. 🎯 Calibration files

Calibration file `CalFile` directory must be place in `/usr/bin` or at least you need to make a symlink as `/usr/bin/CalFile` pointing to that directory:

```bash
$ ls /usr/bin/CalFile 
010_ff00aa00_ifacal.txt  022_ff00aa00_rfacal.txt  056_ff00aa00_rfacal.txt
010_ff00aa00_rfacal.txt  023_ff00aa00_ifacal.txt  057_ff00aa00_ifacal.txt
011_ff00aa00_ifacal.txt  023_ff00aa00_rfacal.txt  057_ff00aa00_iqcal.txt
011_ff00aa00_rfacal.txt  054_ff00aa00_ifacal.txt  057_ff00aa00_rfacal.txt
012_ff00aa00_ifacal.txt  054_ff00aa00_iqcal.txt   057_ff00aa00_txlcal.txt
012_ff00aa00_rfacal.txt  054_ff00aa00_rfacal.txt  091_ff00aa00_ifacal.txt
013_ff00aa00_ifacal.txt  054_ff00aa00_txlcal.txt  091_ff00aa00_rfacal.txt
013_ff00aa00_rfacal.txt  056_ff00aa00_ifacal.txt
022_ff00aa00_ifacal.txt  056_ff00aa00_iqcal.txt

```

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

The primary gain setting for this device is the **Reference Level (REF)**. This is a critical parameter that tells the device's automatic gain control (AGC) the expected power level of the signal you want to receive. Setting this correctly is essential to avoid errors and get good performance.

The Reference Level works somewhat counter-intuitively:

- **Set a HIGH value (e.g., 7 dBm, -20 dBm)** when you are receiving **STRONG** signals. This tells the AGC to apply less gain (more attenuation) to prevent the internal electronics from being overloaded.
    
- **Set a LOW value (e.g., -30 dBm, -50 dBm)** when you are receiving **WEAK** signals. This tells the AGC to apply more gain to make the signal visible.
    

**What is Error -12?**  
If the device's internal amplifiers or ADC are saturated by a signal that is too strong for the current gain setting, the hardware will report an **IF Overflow** (error code -12).

With the current driver, this is handled as a non-fatal warning. You will see a D (for Dropped/Distorted packet) in your console log, and the stream will continue, but the data will be clipped and unusable.

💡 **How to fix IF Overflow (D spam):** If you see D's in your log or your signal looks "flat-topped" in the waterfall, it means the gain is too high. **Increase the Reference Level** (e.g., from -70 to -50) until the overflow stops.

A good starting point for the Reference Level is typically between **-10 dBm and -30 dBm**. Adjust it based on your antenna and the strength of the signals in your environment.

### 🔢 Sample Format (8-bit vs. 16-bit)

The device automatically uses different ADC sample resolutions depending on the selected sample rate to optimize performance. This driver handles the conversion automatically. 🤖

- ⚡ For sample rates **greater than 64 MS/s**, the device will stream in **8-bit** format (CS8)
- 🎯 For sample rates **at or below 64 MS/s**, the device will stream in **16-bit** format (CS16)

The driver always converts the native samples to 32-bit complex floats (CF32) for the application, so this is handled transparently. ✨

## SoapyHarogic Driver Options

This document outlines all the configurable options for the SoapyHarogic driver, for use in applications like GQRX, SDR++, GNU Radio, or via the command line with tools like SoapySDRUtil.

### Device Arguments

These arguments are used to find and select a specific Harogic device. They are entered in the "Device Arguments" field in GRC, GQRX, or as part of the device string.

|Argument Key|Example Value|Description|
|---|---|---|
|driver|harogic|**(Required)** This tells SoapySDR to load the Harogic driver.|
|serial|313251180036001A|**(Optional)** The unique serial number of the device. Use this if you have multiple Harogic devices connected to your system and need to select a specific one. If omitted, the first device found will be used.|
|label|Harogic 3132...|**(Read-Only)** A human-readable name for the device, automatically generated by the driver. You can use this to identify the device but cannot set it as an argument.|

**Example Device String:** driver=harogic,serial=313251180036001A

---

### Stream Arguments

These arguments configure the data stream itself and are typically set once before the stream is activated. They should be placed in the **"Stream Arguments"** field in GRC or SDR++.

|Argument Key|Example Value|Default|Description|
|---|---|---|---|
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

|Gain Element|Type|Range|Description|
|---|---|---|---|
|REF|Integer|-100 to 7 [dBm]|**Reference Level.** This is the primary gain control. It sets the target power level for the top of the ADC's range. **Use a high value for strong signals to prevent overflow.**|
|PREAMP|Boolean|On/Off|Toggles the front-end low-noise preamplifier. On (1.0) enables it for better sensitivity on weak signals. Off (0.0) disables it.|
|IF_AGC|Boolean|On/Off|Toggles the Intermediate Frequency (IF) Automatic Gain Control.|

#### Other RF Settings

|Setting Key|Type|Options / Range|Description|
|---|---|---|---|
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

## ⚡ Performance Tuning and Testing

High-speed SDR streaming (over 60 MS/s) is demanding on the host computer's USB subsystem and CPU. If you experience stream interruptions (often shown as T for timeout or O for overflow in the console), you may need to tune your system for performance.

### Performance Tuning Script

This repository includes a Bash script, performance_mode.sh, to help apply common system optimizations.

**Usage:**

```bash
# Run the script with sudo
sudo ./performance_mode.sh
```

The script provides an interactive menu to:

1. **Set CPU Governor to 'performance'**: Prevents CPU from down-clocking, reducing latency.
2. **Disable USB Autosuspend**: Stops the kernel from trying to power-save the active SDR.
3. **Increase USB-FS Memory Buffer**: Allocates more RAM for USB transfers, critical for high data rates.
4. **Set USB 3.0 IRQ Affinity**: Pins the USB controller's interrupts to a specific CPU core to reduce jitter.
5. **Disable DRM KMS Polling**: Reduces display driver-related system load.
    
You can apply all tweaks at once or individually. A "Revert to Defaults" option is also available. It is recommended to apply these tweaks before running high-bandwidth applications.

🎉 **Happy SDR-ing!** If you encounter any issues, please check the troubleshooting section or open an issue on GitHub. 🐛➡️🔧
