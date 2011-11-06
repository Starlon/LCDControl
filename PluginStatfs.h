/* $Id$
 * $URL$
 *
 * Copyright (C) 2003 Michael Reinelt <michael@reinelt.co.at>
 * Copyright (C) 2004 The LCD4Linux Team <lcd4linux-devel@users.sourceforge.net>
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

#ifndef __PLUGIN_STATFS_H__
#define __PLUGIN_STATFS_H__

#include <QObject>
#include "PluginInterface.h"

namespace LCD {

class Evaluator;

class PluginStatfs : public QObject, PluginInterface {
    Q_OBJECT
    Q_INTERFACES(LCD::PluginInterface)

    public:
    void Connect(Evaluator *visitor);
    void Disconnect() {}

    public slots:
    double Statfs(QString arg1, QString arg2);
};

}; // End namespace
#endif
