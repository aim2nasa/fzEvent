/**
 * @file main.cpp
 * @brief 시작 위치
 * @details 바이너리 실행에 필요한 상위 기능 포함
 *
 * @date 2015.03.09
 * @version 0.1.0
 *
 * @author parksunghwa(park.sh@funzin.co.kr)
 */

#include "common/fz_common.h"
#include "common/fz_trace.h"
#include "common/fz_tcp_server.h"

#include "capture/surface_getter.h"
#include "event/event_getter.h"
#include "event/event_player.h"

#include <pthread.h>
#include <sys/time.h>

using namespace fz;

#define DEFAULT_LISTEN_PORT 18000

namespace /* anonymous */
{
	enum RUNNING_MODE {
		RUNNING_WAIT				= 0x00000000,
		RUNNING_SCREEN_CAPTURE		= 0x00000010,
		RUNNING_EVENT_RECORD		= 0x00000100,
		RUNNING_EVENT_PLAY			= 0x00000200,
	};
	_u32 running_mode = 
//		RUNNING_EVENT_RECORD 
//		| 
//		RUNNING_SCREEN_CAPTURE
//		|
		RUNNING_WAIT
		;

	qlist send_queue ;
	pthread_mutex_t running_mutex;
	pthread_mutex_t running_mode_mutex;

	tcp_server		ts;
	surface_getter	sg;
	event_getter	eg;
	event_player	ep;

	bool running = true;
	bool is_running() {
		bool ret;
		pthread_mutex_lock(&running_mutex);
		{
			ret = running;
		}
		pthread_mutex_unlock(&running_mutex);
		return ret;
	}
	_u32 get_running_mode(const _u32 _mask) {
		_u32 ret;
		pthread_mutex_lock(&running_mode_mutex);
		{
			ret = running_mode & _mask;
		}
		pthread_mutex_unlock(&running_mode_mutex);
		return ret;
	}
	void set_running_mode(const RUNNING_MODE _r) {
		pthread_mutex_lock(&running_mode_mutex);
		{
			running_mode |= _r;
		}
		pthread_mutex_unlock(&running_mode_mutex);
	}
	void unset_running_mode(const RUNNING_MODE _r) {
		pthread_mutex_lock(&running_mode_mutex);
		{
			running_mode &= ~_r;
		}
		pthread_mutex_unlock(&running_mode_mutex);
	}

	void set_running_state(const bool _state) {
		pthread_mutex_lock(&running_mutex);
		{
			running = _state;
		}
		pthread_mutex_unlock(&running_mutex);
	}

	/* 일정 이상의 버퍼가 queue에 쌓이면 강제종료 하도록 한다. 문제 있는것임 */
	const _u32 send_queue_size_max = 50;
	bool check_send_queue_max(){
		_u32 queue_size = send_queue.size();
		return (queue_size >= send_queue_size_max) ? true : false ;
	}


	_u8* make_surface_packet(const struct fz::packet_header& _header,
			/*const timeval& tv, const _u16 w, const _u16 h, */
			const _u32 len, _u32* out_len, _u8 const * const offset)
	{
		_u8* send_buffer; 
		*out_len = sizeof(_header.tv) + 
			sizeof(_header.width) + sizeof(_header.height) + 
			sizeof(len) + len ;

		send_buffer = (_u8*)calloc(1, *out_len);
		_u8* p = send_buffer;
		if(!send_buffer)
			return 0;

		/* header */
		memcpy(p, &_header.tv, sizeof(_header.tv)); p += sizeof(_header.tv);
		memcpy(p, &_header.width, sizeof(_header.width)); p += sizeof(_header.width);
		memcpy(p, &_header.height, sizeof(_header.height)); p += sizeof(_header.height);

		/* body */
		memcpy(p, &len, sizeof(len)); p += sizeof(len);
		memcpy(p, offset, len);

		return send_buffer;
	}

