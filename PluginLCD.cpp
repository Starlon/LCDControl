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

#include <string>
#include <vector>
#include <iostream>

#include "PluginLCD.h"
#include "LCDCore.h"
#include "RGBA.h"
#include "SpecialChar.h"
#include "Evaluator.h"
#include "debug.h"

using namespace LCD;
using namespace std;

void PluginLCD::TickUpdate() {
    emit _TickUpdate();
}

void PluginLCD::Transition(int i) {
    visitor_->Transition(i);
}

void PluginLCD::SelectLayout(QString layout) {
    if(type_ == LCD_TEXT) 
        visitor_->SelectLayout(layout.toStdString());
    else if(type_ == LCD_GRAPHIC)
        visitor_->SelectLayout(layout.toStdString());
}

int PluginLCD::RemoveWidget(QString widget) {
    return visitor_->RemoveWidget(widget.toStdString());
}

QString PluginLCD::AddWidget(QString layout, 
    int row, int col, int layer, QString object) {
    return QString(visitor_->AddWidget(layout.toStdString(), row, col, 
        layer, object.toStdString()).c_str());
}

int PluginLCD::MoveWidget(QString widget, int rows, int cols) {
    return visitor_->MoveWidget(widget.toStdString(), rows, cols);
}

int PluginLCD::ResizeLCD(int rows, int cols) {
    return visitor_->ResizeLCD(rows, cols);
}

void PluginLCD::SendData(int row, int col, 
    int layer, QString val) {
    QByteArray data = val.toAscii();
    if(type_ == LCD_TEXT) {
        LCDText *lcd = (LCDText *)visitor_->GetLCD();
        if(layer >= lcd->LAYERS) {
            LCDError("PluginLCD::SendData: Invalid layer");
            return;
        }
        unsigned char *FB = lcd->LayoutFB[layer];
        int cols = lcd->LCOLS;
        int rows = lcd->LROWS;
        if( row >= rows )
            return;
        for(int i = 0; i < (int)data.size() && 
            i + col < cols; i++) {
            FB[row * cols + col + i] = data[i];
        }
        lcd->TextBlit(row, col, 1, data.size());
    } else if(type_ == LCD_GRAPHIC) {
	LCDGraphic *lcd = (LCDGraphic *)visitor_->GetLCD();
	RGBA fg_(0, 0, 0);
	RGBA bg_(255, 255, 255);
	row*=lcd->YRES;
	col*=lcd->XRES;
	if(layer >= lcd->LAYERS) {
		LCDError("PluginLCD::SendData: Invalid layer");
		return;
	}
	lcd->GraphicRender(layer, row, col, fg_, bg_, val.toStdString().c_str(), 0, 0, GetCurrentLayout().toStdString().c_str());
	lcd->GraphicBlit(0, 0, lcd->LROWS, lcd->LCOLS); 
    }
}

void PluginLCD::SetSpecialChar(int ch, SpecialChar matrix) {
    if(type_ == LCD_TEXT) {
        LCDText *lcd = (LCDText *)visitor_->GetLCD();
        lcd->special_chars[ch] = matrix;
        lcd->TextSpecialCharChanged(ch);
    }
}

void PluginLCD::AddSpecialChar(SpecialChar matrix) {
    if(type_ == LCD_TEXT) {
        LCDText *lcd = (LCDText *)visitor_->GetLCD();
        int size = lcd->special_chars.size();
        if(size < lcd->CHARS) {
            lcd->special_chars.push_back(
                SpecialChar(matrix.Size()));
            SetSpecialChar(size, matrix);
        }
    }
}

void PluginLCD::Clear() {
    if(type_ == LCD_TEXT)
        ((LCDText *)visitor_->GetLCD())->TextClear();
    else if(type_ == LCD_GRAPHIC)
        ((LCDGraphic *)visitor_->GetLCD())->GraphicClear();
}

void PluginLCD::ClearChars() {
    if(type_ == LCD_TEXT)
        ((LCDText *)visitor_->GetLCD())->TextClearChars();
}

QString PluginLCD::GetCurrentLayout() {
    return visitor_->GetCurrentLayout().c_str();
}

int PluginLCD::GetRows() {
    if (type_ == LCD_TEXT) {
        return visitor_->GetLCD()->DROWS;
    } else {
        return visitor_->GetLCD()->LROWS / visitor_->GetLCD()->YRES;
    }
}

int PluginLCD::GetCols() {
    if (type_ == LCD_TEXT) {
    	return visitor_->GetLCD()->DCOLS;
    } else {
    	return visitor_->GetLCD()->LCOLS / visitor_->GetLCD()->XRES;
    }
}

int PluginLCD::GetXres() {
    return visitor_->GetLCD()->XRES;
}

int PluginLCD::GetYres() {
    return visitor_->GetLCD()->YRES;
}

QString PluginLCD::GetType() {
	if ( type_ == LCD_TEXT ) {
		return "text";
	} else if (type_ == LCD_GRAPHIC) {
		return "graphic";
	}
	return "none";
}

void PluginLCD::SetTimeout(int val) {
    tick_timer_->setInterval(val);
    tick_timer_->start();
}

PluginLCD::PluginLCD(LCDCore *visitor) {
    visitor_ = visitor;
    type_ = visitor_->GetType();

    QObject::connect(visitor_->GetWrapper(), SIGNAL(_KeypadEvent(const int)),
        this, SIGNAL(_KeypadEvent(const int)));

    tick_timer_ = new QTimer();
    tick_timer_->setInterval(100);
    QObject::connect(tick_timer_, SIGNAL(timeout()),
        this, SLOT(TickUpdate()));
    tick_timer_->start();

}

PluginLCD::~PluginLCD() {
    delete tick_timer_;
}

