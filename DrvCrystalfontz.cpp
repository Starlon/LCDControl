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


#include <iostream>
#include <vector>
#include <sys/time.h>
#include <queue>
#include <QObject>
#include <boost/regex.hpp>
#include <cmath>

#include "LCDControl.h"
#include "DrvCrystalfontz.h"
#include "BufferedReader.h"
#include "LCDCore.h"
#include "GenericSerial.h"
#include "LCDText.h"

using namespace LCD;

const char *command_names[36] = {
    " 0 = Ping",
    " 1 = Read Version",
    " 2 = Write Flash",
    " 3 = Read Flash",
    " 4 = Store Boot State",
    " 5 = Reboot",
    " 6 = Clear LCD",
    " 7 = LCD Line 1",
    " 8 = LCD Line 2",
    " 9 = LCD CGRAM",
    "10 = Read LCD Memory",
    "11 = Place Cursor",
    "12 = Set Cursor Style",
    "13 = Contrast",
    "14 = Backlight",
    "15 = Read Fans",
    "16 = Set Fan Rpt.",
    "17 = Set Fan Power",
    "18 = Read DOW ID",
    "19 = Set Temp. Rpt",
    "20 = DOW Transaction",
    "21 = Set Live Display",
    "22 = Direct LCD Command",
    "23 = Set Key Event Reporting",
    "24 = Read Keypad, Polled Mode",
    "25 = Set Fan Fail-Safe",
    "26 = Set Fan RPM Glitch Filter",
    "27 = Read Fan Pwr & Fail-Safe",
    "28 = Set ATX switch functionality",
    "29 = Watchdog Host Reset",
    "30 = Rd Rpt",
    "31 = Send Data to LCD (row,col)",
    "32 = Key Legends",
    "33 = Set Baud Rate",
    "34 = Set/Configure GPIO",
    "35 = Read GPIO & Configuration"};

    /* No command for Protocol1 since Command.command_ isn't used */

#define NO_CMD -1

    /* Packet version command codes */

#define TYPE_PING                 0x00
#define TYPE_GET_VERSION_INFO     0x01
#define TYPE_WRITE_FLASH_AREA     0x02
#define TYPE_READ_FLASH_AREA      0x03
#define TYPE_STORE_AS_BOOT        0x04
#define TYPE_POWER_CONTROL        0x05
#define TYPE_CLEAR_SCREEN         0x06
    /*  DEPRECATED                0x07 */
    /*  DEPRECATED                0x08 */
#define TYPE_SET_CHAR_DATA        0x09
#define TYPE_READ_LCD_MEMORY      0x0A
#define TYPE_SET_CURSOR_POS       0x0B
#define TYPE_SET_CURSOR_STYLE     0x0C
#define TYPE_SET_LCD_CONTRAST     0x0D
#define TYPE_SET_BACKLIGHT        0x0E
    /*  DEPRECATED                0x0F */
#define TYPE_SET_FAN_REPORTING    0x10
#define TYPE_SET_FAN_POWER        0x11
#define TYPE_READ_DOW_DEVICE      0x12
#define TYPE_SET_TEMP_REPORTING   0x13
#define TYPE_DOW_TRANSACTION      0x14
    /*  DEPRECATED                0x15 */
#define TYPE_DIRECT_LCD_CMD       0x16
#define TYPE_SET_KEY_REPORTING    0x17
#define TYPE_READ_KEY_STATE       0x18
#define TYPE_SET_FAN_FAILSAFE     0x19
#define TYPE_SET_FAN_TACH_GLITCH  0x1A
#define TYPE_GET_FAN_FAILSAFE     0x1B
#define TYPE_SET_ATX_POWER_SW     0x1C
#define TYPE_MANAGE_WATCHDOG      0x1D
#define TYPE_REPORTING_STATUS     0x1E
#define TYPE_WRITE_DATA           0x1F
    /*  RESERVED                  0x20 */
#define TYPE_SET_BAUD_RATE        0x21
#define TYPE_SET_GPIO             0x22
#define TYPE_GET_GPIO             0x23

    /* =============================================================== */
    /* Types of packets which can be received from the display          */
    /* =============================================================== */

#define TYPE_RPT_KEY_ACTIVITY     0x80
#define TYPE_RPT_FAN_SPEED        0x81
#define TYPE_RPT_TEMP_SENSOR      0x82

