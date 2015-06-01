#include "ace/SOCK_Connector.h" 
#include "ace/Log_Msg.h" 
#include "ace/OS.h"
#include "CEvtRcv.h"
#include <iostream>

#define CONTROL_MESSAGE_EVENT_RECORD_START	(0x00020001)
#define CONTROL_MESSAGE_EVENT_RECORD_STOP	(0x00020002)
#define CONTROL_MESSAGE_TERMINATE			(0x00080001)

static char* SERVER_HOST = "127.0.0.1";
static u_short SERVER_PORT = 19002;

ssize_t sendCmd(ACE_SOCK_Stream& stream, unsigned int cmdCode);

int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
	ACE_TRACE(ACE_TEXT("main"));

	const char *server_host = argc > 1 ? argv[1] : SERVER_HOST;
	u_short server_port = argc > 2 ? ACE_OS::atoi(argv[2]) : SERVER_PORT;

	ACE_DEBUG((LM_INFO, "(%P|%t) server info(addr:%s,port:%d)\n",server_host, server_port));

	ACE_SOCK_Stream client_stream;
	ACE_INET_Addr remote_addr(server_port, server_host);
	ACE_SOCK_Connector connector;

	ACE_DEBUG((LM_DEBUG, "(%P|%t) Starting connect to %s: %d \n", remote_addr.get_host_name(), remote_addr.get_port_number()));
	if (connector.connect(client_stream, remote_addr) == -1)
		ACE_ERROR_RETURN((LM_ERROR, "(%P|%t) %p \n", "connection failed"), -1);
	else
		ACE_DEBUG((LM_DEBUG, "(%P|%t) connected to %s \n", remote_addr.get_host_name()));

	ACE_OS::sleep(2);

	//연결이 제대로 되는지 검사 테스트
	ACE_DEBUG((LM_INFO, "(%P|%t) Connection testing...\n"));
	char inpBuff[128];
	ACE_Time_Value timeout(1);
	ssize_t result = client_stream.recv(inpBuff, sizeof(inpBuff),&timeout);
	if (result == -1 && ACE_OS::last_error()==ETIME)
		ACE_DEBUG((LM_INFO, "(%P|%t) Connection test ok\n"));	//연결이 정상적이고 타임아웃된 경우이므로 정상 연결
	else
		ACE_ERROR_RETURN((LM_ERROR, "(%P|%t) %p \n", "Connection test failed"), -1);	//정상연결이 되지 않은 경우는 모두 실패 처리

	CEvtRcv er(&client_stream);
	er.activate();

	bool bRun = true;
	while (bRun)
	{
		ACE_DEBUG((LM_INFO, "\n<-Menu: Event Record Start[r],Event Record Stop:[t],Terminate Server[x]->\n"));
		std::cin >> inpBuff;
		std::cout << "Choice:" << inpBuff << std::endl;

		switch (inpBuff[0])
		{
		case 'r':
			if (sendCmd(client_stream, CONTROL_MESSAGE_EVENT_RECORD_START) != sizeof(unsigned int))
				ACE_ERROR_RETURN((LM_ERROR, "(%P|%t) %p\n", "error sendCmd(CONTROL_MESSAGE_EVENT_RECORD_START)"), -1);
			CEvtRcv::_sEventSequence = 0;	//event sequence 초기화
			ACE_DEBUG((LM_DEBUG, "(%P|%t) EVENT Record start\n"));
			break;
		case 't':
			if (sendCmd(client_stream, CONTROL_MESSAGE_EVENT_RECORD_STOP) != sizeof(unsigned int))
				ACE_ERROR_RETURN((LM_ERROR, "(%P|%t) %p\n", "error sendCmd(CONTROL_MESSAGE_EVENT_RECORD_STOP)"), -1);
			ACE_DEBUG((LM_DEBUG, "(%P|%t) EVENT Record stop\n"));
			break;
		case 'x':
			bRun = false;
			sendCmd(client_stream, CONTROL_MESSAGE_TERMINATE);
			ACE_DEBUG((LM_DEBUG, "(%P|%t) terminate server\n"));
			break;
		default:
			break;
		}
	}

	if (client_stream.close() == -1)
		ACE_ERROR_RETURN((LM_ERROR, "(%P|%t) %p \n", "close"), -1);
	er.wait();

	ACE_DEBUG((LM_INFO, ACE_TEXT("(%t) end\n")));
	ACE_RETURN(0);
}

ssize_t sendCmd(ACE_SOCK_Stream& stream,unsigned int cmdCode)
{
	ssize_t nSent = stream.send_n(&cmdCode, sizeof(unsigned int));
	ACE_ASSERT(nSent == sizeof(unsigned int));
	return nSent;
}