/**
 *  NiftyPreview - Standalone output for libnifty daemons
 *
 *  Copyright (C) 2007,2008,2009  Sandro Gießl <sandro@niftylight.de>, <sgiessl@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/time.h>

#include <stdio.h>

#include <QSocketNotifier>
#include <QTimerEvent>
#include <QObject>

#include "niftyeventsource.h"

static int _sock_register(nlEventContext *context, void *internal_pointer,int fd, nlEventSocketStatus status, nlEventSocketActivatedFunc *callback, void *user_pointer)
{
    NiftyEventSource *h = static_cast<NiftyEventSource*>(internal_pointer);
    if(!h)
        return NL_FAILURE;

    if(!h->registerSocket(fd, status, callback, user_pointer))
        return NL_FAILURE;

    return NL_SUCCESS;
}

static int _sock_unregister(nlEventContext *context, void *internal_pointer,int fd, nlEventSocketStatus status)
{
    NiftyEventSource *h = static_cast<NiftyEventSource*>(internal_pointer);
    if(!h)
        return NL_FAILURE;

    if(!h->unregisterSocket(fd, status))
        return NL_FAILURE;

    return NL_SUCCESS;
}

static int _timer_register(nlEventContext *context, void *internal_pointer, int timeout_msec,
                                     nlEventTimerActivatedFunc *callback, void *user_pointer)
{
    NiftyEventSource *h = static_cast<NiftyEventSource*>(internal_pointer);
    if(!h)
        return NL_FAILURE;

    return h->registerTimer(timeout_msec, callback, user_pointer);
}

static int _timer_unregister(nlEventContext *context, void *internal_pointer, int timer_id)
{
    NiftyEventSource *h = static_cast<NiftyEventSource*>(internal_pointer);
    if(!h)
        return NL_FAILURE;

    if(!h->unregisterTimer(timer_id))
        return NL_FAILURE;

    return NL_SUCCESS;
}

static int _source_process(nlEventContext *context, void *internal_pointer)
{
    NiftyEventSource *h = static_cast<NiftyEventSource*>(internal_pointer);
    if(!h)
        return NL_FAILURE;

    if(!h->process())
        return NL_FAILURE;

    return NL_SUCCESS;
}


static void _source_init(nlEventContext *context, void *internal_pointer)
{
    NiftyEventSource *h = static_cast<NiftyEventSource*>(internal_pointer);

    Q_ASSERT(h);
    if(!h)
        return;

    h->init(context);
}

static void _source_free(void *internal_pointer)
{
    NiftyEventSource *h = static_cast<NiftyEventSource*>(internal_pointer);

    Q_ASSERT(h);
    if(!h)
        return;

    delete(h);
}


nlEventContext * NiftyEventSource::allocate()
{
    nlEventContext *context = NULL;

    NL_X_TRY {
        NiftyEventSource *h = new NiftyEventSource();

        if(!h->initialized())
        {
            delete(h);
            nl_assert(0);
        }

        context = nl_event_context_custom_alloc(h->descriptor(), h);

        // handle will be owned by nl_event_context_custom_alloc and
        // free'd via source_free destructor
        h = NULL;
    } NL_X_CATCH(e) {
        // don't throw exceptions
    } NL_X_END;

    return context;
}

NiftyEventSource::NiftyEventSource()
    : m_initialized(false), m_context(NULL), m_mutex(NULL)
{
    m_desc.source_socket_register = _sock_register;
    m_desc.source_socket_unregister = _sock_unregister;
    m_desc.source_timer_register = _timer_register;
    m_desc.source_timer_unregister = _timer_unregister;
    m_desc.source_process = _source_process;
    m_desc.source_init = _source_init;
    m_desc.source_free = _source_free;

    NL_X_TRY {
        m_mutex = nl_mutex_alloc(NL_MUTEX_RECURSIVE, __FILE__);
        nl_exception_cleanup_push(m_mutex,
                                  (nlExceptionCleanupFunc*)
                                  nl_mutex_free);

        nl_exception_cleanup_pop(m_mutex, NL_CLEANUP_KEEP);

        m_initialized = true;

    } NL_X_CATCH(e) {
        m_context = NULL;
        m_mutex = NULL;

        // no exceptions
    } NL_X_END;
}

NiftyEventSource::~NiftyEventSource()
{
    NL_X_TRY {
        if(m_mutex)
            NL_LOCK_PUSH(m_mutex);

        foreach(QSocketNotifier* n, m_writeSockNotifiers.keys())
        {
            delete(n);
        }
        m_writeSockNotifiers.clear();

        foreach(QSocketNotifier* n, m_readSockNotifiers.keys())
        {
            delete(n);
        }
        m_readSockNotifiers.clear();

        foreach(int id, m_timerCallbacks.keys())
        {
            killTimer(id);
        }

        if(m_mutex)
        {
            NL_UNLOCK_POP(m_mutex);

            /** @todo avoid race condition at this point */

            nl_mutex_free(m_mutex);
            m_mutex = NULL;
        }
    } NL_X_CATCH(e) {
        // no exceptions
    } NL_X_END;
}

const nlEventSourceDescriptor * NiftyEventSource::descriptor() const
{
    return &m_desc;
}

