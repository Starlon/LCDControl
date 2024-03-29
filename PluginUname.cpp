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

#include <stdlib.h>
#include <cstring>
#include <errno.h>
#include <sys/utsname.h>

#include "PluginUname.h"
#include "Evaluator.h"
#include "debug.h"

using namespace LCD;

string PluginUname::Uname(string arg1)
{
    struct utsname utsbuf;
    std::string key;
    const char *value;

    key = arg1;

    if (uname(&utsbuf) != 0) {
        LCDError("uname() failed: %s", strerror(errno));
        return "";
    }

    if (key == "sysname") {
        value = utsbuf.sysname;
    } else if (key == "nodename") {
        value = utsbuf.nodename;
    } else if (key == "release") {
        value = utsbuf.release;
    } else if (key == "version") {
        value = utsbuf.version;
    } else if (key == "machine") {
        value = utsbuf.machine;
/*#if defined(_GNU_SOURCE) && ! defined(__APPLE__) && ! defined(__CYGWIN__)
    else if (key == "domainname")
        value = utsbuf.domainname;
#endif
*/
    } else {
        LCDError("uname: unknown field '%s'", key.c_str());
        value = "";
    }

    return value;
}

void PluginUname::Connect(Evaluator *visitor) {
/*
    QScriptEngine *engine = visitor->GetEngine();
    QScriptValue val = engine->newObject();
    QScriptValue objVal = engine->newQObject(val, this);
    engine->globalObject().setProperty("uname", objVal);
*/
}

Q_EXPORT_PLUGIN2(_PluginUname, PluginUname)
