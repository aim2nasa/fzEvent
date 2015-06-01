#ifndef __DEVICE_PACKET_HEADER_H__
#define __DEVICE_PACKET_HEADER_H__

struct device_packet_header {
	timeval tv;
	union {
		struct {
			_u16	width;
			_u16	height;
		};
		_u32 type;
	};
	void clear() {
		tv.tv_sec = 0xffffffff;
		tv.tv_usec = 0xffffffff;
		type = 0xffffffff;
	}
	device_packet_header() { tv.tv_sec = 0; tv.tv_usec = 0; type = 0; }
};

#endif