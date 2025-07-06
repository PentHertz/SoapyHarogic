/* SoapySDR module for Harogic Devices
 *
 * Copyright (C) 2025 Sébastien Dudek / penthertz.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * Or see <https://www.gnu.org/licenses/>.
 */

#include "HarogicDevice.hpp"

SoapyHarogic::SoapyHarogic(const SoapySDR::Kwargs &args) : 
    _dev_index(-1),
    _dev_handle(nullptr),
    _samps_int8(false),
    _mtu(0),
    _rx_thread_running(false),
    _ring_buffer(RING_BUFFER_SIZE),
    _sample_rate(0.0),
    _center_freq(100e6),
    _ref_level(-10),
    _gain_strategy(LowNoisePreferred),
    _preamp_mode(AutoOn),
    _if_agc(false),
    _lo_mode(LOOpt_Auto),
    _force_8bit(false),
    _overflow_flag(false)
{
    if (args.count("serial")) _serial = args.at("serial");

    BootProfile_TypeDef profile = {};
    profile.PhysicalInterface = PhysicalInterface_TypeDef::USB;
    profile.DevicePowerSupply = DevicePowerSupply_TypeDef::USBPortOnly;

    void* dev_tmp;
    BootInfo_TypeDef binfo;
    for (int i = 0; i < 128; i++) {
        if (Device_Open(&dev_tmp, i, &profile, &binfo) < 0) break;
        char serial_str[64];
        snprintf(serial_str, sizeof(serial_str), "%" PRIX64, binfo.DeviceInfo.DeviceUID);
        if (_serial.empty()) _serial = serial_str;
        if (_serial == serial_str) {
            _dev_index = i;
            _dev_info = binfo.DeviceInfo;
            Device_Close(&dev_tmp);
            break;
        }
        Device_Close(&dev_tmp);
    }

    if (_dev_index == -1) throw std::runtime_error("Harogic device not found for serial: " + _serial);
    SoapySDR_logf(SOAPY_SDR_INFO, "Found Harogic device: %s (Index %d)", _serial.c_str(), _dev_index);
    SoapySDR_logf(SOAPY_SDR_INFO, "  - Device Model: %u", _dev_info.Model);
    SoapySDR_logf(SOAPY_SDR_INFO, "  - Hardware Version: %u", _dev_info.HardwareVersion);


    if (Device_Open(&dev_tmp, _dev_index, &profile, &binfo) < 0) {
        throw std::runtime_error("Failed to open device to query capabilities.");
    }

    IQS_Profile_TypeDef default_profile;
    IQS_ProfileDeInit(&dev_tmp, &default_profile);

    for (int i = 0; i < 8; i++) {
        _available_sample_rates.push_back(default_profile.NativeIQSampleRate_SPS / (double)(1 << i));
    }
    _sample_rate = _available_sample_rates[0];

    _rx_ports["External"] = ExternalPort;
    _rx_ports["Internal"] = InternalPort;
    _rx_ports["ANT"] = ANT_Port;
    _rx_ports["T/R"] = TR_Port;
    _rx_ports["SWR"] = SWR_Port;
    _rx_ports["INT"] = INT_Port;
    _antenna = "External";

    Device_Close(&dev_tmp);
}

SoapyHarogic::~SoapyHarogic() {
    deactivateStream(nullptr, 0, 0);
}

std::string SoapyHarogic::getDriverKey() const { return "Harogic"; }
std::string SoapyHarogic::getHardwareKey() const { return "HTRA"; }
SoapySDR::Kwargs SoapyHarogic::getHardwareInfo() const {
    SoapySDR::Kwargs info;
    info["serial"] = _serial;
    info["model"] = std::to_string(_dev_info.Model);
    info["hardware_version"] = std::to_string(_dev_info.HardwareVersion);
    info["mcu_firmware_version"] = std::to_string(_dev_info.MFWVersion);
    info["fpga_firmware_version"] = std::to_string(_dev_info.FFWVersion);
    return info;
}
size_t SoapyHarogic::getNumChannels(const int dir) const { return (dir == SOAPY_SDR_RX) ? 1 : 0; }
std::vector<std::string> SoapyHarogic::getStreamFormats(const int, const size_t) const { return {SOAPY_SDR_CF32}; }
std::string SoapyHarogic::getNativeStreamFormat(const int, const size_t, double &fullScale) const {
    bool use_cs8 = (_sample_rate > RESOLTRIG) || _force_8bit;
    
    if (use_cs8) {
        fullScale = 128.0;
        return SOAPY_SDR_CS8;
    } else {
        fullScale = 32768.0;
        return SOAPY_SDR_CS16;
    }
}

