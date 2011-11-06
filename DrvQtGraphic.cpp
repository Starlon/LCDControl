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

#include "LCDControl.h"
#include <iostream>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include "DrvQtGraphic.h"
#include "LCDCore.h"
#include "Property.h"

#define BUTTON_WIDTH 60

using namespace LCD;

DrvQtGraphic::DrvQtGraphic(std::string name, LCDControl *visitor, 
    Json::Value *config, int rows, int cols, int layers) : 
    LCDCore(visitor, name, config, LCD_GRAPHIC) {

    if(rows == 0)
        rows = 64;
    if(cols == 0)
        cols = 256;

    display_ = new QtGraphicDisplay((LCDCore *)this, 
        name, config, rows, cols, 8, 6, layers);
    std::cout << "DrvQtGraphic" << ::std::endl;
    lcd_ = display_;
    visitor_ = visitor;
    widget_ = new QWidget();
    vbox_ = new QVBoxLayout();
    hbox_ = new QHBoxLayout();
    upButton_ = new QPushButton("Up");
    downButton_ = new QPushButton("Down");
    upButton_->setMaximumSize(BUTTON_WIDTH, (rows + display_->GetBorder()) / 2);
    downButton_->setMaximumSize(BUTTON_WIDTH, (rows + display_->GetBorder()) / 2);
    signalMapper_ = new QSignalMapper(widget_);
    signalMapper_->setMapping(upButton_, 1);
    signalMapper_->setMapping(downButton_, 2);
    QObject::connect(upButton_, SIGNAL(clicked()), signalMapper_, SLOT(map()));
    QObject::connect(downButton_, SIGNAL(clicked()), signalMapper_, SLOT(map()));
    QObject::connect(signalMapper_, SIGNAL(mapped(const int &)), GetWrapper(), 
        SIGNAL(_KeypadEvent(const int &)));
    hbox_->addWidget(display_);
    vbox_->addWidget(upButton_);
    vbox_->addWidget(downButton_);
    hbox_->addLayout(vbox_);
    hbox_->setContentsMargins(0, 0, 0, 0);
    widget_->setLayout(hbox_);
    widget_->show();
    widget_->setMinimumSize(cols * display_->XPixels() + 
        display_->GetBorder() * 3 + BUTTON_WIDTH, 
        rows * display_->YPixels() + display_->GetBorder() * 2);
/*
    std::cout << "Eval: " << Eval("uptime.Uptime('%H')").toString().toStdString() << std::endl;
    std::cout << "Eval: " << Eval("uptime.Uptime('%s')").toString().toStdString() << std::endl;
    std::cout << "Eval: " << Eval("netdev.Fast('wlan0', 'Tx_bytes', 0) / 1024.0").toString().toStdString() << std::endl;
    std::cout << "Eval: " << Eval("netdev.Fast('wlan0', 'Rx_bytes', 0) / 1024.0").toString().toStdString() << std::endl;
    std::cout << "Eval: " << Eval("name").toString().toStdString() << std::endl;
    Property test = Property(CFG_Get_Root(), CFG_Get_Root(), "widget_wlan0_bar.expression", NULL);
    test.Parse();
    std::cout << "Parse: " << test.Valid() << ", " << test.P2N() << std::endl;
*/
}

DrvQtGraphic::~DrvQtGraphic() {
    delete vbox_;
    delete hbox_;
    delete upButton_;
    delete downButton_;
    delete signalMapper_;
    delete display_;
    delete widget_;
    //delete window;
}
