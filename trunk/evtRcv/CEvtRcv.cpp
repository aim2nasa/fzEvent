#include "CEvtRcv.h"
#include "ace/SOCK_Stream.h"
#include "device_packet_header.h"

CEvtRcv::CEvtRcv(ACE_SOCK_Stream* p)
	:_pStream(p)
{
	ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) CEvtRcv Constructor\n")));
}

CEvtRcv::~CEvtRcv()
{
	ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) CEvtRcv() Destructor\n")));
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
	}
	return 0;
}

_u32 CEvtRcv::parseHeader(device_packet_header* _header,char* pBuffer)
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