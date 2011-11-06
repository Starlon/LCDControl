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

#ifndef _NIFTY_EVENT_SOURCE_QT
#define _NIFTY_EVENT_SOURCE_QT

#include <QObject>
#include <QAbstractEventDispatcher>
#include <QHash>

#include <libnifty/libnifty.h>
#include <libnifty/mutex.h>
#include <libnifty/event.h>
#include <libnifty/client/remote.h>
#include <libnifty/client/output.h>

class QSocketNotifier;

class NiftyEventSource : public QObject
{
Q_OBJECT

 public:
    static nlEventContext *allocate();
    virtual ~NiftyEventSource();

    const nlEventSourceDescriptor *descriptor() const;

    bool registerSocket(int fd, nlEventSocketStatus status,
                        nlEventSocketActivatedFunc *callback,
                        void *user_pointer);
    bool unregisterSocket(int fd, nlEventSocketStatus status);
    int registerTimer(int timeout_msec,
                      nlEventTimerActivatedFunc *callback, void *user_pointer);
    bool unregisterTimer(int timer_id);
    bool process();

    void init(nlEventContext *context);

    bool initialized() const;

 private:
    NiftyEventSource();

    virtual void timerEvent ( QTimerEvent * event );

    bool m_initialized;

    nlEventContext *m_context;

    nlEventSourceDescriptor m_desc;
    nlMutex *m_mutex;
    typedef QPair<nlEventSocketActivatedFunc*, void *> SocketCallbackEntry;
    typedef QPair<nlEventTimerActivatedFunc*, void *> TimerCallbackEntry;

    QHash<int, TimerCallbackEntry> m_timerCallbacks;
    QHash<QSocketNotifier*, SocketCallbackEntry> m_readSockNotifiers;
    QHash<QSocketNotifier*, SocketCallbackEntry> m_writeSockNotifiers;

 private slots:
    void socketEvent(int socket);
 public:
};

#endif

