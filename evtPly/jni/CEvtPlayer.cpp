#include "CEvtPlayer.h"
#include "ace/Log_Msg.h"
#include "typeDef.h"
#include <fcntl.h>
#include <linux/input.h>
#include "event_list.h"

CEvtPlayer::CEvtPlayer()
:_fp(NULL)
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

    ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) openning event file :%s...\n"),filename));
    _fp = ACE_OS::fopen(filename,ACE_TEXT("rb"));
    if(_fp==0){
        ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) event file open error\n")));
    }

    while(read_event() != 0)
        ;

    close_event_file();
    ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) open event file done, %s\n"),filename));
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

	if((ret = ACE_OS::fread(&_tv,1,sizeof(_tv),_fp))!=sizeof(_tv)) break;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) timevalue read\n")));
	if(_tv.tv_sec == 0xffffffff) 
	{	
	    if(_tv.tv_usec == 0xffffffff) {
                ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) --- READ EVENT TOK DONE ---\n")));
		break;
	     }else if(_tv.tv_usec == 0x8fffffff){
                ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) --- READ TOK END ---\n")));
	        return 0;
	    }
	}

	if((ret = ACE_OS::fread(&id,1,sizeof(id),_fp))!=sizeof(id)) break;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) (id=%d) read\n"),id));
	if((ret = ACE_OS::fread(&type,1,sizeof(type),_fp))!=sizeof(type)) break;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) (type=%d) read\n"),type));
	if((ret = ACE_OS::fread(&code,1,sizeof(code),_fp))!=sizeof(code)) break;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) (code=%d) read\n"),code));
	if((ret = ACE_OS::fread(&value,1,sizeof(value),_fp))!=sizeof(value)) break;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) (value=%d) read\n"),value));

	if(e) {
	    struct input_event* new_e = 
	        (struct input_event*)realloc(e, sizeof(struct input_event)*(count+1));
            e = new_e;
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) e!=0\n")));
	}
	else{
	    e = (struct input_event*)malloc(sizeof(struct input_event));
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) e==0\n")));
        }

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

    if(e) {
        int elements = insert_event_list(id, count, e);
        ACE_DEBUG((LM_INFO,ACE_TEXT("(%P|%t) insert event(id:%d,count:%d),event list=%d\n"),id,count,elements));
    }

    return -1;
}

int CEvtPlayer::close_event_file()
{ 
    ACE_TRACE(ACE_TEXT("close_event_file"));

    if(_fp) {
        ACE_OS::fclose(_fp);
	_fp = NULL;
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

    FILE* fpw = ACE_OS::fopen(file_buffer,ACE_TEXT("w"));
    if(fpw==NULL){
        ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) event file ope failure :%s\n"),file_buffer));
        return -1;
    }

    size_t written=0;
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
        written = ACE_OS::fwrite(&evt,1,sizeof(evt),fpw);
        ACE_ASSERT(written==sizeof(evt));

	prev_time = p->e[i].time;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Play Event: %x %x %x\n"),
            p->e[i].type,p->e[i].code,p->e[i].value));
    }

    struct input_event report;
    memset(&report, 0, sizeof(report));
    report.time = prev_time;
    written = ACE_OS::fwrite(&report,1,sizeof(report),fpw);
    ACE_ASSERT(written==sizeof(report));

    ACE_OS::fclose(fpw);
    ACE_DEBUG((LM_INFO,ACE_TEXT("(%P|%t) play event(seq:%d)\n"),seq));
    return 0;
}
