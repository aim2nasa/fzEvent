#ifndef __CEVTRCV_H__
#define __CEVTRCV_H__

#include "typedef.h"
#include "ace/Task.h"

class ACE_SOCK_Stream;
struct device_packet_header;

class CEvtRcv : public ACE_Task < ACE_MT_SYNCH >
{
	CEvtRcv();
public:
	CEvtRcv(ACE_SOCK_Stream* p);
	~CEvtRcv();

	virtual int svc(void);
	_u32 parseHeader(device_packet_header* _header, char* pBuffer);

private:
	ACE_SOCK_Stream* _pStream;
};

#endif