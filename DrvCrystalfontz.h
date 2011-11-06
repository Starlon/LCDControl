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

#ifndef __DRV_CRYSTALFONTZ__
#define __DRV_CRYSTALFONTZ__

#include <vector>
#include <deque>
#include <string>
#include <QObject>
#include <QTimer>
#include <sys/time.h>

#include "BufferedReader.h"
#include "GenericSerial.h"
#include "LCDCore.h"
#include "LCDControl.h"
#include "LCDText.h"

#define MAX_DATA_SIZE 22
#define MAX_COMMAND 36

namespace LCD {

class CRC {
    unsigned char crc_[2];

    public:
    int AsWord() { return crc_[0] + crc_[1] * 256; }
    unsigned char &operator[] (int index);
};

class Packet {
    std::vector<unsigned char> data_;
    int command_;
    int data_length_;
    CRC crc_;

    public:
    Packet() {}
    Packet(int command, unsigned char *data, int length) {
        SetData(data, length);
        command_ = command;
    }
    unsigned char *GetData() { 
        unsigned char *data = new unsigned char[data_length_];
        for(int i = 0; i < data_length_; i++) {
            data[i] = data_[i];
        }
        return data; 
    }
    void SetData(unsigned char *data, int len) { 
        data_.clear();
        for(int i = 0; i < len; i++)
            data_.push_back(data[i]);
        data_length_ = len;
    }
    int GetCommand() { return command_; }
    void SetCommand(int cmd) { command_ = cmd; }
    int GetDataLength() { return data_length_; }
    void SetDataLength(int len) { data_length_ = len; }
    CRC &GetCrc() { return crc_; }
};

class CFInterface {
    public:
    virtual ~CFInterface() {}
    virtual void PacketReady(Packet *) = 0;
    virtual void KeyPacketReady(Packet *) = 0;
    virtual void FanPacketReady(Packet *) = 0;
    virtual void TempPacketReady(Packet *) = 0;
    virtual void Update() = 0;
    virtual void CommandWorker() = 0;
    virtual void CheckPacket() = 0;
    virtual void FillBuffer() = 0;
    virtual void BinWatch() = 0;
};

class CFEvents {
    public:
    virtual void _PacketReady(Packet *) = 0;
    virtual void _KeyPacketReady(Packet *) = 0;
    virtual void _FanPacketReady(Packet *) = 0;
    virtual void _TempPacketReady(Packet *) = 0;
};

class CFWrapper : public QObject, public CFInterface, public CFEvents {
    Q_OBJECT

    CFInterface *wrappedObject;

    public:
    CFWrapper(CFInterface *v, QObject *parent);

    public slots:
    void PacketReady(Packet *pkt) { wrappedObject->PacketReady(pkt); }
    void KeyPacketReady(Packet *pkt) { wrappedObject->KeyPacketReady(pkt); }
    void FanPacketReady(Packet *pkt) { wrappedObject->FanPacketReady(pkt); }
    void TempPacketReady(Packet *pkt) { wrappedObject->TempPacketReady(pkt); }
    void Update() { wrappedObject->Update(); }
    void CommandWorker() { wrappedObject->CommandWorker(); }
    void CheckPacket() { wrappedObject->CheckPacket(); }
    void FillBuffer() { wrappedObject->FillBuffer(); }
    void BinWatch() { wrappedObject->BinWatch(); }

    signals:
    void _PacketReady(Packet *);
    void _KeyPacketReady(Packet *);
    void _FanPacketReady(Packet *);
    void _TempPacketReady(Packet *);

};

class CFCallback {
    int command_;
    int transfer_rate_;
    public:
    CFCallback(int command, int transfer_rate = 0) { 
        command_ = command; transfer_rate_ = transfer_rate; }
    virtual ~CFCallback() {}
    virtual void operator()(void *data) = 0;
    int GetCommand() { return command_; }
    int GetTransferRate() { return transfer_rate_; }
};

class Command {
    unsigned char *data_;
    int len_;
    int command_;
    public:
    Command(int cmd, unsigned char *d = NULL, int len = 0) { 
        command_ = cmd; data_ = d; len_ = len; 
    }
    Command(unsigned char *d, int len) {
        data_ = d; len_ = len;
    }
    ~Command() { 
        if(data_)
            free(data_); 
    }
    unsigned char *GetData() { return data_; }
    int GetDataLength() { return len_; }
    int GetCommand() { return command_; }
    int GetLength() { return len_; }
    bool operator==(Command &cmd) {
        if(cmd.GetCommand() == command_ &&
            cmd.GetLength() == len_) {
            char first[len_ + 1];
            char second[len_ + 1];
            first[len_] = 0;
            second[len_] = 0;
            memcpy(first, cmd.GetData(), len_);
            memcpy(second, data_, len_);
            if(strcmp(first, second) == 0)
                return true;
        }
        return false;
    }
    bool operator!=(Command &cmd) {
        return !(*this == cmd);
    }
};

class Model {
    std::string name_;
    int rows_;
    int cols_;
    int gpis_;
    int gpos_;
    int protocol_;
    int payload_;
    public:
    Model() {}
    Model(std::string name, int rows, int cols, int gpis, int gpos, 
        int protocol, int payload) {
        name_ = name;
        rows_ = rows;
        cols_ = cols;
        gpis_ = gpis;
        gpos_ = gpos;
        protocol_ = protocol;
        payload_ = payload;
    }
    std::string GetName() { return name_; }
    int GetRows() { return rows_; }
    int GetCols() { return cols_; }
    int GetGPIS() { return gpis_; }
    int GetGPOS() { return gpos_; }
    int GetProtocol() { return protocol_; }
    int GetPayload() { return payload_; }
    void SetName(std::string name) { name_ = name; }
};

class Bin {
    Command *command_;
    CFCallback *callback_;
    int priority_;
    bool send_now_;
    struct timeval time_;

