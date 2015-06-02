#include "ace/Log_Msg.h"

int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
    ACE_TRACE(ACE_TEXT("main"));
    ACE_DEBUG((LM_DEBUG,"(%P:%t) end of main\n"));
    return 0;
}