	_u8* make_event_packet(const struct fz::packet_header& _header,
			const struct event_getter::event_info& _ei, _u32* _len)
	{
		/*
		 * ---format--- 
		 * [h]send time [8]
		 * [h]event packet type [4]
		 * [b]len[4]
		 * [b]event name [2]
		 * [b]event count [4]
		 * [b]device id [4]
		 * [b-n]event time [8]
		 * [b-n]event id [4]
		 * [b-n]event name [2]
		 * [b-n]event value [4]
		 */
			
		int i ;
		_u8* send_buffer = 0;
		_u32 _header_len = sizeof (_header.tv) + sizeof(_header.type) + sizeof(*_len); 
		*_len = 
			sizeof(_ei.event_type) +
			sizeof(_ei.count) +
			sizeof(_ei.dev_id) +
			(
			 (sizeof(*_ei.tv) + sizeof(*_ei.id) + sizeof(*_ei.code) + sizeof(*_ei.value)) * 
			 _ei.count
			);

		send_buffer = (_u8*)calloc(1, _header_len + *_len);
		_u8* p = send_buffer;

		/* header */
		memcpy(p, &_header.tv, sizeof(_header.tv)); p += sizeof(_header.tv);
		memcpy(p, &_header.type, sizeof(_header.type));	p += sizeof(_header.type);

		memcpy(p, _len, sizeof(*_len)); p += sizeof(*_len); 

		/* body */
		memcpy(p, &_ei.event_type, sizeof(_ei.event_type)); p += sizeof(_ei.event_type);
		memcpy(p, &_ei.count, sizeof(_ei.count)); p += sizeof(_ei.count);
		memcpy(p, &_ei.dev_id, sizeof(_ei.dev_id)); p += sizeof(_ei.dev_id);
		for(i = 0 ; i < _ei.count ; ++i) {
#if 1 // test only
			printf("write=[%ld.%06ld][dev=%d][id=%d,code=%d,value=%d]\n", 
					_ei.tv[i].tv_sec, _ei.tv[i].tv_usec,
					_ei.dev_id,
					_ei.id[i], _ei.code[i], _ei.value[i]);
#endif
			memcpy(p, &_ei.tv[i], sizeof(_ei.tv[i])); p += sizeof(_ei.tv[i]);
			memcpy(p, &_ei.id[i], sizeof(_ei.id[i])); p += sizeof(_ei.id[i]);
			memcpy(p, &_ei.code[i], sizeof(_ei.code[i])); p += sizeof(_ei.code[i]);
			memcpy(p, &_ei.value[i], sizeof(_ei.value[i])); p += sizeof(_ei.value[i]);
		}

		*_len += _header_len;
		return send_buffer;
	}

	/* 차후에 중복된 부분 리팩토링 필요함... */
	void* get_surface_immediately(void* arg) {
		fz::TRACE(FZ_LOG_INFO, "get_surface_immediately()\n");
		if(get_running_mode(RUNNING_SCREEN_CAPTURE)) {
			fz::TRACE(FZ_LOG_ERROR, "get_surface_immediately()->NOW RUNNING SCREEN_CAPTURE!!\n");
			return 0;
		}
		surface_getter* sg = (surface_getter*)arg;
		_u16 w, h;
		_u32 len, send_len;
		_u8* offset = 0;
		_u8* send_buffer;
		sg->get_buffer(&offset, &len, &w, &h);
		send_buffer = make_surface_packet(fz::packet_header(w, h), 
				len, &send_len, offset);
		safe_free(offset);
		send_queue.push(send_buffer, send_len);
		fz::TRACE(FZ_LOG_INFO, "get_surface_immediately()->push(); SUCCESS\n");
		return 0;
	}

