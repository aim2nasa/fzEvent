#ifndef ___EVENT_GETTER_H
#define ___EVENT_GETTER_H

#include "../common/fz_common.h"
#include "../common/fz_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/inotify.h>

#include <linux/input.h>

#include "getevent.h"

FZ_BEGIN

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DEFAULT_DEVICE_PATH "/dev/input"
#define DEFAULT_OUTPUT_PATH "/mnt/sdcard"
#define DEFAULT_OUTPUT_FILENAME "evt_out"

class event_getter {
	public:
		enum {
			PRINT_DEVICE_ERRORS     = 1U << 0,
			PRINT_DEVICE            = 1U << 1,
			PRINT_DEVICE_NAME       = 1U << 2,
			PRINT_DEVICE_INFO       = 1U << 3,
			PRINT_VERSION           = 1U << 4,
			PRINT_POSSIBLE_EVENTS   = 1U << 5,
			PRINT_INPUT_PROPS       = 1U << 6,
			PRINT_HID_DESCRIPTOR    = 1U << 7,

			PRINT_ALL_INFO          = (1U << 8) - 1,

			PRINT_LABELS            = 1U << 16,
		};
		bool check_discard_event(_u16 code) {
			static const int discard_event_list[] = {
				BTN_TOOL_FINGER, 
				/*BTN_TOUCH,*/
				KEY_SLASH,
			};
			int len = sizeof(discard_event_list) / sizeof(*discard_event_list);
			int i;
			for(i = 0 ; i < len ; ++i)
				if(discard_event_list[i] == code)
					return true;
			return false;
		}

		int open_device(const char *device, int print_flags)
		{
			int version;
			int fd;
			int clkid = CLOCK_MONOTONIC;
			struct pollfd *new_ufds;
			char **new_device_names;
			char name[80];
			char location[80];
			char idstr[80];
			struct input_id id;

			fd = open(device, O_RDWR);
			if(fd < 0) {
				if(print_flags)
					fprintf(stderr, "could not open %s, %s\n", device, strerror(errno));
				return -1;
			}

			if(ioctl(fd, EVIOCGVERSION, &version)) {
				if(print_flags)
					fprintf(stderr, "could not get driver version for %s, %s\n", device, strerror(errno));
				return -1;
			}
			if(ioctl(fd, EVIOCGID, &id)) {
				if(print_flags)
					fprintf(stderr, "could not get driver id for %s, %s\n", device, strerror(errno));
				return -1;
			}
			name[sizeof(name) - 1] = '\0';
			location[sizeof(location) - 1] = '\0';
			idstr[sizeof(idstr) - 1] = '\0';
			name[sizeof(name) - 1] = '\0';
			location[sizeof(location) - 1] = '\0';
			idstr[sizeof(idstr) - 1] = '\0';
			if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
				fprintf(stderr, "could not get device name for %s, %s\n", device, strerror(errno));
				name[0] = '\0';
			}
			if(ioctl(fd, EVIOCGPHYS(sizeof(location) - 1), &location) < 1) {
				fprintf(stderr, "could not get location for %s, %s\n", device, strerror(errno));
				location[0] = '\0';
			}


			if(ioctl(fd, EVIOCGUNIQ(sizeof(idstr) - 1), &idstr) < 1) {
				fprintf(stderr, "could not get idstring for %s, %s\n", device, strerror(errno));
				idstr[0] = '\0';
			}


			if (ioctl(fd, EVIOCSCLOCKID, &clkid) != 0) {
				fprintf(stderr, "Can't enable monotonic clock reporting: %s\n", strerror(errno));
			}

			new_ufds = (struct pollfd*)realloc(ufds, sizeof(ufds[0]) * (nfds + 1));

			if(new_ufds == NULL) {
				fprintf(stderr, "out of memory\n");
				return -1;
			}


			ufds = new_ufds;
			new_device_names = (char**)realloc(device_names, sizeof(device_names[0]) * (nfds + 1));
			if(new_device_names == NULL) {
				fprintf(stderr, "out of memory\n");
				return -1;
			}
			device_names = new_device_names;

			if(print_flags)
				printf("add device %d: %s\n", nfds, device);
			if(print_flags)
				printf("  bus:      %04x\n"
						"  vendor    %04x\n"
						"  product   %04x\n"
						"  version   %04x\n",
						id.bustype, id.vendor, id.product, id.version);
			if(print_flags)
				printf("  name:     \"%s\"\n", name);
			if(print_flags)
				printf("  location: \"%s\"\n"
						"  id:       \"%s\"\n", location, idstr);
			if(print_flags)
				printf("  version:  %d.%d.%d\n",
						version >> 16, (version >> 8) & 0xff, version & 0xff);

