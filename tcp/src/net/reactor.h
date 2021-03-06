#ifndef _RECTOR_H_
#define _RECTOR_H_

// #include "protocol_head.h"
// #include "protocol_codec.h"
// #include "json_protocol_codec.h"

// #include "../common/event.h"
// #include "../common/protocol_codec.h"

#include "io_model.h"

class INetSocket;
class CReactor : public CThread
{
public:
    CReactor();
    virtual ~CReactor();
	virtual void run();

public:
    bool fire();
    void misfire();

    bool add_epoll_event(INetSocket *sock, int events);
	bool del_epoll_event(INetSocket *sock);
	bool modify_epoll_event(INetSocket *sock, int events);

    void push_event(IEvent* ev) { que_of_event_.push(ev); }
	void push_delayevent(IEvent* ev) { que_of_delay_event_.push(ev); }

protected:
    void do_event_queue();

private:
    CIOModel* io_model_;
	CEventQueue<IEvent *> que_of_event_;
	CEventQueue<IEvent *> que_of_delay_event_;
};

#endif//_RECTOR_H_