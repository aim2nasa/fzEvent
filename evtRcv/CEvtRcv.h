#ifndef __CEVTRCV_H__
#define __CEVTRCV_H__

#include "ace/Task.h"

class ACE_SOCK_Stream;

class CEvtRcv : public ACE_Task < ACE_MT_SYNCH >
{
public:
	CEvtRcv();
	~CEvtRcv();

	virtual int svc(void);

	ACE_SOCK_Stream* _pStream;
};

#endif