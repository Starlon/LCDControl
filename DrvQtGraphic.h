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

#ifndef __DRV_QT_GRAPHIC_H__
#define __DRV_QT_GRAPHIC_H__

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include "QtGraphicDisplay.h"
#include "LCDControl.h"

namespace LCD {

class LCDCore;

class LCDText;

class DrvQtGraphic : public LCDCore {
    LCDControl *visitor_;
    QtGraphicDisplay *display_;
    QWidget *widget_;
    QVBoxLayout *vbox_;
    QHBoxLayout *hbox_;
    QPushButton *upButton_;
    QPushButton *downButton_;
    QSignalMapper *signalMapper_;
    public:
    DrvQtGraphic(std::string name, LCDControl *, Json::Value *config, 
        int rows, int cols, int layers);
    ~DrvQtGraphic();
};

};

#endif
