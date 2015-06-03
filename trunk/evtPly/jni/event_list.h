#ifndef __EVENT_LIST_H__
#define __EVENT_LIST_H__

struct input_event;

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
int get_count();
struct event_list* get_event_data(int _seq);
int destroy_event_list();

#endif
