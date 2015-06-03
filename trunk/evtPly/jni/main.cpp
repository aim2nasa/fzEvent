#include "ace/Log_Msg.h"
#include "CEvtPlayer.h"

#include "ace/OS.h"

int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
    ACE_TRACE(ACE_TEXT("main"));

    ACE_LOG_MSG->priority_mask( LM_INFO|LM_ERROR, ACE_Log_Msg::PROCESS);

    if(argc<2)
        ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT ("usage:ep <event file>\n")), -1);

    CEvtPlayer ep;
    ep.open_event_file(argv[1]); 
    ep.play_event(0);
    ACE_OS::sleep(3);
    ep.play_event(1);
    ACE_OS::sleep(3);
    ep.play_event(2);
    ACE_OS::sleep(3);
    ep.play_event(3);
    ACE_OS::sleep(3);
    ep.play_event(4);

    ACE_OS::sleep(3);
    ep.play_event(5);
    ACE_OS::sleep(3);
    ep.play_event(6);

    ACE_DEBUG((LM_INFO,"(%P:%t) event player end\n"));
    return 0;
}
