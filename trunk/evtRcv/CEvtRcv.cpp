#include "CEvtRcv.h"
#include "ace/SOCK_Stream.h"
#include "device_packet_header.h"
#include "device_packet_event.h"
#include "input.h"

#define DEVM_DEVICE_PACKET_TYPE_MASK				(0x80008000) 
#define DEVM_DEVICE_PACKET_TYPE_SCREEN_CAPTURE		(0x00000000)
#define DEVM_DEVICE_PACKET_TYPE_EVENT_CAPTURE		(0x80008000)
#define DEVM_DEVICE_PACKET_TYPE_CONTROL				(0x00008000)
#define DEVM_DEVICE_PACKET_TYPE_RESERVED1			(0x80000000)

static const ACE_TString write_buffer_format[] = {
	ACE_TEXT("eventname : "),			/* [0]1 */
	ACE_TEXT("eventcount : "),			/* [1]1 */
	ACE_TEXT("eventtime : "),			/* [2]n */
	ACE_TEXT("eventid : "),				/* [3]n */
	ACE_TEXT("eventnamebyid : "),		/* [4]n */
	ACE_TEXT("eventvaluebyid : "),		/* [5]n */
};

CEvtRcv::CEvtRcv(ACE_SOCK_Stream* p)
	:_pStream(p)
{
	ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) CEvtRcv Constructor\n")));
}

CEvtRcv::~CEvtRcv()
{
	ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) CEvtRcv() Destructor\n")));
}

_u32 CEvtRcv::parseHeader(device_packet_header* _header, char* pBuffer)
{
	char* p = pBuffer;
	ACE_OS::memcpy(&_header->tv, p, sizeof(_header->tv));
	p += sizeof(_header->tv);
	ACE_OS::memcpy(&_header->type, p, sizeof(_header->type));
	p += sizeof(_header->type);

	_u32 len = 0;
	ACE_OS::memcpy(&len, p, sizeof(_u32));
	return len;
}

_u32 CEvtRcv::parseEvtPacket(device_packet_event* _event, char* pBuffer)
{
	_u8* p = reinterpret_cast<_u8*>(pBuffer);
	_u32 len = 0;
	_event->type = *(_u16*)p; p += sizeof(_event->type); len += sizeof(_event->type);
	_event->count = *(_u32*)p; p += sizeof(_event->count); len += sizeof(_event->count);
	_event->dev_id = *(_u32*)p; p += sizeof(_event->dev_id); len += sizeof(_event->dev_id);

	timeval _tv; _s32 _id; _u16 _code; _u32 _value;
	for (_u32 i = 0; i < _event->count; ++i)
	{
		_tv = *(timeval*)p; p += sizeof(_tv); len += sizeof(_tv);
		_id = *(_s32*)p; p += sizeof(_id); len += sizeof(_id);
		_code = *(_u16*)p; p += sizeof(_code); len += sizeof(_code);
		_value = *(_u32*)p; p += sizeof(_value); len += sizeof(_value);

		_event->tv.push_back(_tv);
		_event->id.push_back(_id);
		_event->code.push_back(_code);
		_event->value.push_back(_value);
	}
	return len;
}

void CEvtRcv::unix_timeval_to_win32_systime(const timeval& in, LPSYSTEMTIME st)
{
	LONGLONG ll;
	FILETIME ft;

	//#define USING_TIMEVAL_TO_SYSTIME_GMT_9
#ifdef USING_TIMEVAL_TO_SYSTIME_GMT_9
	{
		ll = Int32x32To64(in.tv_sec + (60 * 60 * 9), 10000000) + 116444736000000000LL;
	}
#else /*USING_TIMEVAL_TO_SYSTIME_GMT_9*/
		{
			ll = Int32x32To64(in.tv_sec, 10000000) + 116444736000000000LL;
		}
#endif /*USING_TIMEVAL_TO_SYSTIME_GMT_9*/

		ft.dwLowDateTime = (DWORD)ll;
		ft.dwHighDateTime = ll >> 32;

		FileTimeToSystemTime(&ft, st);
		st->wMilliseconds = (WORD)(in.tv_usec / 1000);
}

void CEvtRcv::OnEventCapture(char* pBuffer, _u32 len, const SYSTEMTIME& st, const timeval& tv)
{
	device_packet_event e;
	_u32 lenGet = parseEvtPacket(&e, pBuffer);
	ACE_ASSERT(lenGet == len);

	ACE_TString evtType = recognize_event(st, tv, e);
	ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) %s event\n"),evtType.c_str()));

	write(st);
}