    public:
    Bin(Command *cmd, CFCallback *cb, int priority, bool send_now) {
        command_ = cmd;
        callback_ = cb;
        priority_ = priority;
        send_now_ = send_now;
        gettimeofday(&time_, NULL);;
    }
    Command *GetCommand() { return command_; }
    CFCallback *GetCallback() { return callback_; }
    int GetPriority() { return priority_; }
    bool GetSendNow() { return send_now_; }
    struct timeval GetTime() { return time_; }
    bool operator==(Bin &bin) {
        if(*bin.GetCommand() == *command_ &&
            bin.GetPriority() == priority_ &&
            bin.GetSendNow() == send_now_ &&
            bin.GetTime().tv_sec == time_.tv_sec &&
            bin.GetTime().tv_usec == time_.tv_usec)
            return true;
        return false;
    }
    bool operator!=(Bin &bin) {
        return !(*this == bin);
    }
};

class CFPacketVersion: public virtual LCDCore, public GenericSerial,
    public LCDText, public CFInterface {
    CFWrapper *wrapper_;
    QTimer *update_timer_;
    QTimer *command_timer_;
    QTimer *check_timer_;
    QTimer *fill_timer_;
    QTimer *watch_timer_;
    int transfer_rate_;
    int commands_sent_;
    int commands_received_;
    int commands_resent_;
    int tossed_;
    int errors_;
    Packet packet_;
    BufferedReader buffer_;
    std::string port_;
    int speed_;
    bool scab_;
    std::string hardware_version_;
    std::string firmware_version_;
    int current_command_;
    int contrast_;
    int backlight_;
    int response_state_;
    int response_timeout_;
    bool active_;
    int layout_timeout_;
    int command_limit_;
    std::deque<CFCallback *> command_queue1_;
    std::deque<CFCallback *> command_queue2_;
    std::map<int, std::vector<Bin *> > bin_dict_;
    bool bin_locked_;
    std::vector<Bin *> bin_resent_commands_;
    std::vector<int> bin_current_memory_;

    protected:
    Model *model_;
    char fans_[4];
    void TakeDown();
    void InitiateVersion();
    void InitiateContrastBacklight();
    void SendCommand( Command *cmd, CFCallback *cb = NULL, 
        int priority = 0, bool send_now = false );
    void PacketReady(Packet *);
    void KeyPacketReady(Packet *);
    void FanPacketReady(Packet *);
    void TempPacketReady(Packet *);
    void Update();
    void CommandWorker();
    void CheckPacket();
    void FillBuffer();
    void BinWatch();
    void ReadPacket(Packet *packet);
    void BinAddCurrentAddress(int address);
    void BinDelCurrentAddress(int address);
    void BinProcessPacket(Packet *pkt);

    public:

    CFPacketVersion(std::string name, LCDControl *v, Json::Value *config, 
        Model *model, bool scab, int layers);
    ~CFPacketVersion();
    void CFGSetup();
    int GetCrc(Command *command);
    int GetCrc(Packet pkt);
    void SetupDevice();
    void Connect();
    Model *GetModel() { return model_; }
    void AddResponseState() { response_state_++; }
    void BinAdd(Command *cmd, CFCallback *cb, int priority, bool send_now);
    void SetHardwareVersion(std::string version) {hardware_version_ = version;}
    void SetFirmwareVersion(std::string version) {firmware_version_ = version;}
    virtual void Ping(std::string msg, CFCallback *cb) = 0; // 0
    virtual void GetVersion(CFCallback *cb) = 0; // 1
    //virtual void WriteUserFlash() = 0; // 2
    //virtual void ReadUserFlash(CFCallback *cb) = 0; // 3
    //virtual void SaveAsBootState() = 0; // 4
    virtual void Reboot(CFCallback *cb=NULL, bool send_now=false) = 0; // 5
    //virtual void ResetHost() = 0; // 5
    //virtual void TurnOffHost() = 0; // 5
    virtual void ClearLCD() = 0; // 6
    //virtual void Line1() = 0; // 7
    //virtual void Line2() = 0; // 8
    virtual void SetSpecialChar(int num, SpecialChar ch) = 0; //  9
    virtual void SetLCDCursorStyle(int style=0, CFCallback *cb=NULL) = 0; // 12
    virtual void SetLCDContrast(int level) = 0; // 13
    virtual void SetLCDBacklight(int level) = 0; // 14
    //virtual void ReadFans(CFCallback *cb) = 0; // 15
    //virtual void SetFanReporting() = 0; // 16
    virtual void SetFanPower(int num, int val) = 0; // 17
    //virtual void ReadDOWInformation(int index, CFCallback *cb) = 0; // 18
    //virtual void SetTempReporting(int index) = 0; // 19
    //virtual void DOWTransaction() = 0; // 20
    //virtual void SetLiveDisplay() = 0; // 21
    //virtual void DirectLCDCommand() = 0; // 22
    //virtual void SetKeyEventReporting() = 0; // 23
    //virtual void ReadKeypadPolledMode() = 0; // 24
    //virtual void SetFanFailSafe(int fan, int timeout) = 0; // 25
    //virtual void SetFanRPMGlitchFilter(int, int, int, int) = 0; // 26
    //virtual void ReadFanPwrFailSafe(CFCallback *cb) = 0; // 27
    //virtual void SetATXSwitch() = 0; // 28
    //virtual void WatchdogHostReset() = 0; // 29
    //virtual void ReadReport() = 0; // 30
    virtual void SendData(int row, int col, unsigned char *buf, int len, 
        bool send_now = false) = 0; // 31
    //virtual void KeyLegends() = 0; // 32
    //virtual void SetBaudRate() = 0; // 33
    virtual void SetGPIO(int num, int val) = 0; // 34
    //virtual void ReadGPIO() = 0; // 35

};

class DrvCrystalfontz {

