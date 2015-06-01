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

	static _u32 parseHeader(device_packet_header* _header, char* pBuffer);
	static _u32 parseEvtPacket(device_packet_event* _event,char* pBuffer);
	static void OnEventCapture(char* pBuffer, _u32 len, const SYSTEMTIME& st, const timeval& tv);
	static void unix_timeval_to_win32_systime(const timeval& in, LPSYSTEMTIME st);
	static ACE_TString recognize_event(const SYSTEMTIME& st, const timeval& tv, const device_packet_event& e);
	static void write(const SYSTEMTIME& st, const timeval& tv, bool is_key, bool is_multitouch, bool is_swipe, const device_packet_event& e);
	static ACE_TString get_label(const struct label *labels, int value);

	static unsigned int _sEventSequence;
private:
	ACE_SOCK_Stream* _pStream;
};

#endif