SoapySDR::ArgInfoList SoapyHarogic::getStreamArgsInfo(const int, const size_t) const {
    SoapySDR::ArgInfoList infos;

    SoapySDR::ArgInfo force_8bit_arg;
    force_8bit_arg.key = "force_8bit";
    force_8bit_arg.name = "Force 8-Bit";
    force_8bit_arg.description = "Force 8-bit sample format regardless of the sample rate.";
    force_8bit_arg.type = SoapySDR::ArgInfo::BOOL;
    force_8bit_arg.value = "false"; // Default value

    infos.push_back(force_8bit_arg);

    return infos;
}

SoapySDR::Stream *SoapyHarogic::setupStream(const int direction, const std::string &format, const std::vector<size_t> &, const SoapySDR::Kwargs &args) {
    if (direction != SOAPY_SDR_RX) throw std::runtime_error("Harogic driver only supports RX");
    if (format != SOAPY_SDR_CF32) throw std::runtime_error("Please request CF32 format.");

    if (args.count("force_8bit")) {
        _force_8bit = (args.at("force_8bit") == "true");
        if (_force_8bit) {
            SoapySDR_log(SOAPY_SDR_INFO, "User has forced 8-bit sample mode.");
        }
    }

    // Update the logic: use 8-bit if the rate is high OR if forced by the user
    _samps_int8 = (_sample_rate > RESOLTRIG) || _force_8bit;

    return (SoapySDR::Stream *)this;
}

void SoapyHarogic::closeStream(SoapySDR::Stream *stream) {
    this->deactivateStream(stream, 0, 0); 
    _ring_buffer.clear();
    _force_8bit = false; // <<< ADD THIS LINE to reset the flag
}

size_t SoapyHarogic::getStreamMTU(SoapySDR::Stream *) const { return _mtu; }

