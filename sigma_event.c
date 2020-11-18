#include "sigma_event.h"
#include "sigma_log.h"
#include "interface_os.h"

typedef struct _sigma_event_listener
{
    struct _sigma_event_listener *_next;

    uint8_t event;
    SigmaEventHandler handler;
    uint8_t ctx[];
}SigmaEventListener;

static SigmaEventListener *_listeners = 0;

void *sigma_event_listen(uint8_t event, SigmaEventHandler handler, uint32_t size)
{
    if (!handler)
        return 0;

    SigmaEventListener *listener = (SigmaEventListener *)os_malloc(sizeof(SigmaEventListener) + size);
    if (!listener)
    {
        SigmaLogError(0, 0, "out of memory");
        return 0;
    }
    os_memset(listener, 0, sizeof(SigmaEventListener) + size);
    listener->event = event;
    listener->handler = handler;
    listener->_next = _listeners;
    _listeners = listener;
    return listener->ctx;
}

void sigma_event_dispatch(uint8_t event, void *msg, int size)
{
    SigmaEventListener *listener = _listeners;
    while (listener)
    {
        if (event == listener->event)
            listener->handler(listener->ctx, event, msg, size);

        listener = listener->_next;
    }
}
