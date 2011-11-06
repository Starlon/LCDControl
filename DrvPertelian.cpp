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

#include "DrvPertelian.h"
#include "LCDControl.h"
#include "LCDCore.h"
#include "GenericSerial.h"
#include "LCDText.h"
#include "SpecialChar.h"

using namespace LCD;

/*int round(float n) {
    n+=0.5;
    return floor(n);
}*/

void PertelianBlit(LCDText *obj, int row, int col,
    unsigned char *data, int len) {
    DrvPertelian *self = dynamic_cast<DrvPertelian *>(obj);
    if( col + len > self->LCOLS ) {
        len = len - ((col + len) - self->LCOLS);
    }

    self->SendData(row, col, data, len);
}

void PertelianDefChar(LCDText *obj, const int ascii, SpecialChar matrix) {
    DrvPertelian *self = dynamic_cast<DrvPertelian *>(obj);
    self->SetSpecialChar(ascii, matrix); 
}

DrvPertelian::DrvPertelian(std::string name, LCDControl *v,
    Json::Value *config, int layers) :
    LCDCore(v, name, config, LCD_TEXT, (LCDText *)this),
    LCDText((LCDCore *)this), GenericSerial(name) {

    TextRealBlit = PertelianBlit;
    TextRealDefChar = PertelianDefChar;

    TextInit(4, 20, 8, 5, 0, 8, 0, layers);

    wrapper_ = new PTWrapper((PTInterface *)this);

    command_timer_ = new QTimer();
    command_timer_->setSingleShot(true);
    QObject::connect(command_timer_, SIGNAL(timeout()),
        wrapper_, SLOT(CommandWorker()));

}

DrvPertelian::~DrvPertelian() {
    delete wrapper_;
    delete command_timer_;
}

void DrvPertelian::CommandWorker() {
    if(command_queue_.size() < 1) {
        command_timer_->start(50);
        return;
    }
    PTCallback *callback = command_queue_.front();
    if(!callback) return;
    command_queue_.pop_front();
    (*callback)();
    command_timer_->start(callback->GetTransferRate());
    delete callback;
}

void DrvPertelian::QueueWrite(unsigned char *data, int len, 
    int transfer_rate, bool send_now) {

    class QueueWriteCB : public PTCallback {
        DrvPertelian *visitor_;
        unsigned char *data_;
        int len_;
        public:
        QueueWriteCB(DrvPertelian *v, unsigned char *data, int len, 
            int transfer_rate) : 
            PTCallback(transfer_rate) {
            visitor_ = v;
            data_ = new unsigned char[len];
            int i = len;
            while(--i>=0) data_[i] = data[i];
            //data_ = data ;
            len_ = len;
        }
        ~QueueWriteCB() {
            if(data_) delete data_;
            data_ = NULL;
        }
        void operator()() {
            visitor_->SerialWrite(data_, len_);
        }
    };
    QueueWriteCB *callback = new QueueWriteCB(this, data, len, transfer_rate);
    if(send_now) {
        (*callback)();
        delete callback;
    } else {
        command_queue_.push_back(callback);
    }
}

void DrvPertelian::CFGSetup() {
    Json::Value *val = CFG_Fetch_Raw(CFG_Get_Root(),
        name_ + ".port", new Json::Value(""));
    port_ = val->asString();
    delete val;
    val = CFG_Fetch(CFG_Get_Root(), name_ + ".speed", new Json::Value(19200));
    speed_ = val->asInt();
    delete val;
    val = CFG_Fetch(CFG_Get_Root(), name_ + ".backlight", new Json::Value(100));
    backlight_ = val->asInt();
    delete val;
    LCDCore::CFGSetup();
    transfer_rate_ = round(2/(double)speed_*8000);
    
}

void DrvPertelian::SetupDevice() {
    InitDriver();
    SetBacklight(backlight_);
    ClearLCD();
    command_timer_->start(transfer_rate_);
}

void DrvPertelian::InitDriver() {
    unsigned char buff[1];
    InstructionMode();
    buff[0] = 0x38; //Function set with 8-bit data length, 2 lines, and 5x7 dot size
    QueueWrite(buff, 1, transfer_rate_);
    
    InstructionMode();
    buff[0] = 0x06; // Entry mode set; increment cursor; do not automatically shift
    QueueWrite(buff, 1, transfer_rate_);
    
    InstructionMode();
    buff[0] = 0x10; // Cursor/display shift; cursor move
    QueueWrite(buff, 1, transfer_rate_);
    
    InstructionMode();
    buff[0] = 0x0c; // Display on; cursor off; do not blink
    QueueWrite(buff, 1, transfer_rate_);
}

void DrvPertelian::Connect() {
    SerialOpen(port_, speed_);
}

void DrvPertelian::TakeDown() {
    ClearLCD(true);
}

void DrvPertelian::InstructionMode(bool send_now) {
    unsigned char buff[1];
    buff[0] = PERTELIAN_LCDCOMMAND;
    QueueWrite(buff, 1, transfer_rate_, send_now);
}

void DrvPertelian::ClearLCD(bool send_now) {
    unsigned char buff[1];
    buff[0] = 0x01;

    InstructionMode(send_now);
    QueueWrite(buff, 1, transfer_rate_, send_now);
}

void DrvPertelian::SetBacklight(int val) {
    unsigned char buff[1];
    if( val <= 0) {
        val = 2;
    } else {
        val = 3;
    }

    buff[0] = val;

    InstructionMode();
    QueueWrite(buff, 1, transfer_rate_);
}


void DrvPertelian::SendData(int row, int col, unsigned char *data, int len) {
    unsigned char rowoffset[4] = {0x80, 0xC0, 0x94, 0xD4};
    unsigned char buff[1];
    InstructionMode();
    buff[0] = rowoffset[row] + col;
    QueueWrite(buff, 1, transfer_rate_);
    for(int i = 0; i < len; i++) {
        buff[0] = data[i];
        QueueWrite(buff, 1, transfer_rate_);
    }
}

void DrvPertelian::SetSpecialChar(int num, SpecialChar matrix) {
    unsigned char buff[1];
    InstructionMode();
    buff[0] = 0x40 + 8 * num;
    QueueWrite(buff, 1, transfer_rate_);
    for(int i = 0; i < 8; i++) {
        buff[0] = matrix[i] & 0x1f;
        QueueWrite(buff, 1, transfer_rate_);
    }
}

