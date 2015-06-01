#include "CEvtRcv.h"
#include "ace/SOCK_Stream.h"
#include "device_packet_header.h"
#include "device_packet_event.h"

#define DEVM_DEVICE_PACKET_TYPE_MASK				(0x80008000) 
#define DEVM_DEVICE_PACKET_TYPE_SCREEN_CAPTURE		(0x00000000)
#define DEVM_DEVICE_PACKET_TYPE_EVENT_CAPTURE		(0x80008000)
#define DEVM_DEVICE_PACKET_TYPE_CONTROL				(0x00008000)
#define DEVM_DEVICE_PACKET_TYPE_RESERVED1			(0x80000000)

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