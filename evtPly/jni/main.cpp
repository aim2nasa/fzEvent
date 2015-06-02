#include "ace/Log_Msg.h"
#include "CEvtPlayer.h"

int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
    ACE_TRACE(ACE_TEXT("main"));
    if(argc<2)
        ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT ("usage:ep <event file>\n")), -1);

    CEvtPlayer ep;
    ep.open_event_file(argv[1]); 
    ep.play_event(0);

    ACE_DEBUG((LM_DEBUG,"(%P:%t) event player end\n"));
    return 0;
}
