#ifndef __CEVTPLAYER_H__
#define __CEVTPLAYER_H__

#include "ace/OS_NS_stdio.h"

class CEvtPlayer{
public:
    CEvtPlayer();
    ~CEvtPlayer();

    int open_event_file(const char* filename);
    int read_event();
    int close_event_file();
    int play_event(const int seq);

private:
    FILE* _fp;
};

#endif
