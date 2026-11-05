// Isochronous packet scheduling uses endpoint service intervals, not a fixed
// packet rate. USB 2.0 endpoint bInterval is an exponent for ISO endpoints at
// both full speed (1 ms ticks) and high speed (125 us ticks). SuperSpeed uses
// the same 125 us ticks; its companion descriptor supplies interval capacity.
#pragma once

#include <cstdint>
#include <limits>
#include <optional>

#include "libusb/libusb.h"

/** Keep fractional audio frames exact at the selected endpoint's USB cadence. */
struct UsbIsoCadence {
    uint32_t base_ticks_per_second;
    uint32_t interval_ticks;

    /** Reject unknown/low speed and invalid ISO bInterval instead of guessing. */
    static std::optional<UsbIsoCadence> from_endpoint(int speed, uint8_t interval) noexcept {
        if (interval < 1U || interval > 16U) return std::nullopt;
        uint32_t base;
        switch (speed) {
            case LIBUSB_SPEED_FULL: base = 1000U; break;
            case LIBUSB_SPEED_HIGH:
            case LIBUSB_SPEED_SUPER:
            case LIBUSB_SPEED_SUPER_PLUS:
            case LIBUSB_SPEED_SUPER_PLUS_X2: base = 8000U; break;
            default: return std::nullopt;
        }
        return UsbIsoCadence{base, uint32_t{1} << (interval - 1U)};
    }

    /** Maximum complete audio-frame payload in one service interval; overflow fails. */
    std::optional<uint32_t> max_packet_bytes(uint32_t rate, uint32_t frame_bytes) const noexcept {
        if (!rate || !frame_bytes) return std::nullopt;
        const uint64_t numerator = uint64_t{rate} * interval_ticks;
        const uint64_t frames = (numerator + base_ticks_per_second - 1U) / base_ticks_per_second;
        if (frames > std::numeric_limits<uint32_t>::max() / frame_bytes) return std::nullopt;
        return static_cast<uint32_t>(frames * frame_bytes);
    }

    /** Advance a caller-owned remainder. Caller validates max_packet_bytes first. */
    uint64_t next_frames(uint32_t rate, uint64_t& remainder) const noexcept {
        remainder += uint64_t{rate} * interval_ticks;
        const uint64_t frames = remainder / base_ticks_per_second;
        remainder %= base_ticks_per_second;
        return frames;
    }
};

/** Decode bytes per ISO service interval, including HS transactions/SS bursts.
 * Zero means invalid/unsupported. Extended SSP isochronous companions (bit 7)
 * are rejected: this path supports the ordinary SS wBytesPerInterval contract.
 */
inline uint32_t usb_iso_service_capacity(int speed, uint16_t raw,
                                       bool companion = false, uint8_t burst = 0,
                                       uint8_t attributes = 0, uint16_t interval_bytes = 0) noexcept {
    const uint32_t packet = raw & 0x07ffU;
    if (!packet) return 0;
    if (speed == LIBUSB_SPEED_FULL) return raw == packet && packet <= 1023U ? packet : 0;
    if (speed == LIBUSB_SPEED_HIGH) {
        const uint32_t additional = (raw >> 11U) & 3U;
        return (raw & 0xe000U) == 0 && packet <= 1024U && additional <= 2U
                ? packet * (additional + 1U) : 0;
    }
    if (speed == LIBUSB_SPEED_SUPER || speed == LIBUSB_SPEED_SUPER_PLUS || speed == LIBUSB_SPEED_SUPER_PLUS_X2) {
        if (!companion || raw != packet || packet > 1024U || burst > 15U
                || (attributes & ~3U) != 0 || (attributes & 3U) == 3U || !interval_bytes) return 0;
        const uint32_t ceiling = packet * (uint32_t{burst} + 1U) * ((attributes & 3U) + 1U);
        return interval_bytes <= ceiling ? interval_bytes : 0;
    }
    return 0;
}

/** Compare endpoint throughput exactly, including intervals longer than a second. */
inline bool usb_iso_more_bandwidth(int a_speed, uint8_t a_interval, uint32_t a_capacity,
                                  int b_speed, uint8_t b_interval, uint32_t b_capacity) noexcept {
    const auto a = UsbIsoCadence::from_endpoint(a_speed, a_interval);
    const auto b = UsbIsoCadence::from_endpoint(b_speed, b_interval);
    if (!a) return false;
    if (!b) return true;
    return uint64_t{a_capacity} * a->base_ticks_per_second * b->interval_ticks
         > uint64_t{b_capacity} * b->base_ticks_per_second * a->interval_ticks;
}