int CEvtRcv::svc()
{
	device_packet_header header;
	char buf[BUFSIZ];
	while (1){
		header.clear();
		ssize_t rcvSize = _pStream->recv_n(buf, sizeof(header) + sizeof(_u32));
		if (rcvSize <= 0) {
			ACE_DEBUG((LM_ERROR, ACE_TEXT("(%P|%t) Connection closed while receiving header\n")));
			return -1;
		}
		ACE_ASSERT(rcvSize == (sizeof(header) + sizeof(_u32)));
		_u32 len = parseHeader(&header, buf);
		ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Header %dbytes received, Len=%dbytes\n"), rcvSize,len));

		rcvSize = _pStream->recv_n(buf,len);
		if (rcvSize <= 0) {
			ACE_DEBUG((LM_ERROR, ACE_TEXT("(%P|%t) Connection closed while receiving body\n")));
			return -1;
		}
		ACE_ASSERT(rcvSize==len);
		ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Body %dbytes received\n"),rcvSize));

		//convert unix timeval to SYSTEMTIME
		SYSTEMTIME st;
		unix_timeval_to_win32_systime(header.tv, &st);

		switch (header.type & DEVM_DEVICE_PACKET_TYPE_MASK)
		{
		case DEVM_DEVICE_PACKET_TYPE_SCREEN_CAPTURE:
			ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Screen Capture\n")));
			break;
		case DEVM_DEVICE_PACKET_TYPE_EVENT_CAPTURE:
			ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Event Capture\n")));
			OnEventCapture(buf, len,st,header.tv);
			break;
		case DEVM_DEVICE_PACKET_TYPE_CONTROL:
			ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Control\n")));
			break;
		case DEVM_DEVICE_PACKET_TYPE_RESERVED1:
			ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Reserved1\n")));
			break;
		default:
			break;
		}
	}
	return 0;
}

ACE_TString CEvtRcv::recognize_event(const SYSTEMTIME& st, const timeval& tv, const device_packet_event& e)
{
	ACE_TString code_label, value_label, ret;

	bool is_multitouch, is_swipe, is_key;
	is_multitouch = is_swipe = is_key = false;

	switch (e.type)
	{
	case EV_ABS: /* touch event */
	{
		{
			_s32 prev = -1;
			for (auto i = e.id.begin(); i != e.id.end(); ++i){
				if (*i != -1 && prev != -1 && *i != prev) {
					is_multitouch = true;
				}
				prev = *i;
			}
			if (is_multitouch){
				ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) EVENT: MultiTouch\n")));
				ret = ACE_TEXT("MULTITOUCH");
			}
			else{
				/* swipe는 velocity로 처리해야 하지만 현재는 우선 쉽게... */

				int first_x = -1, first_y = -1;
				int last_x = -1, last_y = -1;
				int x = -1, y = -1;
				for (_u32 i = 0; i < e.count; ++i) {
					switch (e.code[i]) {
					case ABS_MT_POSITION_X: x = e.value[i]; break;
					case ABS_MT_POSITION_Y: y = e.value[i]; break;
					}
					if (first_x == -1)
						first_x = x;
					if (first_y == -1)
						first_y = y;

					last_x = x;
					last_y = y;

					/* 우선은 임시로 이렇게 처리하는 것임, 나중에 바꿔야 함. */
					static const int velocity = 50;
					if (abs(first_x - last_x) > velocity ||
						abs(first_y - last_y) > velocity)
						is_swipe = true;
				}
				if (is_swipe) {
					ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) EVENT: Swipe\n")));
					ret = ACE_TEXT("SWIPE");
				}
				else {
					ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) EVENT: Tap\n")));
					ret = ACE_TEXT("TAP");
				}
			}
		}
		break;
	}
	case EV_KEY: /* key는 하나의 이벤트만 들어옴 */
	{
		if (e.count != 1) {
			/* 이 경우 문제 있는 것임 */
			return ACE_TEXT("");
		}
		is_key = true;
		ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) EVENT: GPIO Key\n")));
		ret = ACE_TEXT("KEY");
		break;
	}
	default:
		/* discard */
		return ACE_TEXT("");
	}
	return ret;
}

void CEvtRcv::write(const SYSTEMTIME& st)
{
	/* [path]\\[dev_name]_[YYYYMMDD_HHMMSSsss].txt */
	ACE_TCHAR filename[512];
	ACE_OS::sprintf(filename, ACE_TEXT("%s%s_%04d%02d%02d_%02d%02d%02d_%03d.txt"),ACE_TEXT("SCP"),ACE_TEXT("DEVID"),
		st.wYear, st.wMonth, st.wDay,st.wHour, st.wMinute, st.wSecond,st.wMilliseconds);
}