bool NiftyEventSource::registerSocket(int fd, nlEventSocketStatus status,
                                      nlEventSocketActivatedFunc *callback,
                                      void *user_pointer)
{
    if(fd < 0)
        return false;

    if(!m_initialized)
        return false;

    NL_LOCK_PUSH(m_mutex);

    QHash<QSocketNotifier*, SocketCallbackEntry> *notifiers = 0;
    QSocketNotifier::Type t;

    switch(status)
    {
    case NL_EVENT_SOCK_READABLE:
        notifiers = &m_readSockNotifiers;
        t = QSocketNotifier::Read;
        break;
    case NL_EVENT_SOCK_WRITABLE:
        notifiers = &m_writeSockNotifiers;
        t = QSocketNotifier::Write;
        break;
    }
    nl_assert(notifiers);

    // make sure the socket isn't registered yet
    foreach(QSocketNotifier* n, notifiers->keys())
    {
        int registered_fd = n->socket();
        nl_assert(registered_fd != fd);
    }

    // register
    QSocketNotifier *n = new QSocketNotifier(fd, t, this);
    connect(n, SIGNAL(activated(int)), this, SLOT(socketEvent(int)));
    (*notifiers)[n] =
        SocketCallbackEntry(callback, user_pointer);

    NL_UNLOCK_POP(m_mutex);

    return true;
}

bool NiftyEventSource::unregisterSocket(int fd, nlEventSocketStatus status)
{
    if(fd < 0)
        return false;

    if(!m_initialized)
        return false;

    NL_LOCK_PUSH(m_mutex);

    QHash<QSocketNotifier*, SocketCallbackEntry> *notifiers = 0;
    QSocketNotifier::Type t;

    switch(status)
    {
    case NL_EVENT_SOCK_READABLE:
        notifiers = &m_readSockNotifiers;
        t = QSocketNotifier::Read;
        break;
    case NL_EVENT_SOCK_WRITABLE:
        notifiers = &m_writeSockNotifiers;
        t = QSocketNotifier::Write;
        break;
    }
    nl_assert(notifiers);

    // make sure the socket is already registered + unregister
    bool found = false;
    foreach(QSocketNotifier* n, notifiers->keys())
    {
        int registered_fd = n->socket();
        if(registered_fd == fd)
        {
            notifiers->remove(n);
            delete(n);
            found = true;
            break;
        }
    }
    nl_assert(found);

    NL_UNLOCK_POP(m_mutex);

    return true;
}

int NiftyEventSource::registerTimer(int timeout_msec,
                                    nlEventTimerActivatedFunc *callback, void *user_pointer)
{
    if(!m_initialized)
        return NL_FAILURE;

    NL_LOCK_PUSH(m_mutex);

    int id = startTimer(timeout_msec);
    // in qt, 0 means the timer was invalid
    nl_assert(id > 0);

    nl_assert(!m_timerCallbacks.contains(id));

    m_timerCallbacks[id] = TimerCallbackEntry(callback,user_pointer);

    NL_UNLOCK_POP(m_mutex);

    return id;
}

bool NiftyEventSource::unregisterTimer(int timer_id)
{
    if(!m_initialized)
        return false;

    NL_LOCK_PUSH(m_mutex);

    nl_assert(m_timerCallbacks.contains(timer_id));

    killTimer(timer_id);

    m_timerCallbacks.remove(timer_id);

    NL_UNLOCK_POP(m_mutex);

    return true;
}

bool NiftyEventSource::process()
{
    if(!m_initialized)
        return false;

    QAbstractEventDispatcher *ev =
        QAbstractEventDispatcher::instance();

    ev->processEvents(QEventLoop::AllEvents);

    return true;
}

void NiftyEventSource::socketEvent(int socket)
{
    if(!m_initialized)
        return;

    QSocketNotifier *notifier =
        static_cast<QSocketNotifier*>(sender());
    if(!notifier)
        return;

    QHash<QSocketNotifier*, SocketCallbackEntry> *notifiers = 0;
    nlEventSocketStatus type;

    switch(notifier->type() )
    {
    case QSocketNotifier::Read:
        notifiers = &m_readSockNotifiers;
        type = NL_EVENT_SOCK_READABLE;
        break;
    case QSocketNotifier::Write:
        notifiers = &m_writeSockNotifiers;
        type = NL_EVENT_SOCK_WRITABLE;
        break;
    default:
        break;
    }

    if(!notifiers)
        return;

    if(!notifiers->contains(notifier))
        return;

    const SocketCallbackEntry &cb = (*notifiers)[notifier];

    if(cb.first)
    {
        // execute callback
        (*cb.first) (m_context, socket, type, cb.second);
    }
}

void NiftyEventSource::timerEvent ( QTimerEvent * event )
{
    if(!m_initialized)
        return;

    // overwritten from QObject
    if(!event)
        return;

    int timer_id = event->timerId();

    if(!m_timerCallbacks.contains(timer_id))
        return;

    const TimerCallbackEntry cb = m_timerCallbacks[timer_id];

    if(cb.first)
    {
        (*cb.first) (m_context, timer_id, cb.second);
    }
}

bool NiftyEventSource::initialized() const
{
    return m_initialized;
}

void NiftyEventSource::init(nlEventContext *context)
{
    m_context = context;
}

//#include "niftyeventsource.moc"
