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

#include <QString>
#include <QtScript>

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <ctype.h>
#include <errno.h>

#include "debug.h"
#include "qprintf.h"
#include "Hash.h"
#include "PluginNetDev.h"
#include "Evaluator.h"

using namespace LCD;


int PluginNetDev::ParseNetDev()
{
    const char* DELIMITER = " :|\t\n";
    int age;
    int row, col;

    /* reread every 10 msec only */
    age = hash_age(&NetDev, NULL);
    if (age > 0 && age <= 10)
        return 0;
    if (Stream == NULL)
        Stream = fopen("/proc/net/dev", "r");
    if (Stream == NULL) {
        LCDError("fopen(/proc/net/dev) failed: %s", strerror(errno));
        return -1;
    }

    rewind(Stream);
    row = 0;

    while (!feof(Stream)) {
        char buffer[256];
        char dev[16];
        char *beg, *end;
        unsigned int len;

        beg = NULL;
        end = NULL;

        if (fgets(buffer, sizeof(buffer), Stream) == NULL)
            break;

        switch (++row) {

        case 1:
            /* skip row 1 */
            continue;

        case 2:
            /* row 2 used for headers */
            if (first_time || false) {
                char *RxTx = strrchr(buffer, '|');
                first_time = 0;
                col = 0;
                beg = buffer;
                while (beg && strlen(beg) > 0) {
                    char key[32];
                    while (strlen(beg) > 0 && strchr(DELIMITER, *beg))
                        beg++;
                    if (strlen(beg) == 0 || (end = strpbrk(beg, DELIMITER)) != NULL)
                        *end = '\0';
                    if(strlen(beg) > 0) {
                        qprintf(key, sizeof(key), "%s_%s", beg < RxTx ? "Rx" : "Tx", beg);
                        hash_set_column(&NetDev, col++, key);
                    }
                    beg = end ? end + 1 : NULL;
                }
            }
            continue;

        default:
            /* fetch interface name (1st column) as key */
            beg = buffer;
            while (strlen(beg) > 0 && *beg && *beg == ' ')
                beg++;
            if(strlen(beg) > 0) {
                end = beg + 1;
                while (strlen(end) > 0 && *end && *end != ':')
                    end++;
                len = end - beg;
                if (len >= sizeof(dev))
                    len = sizeof(dev) - 1;
                if(strlen(beg) >= len) {
                    strncpy(dev, beg, len);
                    dev[len] = '\0';
    
                    hash_put_delta(&NetDev, dev, buffer);
                }
            }
	        continue;
        }
    }

    return 0;
}

QScriptValue PluginNetDev::Regex(QString arg1, QString arg2, int arg3)
{
    std::string dev, key;
    int delay;
    double value;

    if (ParseNetDev() < 0) {
        return -1;
    }

    dev = arg1.toStdString();
    key = arg2.toStdString();
    delay = arg3;

    value = hash_get_regex(&NetDev, dev.c_str(), key.c_str(), delay);

    return value;
}

QScriptValue PluginNetDev::Fast(QString arg1, QString arg2, int arg3)
{
    std::string dev, key;
    int delay;
    double value;

    if (ParseNetDev() < 0) {
        return -1;
    }

    dev = arg1.toStdString();
    key = arg2.toStdString();
    delay = arg3;

    value = hash_get_delta(&NetDev, dev.c_str(), key.c_str(), delay);

    return QScriptValue(value);
}


PluginNetDev::PluginNetDev()
{
    first_time = 1;
    Stream = NULL;
    hash_create(&NetDev);
    hash_set_delimiter(&NetDev, " :|\t\n");
}

PluginNetDev::~PluginNetDev()
{
    if (Stream != NULL) {
        fclose(Stream);
        Stream = NULL;
    }
    hash_destroy(&NetDev);
}

void PluginNetDev::Connect(Evaluator *visitor) {
    QScriptEngine *engine = visitor->GetEngine();
    QScriptValue val = engine->newObject();
    QScriptValue objVal = engine->newQObject(val, this);
    engine->globalObject().setProperty("netdev", objVal);
}

Q_EXPORT_PLUGIN2(_PluginNetDev, PluginNetDev)