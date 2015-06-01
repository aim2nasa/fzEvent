#ifndef __CEVTRCV_H__
#define __CEVTRCV_H__

#include "typedef.h"
#include "ace/Task.h"

class ACE_SOCK_Stream;
struct device_packet_header;
struct device_packet_event;

class CEvtRcv : public ACE_Task < ACE_MT_SYNCH >
{
	CEvtRcv();
public:
	CEvtRcv(ACE_SOCK_Stream* p);
	~CEvtRcv();

	virtual int svc(void);
	_u32 parseHeader(device_packet_header* _header, char* pBuffer);
	_u32 parseEvtPacket(device_packet_event* _event,char* pBuffer);
	static void unix_timeval_to_win32_systime(const timeval& in, LPSYSTEMTIME st);

private:
	ACE_SOCK_Stream* _pStream;
};

#endif