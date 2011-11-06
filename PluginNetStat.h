/* $Id$
 * $URL$
 *
 * Copyright (C) 2009 Scott Sibley <scott@starlon.net>
 *
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

#ifndef __PLUGIN_NETSTAT_H__
#define __PLUGIN_NETSTAT_H__

#include <QObject>
#include <QtScript>

#include "Hash.h"
#include "PluginInterface.h"

namespace LCD {

class Evaluator;

class PluginNetStat : public QObject, PluginInterface {
    Q_OBJECT
    Q_INTERFACES(LCD::PluginInterface)

    HASH NetStat;
    FILE *Stream;
    bool first_time;
    int linecount;

    int ParseNetStat();

    public:
    PluginNetStat();
    ~PluginNetStat();
    void Connect(Evaluator *visitor);
    void Disconnect() {}

    public slots:
    QString Netstat(QString arg1, QString arg2);
    int LineCount();
};

} // End namespace

#endif
