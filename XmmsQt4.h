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

#ifndef __XMMSQT4_H__
#define __XMMSQT4_H__

#include <xmmsclient/xmmsclient++/mainloop.h>

#include <QApplication>
#include <QObject>
#include <QSocketNotifier>

class XmmsQt4 : public QObject, public Xmms::MainloopInterface
{
        Q_OBJECT
        public:
                XmmsQt4(xmmsc_connection_t *xmmsc);
                ~XmmsQt4();

                void run ();

                void ToggleWrite(bool toggle);
                xmmsc_connection_t *GetXmmsConnection();

        public slots:
                void OnRead ();
                void OnWrite ();

        private:
                int m_fd;
                QSocketNotifier *m_rsock;
                QSocketNotifier *m_wsock;
                xmmsc_connection_t *m_xmmsc;
};

#endif
