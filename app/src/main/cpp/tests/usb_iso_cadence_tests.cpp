#include "usb_iso_cadence.h"
#include "usb_iso_transfer_pool.h"
#include "uac2_descriptor_parser.h"
#include "uac2_dsd_detector.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// The standalone host target supplies no-device libusb/log stubs.
extern void test_set_endpoint(int speed, uint16_t packet, uint8_t interval,
                              bool companion, uint8_t burst, uint8_t mult, uint16_t bytes);
extern unsigned test_usb_submissions;
static unsigned checks = 0;
static void check(bool condition, const char* label) {
    ++checks;
    if (!condition) throw std::runtime_error(label);
}
static Uac2StreamingEndpointInfo endpoint(uint8_t alt, uint8_t interval, uint16_t capacity) {
    Uac2StreamingEndpointInfo e;
    e.device_speed = LIBUSB_SPEED_HIGH;
    e.interface_number = 1;
    e.alt_setting = alt;
    e.endpoint_address = 1;
    e.endpoint_interval = interval;
    e.effective_bytes_per_uframe = capacity;
    e.as_general.is_present = true;
    e.as_general.format_type = UAC2_FORMAT_TYPE_I;
    e.as_general.bm_formats = UAC_FORMAT_TYPE_I_PCM;
    e.as_general.nr_channels = 2;
    e.as_general.subslot_size = 4;
    e.as_general.bit_resolution = 32;
    return e;
}
int main() {
    try {
        constexpr std::array<uint32_t, 6> rates{44100,48000,88200,96000,176400,192000};
        auto* handle = reinterpret_cast<libusb_device_handle*>(uintptr_t{1});
        // This is the observed 96 kHz/bInterval=3 regression, through the real pool.
        IsoTransferPoolConfig quarter;
        quarter.sample_rate_hz=96000; quarter.bytes_per_audio_frame=8; quarter.endpoint_address=1;
        quarter.device_speed=LIBUSB_SPEED_HIGH; quarter.endpoint_interval=3;
        quarter.endpoint_capacity_bytes=384; quarter.pool_size=1; quarter.packets_per_transfer=1;
        auto quarter_pool=IsoTransferPool::create(handle,quarter);
        check(bool(quarter_pool), "96k interval3 pool setup");
        uint32_t quarter_length[64]{};
        check(quarter_pool->compute_next_transfer_lengths(quarter_length)==384 && quarter_length[0]==384,
              "96k interval3 quarter-rate regression: packet must contain48frames not12");
        for (int speed : {LIBUSB_SPEED_FULL, LIBUSB_SPEED_HIGH, LIBUSB_SPEED_SUPER}) {
            for (uint8_t interval = 1; interval <= 4; ++interval) {
                for (uint32_t rate : rates) {
                    const auto cadence = UsbIsoCadence::from_endpoint(speed, interval);
                    check(cadence.has_value(), "valid cadence missing");
                    const uint32_t tick_hz = speed == LIBUSB_SPEED_FULL ? 1000 : 8000;
                    const uint32_t ticks = 1U << (interval - 1U);
                    const uint32_t packets = tick_hz / ticks;
                    const uint32_t expected_max = static_cast<uint32_t>((uint64_t{rate} * ticks + tick_hz - 1) / tick_hz * 8);
                    check(*cadence->max_packet_bytes(rate, 8) == expected_max, "capacity ceiling mismatch");
                    uint64_t phase=0, frames=0;
                    for (uint32_t n=0; n<packets; ++n) frames += cadence->next_frames(rate, phase);
                    check(frames == rate && phase == 0, "one physical second must deliver exact nominal frames");
                    IsoTransferPoolConfig cfg;
                    cfg.sample_rate_hz=rate; cfg.bytes_per_audio_frame=8; cfg.endpoint_address=1;
                    cfg.pool_size=1; cfg.packets_per_transfer=1;
                    cfg.device_speed=speed; cfg.endpoint_interval=interval;
                    cfg.endpoint_capacity_bytes=speed == LIBUSB_SPEED_FULL ? 1023 : speed == LIBUSB_SPEED_HIGH ? 3072 : 49152;
                    std::string error;
                    auto pool=IsoTransferPool::create(handle,cfg,&error);
                    if (expected_max > cfg.endpoint_capacity_bytes) {
                        check(!pool, "insufficient endpoint must fail before allocation");
                        continue;
                    }
                    check(bool(pool), "valid pool refused");
                    check(pool->bytes_per_uframe() == expected_max, "actual pool maximum does not follow interval");
                    uint64_t total=0;
                    uint64_t legacy_phase=rate % 8000;
                    for (uint32_t n=0; n<packets; ++n) {
                        uint32_t lengths[64]{};
                        const auto bytes=pool->compute_next_transfer_lengths(lengths);
                        check(bytes == lengths[0] && bytes % 8 == 0 && bytes <= expected_max, "pool packet overflow/alignment");
                        total += bytes;
                        if (speed != LIBUSB_SPEED_FULL && interval == 1) {
                            legacy_phase += rate;
                            check(bytes == (legacy_phase / 8000) * 8, "125us packet sequence regressed");
                            legacy_phase %= 8000;
                        }
                    }
                    check(total == uint64_t{rate} * 8, "actual pool drains wrong bytes per physical second");
                }
            }
        }
        const auto hs3=UsbIsoCadence::from_endpoint(LIBUSB_SPEED_HIGH,3);
        check(*hs3->max_packet_bytes(96000,8) == 384, "96k/interval3 must carry48frames, not12");
        check(uint64_t{96} * 2000 == uint64_t{96000} * 8 / 4, "old96-byte schedule demonstrates quarter rate");
        for (int speed : {LIBUSB_SPEED_FULL, LIBUSB_SPEED_HIGH}) {
            const auto slow=UsbIsoCadence::from_endpoint(speed,16);
            uint64_t phase=0,total=0;
            for (int i=0;i<125;++i) total += slow->next_frames(1,phase);
            check(total == (speed == LIBUSB_SPEED_FULL ? 4096U : 512U) && phase == 0,
                  "long intervals must not use truncated integer packet frequency");
        }
        check(!UsbIsoCadence::from_endpoint(LIBUSB_SPEED_LOW,1), "low speed must fail");
        check(!UsbIsoCadence::from_endpoint(LIBUSB_SPEED_UNKNOWN,1), "unknown speed must fail");
        check(!UsbIsoCadence::from_endpoint(LIBUSB_SPEED_HIGH,0), "interval0 must fail");
        check(!UsbIsoCadence::from_endpoint(LIBUSB_SPEED_HIGH,17), "interval17 must fail");
        check(!hs3->max_packet_bytes(0,8) && !hs3->max_packet_bytes(48000,0), "zero stream dimensions must fail");
        check(!hs3->max_packet_bytes(UINT32_MAX,UINT32_MAX), "packet-byte overflow must fail");
        check(usb_iso_service_capacity(LIBUSB_SPEED_FULL,1023)==1023, "FS capacity");
        check(usb_iso_service_capacity(LIBUSB_SPEED_FULL,1024)==0, "invalid FS capacity");
        check(usb_iso_service_capacity(LIBUSB_SPEED_HIGH,0x1400)==3072, "HS transactions lost");
        check(usb_iso_service_capacity(LIBUSB_SPEED_HIGH,0x1c00)==0, "reserved HS transaction count accepted");
        check(usb_iso_service_capacity(LIBUSB_SPEED_SUPER,1024,true,3,1,8000)==8000, "SS companion capacity lost");
        check(usb_iso_service_capacity(LIBUSB_SPEED_SUPER,1024,true,3,1,8193)==0, "SS invalid capacity accepted");
        check(usb_iso_service_capacity(LIBUSB_SPEED_SUPER,1024)==0, "SS missing companion accepted");
        check(usb_iso_service_capacity(LIBUSB_SPEED_SUPER_PLUS,1024,true,0,0x80,1024)==0, "unsupported extended SSP accepted");

        test_set_endpoint(LIBUSB_SPEED_HIGH,0x1400,3,false,0,0,0);
        auto scanned=uac2_find_streaming_endpoints(handle);
        check(scanned.size()==1 && scanned[0].effective_bytes_per_uframe==3072 && scanned[0].endpoint_interval==3
              && scanned[0].device_speed==LIBUSB_SPEED_HIGH, "scanner failed HS cadence/capacity binding");
        test_set_endpoint(LIBUSB_SPEED_SUPER,200,3,true,1,0,384);
        scanned=uac2_find_streaming_endpoints(handle);
        check(scanned.size()==1 && scanned[0].effective_bytes_per_uframe==384, "scanner ignored SS companion");
        test_set_endpoint(LIBUSB_SPEED_SUPER,200,3,false,0,0,0);
        check(uac2_find_streaming_endpoints(handle).empty(), "scanner accepted SS without companion");
        test_set_endpoint(LIBUSB_SPEED_HIGH,200,0,false,0,0,0);
        check(uac2_find_streaming_endpoints(handle).empty(), "scanner accepted interval0");

        std::vector<Uac2StreamingEndpointInfo> candidates{endpoint(1,3,200),endpoint(2,1,200)};
        const auto* selected=uac2_select_endpoint_for_format(candidates,32,2,96000,true);
        check(selected && selected->alt_setting==2, "selector accepted quarter-rate bandwidth estimate");
        candidates.resize(1);
        check(!uac2_select_endpoint_for_format(candidates,32,2,96000,true), "selector accepted undersized interval3 endpoint");
        candidates[0]=endpoint(1,3,250);
        check(!uac2_select_endpoint_for_format(candidates,16,2,96000,false), "selector sized requested16 instead of endpoint32 wire width");
        candidates={endpoint(1,3,384),endpoint(2,1,200)};
        check(uac2_select_best_endpoint(candidates)->alt_setting==2, "best endpoint compared bytes instead of bytes/time");
        auto capability=uac2_detect_native_dsd(candidates);
        check(capability.supports_dop && capability.pcm_interval==1 && capability.pcm_device_speed==LIBUSB_SPEED_HIGH
              && capability.pcm_bandwidth==200, "DSD summary lost chosen DoP cadence");
        candidates.resize(1);
        capability=uac2_detect_native_dsd(candidates);
        check(capability.pcm_interval==3, "DoP fallback must retain its own interval3");
        IsoTransferPoolConfig dop;
        dop.device_speed=capability.pcm_device_speed; dop.endpoint_interval=capability.pcm_interval;
        dop.endpoint_capacity_bytes=capability.pcm_bandwidth; dop.sample_rate_hz=96000;
        dop.bytes_per_audio_frame=8; dop.endpoint_address=1; dop.pool_size=1; dop.packets_per_transfer=8;
        auto pool=IsoTransferPool::create(handle,dop);
        check(pool && pool->bytes_per_uframe()==384, "DoP-selected cadence not accepted by pool");
        dop.endpoint_capacity_bytes=383;
        check(!IsoTransferPool::create(handle,dop), "pool accepted packet above selected capacity");
        dop.endpoint_capacity_bytes=384; dop.packets_per_transfer=65;
        check(!IsoTransferPool::create(handle,dop), "callback packet-array bound not enforced");
        check(test_usb_submissions==0, "host test must never submit USB transfers");
        std::cout << "{\"passed\":true,\"assertions\":" << checks << ",\"scope\":\"draft pool/scanner/selector/DSD-summary with no-device libusb stubs\"}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
