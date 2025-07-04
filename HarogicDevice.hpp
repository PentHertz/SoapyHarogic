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

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Logger.h>
#include <SoapySDR/Formats.hpp>

#include <htra_api.h>

#include <stdexcept>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <cinttypes>

// Ring buffer for thread-safe sample transfer
template <typename T>
class RingBuffer
{
    public:
        RingBuffer(size_t capacity) : _capacity(capacity), _size(0), _readPos(0), _writePos(0) {
            _buffer.resize(capacity);
        }

        bool write(const T *data, size_t len) {
            if (len > _capacity - _size.load()) return false;
            for (size_t i = 0; i < len; ++i) {
                _buffer[_writePos] = data[i];
                _writePos = (_writePos + 1) % _capacity;
            }
            _size += len;
            return true;
        }

        size_t read(T *data, size_t len) {
            size_t toRead = std::min(len, _size.load());
            for (size_t i = 0; i < toRead; ++i) {
                data[i] = _buffer[_readPos];
                _readPos = (_readPos + 1) % _capacity;
            }
            _size -= toRead;
            return toRead;
        }

        void clear() {
            _size = 0;
            _readPos = 0;
            _writePos = 0;
        }

        size_t size() const {
            return _size.load();
        }

        size_t capacity() const {
            return _capacity;
        }

    private:
        std::vector<T> _buffer;
        size_t _capacity;
        std::atomic<size_t> _size;
        size_t _readPos;
        size_t _writePos;
};

// Main Device Class
class SoapyHarogic : public SoapySDR::Device
{
    public:
        SoapyHarogic(const SoapySDR::Kwargs &args);
        ~SoapyHarogic();

        /*******************************************************************
         * Identification API
         ******************************************************************/
        std::string getDriverKey() const override;
        std::string getHardwareKey() const override;
        SoapySDR::Kwargs getHardwareInfo() const override;

        /*******************************************************************
         * Channels API
         ******************************************************************/
        size_t getNumChannels(const int dir) const override;

        /*******************************************************************
         * Stream API
         ******************************************************************/
        std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const override;
        std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const override;
        SoapySDR::ArgInfoList getStreamArgsInfo(const int direction, const size_t channel) const override;
        SoapySDR::Stream *setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels = {}, const SoapySDR::Kwargs &args = {}) override;
        void closeStream(SoapySDR::Stream *stream) override;
        size_t getStreamMTU(SoapySDR::Stream *stream) const override;
        int activateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0, const size_t numElems = 0) override;
        int deactivateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0) override;
        int readStream(SoapySDR::Stream *stream, void *const *buffs, const size_t numElems, int &flags, long long &timeNs, const long timeoutUs = 100000) override;

        /*******************************************************************
         * Settings API
         ******************************************************************/
        SoapySDR::ArgInfoList getSettingInfo(void) const override;
        void writeSetting(const std::string &key, const std::string &value) override;
        std::string readSetting(const std::string &key) const override;

        /*******************************************************************
         * Antenna API
         ******************************************************************/
        std::vector<std::string> listAntennas(const int direction, const size_t channel) const override;
        void setAntenna(const int direction, const size_t channel, const std::string &name) override;
        std::string getAntenna(const int direction, const size_t channel) const override;
        
        /*******************************************************************
         * Gain API
         ******************************************************************/
        std::vector<std::string> listGains(const int direction, const size_t channel) const override;
        bool hasGainMode(const int direction, const size_t channel) const override;
        void setGainMode(const int direction, const size_t channel, const bool automatic) override;
        bool getGainMode(const int direction, const size_t channel) const override;
        void setGain(const int direction, const size_t channel, const double value) override;
        double getGain(const int direction, const size_t channel) const override;
        SoapySDR::Range getGainRange(const int direction, const size_t channel) const override;
        void setGain(const int direction, const size_t channel, const std::string &name, const double value) override;
        double getGain(const int direction, const size_t channel, const std::string &name) const override;
        SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const override;

        /*******************************************************************
         * Frequency API
         ******************************************************************/
        void setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args = {}) override;
        double getFrequency(const int direction, const size_t channel, const std::string &name) const override;
        std::vector<std::string> listFrequencies(const int direction, const size_t channel) const override;
        SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const override;
        
        /*******************************************************************
         * Sample Rate API
         ******************************************************************/
        void setSampleRate(const int direction, const size_t channel, const double rate) override;
        double getSampleRate(const int direction, const size_t channel) const override;
        std::vector<double> listSampleRates(const int direction, const size_t channel) const override;


    private:
        void _rx_thread();
        void _apply_settings();

        std::string _serial;
        int _dev_index;
        void* _dev_handle;
        DeviceInfo_TypeDef _dev_info;
        IQS_Profile_TypeDef _profile;
        bool _samps_int8;
        size_t _mtu;
        std::atomic<bool> _rx_thread_running;
        std::thread _rx_worker_thread;
        RingBuffer<std::complex<float>> _ring_buffer;
        std::mutex _device_mutex;
        std::mutex _buffer_mutex;
        std::condition_variable _buffer_cv;
        double _sample_rate;
        double _center_freq;
        int _ref_level;
        std::string _antenna;
        GainStrategy_TypeDef _gain_strategy;
        PreamplifierState_TypeDef _preamp_mode;
        bool _if_agc;
        LOOptimization_TypeDef _lo_mode;
        std::vector<double> _available_sample_rates;
        std::map<std::string, RxPort_TypeDef> _rx_ports;
};
