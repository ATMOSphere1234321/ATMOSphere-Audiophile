#include "libusb/libusb.h"
#include <cstdlib>
static int speed_value=LIBUSB_SPEED_HIGH;
static bool companion_present=false;
static libusb_ss_endpoint_companion_descriptor companion_value{};
static libusb_endpoint_descriptor ep{};
static libusb_interface_descriptor alt{};
static libusb_interface iface{};
static libusb_config_descriptor config{};
unsigned test_usb_submissions=0;
void test_set_endpoint(int speed, uint16_t packet, uint8_t interval, bool companion,
                       uint8_t burst, uint8_t mult, uint16_t bytes) {
    speed_value=speed; companion_present=companion;
    companion_value={}; companion_value.bMaxBurst=burst;
    companion_value.bmAttributes=mult; companion_value.wBytesPerInterval=bytes;
    ep={}; ep.bLength=7; ep.bEndpointAddress=1; ep.bmAttributes=1;
    ep.wMaxPacketSize=packet; ep.bInterval=interval;
    alt={}; alt.bInterfaceNumber=1; alt.bAlternateSetting=1; alt.bNumEndpoints=1;
    alt.bInterfaceClass=1; alt.bInterfaceSubClass=2; alt.bInterfaceProtocol=0x20; alt.endpoint=&ep;
    iface={}; iface.altsetting=&alt; iface.num_altsetting=1;
    config={}; config.bConfigurationValue=1; config.bNumInterfaces=1; config.interface=&iface;
}
extern "C" {
int __android_log_print(int, const char*, const char*, ...) { return 0; }
int __android_log_write(int, const char*, const char*) { return 0; }
libusb_device* libusb_get_device(libusb_device_handle*) { return reinterpret_cast<libusb_device*>(1); }
int libusb_get_device_speed(libusb_device*) { return speed_value; }
int libusb_get_active_config_descriptor(libusb_device*, libusb_config_descriptor** out) { *out=&config; return 0; }
void libusb_free_config_descriptor(libusb_config_descriptor*) {}
int libusb_get_ss_endpoint_companion_descriptor(libusb_context*, const libusb_endpoint_descriptor*, libusb_ss_endpoint_companion_descriptor** out) {
    if (!companion_present) return LIBUSB_ERROR_NOT_FOUND;
    *out=new libusb_ss_endpoint_companion_descriptor(companion_value); return 0;
}
void libusb_free_ss_endpoint_companion_descriptor(libusb_ss_endpoint_companion_descriptor* p) { delete p; }
libusb_transfer* libusb_alloc_transfer(int packets) {
    return static_cast<libusb_transfer*>(std::calloc(1,sizeof(libusb_transfer)+sizeof(libusb_iso_packet_descriptor)*packets));
}
void libusb_free_transfer(libusb_transfer* p) { std::free(p); }
int libusb_submit_transfer(libusb_transfer*) { ++test_usb_submissions; return LIBUSB_ERROR_NOT_SUPPORTED; }
int libusb_cancel_transfer(libusb_transfer*) { return LIBUSB_ERROR_NOT_FOUND; }
const char* libusb_error_name(int) { return "host-stub"; }
}
