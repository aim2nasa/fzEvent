#include "ace/SOCK_Connector.h" 
#include "ace/Log_Msg.h" 
#include "ace/OS.h"

#define CONTROL_MESSAGE_EVENT_RECORD_START	(0x00020001)
#define CONTROL_MESSAGE_EVENT_RECORD_STOP	(0x00020002)

static char* SERVER_HOST = "127.0.0.1";
static u_short SERVER_PORT = 19002;

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

	//Do something
	while (1){
		unsigned int cmdCode;
		cmdCode = CONTROL_MESSAGE_EVENT_RECORD_START;
		ssize_t nSent = client_stream.send_n(&cmdCode, sizeof(unsigned int));
		ACE_ASSERT(nSent == sizeof(unsigned int));

		ACE_OS::sleep(5);

		cmdCode = CONTROL_MESSAGE_EVENT_RECORD_STOP;
		client_stream.send_n(&cmdCode, sizeof(unsigned int));
		ACE_ASSERT(nSent == sizeof(unsigned int));

		ACE_OS::sleep(1);
	}

	if (client_stream.close() == -1)
		ACE_ERROR_RETURN((LM_ERROR, "(%P|%t) %p \n", "close"), -1);

	ACE_DEBUG((LM_INFO, ACE_TEXT("(%t) end\n")));
	ACE_RETURN(0);
}