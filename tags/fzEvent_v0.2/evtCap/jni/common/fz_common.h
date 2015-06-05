#ifndef ___FZ_COMMON_H
#define ___FZ_COMMON_H

#include <stdlib.h>
#include <pthread.h>

#include <stdio.h>
#include <unistd.h>

namespace fz{
	typedef unsigned char _u8;
	typedef unsigned short _u16;
	typedef unsigned int _u32;

	typedef signed char _s8;
	typedef signed short _s16;
	typedef signed int _s32;

#define safe_free(x) { if(x){ free(x); x = 0; } }

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

	#define CONTROL_MESSAGE_CAPTURE_START		(0x00010001)
	#define CONTROL_MESSAGE_CAPTURE_STOP		(0x00010002)
	#define CONTROL_MESSAGE_CAPTURE_IMMEDIATELY	(0x00010004)
	#define CONTROL_MESSAGE_EVENT_RECORD_START	(0x00020001)
	#define CONTROL_MESSAGE_EVENT_RECORD_STOP	(0x00020002)
	#define CONTROL_MESSAGE_EVENT_PLAY_START	(0x00040001)
	#define CONTROL_MESSAGE_EVENT_PLAY_STOP		(0x00040002)
	#define CONTROL_MESSAGE_EVENT_PLAY_STEP		(0x00040004)
	#define CONTROL_MESSAGE_TERMINATE			(0x00080001)

	#define PACKET_TYPE_MASK		(0x80008000) 
	#define PACKET_TYPE_CAPTURE		(0x00000000)
	#define PACKET_TYPE_EVENT		(0x80008000)
	#define PACKET_TYPE_CONTROL		(0x00008000)
	#define PACKET_TYPE_RESERVED1	(0x80000000)

	struct packet_header {
		timeval tv;
		union {
			struct {
				_u16 width;
				_u16 height;
			};
			_u32 type;
		};
		_u8* body;
		packet_header(const _u16 _width, const _u16 _height){
			gettimeofday(&tv, 0);
			width = _width; height = _height;
		}
		packet_header(const _u32 _type) {
			gettimeofday(&tv, 0);
			type = PACKET_TYPE_MASK | _type;
		}
	};
	struct packet_body_surface {
		_u32 len;
		void* offset;
	};
	struct packet_body_event {
	};

	/* 2015.02.13. parksunghwa
	 * 복사 성능 개선을 위하여 intrusive 형태로 구현
	 * 사용자가 직접 메모리를 해제해 주어야 함!!
	 * 직접 메모리 관리를 하지 않는 경우 사용하지 않도록 함!
	 * 지금은 귀찮아서 한 파일에 몰아 넣었는데 파일 나눠서 정리해야됨!
	 * */
	class qlist {
//#define DEBUG_QUEUELIST

#if 1 /* mutex에 관한 검증 필요 */
#define _NODE_LOCK		pthread_mutex_lock(&node_lock_mutex)
#define _NODE_UNLOCK	pthread_mutex_unlock(&node_lock_mutex)
#define _NODE_TRYLOCK	pthread_mutex_unlock(&node_lock_mutex)
//#define _NODE_UNLOCK	pthread_mutex_unlock(&node_lock_mutex)
//#define _NODE_TRYLOCK	while(pthread_mutex_trylock(&node_lock_mutex)) {usleep(10);}
#else
#define _NODE_LOCK
#define _NODE_UNLOCK
#define _NODE_TRYLOCK
#endif

		public:
			/* node impl */
			struct node {
				struct _container {
					void*	data_;
					_u32	len_;
					_container(void* data, const _u32 len)
						: data_(data), len_(len) {}
					_container(const _container& rhs)
						: data_(rhs.data_), len_(rhs.len_) {}
					_container& operator = (const _container& rhs){
						this->data_ = rhs.data_;
						this->len_ = rhs.len_;
					}
				} *container;
				node* next;
			};

			/* var */
			union {
				struct {
					node* head;
					node* tail;
				};
				node* v[2];
			};
			_u32 count;
			pthread_mutex_t node_lock_mutex;

		public:
			/* default method */
			int push(void* _p, const _u32 _l) 
			{
#ifdef DEBUG_QUEUELIST
				printf("queue: push\n");
#endif /* DEBUG_QUEUELIST */

				node* insert_node = 
					(node*)malloc(sizeof(node));
				insert_node->container = /* 이건 사용자가 지워야 함!!! */ 
					(node::_container*)malloc(sizeof(node::_container));

				insert_node->container->data_ = _p;
				insert_node->container->len_ = _l;
				insert_node->next = 0;

				_NODE_TRYLOCK;
				{
					if(tail) {
						tail->next = insert_node;
						tail = tail->next;
					}
					else {
						tail = insert_node;
						head = tail;
					}
					++count;
				}
				_NODE_UNLOCK;

				return 0;
			}
			node::_container* pop() /* with get data */
			{
#ifdef DEBUG_QUEUELIST
				printf("queue: pop...\n");
#endif /* DEBUG_QUEUELIST */

				_NODE_TRYLOCK;
				if(!head) {
#ifdef DEBUG_QUEUELIST
					printf("queue: is empty...\n");
#endif /* DEBUG_QUEUELIST */
					return 0;
				}

				node::_container* ret;
				node* remove_node;

				{
					remove_node = head;
					ret = remove_node->container;
					if(head->next) 
						head = head->next;
					else
						tail = head = 0;
					free(remove_node);
					--count;
				}
				_NODE_UNLOCK;

				return ret;
			}

			void destroy() {
				while(is_empty() == false){ 
					node::_container* p = pop();
					if(p == 0)
						continue; /* 이 경우 왜 발생되는지 확인 필 */
					safe_free(p->data_);
					safe_free(p);
				}
			}

			/* util method */
			bool is_empty()
			{
#ifdef DEBUG_QUEUELIST
				printf("queue: check empty\n");
#endif /* DEBUG_QUEUELIST */
				bool ret;
				_NODE_TRYLOCK;
				{
					ret = (head ? false : true);
				}
				_NODE_UNLOCK;
#ifdef DEBUG_QUEUELIST
				printf("queue: check empty(%s)\n", ret == true ? "true":"false");
#endif /* DEBUG_QUEUELIST */

				return ret;
			}
			const _u32 size() const {
				//_u32 ret;
				//_NODE_TRYLOCK; 
				//ret = count;
				//_NODE_UNLOCK;
				//return ret;
				return count;
			}
		public:
			qlist(const qlist& rhs); /* do not impl */
			qlist& operator = (const qlist& rhs); /* do not impl */

			qlist(): head(0), tail(0), count(0){
				pthread_mutex_init(&node_lock_mutex, 0);
			}
			~qlist(){
				destroy();
				pthread_mutex_destroy(&node_lock_mutex);
			}
	};
}

#define FZ_BEGIN	namespace fz {
#define FZ_END		} /*namespace fz*/

#endif /* ___FZ_COMMON */