int SoapyHarogic::activateStream(SoapySDR::Stream *, const int, const long long, const size_t) {
    std::lock_guard<std::mutex> lock(_device_mutex);
    if (_rx_thread_running) return 0;
    _samps_int8 = (_sample_rate > RESOLTRIG) || _force_8bit;
    BootProfile_TypeDef bprofile = {};
    bprofile.PhysicalInterface = PhysicalInterface_TypeDef::USB;
    bprofile.DevicePowerSupply = DevicePowerSupply_TypeDef::USBPortAndPowerPort;
    BootInfo_TypeDef binfo;
    int ret = Device_Open(&_dev_handle, _dev_index, &bprofile, &binfo);
    if (ret < 0) { 
        SoapySDR_logf(SOAPY_SDR_ERROR, "activateStream: Device_Open failed: %d", ret); return SOAPY_SDR_STREAM_ERROR;
        SoapySDR_log(SOAPY_SDR_WARNING, "At high sample rates, ensure the device is on a dedicated USB 3.0+ port with sufficient power.");
    }
    IQS_ProfileDeInit(&_dev_handle, &_profile);
    _profile.Atten = -1;
    _profile.BusTimeout_ms = 1000;
    _profile.TriggerSource = Bus;
    _profile.TriggerMode = Adaptive;
    _profile.DataFormat = _samps_int8 ? Complex8bit : Complex16bit;
    _profile.CenterFreq_Hz = _center_freq;
    _profile.RefLevel_dBm = _ref_level;
    _profile.DecimateFactor = (uint32_t)(_available_sample_rates[0] / _sample_rate);
    _profile.RxPort = _rx_ports.at(_antenna);
    _profile.GainStrategy = _gain_strategy;
    _profile.Preamplifier = _preamp_mode;
    _profile.EnableIFAGC = _if_agc;
    _profile.LOOptimization = _lo_mode;

    SoapySDR_log(SOAPY_SDR_INFO, "[ SoapyHarogic by FlUxIuS @ Penthertz.com ]");
    SoapySDR_log(SOAPY_SDR_INFO, "--- Harogic Activating Stream with Settings ---");
    SoapySDR_logf(SOAPY_SDR_INFO, "  - Center Frequency: %.3f MHz", _profile.CenterFreq_Hz / 1e6);
    SoapySDR_logf(SOAPY_SDR_INFO, "  - Sample Rate:      %.3f MS/s", _sample_rate / 1e6);
    SoapySDR_logf(SOAPY_SDR_INFO, "  - Sample Format:    %s", _samps_int8 ? "CS8" : "CS16");
    SoapySDR_logf(SOAPY_SDR_INFO, "  - Reference Level:  %d dBm", (int)_profile.RefLevel_dBm);
    
    std::string antennaName = "Unknown";
    for (const auto& pair : _rx_ports) { if (pair.second == _profile.RxPort) { antennaName = pair.first; break; } }
    SoapySDR_logf(SOAPY_SDR_INFO, "  - Antenna:          %s", antennaName.c_str());
    
    SoapySDR_logf(SOAPY_SDR_INFO, "  - Preamp:           %s", (_profile.Preamplifier == AutoOn) ? "Auto" : "Off");
    SoapySDR_logf(SOAPY_SDR_INFO, "  - IF AGC:           %s", _profile.EnableIFAGC ? "On" : "Off");

    std::string gainStratStr = (_profile.GainStrategy == LowNoisePreferred) ? "Low Noise" : "High Linearity";
    SoapySDR_logf(SOAPY_SDR_INFO, "  - Gain Strategy:    %s", gainStratStr.c_str());
    SoapySDR_log(SOAPY_SDR_INFO, "---------------------------------------------");

    IQS_StreamInfo_TypeDef info;
    ret = IQS_Configuration(&_dev_handle, &_profile, &_profile, &info);
    if (ret < 0) { Device_Close(&_dev_handle); _dev_handle = nullptr; SoapySDR_logf(SOAPY_SDR_ERROR, "activateStream: IQS_Configuration failed: %d", ret); return SOAPY_SDR_STREAM_ERROR; }
    _mtu = info.PacketSamples;
    if (_mtu == 0) { SoapySDR_log(SOAPY_SDR_ERROR, "activateStream: Device returned an MTU of 0 samples."); Device_Close(&_dev_handle); _dev_handle = nullptr; return SOAPY_SDR_STREAM_ERROR; }
    ret = IQS_BusTriggerStart(&_dev_handle);
    if (ret < 0) { Device_Close(&_dev_handle); _dev_handle = nullptr; SoapySDR_logf(SOAPY_SDR_ERROR, "activateStream: IQS_BusTriggerStart failed: %d", ret); return SOAPY_SDR_STREAM_ERROR; }
    _rx_thread_running = true;
    _rx_worker_thread = std::thread(&SoapyHarogic::_rx_thread, this);
    SoapySDR_logf(SOAPY_SDR_INFO, "Stream activated with MTU %zu", _mtu);
    return 0;
}

int SoapyHarogic::deactivateStream(SoapySDR::Stream *, const int, const long long) {
    if (!_rx_thread_running && !_rx_worker_thread.joinable()) return 0;
    _rx_thread_running = false;
    _buffer_cv.notify_one();
    if (_rx_worker_thread.joinable()) { 
        _rx_worker_thread.join(); 
    }
    std::lock_guard<std::mutex> lock(_device_mutex);
    if (_dev_handle) { 
        IQS_BusTriggerStop(&_dev_handle); 
        Device_Close(&_dev_handle); 
        _dev_handle = nullptr; 
    }
    SoapySDR_log(SOAPY_SDR_INFO, "Stream deactivated");
    return 0;
}