	void* get_surface(void* arg)
	{
		surface_getter* sg = (surface_getter*)arg;

		_u16 w, h;
		_u32 len, send_len;
		_u8* offset = 0;

		_u8* send_buffer;
		while(is_running())
		{
			if(get_running_mode(RUNNING_SCREEN_CAPTURE) == 0) {
				usleep(1000*1000);
				continue;
			}

			/* queue가 일정 size를 넘어가면 capture를 skip하도록 한다. */
			{
#define CAPTURE_SKIP_QUEUE_SIZE 5 
				_u32 queue_size;
				queue_size = send_queue.size();
				if(queue_size > CAPTURE_SKIP_QUEUE_SIZE) {
					fz::TRACE(FZ_LOG_INFO, "wait queue...\n");
					usleep(10 * 1000);
					continue;
				}
			}

#if 0
			/* queue가 일정 size를 넘어가면 종료하도록 한다. 
			 * 화면 캡쳐시 이 부분은 들어올 일 없음 */
			if(check_send_queue_max()) {
				fz::TRACE(FZ_LOG_ERROR, "send queue is full!!\n");
				set_running_state(false);
			}
#endif

			if( sg->get_buffer(&offset, &len, &w, &h) ) {
				fz::TRACE(FZ_LOG_ERROR, "get_surface()->get_buffer(); FAILURE\n");
				continue;
			}

			send_buffer = make_surface_packet(fz::packet_header(w, h), 
					len, &send_len, offset);
			if(!offset) {
				fz::TRACE(FZ_LOG_ERROR, "get_surface()->make_surface_packet(); FAILURE\n");
				continue;
			}
			safe_free(offset);

			send_queue.push(send_buffer, send_len);
			fz::TRACE(FZ_LOG_INFO, "get_surface()->push(); SUCCESS\n");

#if 0 // test only
			usleep(10);
#endif
		}

		fz::TRACE(FZ_LOG_INFO, "*** TERMINATE get_surface() THREAD ***\n");
		return 0;
	}

	void* get_event(void* arg)
	{
		event_getter* eg = (event_getter*)arg;

		_u32 len;
		_u8* send_buffer;
		struct event_getter::event_info* evt = 0;
		struct timeval tv;

		while(is_running())
		{
			if(get_running_mode(RUNNING_EVENT_RECORD) == 0) {
				usleep(1000*1000);
				continue;
			}

			/* queue가 일정 size를 넘어가면 종료하도록 한다. */
			if(check_send_queue_max()) {
				fz::TRACE(FZ_LOG_ERROR, "send queue is full!!\n");
				set_running_state(false);
			}

			eg->get_event(&evt);
			if(evt) {
#if 1 // test only
				printf("get event: ");
				printf("[%d] [%d] [%ld.%06ld] [%d] [%d %d %d]\n",
						evt->get()->event_type, evt->get()->count,
						evt->get()->tv->tv_sec, evt->get()->tv->tv_usec, 
						evt->get()->dev_id,
						*evt->get()->id, *evt->get()->code, *evt->get()->value);
#endif
				/* send event... */
				gettimeofday(&tv, 0);
				send_buffer = make_event_packet(fz::packet_header(PACKET_TYPE_EVENT),
						*evt, &len);
				send_queue.push(send_buffer, len);
				evt->clear();

				fz::TRACE(FZ_LOG_INFO, "get_event()->push(); SUCCESS\n");
			}

		}
		fz::TRACE(FZ_LOG_INFO, "*** TERMINATE get_event() THREAD ***\n");
		return 0;
	}

	void* play_event(void* arg)
	{
		event_player* ep = (event_player*)arg;

		while(is_running()){
			if(get_running_mode(RUNNING_EVENT_PLAY) == 0) {
				usleep(100*1000);
				continue;
			}

		}

		fz::TRACE(FZ_LOG_INFO, "*** TERMINATE play_event() THREAD ***\n");

		return 0;
	}