const int crcLookupTable[256] = {
    0x00000,0x01189,0x02312,0x0329B,0x04624,0x057AD,0x06536,0x074BF,
    0x08C48,0x09DC1,0x0AF5A,0x0BED3,0x0CA6C,0x0DBE5,0x0E97E,0x0F8F7,
    0x01081,0x00108,0x03393,0x0221A,0x056A5,0x0472C,0x075B7,0x0643E,
    0x09CC9,0x08D40,0x0BFDB,0x0AE52,0x0DAED,0x0CB64,0x0F9FF,0x0E876,
    0x02102,0x0308B,0x00210,0x01399,0x06726,0x076AF,0x04434,0x055BD,
    0x0AD4A,0x0BCC3,0x08E58,0x09FD1,0x0EB6E,0x0FAE7,0x0C87C,0x0D9F5,
    0x03183,0x0200A,0x01291,0x00318,0x077A7,0x0662E,0x054B5,0x0453C,
    0x0BDCB,0x0AC42,0x09ED9,0x08F50,0x0FBEF,0x0EA66,0x0D8FD,0x0C974,
    0x04204,0x0538D,0x06116,0x0709F,0x00420,0x015A9,0x02732,0x036BB,
    0x0CE4C,0x0DFC5,0x0ED5E,0x0FCD7,0x08868,0x099E1,0x0AB7A,0x0BAF3,
    0x05285,0x0430C,0x07197,0x0601E,0x014A1,0x00528,0x037B3,0x0263A,
    0x0DECD,0x0CF44,0x0FDDF,0x0EC56,0x098E9,0x08960,0x0BBFB,0x0AA72,
    0x06306,0x0728F,0x04014,0x0519D,0x02522,0x034AB,0x00630,0x017B9,
    0x0EF4E,0x0FEC7,0x0CC5C,0x0DDD5,0x0A96A,0x0B8E3,0x08A78,0x09BF1,
    0x07387,0x0620E,0x05095,0x0411C,0x035A3,0x0242A,0x016B1,0x00738,
    0x0FFCF,0x0EE46,0x0DCDD,0x0CD54,0x0B9EB,0x0A862,0x09AF9,0x08B70,
    0x08408,0x09581,0x0A71A,0x0B693,0x0C22C,0x0D3A5,0x0E13E,0x0F0B7,
    0x00840,0x019C9,0x02B52,0x03ADB,0x04E64,0x05FED,0x06D76,0x07CFF,
    0x09489,0x08500,0x0B79B,0x0A612,0x0D2AD,0x0C324,0x0F1BF,0x0E036,
    0x018C1,0x00948,0x03BD3,0x02A5A,0x05EE5,0x04F6C,0x07DF7,0x06C7E,
    0x0A50A,0x0B483,0x08618,0x09791,0x0E32E,0x0F2A7,0x0C03C,0x0D1B5,
    0x02942,0x038CB,0x00A50,0x01BD9,0x06F66,0x07EEF,0x04C74,0x05DFD,
    0x0B58B,0x0A402,0x09699,0x08710,0x0F3AF,0x0E226,0x0D0BD,0x0C134,
    0x039C3,0x0284A,0x01AD1,0x00B58,0x07FE7,0x06E6E,0x05CF5,0x04D7C,
    0x0C60C,0x0D785,0x0E51E,0x0F497,0x08028,0x091A1,0x0A33A,0x0B2B3,
    0x04A44,0x05BCD,0x06956,0x078DF,0x00C60,0x01DE9,0x02F72,0x03EFB,
    0x0D68D,0x0C704,0x0F59F,0x0E416,0x090A9,0x08120,0x0B3BB,0x0A232,
    0x05AC5,0x04B4C,0x079D7,0x0685E,0x01CE1,0x00D68,0x03FF3,0x02E7A,
    0x0E70E,0x0F687,0x0C41C,0x0D595,0x0A12A,0x0B0A3,0x08238,0x093B1,
    0x06B46,0x07ACF,0x04854,0x059DD,0x02D62,0x03CEB,0x00E70,0x01FF9,
    0x0F78F,0x0E606,0x0D49D,0x0C514,0x0B1AB,0x0A022,0x092B9,0x08330,
    0x07BC7,0x06A4E,0x058D5,0x0495C,0x03DE3,0x02C6A,0x01EF1,0x00F78};

int round(float n) {
    n+=0.5;
    return floor(n);
}

void CrystalfontzBlit1(LCDText *obj, int row, int col,
    unsigned char *data, int len) {
    Protocol1 *self = dynamic_cast<Protocol1 *>(obj);
    if( col + len > self->LCOLS ) {
        len = len - ((col + len) - self->LCOLS);
    }

    self->SendData(row, col, data, len);
}

void CrystalfontzDefChar1(LCDText *obj, const int ascii, SpecialChar matrix) {
    Protocol1 *self = dynamic_cast<Protocol1 *>(obj);
    self->SetSpecialChar(ascii, matrix);
}

void CrystalfontzBlit2(LCDText *obj, int row, int col,
    unsigned char *data, int len) {
    CFPacketVersion *self = dynamic_cast<CFPacketVersion *>(obj);
    if( col + len > self->LCOLS ) {
        len = len - ((col + len) - self->LCOLS);
    }

    self->SendData(row, col, data, len);
}

void CrystalfontzDefChar2(LCDText *obj, const int ascii, SpecialChar matrix) {
    CFPacketVersion *self = dynamic_cast<CFPacketVersion *>(obj);
    self->SetSpecialChar(ascii, matrix);
}

unsigned char &CRC::operator[] (int index) {
    if(index > 1) {
        LCDError("CRC: Subscript invalid.");
        return crc_[0];
    }
    return crc_[index];
}

CFWrapper::CFWrapper(CFInterface *v, QObject *parent) : QObject(parent) {
    wrappedObject = v;
}