		#if 0
			if(print_flags & PRINT_POSSIBLE_EVENTS) {
				print_possible_events(fd, print_flags);
			}

			if(print_flags & PRINT_INPUT_PROPS) {
				print_input_props(fd);
			}
			if(print_flags & PRINT_HID_DESCRIPTOR) {
				print_hid_descriptor(id.bustype, id.vendor, id.product);
			}
		#endif

			ufds[nfds].fd = fd;
			ufds[nfds].events = POLLIN;
			device_names[nfds] = strdup(device);
			nfds++;

			return 0;
		}
		
		int close_device(const char *device, int print_flags)
		{
			int i;
			for(i = 1; i < nfds; i++) {
				if(strcmp(device_names[i], device) == 0) {
					int count = nfds - i - 1;
					if(print_flags)
						printf("remove device %d: %s\n", i, device);
					free(device_names[i]);
					memmove(device_names + i, device_names + i + 1, sizeof(device_names[0]) * count);
					memmove(ufds + i, ufds + i + 1, sizeof(ufds[0]) * count);
					nfds--;
					return 0;
				}
			}
			if(print_flags)
				fprintf(stderr, "remote device: %s not found\n", device);
			return -1;
		}
		
		int scan_dir(const char *dirname, int print_flags)
		{
			char devname[PATH_MAX];
			char *filename;
			DIR *dir;
			struct dirent *de;
			dir = opendir(dirname);
			if(dir == NULL)
				return -1;
			strcpy(devname, dirname);
			filename = devname + strlen(devname);
			*filename++ = '/';
			while((de = readdir(dir))) {
				if(de->d_name[0] == '.' &&
						(de->d_name[1] == '\0' ||
						 (de->d_name[1] == '.' && de->d_name[2] == '\0')))
					continue;
				strcpy(filename, de->d_name);
				open_device(devname, print_flags);
			}
			closedir(dir);
			return 0;
		}

		int read_notify(const char *dirname, int nfd, int print_flags)
		{
			int res;
			char devname[PATH_MAX];
			char *filename;
			char event_buf[512];
			int event_size;
			int event_pos = 0;
			struct inotify_event *event;

			res = read(nfd, event_buf, sizeof(event_buf));
			if(res < (int)sizeof(*event)) {
				if(errno == EINTR)
					return 0;
				fprintf(stderr, "could not get event, %s\n", strerror(errno));
				return 1;
			}
			printf("got %d bytes of event information\n", res);

			strcpy(devname, dirname);
			filename = devname + strlen(devname);
			*filename++ = '/';

			while(res >= (int)sizeof(*event)) {
				event = (struct inotify_event *)(event_buf + event_pos);
				printf("%d: %08x \"%s\"\n", event->wd, event->mask, event->len ? event->name : "");
				if(event->len) {
					strcpy(filename, event->name);
					if(event->mask & IN_CREATE) {
						open_device(devname, print_flags);
					}
					else {
						close_device(devname, print_flags);
					}
				}
				event_size = sizeof(*event) + event->len;
				res -= event_size;
				event_pos += event_size;
			}
			return 0;
		}

	public:
		/* 기존의 이벤트 처리방식과 동일한 포맷으로 보이나 다른 포맷임. 헷갈리면 안됨
		 * [1]event type (TAP, SWIPE, POWERBUTTON 등의 전체 이름)
		 * [n]timeval (각각의 이벤트별 시간정보)
		 * [n]event id (각각의 발생된 이벤트(터치)에 대한 ID, 터치가 아닐 경우 -1,)
		 * [n]event name (DOWN, MOVEX, MOVEY, UP 등의 정보)
		 * [n]event value (이벤트값, MOVE의 경우 좌표 정보가 넘겨짐, 우선 char*)
		 * */
		struct event_info {
			public:
				_u16 event_type;
				_u32 count;

				_s32 dev_id;

				/* [n] */
				struct timeval* tv;
				_s32* id;
				_u16* code;
				_u32* value;

			public:
				const struct event_info* get() const {
					return this;
				}
			public:
				/* 사용법
				 * set->type, 후 개수만큼 add, 종료 전 clean
				 * */
				void set_type(const _u16 _event_type) {
					event_type = _event_type;
				}
				void set_dev_id(const _s32 _dev_id) {
					dev_id = _dev_id;
				}

				/* gpio key event일 경우 id는 -1(0xff)로 한다. */
				void add(const struct timeval& _tv, 
						const _s32 _id, const _u16 _code, const _u32 _value)
				{
#if 0 // test only
					printf("[%ld.%06ld], %d, %d, %d, (cnt=%d)\n",
							_tv.tv_sec, _tv.tv_usec, _id, _code, _value, count);
#endif
					/* SYN_REPORT가 2번 들어올 경우 하나는 skip하도록 한다... */
					if(_code == SYN_REPORT) {
						if( count == 0 ||
							(count > 0 && code[count-1] == SYN_REPORT) ) {
							//fz::TRACE(FZ_LOG_INFO, "skip SYN_REPORT...\n");
							return;
						}
					}

					tv = (struct timeval*)realloc(tv, sizeof(*tv)*(count+1));
					id = (_s32*)realloc(id, sizeof(*id)*(count+1));
					code = (_u16*)realloc(code, sizeof(*code)*(count+1));
					value = (_u32*)realloc(value, sizeof(*value)*(count+1));
					tv[count].tv_sec = _tv.tv_sec;
					tv[count].tv_usec = _tv.tv_usec;
					id[count] = _id;
					code[count] = _code;
					value[count] = _value;
					count++;

					fz::TRACE(FZ_LOG_INFO, "add event data... \n");
				}
				void clear() {
					safe_free(tv);
					safe_free(id);
					safe_free(code);
					safe_free(value);
					count = 0;
				}

				event_info():count(0), tv(0), id(0), code(0), value(0) {}
				~event_info() {
					clear();
				}
			
			private: /* not use, deep copy 구현할 시간 없음 */
				event_info(const event_info& rhs); /* block */
				event_info operator = (const event_info& rhs); /* block */
		};

		/* touch only (multitouch) */
		struct _event_id_map {
			#define DEFAULT_ID_MAP_SIZE 32
			struct _pair {
				_u32 slot;
				_u32 value;
				_pair(_u32 _slot = 0xffffffff, _u32 _value = 0xffffffff) 
					: slot(_slot), value(_value) {}
			} pair[DEFAULT_ID_MAP_SIZE]; 

			_u32 current_slot;
			int slot_count ;

			_u32& operator [] (const _u32 _slot) {
				static _u32 f = (_u32)-1;
				int r = get(_slot);
				if(r != -1)
					return pair[r].value;
				else
					return f;
			}

			void add_touch_id(const _u32 _value) {
				/* slot id의 경우 순차적으로 증가하는 것으로 보여 다음과 같이 작성됨.
				 * 정확한 확인이 필요하나 현재는 이대로 진행하도록 함.
				 * */
				pair[current_slot].slot = slot_count;
				pair[current_slot].value = _value;
				slot_count++;
#if 0 // debug only
				printf("id add: current count = %d\n", slot_count);
#endif
			}
			void remove_current_touch_id() {
#if 0 // debug only
				printf("remove: current:%d, [%d,%d]\n",
						current_slot, pair[current_slot].slot, pair[current_slot].value);
#endif
				pair[current_slot].slot = (_u32)-1;
				pair[current_slot].value = (_u32)-1;
				slot_count--;
			}
			const int change_current_touch_id(const _u32 _i) {
				current_slot = _i;
#if 0 // debug only
				printf("current id = %d, [%d, %d]\n", current_slot,
						pair[current_slot].slot, pair[current_slot].value);
#endif
				return 0;
			}
			_pair get_current_touch_id() {
				return pair[current_slot];
			}
			const int get_count() const {
				return slot_count;
			}
			const int get(const _u32 _value) {
				int i ;
				for(i = 0 ; i < DEFAULT_ID_MAP_SIZE ; ++i){
					if(pair[i].value == _value)
						return i;
				}
				return -1;
			}
			void clear() {
				memset(pair, 0xff, sizeof(pair));
				slot_count = 0;
				current_slot = 0;
			}
			_event_id_map() : slot_count(0) {
				clear();
			}
		} event_id_map;

	public:
		int get_event(struct event_getter::event_info** _ei) {
			int i, ret;
			poll(ufds, nfds, 1);
			if(ufds[0].revents & POLLIN) {
				read_notify(device_path, ufds[0].fd, debug_print);
			}
			/* 급하게 static으로 사용하였으나 문제 발생 가능성 있음. 이벤트 발생 도중 다른 이벤트 발생 시 
			 * recognize가 정확히 되지 않음. 
			 * */
			static struct event_info ei;

			for(i = 1 ; i < nfds ; ++i) {
				if(ufds[i].revents) {
					if(ufds[i].revents & POLLIN) {
						ret = read(ufds[i].fd, &event, sizeof(event));
						if(ret < (int)sizeof(event)) {
							fz::TRACE(FZ_LOG_ERROR, "could not get event\n");
							return -1;
						}

						switch( parse_event(event, &ei) )
						{
							case PARSE_START:
							case PARSE_CON:
								//write_event((_u32)i, event);
								*_ei = 0;
								break;
							case PARSE_END:
								//write_event((_u32)i, event);
								//write_event_wait();
								{
									char buff[1024];
									strcpy(buff, device_names[i]);
									char* p = strtok(buff, "abcdefghijklmnopqrstuvwxyz/");
									ei.set_dev_id(atoi(p));
#if 1 // debug only
									printf("device id = %d\n", ei.dev_id);
#endif
								}
								*_ei = &ei;
								break;
							case PARSE_SKIP:
								/* 테스트용 and Discard event용 */
								*_ei = 0;
								break;
						}
					}
				}
			}
			return 0;
		}

		enum PARSE_INFO{ 
			PARSE_START = 0,
			PARSE_CON = 1,
			PARSE_SKIP = 2,
			PARSE_END = 0x8fffffff,
		};

		int parse_event(const struct input_event& event, struct event_info* ei){
//#define TEST_LABEL /* 디버깅용 */

			const char* type_label = 0;
			const char* code_label = 0;
			const char* value_label = 0;

			int ret = PARSE_SKIP;

			/* 나중에 빼도록 함... 우선 쓰레드로 사용할 것이 아니므로.... */

			#ifdef TEST_LABEL
			type_label = get_label(ev_labels, event.type);
			#endif

			/* gflex가 id를 여러번 넘겨줌... 우선 이걸로 땜빵, 
			 * 나중에 꼭 고쳐야 함!!
			 * */
			static int slot_flag = 1; 

			/* 계속 바뀔 경우...? */
			ei->set_type(event.type);

			switch(event.type) {
				case EV_SYN:
					switch(event.code) {
						case SYN_REPORT:
							ei->add(event.time, (_u32)-1, event.code, event.value);
							break;
						default:
							break;
					}
					break;
				case EV_ABS:
					/* touch only */
					switch(event.code) {
						case ABS_MT_TRACKING_ID:
							if(event.value == -1/*0xffffffff*/) {
								/* pop id */
								_u32 _id = event_id_map.get_current_touch_id().value;
								event_id_map.remove_current_touch_id();
								fz::TRACE(FZ_LOG_INFO, "current touch count = %d\n", 
										event_id_map.get_count());
								if(event_id_map.get_count() == 0){
									/* send... */
									ei->add(event.time, _id, event.code, event.value);
									ei->add(event.time, -1, SYN_REPORT, 0); /* SYN_REPORT */
									event_id_map.clear();

									fz::TRACE(FZ_LOG_INFO, "Event parse end!\n");
									ret = PARSE_END;
									slot_flag = 1;
								}
								else {
									/* up은 되었지만 남은 touch가 있을 경우.. multi */
									ei->add(event.time, _id, event.code, event.value);
								}

							}
							else { 
								/* push id */
							
								if(slot_flag == 0)
									break;
								slot_flag = 0;
								event_id_map.add_touch_id(event.value);
								ei->add(event.time,
										event_id_map.get_current_touch_id().value,
										event.code, event.value);
							}
							break;
						case ABS_MT_SLOT: /* change id */
							event_id_map.change_current_touch_id(event.value);
							slot_flag = 1;
							break;
						case ABS_MT_POSITION_X:
						case ABS_MT_POSITION_Y:
							{
								_u32 _id = event_id_map.get_current_touch_id().value ;
								if(_id == 0xffffffff) {
									/* 현재 id 없음... 디버깅 필요함 */
									fz::TRACE(FZ_LOG_ERROR, "Invalid Touch ID\n");
									break;
								}
								ei->add(event.time, _id, event.code, event.value);
								ret = PARSE_CON;
							}
							break;
						default: /* silent discard */
							break;
					}
					break;
				case EV_KEY:
#if 0 // debug only
					printf("key input\n");
#endif
					if(check_discard_event(event.code))
						return ret;
#if 0 // debug only
					printf("key input... pass(%d)\n", event.value);
#endif
					/* gpio key only */
					#ifdef TEST_LABEL
					code_label = get_label(key_labels, event.code);
					value_label = get_label(key_value_labels, event.value);
					#endif
					/* 키 입력은 즉시 Write 하도록 한다. */
					ei->add(event.time, (_u32)-1, event.code, event.value);
					switch(event.code)
					{
						/* 2015.04.14. parksunghwa 일부 BTN_TOUCH를 사용하는 단말이 있어 이 경우는 Write하지 않도록 함.*/
						case BTN_TOUCH:
							/* 구현은 나중에 */
							break;
						default:
							ret = PARSE_END;
							break;
					}
					break;
				default:
					break;
			}

			return ret;
		}


	public:
		const char* get_label(const struct label *labels, int value) {
			while(labels->name && value != labels->value) {
				labels++;
			}
			return labels->name;
		}

	public:
