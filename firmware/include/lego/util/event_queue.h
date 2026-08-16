#pragma once

#include <Arduino.h>
#include <lego/packet.h>
#include "log/logger.h"


/**
 * @brief A fixed-capacity queue for outgoing HID event packets.
 *        Items are held for at least DISPATCH_INTERVAL_MS between sending them off.
 *
 *        Call push() to enqueue a packet.
 *        Call update() from the main loop.
 */
class EventQueue {
   public:
    static constexpr uint16_t DISPATCH_INTERVAL_MS = 500;
    static constexpr uint8_t MAX_EVENTS = 16;

    EventQueue() : _head(0), _tail(0), _count(0), _lastDispatchMs(0) {}

    /**
     * @brief Enqueues a copy of the given EventPacket.
     *        If the queue is full the packet is dropped and false is returned.
     *
     * @param packet  The packet to enqueue (copied by value)
     * @return true   Packet accepted
     * @return false  Queue full, packet dropped
     */
    bool push(const LEGODimensions::EventPacket& packet) {
        if (_count >= MAX_EVENTS) {
            log_warn("[EventQueue] Queue full - dropping event packet");
            return false;
        }

        _slots[_tail] = packet;
        _tail = (_tail + 1) % MAX_EVENTS;
        _count++;

        log_dbg("[EventQueue] Enqueued packet (%d/%d)", _count, MAX_EVENTS);
        return true;
    }

    /**
     * @brief Returns true if there are packets waiting to be dispatched.
     */
    bool pending() const { return _count > 0; }

    /**
     * @brief Must be called from the main loop.
     *        Dispatches the front packet if DISPATCH_INTERVAL_MS has elapsed
     *        since the last dispatch, then calls the provided send function.
     *
     * @param sendFn  Callable that accepts a reference to a BasePacket and
     *                forwards it to the HID transport
     */
    template <typename SendFn>
    void update(SendFn sendFn) {
        if (_count == 0) {
            return;
        }

        const uint32_t now = millis();
        if (now - _lastDispatchMs < DISPATCH_INTERVAL_MS) {
            return;
        }

        LEGODimensions::EventPacket& front = _slots[_head];
        log_dbg("[EventQueue] Dispatching packet (%d remaining after this)", _count - 1);

        sendFn(front);

        _head = (_head + 1) % MAX_EVENTS;
        _count--;
        _lastDispatchMs = now;
    }

    /**
     * @brief Discards all pending packets (e.g. on device reset).
     */
    void clear() {
        _head = 0;
        _tail = 0;
        _count = 0;
        log_dbg("[EventQueue] Cleared");
    }

    uint8_t count() const { return _count; }

      private:
    LEGODimensions::EventPacket _slots[MAX_EVENTS];
    uint8_t _head;
    uint8_t _tail;
    uint8_t _count;
    uint32_t _lastDispatchMs;
};