CFPacketVersion::CFPacketVersion(std::string name, LCDControl *visitor, 
    Json::Value *config, Model *model, bool scab, int layers) :
    LCDCore(visitor, name, config, LCD_TEXT, (LCDText *)this), 
    GenericSerial(name),
    LCDText((LCDCore *)this) {

    TextRealBlit = CrystalfontzBlit2;
    TextRealDefChar = CrystalfontzDefChar2;

    model_ = model;
    scab_ = scab;
    command_limit_ = 1;
    response_state_ = 0;
    bin_locked_ = false;
    response_timeout_ = 250;
    memset(fans_, 0, 4);

    TextInit(model->GetRows(), model->GetCols(), 8, 6, 0, 8, 0, layers);

    wrapper_ = new CFWrapper((CFInterface *)this, NULL);
    QObject::connect(wrapper_, SIGNAL(_PacketReady(Packet *)), 
        wrapper_, SLOT(PacketReady(Packet *)));
    QObject::connect(wrapper_, SIGNAL(_KeyPacketReady(Packet *)),
        wrapper_, SLOT(KeyPacketReady(Packet *)));
    QObject::connect(wrapper_, SIGNAL(_FanPacketReady(Packet *)),
        wrapper_, SLOT(FanPacketReady(Packet *)));
    QObject::connect(wrapper_, SIGNAL(_TempPacketReady(Packet *)),
        wrapper_, SLOT(TempPacketReady(Packet *)));

    update_timer_ = new QTimer();
    QObject::connect(update_timer_, SIGNAL(timeout()),
        wrapper_, SLOT(Update()));
/*
    command_timer_ = new QTimer();
    QObject::connect(command_timer_, SIGNAL(timeout()), 
        wrapper_, SLOT(CommandWorker()));

    check_timer_ = new QTimer();
    QObject::connect(check_timer_, SIGNAL(timeout()),
        wrapper_, SLOT(CheckPacket()));

    fill_timer_ = new QTimer();
    QObject::connect(fill_timer_, SIGNAL(timeout()),
        wrapper_, SLOT(FillBuffer()));
*/
    watch_timer_ = new QTimer();
    watch_timer_->setInterval(10);
    QObject::connect(watch_timer_, SIGNAL(timeout()),
        wrapper_, SLOT(BinWatch()));
}

CFPacketVersion::~CFPacketVersion() {
    delete wrapper_;
    //delete command_timer_;
    //delete check_timer_;
    //delete fill_timer_;
    delete update_timer_;
    delete watch_timer_;
    std::map<int, std::vector<Bin *> > dict = bin_dict_;
    for(std::map<int, std::vector<Bin *> >::iterator it = dict.begin();
        it != dict.end(); it++) {
        for(unsigned int i = 0; i < it->second.size(); i++) {
            delete bin_dict_[it->first][i];
        }
    }
    for(unsigned int i = 0; i < bin_resent_commands_.size(); i++) {
        delete bin_resent_commands_[i];
    }
    while(command_queue1_.size() > 0) {
        CFCallback *callback = command_queue1_.front();
        delete callback;
        command_queue1_.pop_front();
    }
    while(command_queue2_.size() > 0) {
        CFCallback *callback = command_queue1_.front();
        delete callback;
        command_queue2_.pop_front();
    }
}

void CFPacketVersion::Update() {
    CommandWorker();
    FillBuffer();
    CheckPacket();
}

void CFPacketVersion::CommandWorker() {
    if( response_state_ >= command_limit_ || 
        (command_queue1_.size() < 1 && command_queue2_.size() < 1))
        return;
    CFCallback *callback = command_queue1_.front();
    current_command_ = callback->GetCommand();
    command_queue1_.pop_front();
    response_state_++;
    (*callback)(NULL);
     delete callback;
}

void CFPacketVersion::CFGSetup() {
    Json::Value *val = CFG_Fetch_Raw(CFG_Get_Root(), name_ + ".port", new Json::Value(""));
    port_ = val->asString();
    delete val;
    val = CFG_Fetch(CFG_Get_Root(), name_ + ".baud", new Json::Value(115200));
    speed_ = val->asInt();
    delete val;
    val = CFG_Fetch(CFG_Get_Root(), name_ +  ".contrast", new Json::Value(120));
    contrast_ = val->asInt();
    delete val;
    val = CFG_Fetch(CFG_Get_Root(), name_ + ".backlight", new Json::Value(100));
    backlight_ = val->asInt();
    delete val;
    LCDCore::CFGSetup();
    transfer_rate_ = round((MAX_DATA_SIZE+4)/(double)speed_*8000);
}

void CFPacketVersion::SetupDevice() {
    //command_timer_->setInterval(transfer_rate_);
    //check_timer_->setInterval(transfer_rate_);
    //fill_timer_->setInterval(transfer_rate_);
    update_timer_->setInterval(transfer_rate_);
    update_timer_->start();
    //command_timer_->start();
    //check_timer_->start();
    //fill_timer_->start();
    watch_timer_->start();

    InitiateVersion();
    InitiateContrastBacklight();
    ClearLCD();
    SetLCDCursorStyle();

    LCDCore::SetupDevice();
}

void CFPacketVersion::Connect() {
    SerialOpen(port_, speed_);
}

void CFPacketVersion::TakeDown() {
    Reboot(NULL, true);
    update_timer_->stop();
    //command_timer_->stop();
    //check_timer_->stop();
    //fill_timer_->stop();
    watch_timer_->stop();
    SerialClose();
}

void CFPacketVersion::InitiateVersion() {
    class InitiateVersionCB : public CFCallback {
        CFPacketVersion *visitor_;
        public:
        InitiateVersionCB(CFPacketVersion *v, int command) :
            CFCallback(command) {
            visitor_ = v;
        }
        void operator()(void *data) {
            Packet *pkt = (Packet *)data;
            unsigned char *pkt_data = pkt->GetData();
            std::string version = "";
            for(int i = 0; i < pkt->GetDataLength(); i++ )
                version+= pkt_data[i];
            delete []pkt_data;
            boost::regex rex("(CFA.*?):h(.*?),v(.*?)$"); 
            boost::smatch what;
            if( boost::regex_match(version, what, rex) ) {
                if(visitor_->GetModel()->GetName() != what[1].str() )
                    LCDInfo("Crystalfontz: Model mismatch %s != %s", visitor_->GetModel()->GetName().c_str(), what[1].str().c_str());
                visitor_->SetHardwareVersion(what[2]);
                visitor_->SetFirmwareVersion(what[3]);
            }
        }
    };
    GetVersion(new InitiateVersionCB(this, 1));
}