int SoapyHarogic::readStream(SoapySDR::Stream *, void *const *buffs, const size_t numElems, int &flags, long long &, const long timeoutUs) {
    std::unique_lock<std::mutex> lock(_buffer_mutex);
    if (!_buffer_cv.wait_for(lock, std::chrono::microseconds(timeoutUs), [&]{ return _ring_buffer.size() >= numElems || !_rx_thread_running; })) { 
        return SOAPY_SDR_TIMEOUT; 
    }
    if (!_rx_thread_running && _ring_buffer.size() < numElems) {
        return SOAPY_SDR_STREAM_ERROR;
    }
    
    // Default flags to 0
    flags = 0;
    
    // Check for an overflow condition reported by the RX thread
    if (_overflow_flag.exchange(false)) {
        // Use END_BURST to signal a discontinuity like an overflow
        flags |= SOAPY_SDR_END_BURST; // <<< FIX IS HERE
        SoapySDR_log(SOAPY_SDR_SSI, "D"); // Log 'D' for Dropped/Distorted packet
    }

    size_t num_read = _ring_buffer.read((std::complex<float>*)buffs[0], numElems);
    return num_read;
}

void SoapyHarogic::_rx_thread() {
    SoapySDR_log(SOAPY_SDR_INFO, "RX worker thread started.");
    std::vector<std::complex<float>> temp_buf;
    if (_mtu > 0) temp_buf.resize(_mtu);
    IQStream_TypeDef iqs;
    
    while (_rx_thread_running) {
        int ret = IQS_GetIQStream_PM1(&_dev_handle, &iqs);
        
        if (ret < 0) {
            if (ret == APIRETVAL_WARNING_BusTimeOut) {
                SoapySDR_log(SOAPY_SDR_SSI, "T"); // Timeout
                continue;
            } 
            else if (ret == APIRETVAL_WARNING_IFOverflow) {
                _overflow_flag = true; // Set the flag for readStream
                // We don't need to log here, readStream will log 'D'
                // This is a WARNING, so we CONTINUE streaming.
                continue;
            }
            else { 
                SoapySDR_logf(SOAPY_SDR_ERROR, "Fatal streaming error: %d. Worker thread stopping.", ret);
                _rx_thread_running = false; 
                break; 
            }
        }
        
        uint32_t packetSamples = iqs.IQS_StreamInfo.PacketSamples;
        if (packetSamples == 0 || iqs.AlternIQStream == nullptr) continue;
        if (temp_buf.size() != packetSamples) temp_buf.resize(packetSamples);
        
        if (_samps_int8) {
            int8_t* in = (int8_t*)iqs.AlternIQStream;
            for(size_t i = 0; i < packetSamples; ++i) temp_buf[i] = std::complex<float>(in[2*i] / 127.0f, in[2*i+1] / 127.0f);
        } else {
            int16_t* in = (int16_t*)iqs.AlternIQStream;
            for(size_t i = 0; i < packetSamples; ++i) temp_buf[i] = std::complex<float>(in[2*i] / 32767.0f, in[2*i+1] / 32767.0f);
        }
        
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        if (!_ring_buffer.write(temp_buf.data(), packetSamples)) {
            SoapySDR_log(SOAPY_SDR_SSI, "O"); // Ring buffer overflow
        }
        lock.unlock();
        _buffer_cv.notify_one();
    }
    
    _buffer_cv.notify_all();
    SoapySDR_log(SOAPY_SDR_INFO, "RX worker thread finished.");
}

