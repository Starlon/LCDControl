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

#ifndef __WIDGET_NIFTY__
#define __WIDGET_NIFTY__

#include <vector>
#include <string>
#include <json/json.h>
#include <QObject>
#include <QTimer>

#include <libnifty/libnifty.h>
#include <libnifty/mutex.h>
#include <libnifty/client/remote.h>
#include <libnifty/client/output.h>

#include "Widget.h"
#include "Property.h"
#include "RGBA.h"

namespace LCD {

class LCDText;
class LCDGraphic;

class LCDCore;

class WidgetNifty : public Widget {
    Q_OBJECT
    Property *visible_;
    Json::Value *file_;
    RGBA *pixmap_;
    QTimer *timer_;
    int update_;

    int zoomFac_;
    std::string telnet_host_;
    int telnet_port_;
    nlNifty *nifty_;
    nlRemoteClient *remote_;
    nlRemoteOutput *output_;
    nlCallback *frameCallback_;
    nlMutex *mutex_;

    void (*Draw)(WidgetNifty *w);

    public:
    WidgetNifty(LCDCore *visitor, std::string name, Json::Value *section, 
    int row, int col, int layer);
    ~WidgetNifty();
    void FrameReceived(nlFrame *frame);
    void Start();
    void Stop();
    RGBA *GetPixmap() { return pixmap_; };

    // Slots
    void Update();
    void TextScroll() {}
    
    public slots:
    void Resize(int rows, int cols, int old_rows, int old_cols);
};

}; // End namespace


#endif