void CFPacketVersion::InitiateContrastBacklight() {
    SetLCDContrast(contrast_);
    SetLCDBacklight(backlight_);
}

int CFPacketVersion::GetCrc(Command *command) {
    Packet pkt(command->GetCommand(), command->GetData(), 
        command->GetDataLength());
    return GetCrc(pkt);
}

int CFPacketVersion::GetCrc(Packet pkt) {
    int newCRC = 0xFFFF;
    newCRC = (newCRC >> 8) ^ crcLookupTable[(newCRC^pkt.GetCommand()) & 0xFF];
    newCRC = (newCRC >> 8) ^ crcLookupTable[(newCRC^pkt.GetDataLength()) & 0xFF];
    unsigned char *data = pkt.GetData();
    for(int i = 0; i < pkt.GetDataLength(); i++) {
        int index = (newCRC ^ data[i]) & 0xFF;
        newCRC = (newCRC >> 8) ^ crcLookupTable[index];
    }
    delete []data;
    return 0x10000+~newCRC;
}

void CFPacketVersion::SendCommand( Command *cmd, CFCallback *cb, 
    int priority, bool send_now) {
    class SendCommandCB : public CFCallback {
        Command *cmd_;
        CFPacketVersion *visitor_;
        CFCallback *cb_;
        int priority_;
        bool send_now_;

        public:
        SendCommandCB(CFPacketVersion *v, Command *cmd, CFCallback *cb = NULL, 
            int priority=0, bool send_now=false):CFCallback(cmd->GetCommand()){
            cmd_ = cmd;
            visitor_ = v;
            cb_ = cb;
            priority_ = priority;
            send_now_ = send_now;
        }
        void operator()(void *data) {
            int i;
            int crc_value = visitor_->GetCrc(cmd_);
            unsigned char buf[cmd_->GetDataLength() + 4];
            buf[0] = (char)cmd_->GetCommand();
            buf[1] = (char)cmd_->GetDataLength();
            for(i = 0; i < cmd_->GetDataLength(); i++)
                buf[i+2] = cmd_->GetData()[i];
            buf[i+2] = (char)(crc_value & 0xFF);
            buf[i+3] = (char)((crc_value>>8)&0xFF);
            visitor_->SerialWrite(buf, cmd_->GetDataLength() + 4);
            visitor_->BinAdd(cmd_, cb_, priority_, send_now_);
        }
    };
    SendCommandCB *callback = new SendCommandCB(this, cmd, cb, 
        priority, send_now);    
    if(send_now) {
        (*callback)(NULL);
        delete callback;
    } else if (priority == 0) { 
        command_queue1_.push_back(callback); 
    } else {
        command_queue2_.push_back(callback);
    }
}

void CFPacketVersion::CheckPacket() {
    if(buffer_.GetLocked()) return;
    buffer_.SetLocked(true);
    buffer_.SetCurrent(0);
    unsigned char data[MAX_DATA_SIZE];
    int read = buffer_.Peek(data, 4);
    if(read != 4) {
        buffer_.SetLocked(false);
        return;
    }
    buffer_.SetCurrent(0);
    read = buffer_.Peek(data);
    if( read == 0 ) {
        LCDError("2)");
        buffer_.SetLocked(false);
        return;
    }
    if( MAX_COMMAND < (0X3F & data[0])) {
        LCDError("3)");
        buffer_.Read(data);
        tossed_++;
        buffer_.SetLocked(false);
        return;
    }
    if((data[0] & 0xC0) == 0xC0) {
        LCDError("4)");
        buffer_.Read(data);
        errors_^=data[0];
        buffer_.SetLocked(false);
        LCDError("Report from display: %d: %s", 
            data[0], command_names[data[0] & 0x3F]);
        return;
    }
    packet_.SetCommand(data[0]);
    read = buffer_.Peek(data);
    if( read == 0 ) {
        LCDError("5)");
        buffer_.SetLocked(false);
        return;
    }
    if( MAX_DATA_SIZE < data[0] ) {
        LCDError("6)");
        buffer_.Read(data);
        tossed_++;
        buffer_.SetLocked(false);
        return;
    }

    packet_.SetDataLength(data[0]);

    unsigned char length = data[0];
    read = buffer_.Peek(data, length + 2);
    if(read < length + 2 ) {
        LCDError("7)");
        buffer_.SetLocked(false);
        return;
    }
    buffer_.SetCurrent(buffer_.GetCurrent() - (length + 2 ));

    read = buffer_.Peek(data, length);
    packet_.SetData(data, length);

    read = buffer_.Peek(data);
    packet_.GetCrc()[0] = data[0];

    read = buffer_.Peek(data);
    packet_.GetCrc()[1] = data[0];

    if(packet_.GetCrc().AsWord() != GetCrc(packet_)) {
        LCDError("8)");
        buffer_.Read(data);
        tossed_++;
        buffer_.SetLocked(false);
        return;
    }
    read = buffer_.Read(data, length + 4);
    Packet *pkt = new Packet(packet_);
    ReadPacket(pkt);
    buffer_.SetLocked(false);
}

void CFPacketVersion::ReadPacket(Packet *packet) {
    switch(packet->GetCommand()) {
        case TYPE_RPT_KEY_ACTIVITY:
            emit static_cast<CFEvents *>(wrapper_)->_KeyPacketReady(packet);
            break;
        case TYPE_RPT_FAN_SPEED:
            emit static_cast<CFEvents *>(wrapper_)->_FanPacketReady(packet);
            break;
        case TYPE_RPT_TEMP_SENSOR:
            emit static_cast<CFEvents *>(wrapper_)->_TempPacketReady(packet);
            break;
        default:   // Command response
            emit static_cast<CFEvents *>(wrapper_)->_PacketReady(packet);
            break;
    }
}

