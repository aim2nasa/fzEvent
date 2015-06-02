#include "CEvtPlayer.h"
#include "ace/Log_Msg.h"
#include "typeDef.h"
#include <fcntl.h>
#include <linux/input.h>
#include "event_list.h"

CEvtPlayer::CEvtPlayer()
:_fd(0)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) CEvtPlayer Constructor\n")));
}

CEvtPlayer::~CEvtPlayer()
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) CEvtPlayer Destructor\n")));
}

int CEvtPlayer::open_event_file(const char* filename)
{
    ACE_TRACE(ACE_TEXT("open_event_file"));

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) open event file :%s\n"),filename));
    _fd = open(filename, O_RDWR);
    if(_fd < 0) {
        ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) event file open error\n")));
    }

    read_event();

    //while(read_event() != 0)
    //    ;

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) event file opened :%s\n"),filename));
    close_event_file();
    return 0;
}

int CEvtPlayer::read_event()
{
    ACE_TRACE(ACE_TEXT("read_event"));

    struct timeval _tv;

    int ret = 0;
    int count = 0;
    struct input_event* e = 0; 
    _s32 id; _u16 type; _u16 code; _s32 value;
    do {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) read event file...\n")));

	if(ret = read(_fd, &_tv, sizeof(_tv)) < 0) break;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) timevalue read\n")));
	if(_tv.tv_sec == 0xffffffff) 
	{	
	    if(_tv.tv_usec == 0xffffffff) {
	        printf("--- READ EVENT TOK DONE ---\n");
		break;
	     }else if(_tv.tv_usec == 0x8fffffff){
	        printf("--- READ TOK END ---\n");
	        return 0;
	    }
	}

	if(ret = read(_fd, &id, sizeof(id)) < 0) break;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) (id=%d) read\n"),id));
		
	if (ret = read(_fd, &type, sizeof(type)) < 0) break;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) (type=%d) read\n"),type));
	if (ret = read(_fd, &code, sizeof(code)) < 0) break;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) (code=%d) read\n"),code));
	if (ret = read(_fd, &value, sizeof(value)) < 0) break;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) (value=%d) read\n"),value));

	if(e) {
	    struct input_event* new_e = 
	        (struct input_event*)realloc(e, sizeof(struct input_event)*(count+1));
            e = new_e;
	}
	else
	    e = (struct input_event*)malloc(sizeof(struct input_event));

        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) event make buffer\n")));
	e[count].time = _tv;
	if(code == 0 && value == 0) {
 	    e[count].type = 0; /* SYN_REPORT */
	}else{
   	    e[count].type = type;
	}
	e[count].code = code;
	e[count].value = value;

	count++;

    } while(1);

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Read Event Count = %d\n"),count));

    if(e)
        insert_event_list(id, count, e);

    return -1;
}

int CEvtPlayer::close_event_file()
{ 
    ACE_TRACE(ACE_TEXT("close_event_file"));

    if(_fd) {
        close(_fd);
	_fd = 0;
    }
}

int CEvtPlayer::play_event(const int seq)
{
    ACE_TRACE(ACE_TEXT("play_event"));

    /* 위치 변경되지 않는다고 가정하고 작성됨 */
    struct event_list* p = get_event_data(seq);
    if(p == 0) {
        ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) event(seq:%d) not found error\n"),seq));
        return -1;
    }

    char file_buffer[512];
    sprintf(file_buffer, "/dev/input/event%d", p->id);

    int event_fd = open(file_buffer, O_WRONLY | O_NDELAY);
    if(event_fd < 0) {
        ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) event file ope failure :%s\n"),file_buffer));
        return -1;
    }

    struct timeval prev_time ;
    prev_time.tv_sec = -1; prev_time.tv_usec = -1;
    for(int i = 0 ; i < p->event_count ; ++i)
    {
        if(prev_time.tv_usec != -1) {
	    struct timeval diff_time ;
	    timersub(&p->e[i].time, &prev_time, &diff_time);
	    if(diff_time.tv_sec > 0 || diff_time.tv_usec > 100)
		select(0,0,0,0,&diff_time);
	}

	struct input_event evt;
	evt.time = p->e[i].time;
	evt.type = p->e[i].type;
	evt.code = p->e[i].code;
	evt.value = p->e[i].value;
	write(event_fd, &evt, sizeof(evt));

	prev_time = p->e[i].time;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Play Event: %x %x %x\n"),
            p->e[i].type,p->e[i].code,p->e[i].value));
    }

    struct input_event report;
    memset(&report, 0, sizeof(report));
    report.time = prev_time;
    write(event_fd, &report, sizeof(report));

    close(event_fd);
    return 0;
}
