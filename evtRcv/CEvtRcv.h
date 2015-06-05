#ifndef __CEVTRCV_H__
#define __CEVTRCV_H__

#include "typedef.h"
#include "ace/Task.h"
#include <vector>

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
	static void OnEventCapture(char* pBuffer, _u32 len, const timeval& tv);
	static ACE_TString recognize_event(const timeval& tv, const device_packet_event& e);
	static void write(const timeval& tv, bool is_key, bool is_multitouch, bool is_swipe, const device_packet_event& e);
	static ACE_TString get_label(const struct label *labels, int value);

	static void makeEventFile();
	static void writeEventFile(const _u32 count, const _s32 dev_id, const std::vector<timeval>& tv,
		const _u16& type,const std::vector<_u16>& code, const std::vector<_u32>& value);
	static void closeEventFile();

	static unsigned int _sEventSequence;
private:
	ACE_SOCK_Stream* _pStream;
	static FILE*	 _sFpEvt;
};

#endif