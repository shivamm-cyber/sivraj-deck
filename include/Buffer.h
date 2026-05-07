/*=============================================================
 * Buffer.h — Packet capture ring buffer
 *=============================================================*/
#ifndef BUFFER_H
#define BUFFER_H

#include <Arduino.h>
#include "configs.h"

struct PacketInfo {
    uint8_t  data[512];
    uint16_t len;
    int8_t   rssi;
    uint8_t  channel;
    uint32_t timestamp;
};

class PacketBuffer {
public:
    PacketBuffer(uint16_t capacity = 64)
        : _capacity(capacity), _head(0), _tail(0), _count(0) {
        _buffer = new PacketInfo[_capacity];
    }

    ~PacketBuffer() {
        delete[] _buffer;
    }

    bool push(const uint8_t* data, uint16_t len, int8_t rssi, uint8_t ch) {
        if (len > 512) len = 512;
        portENTER_CRITICAL(&_mux);
        uint16_t idx = _head;
        _head = (_head + 1) % _capacity;
        if (_count < _capacity) {
            _count++;
        } else {
            _tail = (_tail + 1) % _capacity;
        }
        portEXIT_CRITICAL(&_mux);

        memcpy(_buffer[idx].data, data, len);
        _buffer[idx].len = len;
        _buffer[idx].rssi = rssi;
        _buffer[idx].channel = ch;
        _buffer[idx].timestamp = millis();
        return true;
    }

    bool pop(PacketInfo& pkt) {
        portENTER_CRITICAL(&_mux);
        if (_count == 0) {
            portEXIT_CRITICAL(&_mux);
            return false;
        }
        uint16_t idx = _tail;
        _tail = (_tail + 1) % _capacity;
        _count--;
        portEXIT_CRITICAL(&_mux);

        memcpy(&pkt, &_buffer[idx], sizeof(PacketInfo));
        return true;
    }

    bool peek(PacketInfo& pkt) {
        portENTER_CRITICAL(&_mux);
        if (_count == 0) {
            portEXIT_CRITICAL(&_mux);
            return false;
        }
        memcpy(&pkt, &_buffer[_tail], sizeof(PacketInfo));
        portEXIT_CRITICAL(&_mux);
        return true;
    }

    uint16_t count() {
        portENTER_CRITICAL(&_mux);
        uint16_t c = _count;
        portEXIT_CRITICAL(&_mux);
        return c;
    }

    void clear() {
        portENTER_CRITICAL(&_mux);
        _head = _tail = _count = 0;
        portEXIT_CRITICAL(&_mux);
    }

    bool isFull() {
        return _count >= _capacity;
    }

    uint16_t capacity() { return _capacity; }

private:
    PacketInfo*     _buffer;
    uint16_t        _capacity;
    volatile uint16_t _head;
    volatile uint16_t _tail;
    volatile uint16_t _count;
    portMUX_TYPE    _mux = portMUX_INITIALIZER_UNLOCKED;
};

#endif // BUFFER_H
