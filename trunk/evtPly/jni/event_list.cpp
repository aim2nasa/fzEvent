#include "event_list.h"
#include <stdlib.h>

int event_list::seq_number = 0;
struct event_list* event_list::head = 0;

int insert_event_list(int _id, int _event_count, struct input_event* _e)
{
    struct event_list* p;
    p = (struct event_list*)malloc(sizeof(struct event_list));
    p->seq = event_list::seq_number++;
    p->e = _e;
    p->id = _id;
    p->event_count = _event_count;
    p->next = 0;

    if(event_list::head == 0) {
        event_list::head = p;
    }else{
        struct event_list* t = event_list::head;
        while(t->next) {
            t = t->next;
        }
        t->next = p;
    }
    return 0;
}

event_list* get_event_data(int _seq)
{
    struct event_list* p = event_list::head;
    while(p) {
        if(p->seq == _seq)
            return p;
	p = p->next;
    }
    return 0;
}

int destroy_event_list()
{
    struct event_list* p = event_list::head;
    while (p) {
        struct event_list* r = p;
	p = p->next;
	free(r->e);
	free(r);
    }
    event_list::head = 0;
    event_list::seq_number = 0;
    return 0;
}
