#include "CEvtRcv.h"
#include "ace/SOCK_Stream.h"

CEvtRcv::CEvtRcv()
	:_pStream(NULL)
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
			ACE_DEBUG((LM_DEBUG,ACE_TEXT("(%P|%t) Connection closed\n")));
			return -1;
		}

		ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) %dbytes received\n"),rcvSize));
	}
	return 0;
}