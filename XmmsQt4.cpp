/* $Id$
 * $URL$
 *
 * Copyright (C) 2005-2007 XMMS2 Team
 * Copyright (C) 2009 Scott Sibley <scott@starlon.net>
 *
 * This file is a borrowed from Esperanza, an XMMS2 Client.
 * This file is part of LCDControl.
 *
 * LCDControl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LCDControl is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LCDControl.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient.h>
#include "XmmsQt4.h"

#include <QApplication>
#include <QObject>
#include <QSocketNotifier>

static void CheckWrite (int i, void *userdata);

XmmsQt4::XmmsQt4 (xmmsc_connection_t *xmmsc) :
        QObject (), Xmms::MainloopInterface (xmmsc),
        m_fd (0), m_rsock (0), m_wsock (0), m_xmmsc (xmmsc)
{
        m_fd = xmmsc_io_fd_get (xmmsc);
        xmmsc_io_need_out_callback_set (xmmsc, CheckWrite, this);

        m_rsock = new QSocketNotifier (m_fd, QSocketNotifier::Read, this);
        connect (m_rsock, SIGNAL (activated (int)), SLOT (OnRead ()));
        m_rsock->setEnabled (true);

        m_wsock = new QSocketNotifier (m_fd, QSocketNotifier::Write, this);
        connect (m_wsock, SIGNAL (activated (int)), SLOT (OnWrite ()));
        m_wsock->setEnabled (false);
        running_ = true;
}


XmmsQt4::~XmmsQt4 ()
{
        delete m_rsock;
        delete m_wsock;
}

void XmmsQt4::run ()
{
}

xmmsc_connection_t *XmmsQt4::GetXmmsConnection ()
{
        return m_xmmsc;
}


void XmmsQt4::OnRead ()
{
        if (!xmmsc_io_in_handle (m_xmmsc)) {
                return; /* exception? */
        }
}


void XmmsQt4::OnWrite ()
{
        if (!xmmsc_io_out_handle (m_xmmsc)) {
                return; /* exception? */
        }
}

void XmmsQt4::ToggleWrite (bool toggle)
{
        m_wsock->setEnabled (toggle);
}

static void CheckWrite (int i, void *userdata)
{
        XmmsQt4 *obj = static_cast< XmmsQt4* > (userdata);

        if (xmmsc_io_want_out (obj->GetXmmsConnection ())) {
                obj->ToggleWrite (true);
        } else {
                obj->ToggleWrite (false);
        }
}