	void* send_packet(void* arg)
	{
		int ret;
		tcp_server* ts = (tcp_server*)arg;

		qlist::node::_container* send_packet;
		while(is_running()) {
			send_packet = send_queue.pop();

			/* queue 비어있음 */
			if(send_packet == 0) {
				//fz::TRACE(FZ_LOG_INFO, "EMPTY QUEUE...\n");
				usleep(10);
				continue;
			}

#if 0 // test only (dump)
			int i;
			for(int i = 0 ; i < send_packet->len_ ; ++i)
			{
				printf("%02x ", ((_u8*)send_packet->data_)[i]);
			}
			printf("\n");
#endif

			fz::TRACE(FZ_LOG_INFO, "send_packet(); ...\n");
			ret = ts->send_buffer(send_packet->data_, send_packet->len_);
			if(ret) {
				/* 접속이 끊긴 경우... 차후 제대로 수정 해야 함... */
				set_running_state(false);
			}
			else
				fz::TRACE(FZ_LOG_INFO, "send_packet(); SUCCESS\n--------------\n");

			safe_free(send_packet->data_);
			safe_free(send_packet);
#if 0 // test only
			usleep(10);
#endif
		}
		fz::TRACE(FZ_LOG_INFO, "*** TERMINATE send_packet() THREAD ***\n");
		return 0;
	}

	void* recv_packet(void* arg)
	{
		int ret;
		tcp_server* ts = (tcp_server*)arg;

		_u32 recv_buffer;
		int recv_buffer_len = sizeof(recv_buffer);

		while(is_running())
		{
			/* 4byte의 control message만 받는다. */
			ret = ts->recv_buffer(&recv_buffer, recv_buffer_len);
			if(ret) {
				continue;
			}
			fz::TRACE(FZ_LOG_INFO, "RECV Message = %d.%d\n", 
					(recv_buffer << 16) >> 16, (recv_buffer & 0xffff));
			switch(recv_buffer & 0x00ff00ff) {
				case CONTROL_MESSAGE_CAPTURE_START:
					set_running_mode(RUNNING_SCREEN_CAPTURE);
					fz::TRACE(FZ_LOG_INFO, "** CAPTURE START **\n");
					break;
				case CONTROL_MESSAGE_CAPTURE_STOP:
					unset_running_mode(RUNNING_SCREEN_CAPTURE);
					fz::TRACE(FZ_LOG_INFO, "** CAPTURE STOP **\n");
					break;
				case CONTROL_MESSAGE_CAPTURE_IMMEDIATELY:
					{
						pthread_t th_immediately;
						pthread_create(&th_immediately, 0, get_surface_immediately, &sg);
					}
					fz::TRACE(FZ_LOG_INFO, "** CAPTURE IMMEDIATELY **\n");
					break;
				case CONTROL_MESSAGE_EVENT_RECORD_START:
					set_running_mode(RUNNING_EVENT_RECORD);
					fz::TRACE(FZ_LOG_INFO, "** EVENT RECORD START **\n");
					break;
				case CONTROL_MESSAGE_EVENT_RECORD_STOP:
					unset_running_mode(RUNNING_EVENT_RECORD);
					fz::TRACE(FZ_LOG_INFO, "** EVENT RECORD STOP **\n");
					break;
				case CONTROL_MESSAGE_EVENT_PLAY_START:
					set_running_mode(RUNNING_EVENT_PLAY);
					fz::TRACE(FZ_LOG_INFO, "** EVENT PLAY START **\n");

					/* read event data... */
#define EVENT_FILE_PATH	"/data/local/tmp/evt"
					ep.open_event_file(EVENT_FILE_PATH);
					system("rm /data/local/tmp/evt");
					break;
				case CONTROL_MESSAGE_EVENT_PLAY_STEP:
					_u32 idx;
					idx = ((recv_buffer & 0xff000000) >> 16) | (recv_buffer & 0x0000ff00) >> 8;
					fz::TRACE(FZ_LOG_INFO, "** EVENT PLAY STEP(idx:%d) **\n", idx);
					ep.play_event(idx);
					break;
				case CONTROL_MESSAGE_EVENT_PLAY_STOP:
					unset_running_mode(RUNNING_EVENT_PLAY);
					fz::TRACE(FZ_LOG_INFO, "** EVENT PLAY STOP **\n");
					destroy_event_list();
					break;
				case CONTROL_MESSAGE_TERMINATE:
					fz::TRACE(FZ_LOG_INFO, "** RECEIVE TERMINATE MESSAGE **\n");
					set_running_state(false);
					break;
				default:
					fz::TRACE(FZ_LOG_INFO, "RECV UNKNOWN MESSAGE (%x)\n", recv_buffer);
					break;
			}

		}
		fz::TRACE(FZ_LOG_INFO, "*** TERMINATE recv_packet() THREAD ***\n");
		return 0;
	}