#if 0
		/* file util */
		int write_event(const _u32 evt_id, const struct input_event& event) {
			write(fd, &event.time.tv_sec, sizeof(event.time.tv_sec)); /* 4 */
			write(fd, &event.time.tv_usec, sizeof(event.time.tv_usec)); /* 4 */
			write(fd, &evt_id, sizeof(evt_id)); /* 4 */
			write(fd, &event.type, sizeof(event.type)); /* 2 */
			write(fd, &event.code, sizeof(event.code)); /* 2 */
			write(fd, &event.value, sizeof(event.value)); /* 4 */

			return 0;
		}
		int write_event_wait() { 
			static const unsigned int tok_type = 0xffffffff;
			write(fd, &tok_type, sizeof(tok_type)); /* 4 */
			write(fd, &tok_type, sizeof(tok_type)); /* 4 */

			return 0;
		}

		int open_event_file() {
			if(fd) {
				fz::TRACE(FZ_LOG_ERROR, "Already exist file(parse)\n");
				return -1;
			}
			char path[512];
			sprintf(path, "%s/%s", output_path, output_filename);
			fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
			if(fd == -1) {
				fz::TRACE(FZ_LOG_ERROR, "Cannot create output file!!(%s)\n", output_filename);
				return -1;
			}

			//write_event_wait() ;

			return 0;
		}
		int close_event_file(){
			if(fd) {
				close(fd) ; fd = 0;
			}
			return 0;
		}
