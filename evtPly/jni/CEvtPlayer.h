#ifndef __CEVTPLAYER_H__
#define __CEVTPLAYER_H__

class CEvtPlayer{
public:
    CEvtPlayer();
    ~CEvtPlayer();

    int read_event(const char* filename);
    int play_event(const int seq);
};

#endif