void CFPacketVersion::FillBuffer() {
    unsigned char tmp[MAX_DATA_SIZE + 4];
    int ret = SerialReadData(tmp, MAX_DATA_SIZE + 4);
    if( ret < 1 ) 
        return;
    buffer_.AddData( tmp, ret );
}

void CFPacketVersion::PacketReady(Packet *packet) {
    //LCDError("Incoming packet: Command: %s",
    //    command_names[packet->GetCommand() & 0x3F]);
    BinProcessPacket(packet);
    delete packet;
}

void CFPacketVersion::KeyPacketReady(Packet *packet) {
    LCDError("Incoming key packet");
    unsigned char *data = packet->GetData();
    emit static_cast<LCDEvents *>(
        GetWrapper())->_KeypadEvent(data[0]);
    delete packet;
    delete data;
}

void CFPacketVersion::FanPacketReady(Packet *packet) {
    LCDError("Incoming fan packet");
    delete packet;
}

void CFPacketVersion::TempPacketReady(Packet *packet) {
    LCDError("Incoming temperature packet");
    delete packet;
}

void CFPacketVersion::BinAdd(Command *cmd, CFCallback *cb, 
    int priority, bool send_now) {
    int r = 0x40|cmd->GetCommand();
    bin_dict_[r].push_back(new Bin(cmd, cb, priority, send_now));
    commands_sent_++;
}

void CFPacketVersion::BinAddCurrentAddress(int address) {
    bin_current_memory_.push_back(address);
}

void CFPacketVersion::BinDelCurrentAddress(int address) {
    for(std::vector<int>::iterator it = bin_current_memory_.begin();
        it != bin_current_memory_.end(); it++) {
        if(*it == address) {
            bin_current_memory_.erase(it);
            break;
        }
    }
}

void CFPacketVersion::BinProcessPacket(Packet *pkt) {
    if(bin_dict_.find(pkt->GetCommand()) != bin_dict_.end()) {
        Bin *tmp = NULL;
        if(bin_dict_[pkt->GetCommand()].size() > 0) {
            if( (pkt->GetCommand() ^ 0x40) == current_command_) {
                int command = current_command_ | 0x40;
                if(bin_dict_.find(command) != bin_dict_.end() &&
                    (command | 0x40) == 74 ) {
                    /*for(int i = 0; i < bin_current_memory_.size(); i++) {
                        if(bin_current_memory_[i] != command)
                            continue;
                        for(int j = 0; j < bin_dict_[command].size();
                            j++ ) {
                            if(bin_dict_[command][i]->GetCommand()->
                                GetData().size() > 0 &&
                                bin_dict_[command][i]->GetCommand()->GetData()[0] 
                                    == pkt->GetData()[0] ) {
                                BinDelCurrentAddress(pkt->GetData()[0]);
                                tmp = bin_dict_[pkt->GetCommand()][0];
                                bin_dict_[pkt->GetCommand()].erase(
                                    bin_dict_[pkt->GetCommand()].begin());
                            }
                        }
                    }*/
                } else {
                    tmp = bin_dict_[pkt->GetCommand()][0];
                    bin_dict_[pkt->GetCommand()].erase(
                        bin_dict_[pkt->GetCommand()].begin());
                }
            }
        }
        if(tmp && tmp->GetCallback()) {
            (*tmp->GetCallback())(pkt);
            commands_received_++;
            response_state_--;
            delete tmp->GetCommand();
            delete tmp->GetCallback();
            delete tmp;
        } else if (tmp) {
            response_state_--;
            delete tmp->GetCommand();
            delete tmp->GetCallback();
            delete tmp;
        }
    }
}

void CFPacketVersion::BinWatch() {
    if( bin_locked_) 
        return;
    bin_locked_ = true;
    std::vector<Bin *>::iterator eraseit;
    int erasekey;
    int erasei;
    bool eraseb = false;
    std::map<int, std::vector<Bin *> > dict = bin_dict_;
    for(std::map<int, std::vector<Bin *> >::iterator it = dict.begin(); 
        it != dict.end(); it++) {
        if(it->second.size() > 0) {
            for(unsigned int i = 0; i < it->second.size(); i++) {
                for(std::vector<Bin *>::iterator bit = bin_dict_[it->first].begin();
                    bit!=bin_dict_[it->first].end(); bit++ ) {
                    Bin *bin = bin_dict_[it->first][i];
                    if(*bin != **bit)
                        continue;
                    struct timeval now;
                    gettimeofday(&now, NULL);
                    long seconds = now.tv_sec - bin->GetTime().tv_sec;
                    long useconds = now.tv_usec - bin->GetTime().tv_usec;

                    long mtime = (seconds * 1000 + useconds/1000.0) + 0.5;
                    
                    if( mtime > response_timeout_) {
                        LCDError("Response timeout; resending %s", 
                            command_names[bin->GetCommand()->GetCommand()]); 
                        SendCommand(bin->GetCommand(), bin->GetCallback(), 
                            bin->GetPriority(), bin->GetSendNow());
                        //bin_resent_commands_.push_back(bin);
                        commands_resent_++;
                        eraseit = bit;
                        erasekey = it->first;
                        erasei = i;
                        eraseb = true;
                        break;
                    }
                }
            if(eraseb) break;
            }
        }
    if(eraseb) break;
    }
    if(eraseb) {
        delete bin_dict_[erasekey][erasei];
        bin_dict_[erasekey].erase(eraseit);
        response_state_--;
    }
    bin_locked_ = false;
}

