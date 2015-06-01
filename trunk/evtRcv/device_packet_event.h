#ifndef __DEVICE_PACKET_EVENT_H__
#define __DEVICE_PACKET_EVENT_H__

#include "typedef.h"
#include <vector>

struct device_packet_event {
	_u16 type;
	_u32 count;
	_s32 dev_id;
	std::vector<timeval> tv;
	std::vector<_s32> id;
	std::vector<_u16> code;
	std::vector<_u32> value;
};

#endif