SoapySDR::ArgInfoList SoapyHarogic::getSettingInfo(void) const {
    SoapySDR::ArgInfoList infos;
    SoapySDR::ArgInfo gain_strat_arg;
    gain_strat_arg.key = "gain_strategy";
    gain_strat_arg.name = "Gain Strategy";
    gain_strat_arg.type = SoapySDR::ArgInfo::STRING;
    gain_strat_arg.options = {"Low Noise", "High Linearity"};
    gain_strat_arg.value = (_gain_strategy == LowNoisePreferred) ? "Low Noise" : "High Linearity";
    infos.push_back(gain_strat_arg);
    SoapySDR::ArgInfo lo_mode_arg;
    lo_mode_arg.key = "lo_mode";
    lo_mode_arg.name = "LO Mode";
    lo_mode_arg.type = SoapySDR::ArgInfo::STRING;
    lo_mode_arg.options = {"Auto", "Speed", "Spurs", "Phase Noise"};
    lo_mode_arg.value = "Auto";
    infos.push_back(lo_mode_arg);
    return infos;
}
void SoapyHarogic::writeSetting(const std::string &key, const std::string &value) {
    if (key == "gain_strategy") _gain_strategy = (value == "Low Noise") ? LowNoisePreferred : HighLinearityPreferred;
    else if (key == "lo_mode") {
        if (value == "Speed") _lo_mode = LOOpt_Speed;
        else if (value == "Spurs") _lo_mode = LOOpt_Spur;
        else if (value == "Phase Noise") _lo_mode = LOOpt_PhaseNoise;
        else _lo_mode = LOOpt_Auto;
    }
    if (_rx_thread_running) _apply_settings();
}
std::string SoapyHarogic::readSetting(const std::string &key) const {
    if (key == "gain_strategy") return (_gain_strategy == LowNoisePreferred) ? "Low Noise" : "High Linearity";
    if (key == "lo_mode") {
        if (_lo_mode == LOOpt_Speed) return "Speed";
        if (_lo_mode == LOOpt_Spur) return "Spurs";
        if (_lo_mode == LOOpt_PhaseNoise) return "Phase Noise";
        return "Auto";
    }
    return "";
}

std::vector<std::string> SoapyHarogic::listAntennas(const int, const size_t) const {
    std::vector<std::string> antennas;
    for(const auto& pair : _rx_ports) antennas.push_back(pair.first);
    return antennas;
}
void SoapyHarogic::setAntenna(const int, const size_t, const std::string &name) {
    if (_rx_ports.find(name) == _rx_ports.end()) throw std::runtime_error("Invalid antenna name: " + name);
    _antenna = name;
    if (_rx_thread_running) _apply_settings();
}
std::string SoapyHarogic::getAntenna(const int, const size_t) const { return _antenna; }

std::vector<std::string> SoapyHarogic::listGains(const int, const size_t) const { return {"REF", "PREAMP", "IF_AGC"}; }
bool SoapyHarogic::hasGainMode(const int, const size_t) const { return false; }
void SoapyHarogic::setGainMode(const int, const size_t, const bool) {}
bool SoapyHarogic::getGainMode(const int, const size_t) const { return false; }
void SoapyHarogic::setGain(const int dir, const size_t chan, const double value) { this->setGain(dir, chan, "REF", value); }
double SoapyHarogic::getGain(const int dir, const size_t chan) const { return this->getGain(dir, chan, "REF"); }
SoapySDR::Range SoapyHarogic::getGainRange(const int dir, const size_t chan) const { return this->getGainRange(dir, chan, "REF"); }

void SoapyHarogic::setGain(const int, const size_t, const std::string &name, const double value) {
    if (name == "REF") { _ref_level = static_cast<int>(value); }
    else if (name == "PREAMP") { _preamp_mode = (value > 0.5) ? AutoOn : ForcedOff; }
    else if (name == "IF_AGC") { _if_agc = (value > 0.5); }
    if (_rx_thread_running) _apply_settings();
}
double SoapyHarogic::getGain(const int, const size_t, const std::string &name) const {
    if (name == "REF") return _ref_level;
    if (name == "PREAMP") return (_preamp_mode == AutoOn) ? 1.0 : 0.0;
    if (name == "IF_AGC") return _if_agc ? 1.0 : 0.0;
    return 0;
}
SoapySDR::Range SoapyHarogic::getGainRange(const int, const size_t, const std::string &name) const {
    if (name == "REF") return SoapySDR::Range(-100.0, 7.0);
    if (name == "PREAMP" || name == "IF_AGC") return SoapySDR::Range(0.0, 1.0);
    return SoapySDR::Range(0.0, 0.0);
}

