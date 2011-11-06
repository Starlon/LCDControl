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

#ifndef __DRV_PERTELIAN_H__
#define __DRV_PERTELIAN_H__

#include <string>
#include <deque>
#include <QObject>
#include <QTimer>

#include "LCDCore.h"
#include "GenericSerial.h"
#include "LCDText.h"
#include "LCDControl.h"
#include "SpecialChar.h"

#define PERTELIAN_LCDCOMMAND 0xFE

namespace LCD {

class PTInterface {
    public:
    virtual ~PTInterface() {}
    virtual void CommandWorker() = 0;
};

class PTWrapper : public QObject, public PTInterface {
    Q_OBJECT

    PTInterface *wrappedObject;

    public:
    PTWrapper(PTInterface *v) {wrappedObject = v;}

    public slots:
    void CommandWorker() { wrappedObject->CommandWorker(); }
};

class PTCallback {
    int transfer_rate_;
    public:
    virtual ~PTCallback() {}
    PTCallback(int transfer_rate) { transfer_rate_ = transfer_rate;}
    virtual void operator()() = 0;
    int GetTransferRate() { return transfer_rate_; }
};

class DrvPertelian : public LCDCore, public LCDText,
    public GenericSerial, public PTInterface {

    PTWrapper *wrapper_;
    QTimer *command_timer_;
    std::deque<PTCallback *> command_queue_;
    std::string port_;
    int speed_;
    int contrast_;
    int backlight_;
    int transfer_rate_;

    void CommandWorker();
    void QueueWrite(unsigned char *data, int len, 
        int transfer_rate, bool send_now = false);

    public:
    DrvPertelian(std::string name, LCDControl *v, 
        Json::Value *config, int layers);
    ~DrvPertelian();
    void CFGSetup();
    void SetupDevice();
    void InitDriver();
    void Connect();
    void TakeDown();
    void InstructionMode(bool send_now = false);
    void ClearLCD(bool send_now = false);
    void SetBacklight(int val);
    void SendData(int row, int col, unsigned char *data, int len);
    void SetSpecialChar(int num, SpecialChar matrix);
};

}; // End namespace
#endif
