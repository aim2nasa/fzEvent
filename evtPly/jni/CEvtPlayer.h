#ifndef __CEVTPLAYER_H__
#define __CEVTPLAYER_H__

class CEvtPlayer{
public:
    CEvtPlayer();
    ~CEvtPlayer();

    int open_event_file(const char* filename);
    int read_event();
    int close_event_file();
    int play_event(const int seq);

private:
    int _fd; 
};

#endif
