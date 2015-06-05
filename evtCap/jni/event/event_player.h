#ifndef ___EVENT_PLAYER_H
#define ___EVENT_PLAYER_H

#include "../common/fz_common.h"
#include "../common/fz_trace.h"
#include <stdio.h>
#include <fcntl.h>

#include <linux/input.h>

FZ_BEGIN

struct event_list {
	event_list():seq(-1), e(0){}
	int seq;
	int id;
	int event_count;
	struct input_event* e;
	event_list* next;

	static int seq_number;
	static struct event_list* head;
};

int insert_event_list(int _id, int _event_count, struct input_event* _e);
struct event_list* get_event_data(int _seq);
int destroy_event_list();

class event_player {
	public:
		int destroy() {
		}

		int open_event_file(const char* _filename) {
			fz::TRACE(FZ_LOG_INFO, "open eventfile...\n");
			fd = open(_filename, O_RDWR);
			if(fd < 0) {
				fz::TRACE(FZ_LOG_ERROR, "event file open failure!!!\n");
			}
			while(read_event() != 0)
				;

			fz::TRACE(FZ_LOG_INFO, "open eventfile... done\n");

			close_event_file();
			return 0;
		}
		int read_event() {
			struct timeval _tv;

			int ret = 0;
			int count = 0;
			struct input_event* e = 0; 
			_s32 id; _u16 type; _u16 code; _s32 value;
			do {
				fz::TRACE(FZ_LOG_INFO, "read eventfile...\n");

				if(ret = read(fd, &_tv, sizeof(_tv)) < 0) break;
				fz::TRACE(FZ_LOG_INFO, "(tv)read eventfile...\n");
				if(_tv.tv_sec == 0xffffffff) 
				{	
					if(_tv.tv_usec == 0xffffffff) {
						printf("--- READ EVENT TOK DONE ---\n");
						break;
					}
					else if(_tv.tv_usec == 0x8fffffff) {
						printf("--- READ TOK END ---\n");
						return 0;
					}
				}

				if (ret = read(fd, &id, sizeof(id)) < 0)	break;
				fz::TRACE(FZ_LOG_INFO, "(id=%d)read eventfile...\n", id);
				
				if (ret = read(fd, &type, sizeof(type)) < 0) break;
				fz::TRACE(FZ_LOG_INFO, "(type=%d)read eventfile...\n", type);
				if (ret = read(fd, &code, sizeof(code)) < 0) break;
				fz::TRACE(FZ_LOG_INFO, "(code=%d)read eventfile...\n", code);
				if (ret = read(fd, &value, sizeof(value)) < 0) break;
				fz::TRACE(FZ_LOG_INFO, "(value=%d)read eventfile...\n", value);

				if(e) {
					struct input_event* new_e = 
						(struct input_event*)realloc(e, sizeof(struct input_event)*(count+1));
					e = new_e;
				}
				else
					e = (struct input_event*)malloc(sizeof(struct input_event));
				fz::TRACE(FZ_LOG_INFO, "event make buffer...\n");
				e[count].time = _tv;
				if(code == 0 && value == 0) {
					e[count].type = 0; /* SYN_REPORT */
				}
				else {
					e[count].type = type;
				}
				e[count].code = code;
				e[count].value = value;

				count++;

			} while(1);

			fz::TRACE(FZ_LOG_INFO, "Read Event Count = %d\n", count);

			if(e) {
				insert_event_list(id, count, e);
			}

			return -1;
		}
		int close_event_file() {
			if(fd) {
				close(fd);
				fd = 0;
			}
		}

		int play_event(const int _seq) {
			/* 위치 변경되지 않는다고 가정하고 작성됨 */
			struct event_list* p = get_event_data(_seq);
			if(p == 0) {
				fz::TRACE(FZ_LOG_ERROR, "event not found...!!\n");
				return -1;
			}

			char file_buffer[512];
			sprintf(file_buffer, "/dev/input/event%d", p->id);

#if 0 // debug only
			printf("open event dev file = %s\n", file_buffer);
#endif

			int event_fd = open(file_buffer, O_WRONLY | O_NDELAY);
			if(event_fd < 0) {
				fz::TRACE(FZ_LOG_ERROR, "event file open failure!!!!\n");
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
				//write(event_fd, &p->e[i].type, sizeof(p->e[i].type));
				//write(event_fd, &p->e[i].code, sizeof(p->e[i].code));
				//write(event_fd, &p->e[i].value, sizeof(p->e[i].value));

				prev_time = p->e[i].time;
				fz::TRACE(FZ_LOG_INFO, "Play Event: %x %x %x\n", 
						p->e[i].type, p->e[i].code, p->e[i].value);
			}
			struct input_event report;
			memset(&report, 0, sizeof(report));
			report.time = prev_time;
			write(event_fd, &report, sizeof(report));
			//static const _u16 _zero_16 = 0;
			//static const _u32 _zero_32 = 0;
			//write(event_fd, &_zero_16, sizeof(_zero_16));
			//write(event_fd, &_zero_16, sizeof(_zero_16));
			//write(event_fd, &_zero_32, sizeof(_zero_32));

			close(event_fd);


			return 0;			
		}

		event_player() : fd(0) {
		}
		~event_player() {
		}

	private:
		int fd;
};

FZ_END

#endif /*___EVENT_PLAYER_H*/