	int run() {
		/* tcp server init phase */
		static const int MAX_WAIT_TIME = 150 * 1000 * 1000;
		struct timeval start_time, current_time, tick;

		gettimeofday(&start_time, 0);
		while(ts.init(DEFAULT_LISTEN_PORT)) {
			usleep(100*1000); /* 10ms */ 
			gettimeofday(&current_time, 0);
			timersub(&current_time, &start_time, &tick);
			if( ((tick.tv_sec * 1000 * 1000) + tick.tv_usec) > MAX_WAIT_TIME ) {
				fz::TRACE(FZ_LOG_ERROR, "init() Timeout!\n");
				return 0;
			}
		}

		gettimeofday(&start_time, 0);
		while(ts.wait_client()) {
			gettimeofday(&current_time, 0);
			timersub(&current_time, &start_time, &tick);
			if( ((tick.tv_sec * 1000 * 1000) + tick.tv_usec) > MAX_WAIT_TIME ) {
				fz::TRACE(FZ_LOG_ERROR, "wait_client() Timeout!\n");
				return 0;
			}
		}

		/* thread init phase */
		pthread_t th_surface, th_event, th_event_play;
		pthread_t th_send, th_recv;

		pthread_create(&th_surface, 0, get_surface, &sg);
		pthread_create(&th_event, 0, get_event, &eg);
		//pthread_create(&th_event_play, 0, play_event, &ep);
		pthread_create(&th_send, 0, send_packet, &ts);
		pthread_create(&th_recv, 0, recv_packet, &ts);

		/* running phase... */
		do {
			usleep(100 * 1000);
		} while(is_running());
		usleep(100 * 1000);

		/* destroy */
		printf("terminate...\n");
		int status = 0;

		printf("join th_send...");
		pthread_join(th_send, (void**)&status);
		printf("join th_send...done(%d)\n", status);
		printf("join th_recv...");
		pthread_join(th_recv, (void**)&status);
		printf("join th_recv...done(%d)\n", status);
		ts.destroy();

		printf("join th_surface...");
		pthread_join(th_surface, (void**)&status);
		printf("join th_surface...done(%d)\n", status);
		sg.destroy();

		printf("join th_event...");
		pthread_join(th_event, (void**)&status);
		printf("join th_event...done(%d)\n", status);
		eg.destroy();
#if 0
		printf("join th_event_play...");
		pthread_join(th_event_play, (void**)&status);
		printf("join th_event_play...(%d)\n", status);
#endif
		ep.destroy();

		return 0;
	}
}


/*
 * @brief main function
 * @details 실행을 위해 필요한 mutex 객체의 초기화 이후 run 실행,
 *
 * @bug 일부 단말에서 terminate done! 메시지 이후에 종료되지 않는 문제 있음
 */
int main(int argc, char** argv)
{

	pthread_mutex_init(&running_mutex, 0);
	pthread_mutex_init(&running_mode_mutex, 0);
	/* run method 실행, 프로그램 종료 전까지 실행됨 */
	run();
	printf("## all thread terminate done ##\n");
	pthread_mutex_destroy(&running_mutex);
	pthread_mutex_destroy(&running_mode_mutex);

	printf("destroy send queue...\n");
	send_queue.destroy();
	printf("destroy send queue...done\n");

	printf("@@@@ termiate done! @@@@\n");
	usleep(2*1000*1000); /* 2초 후에 종료 */
	return 0;
}
