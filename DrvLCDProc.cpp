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

#include "DrvLCDProc.h"
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

void LCDProcBlit(LCDText *obj, int row, int col,
    unsigned char *data, int len) {
    DrvLCDProc *self = dynamic_cast<DrvLCDProc *>(obj);
    if( col + len > self->LCOLS ) {
        len = len - ((col + len) - self->LCOLS);
    }

    self->SendData(row, col, data, len);
}

void LCDProcDefChar(LCDText *obj, const int ascii, SpecialChar matrix) {
    DrvLCDProc *self = dynamic_cast<DrvLCDProc *>(obj);
    self->SetSpecialChar(ascii, matrix); 
}

DrvLCDProc::DrvLCDProc(std::string name, LCDControl *v,
    Json::Value *config, int layers) :
    LCDCore(v, name, config, LCD_TEXT, (LCDText *)this),
    LCDText((LCDCore *)this), GenericSerial(name) {

    TextRealBlit = LCDProcBlit;
    TextRealDefChar = LCDProcDefChar;

    TextInit(4, 20, 8, 5, 0, 8, 0, layers);

    wrapper_ = new LCDProcWrapper((LCDProcInterface *)this);

    command_timer_ = new QTimer();
    command_timer_->setSingleShot(true);
    QObject::connect(command_timer_, SIGNAL(timeout()),
        wrapper_, SLOT(CommandWorker()));

}

DrvLCDProc::~DrvLCDProc() {
    delete wrapper_;
    delete command_timer_;
}

void DrvLCDProc::CommandWorker() {
    if(command_queue_.size() < 1) {
        command_timer_->start(50);
        return;
    }
    LCDProcCallback *callback = command_queue_.front();
    if(!callback) return;
    command_queue_.pop_front();
    (*callback)();
    command_timer_->start(callback->GetTransferRate());
    delete callback;
}

void DrvLCDProc::QueueWrite(unsigned char *data, int len, 
    int transfer_rate, bool send_now) {

    class QueueWriteCB : public LCDProcCallback {
        DrvLCDProc *visitor_;
        unsigned char *data_;
        int len_;
        public:
        QueueWriteCB(DrvLCDProc *v, unsigned char *data, int len, 
            int transfer_rate) : 
            LCDProcCallback(transfer_rate) {
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

void DrvLCDProc::CFGSetup() {
    Json::Value *val = CFG_Fetch_Raw(CFG_Get_Root(), name_ + ".port", new Json::Value(12666));
    port_ = val->asInt();
    delete val;
    val = CFG_Fetch(CFG_Get_Root(), name_ + ".address", new Json::Value("localhost"));
    address_ = val->asInt();
    delete val;
    LCDCore::CFGSetup();
    transfer_rate_ = 100;
}

void DrvLCDProc::SetupDevice() {
    InitDriver();
    SetBacklight(backlight_);
    ClearLCD();
    command_timer_->start(transfer_rate_);
}

void DrvLCDProc::InitDriver() {
    std::string out = "hello";
    QueueWrite((unsigned char*)out.c_str(), 5, transfer_rate_);
}

void DrvLCDProc::Connect() {
}

void DrvLCDProc::TakeDown() {
    ClearLCD(true);
}

void DrvLCDProc::ClearLCD(bool send_now) {
    unsigned char buff[1];
    buff[0] = 0x01;

    QueueWrite(buff, 1, transfer_rate_, send_now);
}

void DrvLCDProc::SetBacklight(int val) {
    unsigned char buff[1];
    if( val <= 0) {
        val = 2;
    } else {
        val = 3;
    }

    buff[0] = val;

    QueueWrite(buff, 1, transfer_rate_);
}


void DrvLCDProc::SendData(int row, int col, unsigned char *data, int len) {
    unsigned char rowoffset[4] = {0x80, 0xC0, 0x94, 0xD4};
    unsigned char buff[1];
    buff[0] = rowoffset[row] + col;
    QueueWrite(buff, 1, transfer_rate_);
    for(int i = 0; i < len; i++) {
        buff[0] = data[i];
        QueueWrite(buff, 1, transfer_rate_);
    }
}

void DrvLCDProc::SetSpecialChar(int num, SpecialChar matrix) {
    unsigned char buff[1];
    buff[0] = 0x40 + 8 * num;
    QueueWrite(buff, 1, transfer_rate_);
    for(int i = 0; i < 8; i++) {
        buff[0] = matrix[i] & 0x1f;
        QueueWrite(buff, 1, transfer_rate_);
    }
}