    public:
    static LCDCore *Get(std::string name, LCDControl *v, 
        Json::Value *config, std::string model, int layer);
};

class Protocol1 : public LCDCore, public LCDText, 
    public GenericSerial, public CFInterface  {
    CFWrapper *wrapper_;
    Model *model_;
    QTimer *command_timer_;
    struct timeval update_time_;
    std::deque<CFCallback *> command_queue_;
    std::string port_;
    int speed_;
    int contrast_;
    int backlight_;
    int transfer_rate_;

    void CommandWorker();
    void ConvertToCgrom2(unsigned char *data, int len);
    void SetCursorPos(int row, int col);
    void QueueWrite(Command *data, int transfer_rate, bool send_now = false);
    void SetLCDContrast(int level);
    void SetLCDBacklight(int level);
    void ClearLCD(bool send_now = false);
    void Reboot(bool send_now = false);
    void SetupDevice();
    void TakeDown();
    void CFGSetup();
    void InitiateContrastBacklight();

    void PacketReady(Packet*) {}
    void KeyPacketReady(Packet*) {}
    void FanPacketReady(Packet*) {}
    void TempPacketReady(Packet*) {}
    void CheckPacket() {}
    void FillBuffer() {}
    void BinWatch() {}
    void Update() {}
    

    public:
    Protocol1(std::string name, Model *model, LCDControl *v, 
        Json::Value *config, int layers);
    ~Protocol1();
    void SendData(int row, int col, unsigned char *data, int len);
    void SetSpecialChar(int num, SpecialChar ch);
    void Connect();
};

class Protocol2 : public CFPacketVersion, public virtual LCDCore {

    public:
    Protocol2(std::string name, Model *model, LCDControl *v, 
        Json::Value *config, int layers);
    ~Protocol2();
    void Ping(std::string msg, CFCallback *cb);
    void GetVersion(CFCallback *cb);
    void Reboot(CFCallback *cb = NULL, bool send_now = false);
    void ClearLCD();
    void SendData(int row, int col, unsigned char *data, 
        int len, bool send_now=false);
    void SetSpecialChar(int num, SpecialChar ch);
    void SetLCDCursorStyle(int style=0, CFCallback *cb=NULL);
    void SetLCDContrast(int level);
    void SetLCDBacklight(int level);
    void SetFanPower(int num, int val);
    void SetGPIO(int num, int val);
};

class Protocol3: public CFPacketVersion, public virtual LCDCore {

    public:
    Protocol3(std::string name, Model *model, LCDControl *v, 
        Json::Value *config, bool scab, int layers);
    ~Protocol3();
    void Ping(std::string msg, CFCallback *cb);
    void GetVersion(CFCallback *cb);
    void Reboot(CFCallback *cb = NULL, bool send_now = false);
    void ClearLCD();
    void SendData(int row, int col, unsigned char *data, 
        int len, bool send_now=false);
    void SetSpecialChar(int num, SpecialChar ch);
    void SetLCDCursorStyle(int style=0, CFCallback *cb=NULL);
    void SetLCDContrast(int level);
    void SetLCDBacklight(int level);
    void SetFanPower(int num, int val);
    void SetGPIO(int num, int val);
};

void CrystalfontzBlit(LCDText *obj, int row, int col,
    char *data, int len);
void CrystalfontzDefChar(LCDText *obj, const int ascii, SpecialChar matrix);
};

#endif
