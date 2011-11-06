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

#ifndef __DRV_LCDPROC_H__
#define __DRV_LCDPROC_H__

#include <string>
#include <deque>
#include <QObject>
#include <QTimer>

#include "LCDCore.h"
#include "GenericSerial.h"
#include "LCDText.h"
#include "LCDControl.h"
#include "SpecialChar.h"

#define LCDPROC_LCDCOMMAND 0xFE

namespace LCD {

class LCDProcInterface {
    public:
    virtual ~LCDProcInterface() {}
    virtual void CommandWorker() = 0;
};

class LCDProcWrapper : public QObject, public LCDProcInterface {
    Q_OBJECT

    LCDProcInterface *wrappedObject;

    public:
    LCDProcWrapper(LCDProcInterface *v) {wrappedObject = v;}

    public slots:
    void CommandWorker() { wrappedObject->CommandWorker(); }
};

class LCDProcCallback {
    int transfer_rate_;
    public:
    virtual ~LCDProcCallback() {}
    LCDProcCallback(int transfer_rate) { transfer_rate_ = transfer_rate;}
    virtual void operator()() = 0;
    int GetTransferRate() { return transfer_rate_; }
};

class DrvLCDProc : public LCDCore, public LCDText,
    public GenericSerial, public LCDProcInterface {

    LCDProcWrapper *wrapper_;
    QTimer *command_timer_;
    std::deque<LCDProcCallback *> command_queue_;
    std::string address_;
    int port_;
    int contrast_;
    int backlight_;
    int transfer_rate_;

    void CommandWorker();
    void QueueWrite(unsigned char *data, int len, 
        int transfer_rate, bool send_now = false);

    public:
    DrvLCDProc(std::string name, LCDControl *v, 
        Json::Value *config, int layers);
    ~DrvLCDProc();
    void CFGSetup();
    void SetupDevice();
    void InitDriver();
    void Connect();
    void TakeDown();
    void ClearLCD(bool send_now = false);
    void SetBacklight(int val);
    void SendData(int row, int col, unsigned char *data, int len);
    void SetSpecialChar(int num, SpecialChar matrix);
};

}; // End namespace
#endif