void SoapyHarogic::setFrequency(const int, const size_t, const std::string &, const double frequency, const SoapySDR::Kwargs &) { _center_freq = frequency; if (_rx_thread_running) _apply_settings(); }
double SoapyHarogic::getFrequency(const int, const size_t, const std::string &) const { return _center_freq; }
std::vector<std::string> SoapyHarogic::listFrequencies(const int, const size_t) const { return {"RF"}; }
SoapySDR::RangeList SoapyHarogic::getFrequencyRange(const int, const size_t, const std::string &) const { return {SoapySDR::Range(MIN_FREQ, MAX_FREQ)}; }
void SoapyHarogic::setSampleRate(const int, const size_t, const double rate) { _sample_rate = rate; if (_rx_thread_running) _apply_settings(); }
double SoapyHarogic::getSampleRate(const int, const size_t) const { return _sample_rate; }
std::vector<double> SoapyHarogic::listSampleRates(const int, const size_t) const { return _available_sample_rates; }

void SoapyHarogic::_apply_settings() {
    std::lock_guard<std::mutex> lock(_device_mutex);
    if (!_dev_handle) return;
    
    // Log settings BEFORE applying them
    SoapySDR_log(SOAPY_SDR_INFO, "--- Applying Settings Update ---");
    SoapySDR_logf(SOAPY_SDR_INFO, "  - New Reference Level: %d dBm", _ref_level);
    SoapySDR_logf(SOAPY_SDR_INFO, "  - New Preamp State:    %s", (_preamp_mode == AutoOn) ? "Auto" : "Off");
    SoapySDR_log(SOAPY_SDR_INFO, "------------------------------");

    _samps_int8 = (_sample_rate > RESOLTRIG) || _force_8bit;
    _profile.DataFormat = _samps_int8 ? Complex8bit : Complex16bit;
    _profile.CenterFreq_Hz = _center_freq;
    _profile.RefLevel_dBm = _ref_level;
    _profile.DecimateFactor = (uint32_t)(_available_sample_rates[0] / _sample_rate);
    _profile.RxPort = _rx_ports.at(_antenna);
    _profile.GainStrategy = _gain_strategy;
    _profile.Preamplifier = _preamp_mode;
    _profile.EnableIFAGC = _if_agc;
    _profile.LOOptimization = _lo_mode;
    IQS_StreamInfo_TypeDef info;
    int ret = IQS_Configuration(&_dev_handle, &_profile, &_profile, &info);
    if (ret < 0) { SoapySDR_logf(SOAPY_SDR_ERROR, "Failed to apply settings: %d", ret); return; }
    _mtu = info.PacketSamples;
    ret = IQS_BusTriggerStart(&_dev_handle);
    if (ret < 0) SoapySDR_logf(SOAPY_SDR_ERROR, "Could not re-start stream after settings change: %d", ret);
}

static SoapySDR::KwargsList findHarogic(const SoapySDR::Kwargs &) {
    SoapySDR::KwargsList results;
    BootProfile_TypeDef profile = {};
    profile.PhysicalInterface = PhysicalInterface_TypeDef::USB;
    profile.DevicePowerSupply = DevicePowerSupply_TypeDef::USBPortOnly;
    void* dev;
    BootInfo_TypeDef binfo;
    for (int i = 0; i < 128; i++) {
        if (Device_Open(&dev, i, &profile, &binfo) < 0) break;
        char serial[64];
        snprintf(serial, sizeof(serial), "%" PRIX64, binfo.DeviceInfo.DeviceUID);
        SoapySDR::Kwargs dev_info;
        dev_info["serial"] = serial;
        dev_info["label"] = "Harogic " + std::string(serial);
        dev_info["driver"] = "Harogic";
        results.push_back(dev_info);
        Device_Close(&dev);
    }
    return results;
}

static SoapySDR::Device *makeHarogic(const SoapySDR::Kwargs &args) {
    return new SoapyHarogic(args);
}

static SoapySDR::Registry registerHarogic("harogic", &findHarogic, &makeHarogic, SOAPY_SDR_ABI_VERSION);
