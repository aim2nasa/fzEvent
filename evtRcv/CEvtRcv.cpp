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
	char buf[BUFSIZ];
	while (1){
		ssize_t rcvSize = _pStream->recv(buf, sizeof(buf));
		if (rcvSize <= 0) {
			ACE_DEBUG((LM_ERROR, ACE_TEXT("(%P|%t) Connection closed\n")));
			return -1;
		}

		ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) %dbytes received\n"),rcvSize));

		device_packet_header header;
		header.clear();

		parseHeader(&header, buf);
	}
	return 0;
}

void CEvtRcv::parseHeader(device_packet_header* _header,char* pBuffer)
{
	char* p = pBuffer;
	ACE_OS::memcpy(&_header->tv, p, sizeof(_header->tv));
	p += sizeof(_header->tv);
	ACE_OS::memcpy(&_header->type, p, sizeof(_header->type));
}