// DrvCrystalfontz 

LCDCore *DrvCrystalfontz::Get(std::string name, LCDControl *v, 
    Json::Value *config, std::string model, int layers) {

    std::map<std::string, Model> Models;
    Models["533"] = Model("CFA533", 2, 16, 4, 4, 2, 18);
    Models["626"] = Model("CFA626", 2, 16, 0, 0, 1, 0);
    Models["631"] = Model("CFA631", 2, 20, 4, 0 ,3, 22);
    Models["632"] = Model("CFA632", 2, 16, 0, 0, 1, 0);
    Models["633"] = Model("CFA633", 2, 16, 4, 4, 2, 18);
    Models["634"] = Model("CFA634", 4, 20, 0, 0, 1, 0);
    Models["635"] = Model("CFA635", 4, 20, 4, 12, 3, 22);
    Models["636"] = Model("CFA636", 2, 16, 0, 0, 1, 0);
 
    bool scab = false;
    if(model[model.size()-1] == '+') {
        model = model.substr(0, model.size() - 1);
        scab = true;
    }
    Model *m = new Model(Models[model]);
    switch(m->GetProtocol()) {
        case 1:
            return new Protocol1(name, m, v, config, layers);
            break;
        case 2:
            return new Protocol2(name, m, v, config, layers);
            break;
        case 3:
            return new Protocol3(name, m, v, config, scab, layers);
            break;
        default:
            LCDError("Internal error. Model has bad protocol: <%s>",
                m->GetName().c_str());
            break;
    }
    return NULL;
}

// Protocol 1
Protocol1::Protocol1(std::string name, Model *model, LCDControl *v, 
    Json::Value *config, int layers) : 
    LCDCore(v, name, config, LCD_TEXT, (LCDText *)this),
    LCDText((LCDCore *)this), GenericSerial(name) {
    model_ = model;

    TextRealBlit = CrystalfontzBlit1;
    TextRealDefChar = CrystalfontzDefChar1;

    TextInit(model->GetRows(), model->GetCols(), 8, 6, 0, 8, 128, layers);

    wrapper_ = new CFWrapper((CFInterface *)this, NULL);
    
    command_timer_ = new QTimer();
    command_timer_->setSingleShot(true);
    QObject::connect(command_timer_, SIGNAL(timeout()),
        wrapper_, SLOT(CommandWorker()));
}

Protocol1::~Protocol1() {
    delete wrapper_;
    delete model_;
    delete command_timer_;
}

void Protocol1::CommandWorker() {
    if(command_queue_.size() < 1) {
        command_timer_->start(20);
        return;
    }
    CFCallback *callback = command_queue_.front();
    command_queue_.pop_front();
    (*callback)(NULL);
    command_timer_->start(callback->GetTransferRate());
    delete callback;
}

void Protocol1::CFGSetup() {
    Json::Value *val = CFG_Fetch_Raw(CFG_Get_Root(), name_ + ".port", new Json::Value(""));
    port_ = val->asString();
    delete val;
    val = CFG_Fetch(CFG_Get_Root(), name_ + ".baud", new Json::Value(19200));
    speed_ = val->asInt();
    delete val;
    val = CFG_Fetch(CFG_Get_Root(), name_ + ".contrast", new Json::Value(100));
    contrast_ = val->asInt();
    delete val;
    val = CFG_Fetch(CFG_Get_Root(), name_ + ".backlight", new Json::Value(100));
    backlight_ = val->asInt();
    delete val;
    LCDCore::CFGSetup();
    transfer_rate_ = round(20/(double)speed_*8000);
}

void Protocol1::SetupDevice() {
    command_timer_->start(20);

    Reboot();
    InitiateContrastBacklight();
    unsigned char *buff1 = (unsigned char *)malloc(1);
    buff1[0] = 4; // hide cursor
    QueueWrite(new Command(NO_CMD, buff1, 1), transfer_rate_);
    unsigned char *buff2 = (unsigned char *)malloc(1);
    buff2[0] = 20; // scroll off
    QueueWrite(new Command(NO_CMD, buff2, 1), transfer_rate_);
    unsigned char *buff3 = (unsigned char *)malloc(1);
    buff3[0] = 24; // wrap off
    QueueWrite(new Command(NO_CMD, buff3, 1), transfer_rate_);
    
    LCDCore::SetupDevice();
}

void Protocol1::Connect() {
    SerialOpen(port_, speed_);
}

void Protocol1::TakeDown() {
    Reboot(true);
    command_timer_->stop();
}

void Protocol1::InitiateContrastBacklight() {
    SetLCDContrast(contrast_);
    SetLCDBacklight(backlight_);
}

void Protocol1::ClearLCD(bool send_now) {
    unsigned char *buff = (unsigned char *)malloc(1);
    buff[0] = 12;
    QueueWrite(new Command(NO_CMD, buff, 1), transfer_rate_, send_now);    
}

void Protocol1::Reboot(bool send_now) {
    unsigned char *buff = (unsigned char *)malloc(11);
    for(int i = 0; i <  9; i++)
        buff[i] = ' ';
    buff[9] = 26;
    buff[10] = 26;
    QueueWrite(new Command(NO_CMD, buff, 11), 1000, send_now);
}

void Protocol1::SetLCDContrast(int level) {
    if(level > 100)
        level = 100;
    unsigned char *buff = (unsigned char *)malloc(2);
    buff[0] = 15;
    buff[1] = level; 
    QueueWrite(new Command(NO_CMD, buff, 2), transfer_rate_);
}