#endif
		
	public:
		int init(){
			int ret;

			debug_print = 0;

			nfds = 1;
			ufds = (struct pollfd*)calloc(0, sizeof(ufds[0]));
			ufds[0].fd = inotify_init();
			ufds[0].events = POLLIN;

			ret = inotify_add_watch(ufds[0].fd, device_path, IN_DELETE | IN_CREATE);
			if(ret < 0) {
				fz::TRACE(FZ_LOG_ERROR, "could not add watch for %s, %s\n", device_path, strerror(errno));
				return -1;
			}

			ret = scan_dir(device_path, debug_print);
			if(ret < 0) {
				fz::TRACE(FZ_LOG_ERROR, "scan dir failed for %s\n", device_path);
				return -1;
			}

			return 0;
		}
		int destroy(){
			int i;
#if 0
			for(i = 0 ; i < nfds ; ++i) {
				safe_free(device_names[i]);
			}
#endif
			safe_free(device_names);

			if(ufds) {
				safe_free(ufds); ufds = 0; 
			}
			safe_free(device_path);
			safe_free(output_path);
			safe_free(output_filename);
			return 0;
		}


	public:
		event_getter(const char* _device_path = DEFAULT_DEVICE_PATH, 
				const char* _output_path = DEFAULT_OUTPUT_PATH,
				const char* _output_filename = DEFAULT_OUTPUT_FILENAME)
		{
			if(_device_path) {
				device_path = (char*)malloc(strlen(_device_path) + 1);	
				strcpy(device_path, _device_path);
			}
			else
				device_path = 0;

			if(_output_path) {
				output_path = (char*)malloc(strlen(_output_path) + 1);
				strcpy(output_path, _output_path);
			}
			else
				output_path = 0;

			if(_output_filename) {
				output_filename = (char*)malloc(strlen(_output_filename) + 1);
				strcpy(output_filename, _output_filename);
			}
			else
				output_filename = 0;

			ufds = 0;
			device_names = 0;
			nfds = 0;

			init();

			//open_event_file();
		}
		~event_getter() {
			destroy();
			//close_event_file();
		}
	private:
		char* device_path;
		char* output_path;
		char* output_filename;

		//int fd;

		struct pollfd *ufds;
		char **device_names;
		int nfds;

		struct input_event event;
		int debug_print;
};

FZ_END

#endif /*___EVENT_GETTER_H*/
