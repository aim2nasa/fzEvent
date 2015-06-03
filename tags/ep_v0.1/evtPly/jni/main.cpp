#include "ace/Log_Msg.h"
#include "CEvtPlayer.h"

#include "ace/OS.h"

#define DEFAULT_SLEEP 1

int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
    ACE_TRACE(ACE_TEXT("main"));

    ACE_LOG_MSG->priority_mask( LM_INFO|LM_ERROR, ACE_Log_Msg::PROCESS);

    if(argc<2)
        ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT ("usage:ep <event file> <[Opt]sleep(sec)>\n")), -1);

    int sleep = (argc>2)?ACE_OS::atoi(argv[2]):DEFAULT_SLEEP;
    ACE_DEBUG((LM_INFO,"(%P|%t) -event file:%s\n",argv[1]));
    ACE_DEBUG((LM_INFO,"(%P|%t) -sleep(sec):%d\n",sleep));
    
    CEvtPlayer ep;
    int events = ep.read_event(argv[1]); 
    ACE_DEBUG((LM_INFO,"(%P|%t) Readin events:%d\n",events));
   
    for(int i=0;i<events;i++) {
        ep.play_event(i);
        ACE_OS::sleep(sleep);
    }        
    ACE_DEBUG((LM_INFO,"(%P|%t) event player end\n"));
    return 0;
}