void Protocol1::SetLCDBacklight(int level) {
    if(level > 100)
        level = 100;
    unsigned char *buff = (unsigned char *)malloc(2);
    buff[0] = 14;
    buff[1] = level;
    QueueWrite(new Command(NO_CMD, buff, 2), transfer_rate_);
}

void Protocol1::SetCursorPos(int row, int col) {
    unsigned char *buff = (unsigned char *)malloc(3);
    if( row > model_->GetRows() || row < 0) {
        LCDError("SetCursorPos: row %d is a bad value.", row);
        row = 0;
    }
    if( col > model_->GetCols() || col < 0) {
        LCDError("SetCursorPos: col %d is a bad value.", col);
        col = 0;
    }
    buff[0] = 17;
    buff[1] = col;
    buff[2] = row;
    QueueWrite(new Command(NO_CMD, buff, 3), transfer_rate_);
}

void Protocol1::SendData(int row, int col, unsigned char *data, int len) {
    unsigned char *buff = (unsigned char *)malloc(len);
    SetCursorPos(row, col);
    for(int i = 0; i < len; i++ )
        buff[i] = data[i];
    if( model_->GetName() == "634" || model_->GetName() == "632" ) 
        ConvertToCgrom2(buff, len);
    QueueWrite(new Command(NO_CMD, buff, len), transfer_rate_);
}

void Protocol1::SetSpecialChar(int num, SpecialChar ch) {
    unsigned char *buff = (unsigned char *)malloc(ch.Size() + 2);
    buff[0] = 25;
    buff[1] = num;
    for(int i = 0; i < ch.Size(); i++)
        buff[i+2] = ch[i];
    QueueWrite(new Command(NO_CMD, buff, ch.Size() + 2), transfer_rate_);
}

void Protocol1::ConvertToCgrom2(unsigned char *str, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        switch (str[i]) {
        case 0x5d:              /* ] */
            str[i] = 252;
            break;
        case 0x5b:              /* [ */
            str[i] = 250;
            break;
        case 0x24:              /* $ */
            str[i] = 162;
            break;
        case 0x40:              /* @ */
            str[i] = 160;
            break;
        case 0x5c:              /* \ */
            str[i] = 251;
            break;
        case 0x7b:              /* { */
            str[i] = 253;
            break;
        case 0x7d:              /* } */
            str[i] = 255;
            break;
        case 0x7c:
            str[i] = 254;       /* pipe */
            break;
        case 0x27:
        case 0x60:
        case 0xB4:
            str[i] = 39;        /* ' */
            break;
        case 0xe8:
            str[i] = 164;       /* french e */
            break;
        case 0xe9:
            str[i] = 165;       /* french e */
            break;
        case 0xc8:
            str[i] = 197;       /* french E */
            break;
        case 0xc9:
            str[i] = 207;       /* french E */
            break;
        case 0xe4:
            str[i] = 123;       /* small german ae */
            break;
        case 0xc4:
            str[i] = 91;        /* big german ae */
            break;
        case 0xf6:
            str[i] = 124;       /* small german oe */
            break;
        case 0xd6:
            str[i] = 92;        /* big german oe */
            break;
        case 0xfc:
            str[i] = 126;       /* small german ue */
            break;
        case 0xdc:
            str[i] = 94;        /* big german ue */
            break;
        case 0x5e:              /* ^ */
            str[i] = 253;
            break;
        case 0x5f:              /* _ */
            str[i] = 254;
            break;
        default:
            break;
        }
    }
}


void Protocol1::QueueWrite(Command *cmd, int transfer_rate, bool send_now) {
    class QueueWriteCB : public CFCallback {
        Command *cmd_;
        Protocol1 *visitor_;
        public:
        QueueWriteCB(Protocol1 *v, Command *cmd, int transfer_rate) : 
            CFCallback(cmd->GetCommand(), transfer_rate) { 
            visitor_ = v;
            cmd_ = cmd;
        }
        ~QueueWriteCB() {
            delete cmd_;
        }
        void operator()(void *data) {
            unsigned char buff[cmd_->GetDataLength()];
            for(int i = 0; i < cmd_->GetDataLength(); i++) 
                buff[i] = cmd_->GetData()[i];
            visitor_->SerialWrite(buff, cmd_->GetDataLength());
        }
    };
    QueueWriteCB *callback = new QueueWriteCB(this, cmd, transfer_rate);
    if(send_now) {
        (*callback)(NULL);
        delete callback;
    } else {
        command_queue_.push_back(callback);
    }
}


// Protocol 2
Protocol2::Protocol2(std::string name, Model *model, LCDControl *v, 
    Json::Value *config, int layers) : 
    LCDCore(v, name, config, LCD_TEXT, (LCDText *)this),
    CFPacketVersion(name, v, config, model, false, layers) {
    LCDError("Protocol 2");
}

Protocol2::~Protocol2() {
    delete model_;
}

void Protocol2::Ping(std::string msg, CFCallback *cb) {
    Command *cmd = new Command(0, (unsigned char*)strdup(msg.c_str()), msg.size());
    SendCommand(cmd, cb);
}

void Protocol2::GetVersion(CFCallback *cb) {
    SendCommand(new Command(1), cb);
}

void Protocol2::Reboot(CFCallback *cb, bool send_now) {
    unsigned char *buff = (unsigned char *)malloc(3);
    buff[0] = 8;
    buff[1] = 18;
    buff[2] = 99;
    SendCommand(new Command(5, buff, 3), cb, 0, send_now);
}

void Protocol2::ClearLCD() {
    SendCommand(new Command(6));
}

void Protocol2::SendData(int row, int col, unsigned char *data, 
    int len, bool send_now) {
    unsigned char *buff = (unsigned char *)malloc(len + 2);
    buff[0] = (char)col;
    buff[1] = (char)row;
    for(int i = 0; i < len; i++ )
        buff[i+2] = data[i];
    SendCommand(new Command(31, buff, len+2));
}

void Protocol2::SetSpecialChar(int num, SpecialChar ch) {
    unsigned char *buff = (unsigned char *)malloc(ch.Size() + 1);
    buff[0] = num;
    for(int i = 0; i < ch.Size(); i++ )
        buff[i + 1] = ch[i];
    SendCommand(new Command(9, buff, ch.Size() + 1));
}

void Protocol2::SetLCDCursorStyle(int style, CFCallback *cb) {
    if(style < 0 || style > 4 )
        return;
    unsigned char *buff = (unsigned char *)malloc(1);
    buff[0] = style;
    SendCommand(new Command(12, buff, 1));
}

void Protocol2::SetLCDContrast(int level) {
    if( level < 0 )
        level = 0;
    if( level > 50 )
        level = 50;
    unsigned char *buff = (unsigned char *)malloc(1);
    buff[0] = level;
    SendCommand(new Command(13, buff, 1));
}

void Protocol2::SetLCDBacklight(int level) {
    if( level < 0 )
        level = 0;
    if( level > 100 )
        level = 100;
    unsigned char *buff = (unsigned char *)malloc(1);
    buff[0] = level;
    SendCommand(new Command(14, buff, 1));
}

void Protocol2::SetFanPower(int num, int val) {
    if( val < 0 )
        val = 0;
    if( val > 100 )
        val = 100;
    fans_[num] = val;
    unsigned char *buff = (unsigned char *)malloc(4);
    for(int i = 0; i < 4; i++ )
        buff[i] = fans_[i];
    SendCommand(new Command(17, buff, 4)); 
}

void Protocol2::SetGPIO(int num, int val) {
    if( val < 0 )
        val = 0;
    if( val > 100 )
        val = 100;
    unsigned char *buff = (unsigned char *)malloc(2);
    buff[0] = num;
    buff[1] = val;
    SendCommand(new Command(34, buff, 2));
}

// Protocol 3

Protocol3::Protocol3(std::string name, Model *model, LCDControl *v, 
    Json::Value *config, bool scab, int layers) : 
    LCDCore(v, name, config, LCD_TEXT, (LCDText *)this),
    CFPacketVersion(name, v, config, model, scab, layers) {
    model_ = model;
    
    LCDError("Protocol3");
}

Protocol3::~Protocol3() {
    delete model_;
}
void Protocol3::Ping(std::string msg, CFCallback *cb) {
    Command *cmd = new Command(0, 
        (unsigned char*)strdup(msg.c_str()), msg.size());
    SendCommand(cmd, cb);
}

void Protocol3::GetVersion(CFCallback *cb) {
    SendCommand(new Command(1), cb);
}

void Protocol3::Reboot(CFCallback *cb, bool send_now) {
    unsigned char *buff = (unsigned char *)malloc(3);
    buff[0] = 8;
    buff[1] = 18;
    buff[2] = 99;
    SendCommand(new Command(5, buff, 3), cb, 0, send_now);
}

void Protocol3::ClearLCD() {
    SendCommand(new Command(6));
}

void Protocol3::SendData(int row, int col, unsigned char *data, 
    int len, bool send_now) {
    unsigned char *buff = (unsigned char *)malloc(len + 2);
    buff[0] = (char)col;
    buff[1] = (char)row;
    for(int i = 0; i < len; i++ )
        buff[i+2] = data[i];
    SendCommand(new Command(31, buff, len+2));
}

void Protocol3::SetSpecialChar(int num, SpecialChar ch) {
    unsigned char *buff = (unsigned char *)malloc(ch.Size() + 1);
    buff[0] = num;
    for(int i = 0; i < ch.Size(); i++ )
        buff[i + 1] = ch[i];
    SendCommand(new Command(9, buff, ch.Size() + 1));
}

void Protocol3::SetLCDCursorStyle(int style, CFCallback *cb) {
    if(style < 0 || style > 4 )
        return;
    unsigned char *buff = (unsigned char *)malloc(1);
    buff[0] = style;
    SendCommand(new Command(12, buff, 1));
}

void Protocol3::SetLCDContrast(int level) {
    return;
    if( level < 0 )
        level = 0;
    if( level > 50 )
        level = 50;
    unsigned char *buff = (unsigned char *)malloc(1);
    buff[0] = level;
    SendCommand(new Command(13, buff, 1));
}

void Protocol3::SetLCDBacklight(int level) {
    return;
    if( level < 0 )
        level = 0;
    if( level > 100 )
        level = 100;
    unsigned char *buff = (unsigned char *)malloc(1);
    buff[0] = level;
    SendCommand(new Command(14, buff, 1));
}

void Protocol3::SetFanPower(int num, int val) {
    if( val < 0 )
        val = 0;
    if( val > 100 )
        val = 100;
    fans_[num] = val;
    unsigned char *buff = (unsigned char *)malloc(4);
    for(int i = 0; i < 4; i++ )
        buff[i] = fans_[i];
    SendCommand(new Command(17, buff, 4));
}

void Protocol3::SetGPIO(int num, int val) {
    if( val < 0 )
        val = 0;
    if( val > 100 )
        val = 100;
    unsigned char *buff = (unsigned char *)malloc(2);
    buff[0] = num;
    buff[1] = val;
    SendCommand(new Command(34, buff, 2));
}

