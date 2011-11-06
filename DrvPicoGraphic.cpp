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

#include <libusb-1.0/libusb.h>

#include "DrvPicoGraphic.h"
using namespace LCD;

// LCDGraphic RealBlit
void DrvPicoGraphicBlit(LCDGraphic *lcd, const int row, const int col,
    const int height, const int width) {
    ((DrvPicoGraphic *)lcd)->DrvBlit(row, col, height, width);
}

// ENDPOINT_OUT transfers' callback
void DrvPicoGraphicSendCB(struct libusb_transfer *transfer) {
    DrvPicoGraphic *lcd = (DrvPicoGraphic *)transfer->user_data;
    if(transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        LCDError("Transfer not completed - resubmitting");
        libusb_submit_transfer(transfer);
        return;
    } else
        ;//LCDError("Transfer completed");
    libusb_free_transfer(transfer);
    lcd->SignalFinished();
}

// ENDPOINT_IN Keypad events
void DrvPicoGraphicReadGPICB(struct libusb_transfer *transfer) {
    DrvPicoGraphic *lcd = (DrvPicoGraphic *)transfer->user_data;
    if(transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        if(transfer->buffer[0] == IN_REPORT_KEY_STATE)
            lcd->DrvKeypadEvent(transfer->buffer[1]);
    }
    lcd->SetReadLocked(false);
    lcd->DrvGPI();
}

// Constructor
DrvPicoGraphic::DrvPicoGraphic(std::string name, LCDControl *v,
    Json::Value *config, int layers) :
    LCDCore(v, name, config, LCD_GRAPHIC, (LCDGraphic *)this),
    LCDGraphic((LCDCore *)this) {
    LCDError("DrvPicoGraphic");

    GraphicRealBlit = DrvPicoGraphicBlit;

    devh_ = NULL;
    send_transfer_ = NULL;
    read_transfer_ = NULL;
    transfers_out_ = 0;
    sent_ = 0;
    received_ = 0;

    GraphicInit(SCREEN_H, SCREEN_W, 8, 6, layers);

    drvFB = new bool[SCREEN_W*SCREEN_H];

    wrapper_ = new PGWrapper((PGInterface *)this);

    update_timer_ = new QTimer();
    update_timer_->setInterval(150);
    QObject::connect(update_timer_, SIGNAL(timeout()),
        wrapper_, SLOT(DrvUpdateImg()));
    
    command_thread_ = new PGCommandThread(this);
    poll_thread_ = new PGPollThread(this);
    read_locked_ = false;
    send_locked_ = false;

}

// Destructor
DrvPicoGraphic::~DrvPicoGraphic() {
    delete []drvFB;
    delete wrapper_;
    delete poll_thread_;
    delete command_thread_;
    delete update_timer_;
}

//Separate thread -- monitors command_queue_ and executes callbacks
void DrvPicoGraphic::CommandWorker() { 
    //static timeval last {0, 0};

    while(connected_) {
        if(transfers_out_ >= 1 || command_queue_.size() < 1) { 
            usleep(refresh_rate_);
            continue;
        }
        sent_++;
        transfers_out_++;
        send_locked_ = true;
        PGSendCallback *callback = command_queue_.front();
        command_queue_.pop_front();
        (*callback)();
        delete callback;
        /*timeval now;
        gettimeofday(&now, NULL);
        unsigned long time = (now.tv_sec - last.tv_sec) * 1000 + (now.tv_usec - last.tv_usec) / 1000;
        gettimeofday(&last, NULL);
        if((command_queue_.size() % 10) == 0)
            LCDError("Queue size: %d, time: %lu, out: %d, sent: %d, received: %d, difference: %d", command_queue_.size(), time, 
            transfers_out_, sent_, received_, sent_ - received_);
            */
    }
}

// Initialize device and libusb
void DrvPicoGraphic::SetupDevice() {
    LCDError("SetupDevice");
    int r = libusb_init(&ctx_);
    if( r < 0) {
        LCDError("DrvPicoGraphic: Failed to initialize libusb");
        return;
    }
    devh_ = libusb_open_device_with_vid_pid(ctx_, VENDOR_ID, PRODUCT_ID);
    if(!devh_) {
        LCDError("DrvPicoGraphic: Failed to find device");
        return;
    }
    r = libusb_claim_interface(devh_, 0);
    if( r < 0) {
        LCDError("DrvPicoGraphic: Failed to claim interface %d", r);
        return;
    }
    read_transfer_ = libusb_alloc_transfer(0);
    //send_transfer_ = libusb_alloc_transfer(0);
    libusb_set_debug(ctx_, 0);
    update_timer_->start();
    GraphicStart();
}

// Deinit driver
void DrvPicoGraphic::TakeDown() {
    update_timer_->stop();
    Disconnect();
}

// Configuration setup
void DrvPicoGraphic::CFGSetup() {
    Json::Value *val = CFG_Fetch(CFG_Get_Root(), name_ + ".contrast", 
        new Json::Value(120));
    contrast_ = val->asInt();
    delete val;

    val = CFG_Fetch(CFG_Get_Root(), name_ + ".backlight", 
        new Json::Value(100));
    backlight_ = val->asInt();
    delete val;

    val = CFG_Fetch(CFG_Get_Root(), name_ + ".inverted",
        new Json::Value(0));
    inverted_ = INVERTED = val->asInt();
    delete val;

    val = CFG_Fetch(CFG_Get_Root(), name_ + ".refresh-rate", new Json::Value(50));
    refresh_rate_ = val->asInt();
    delete val;

    LCDCore::CFGSetup();
}

// Connect -- generic method called from main code
void DrvPicoGraphic::Connect() {
    LCDError("Connect");
    if(!devh_)
        return;
    connected_ = true;
    poll_thread_->start();
    command_thread_->start();
    //DrvClear();
    GraphicClear();
    DrvGPI();
}

// Disconnect -- deinit
void DrvPicoGraphic::Disconnect() {
    connected_ = false;
    poll_thread_->wait();
    command_thread_->wait();
    if(!devh_)
        return;
    int r;
    /*if(send_transfer_) {
        r = libusb_cancel_transfer(send_transfer_);
    }
    while(r >= 0 && send_transfer_) {
        struct timeval tv = {0, 1};
        r = libusb_handle_events_timeout(ctx_, &tv);
    }
    
    libusb_free_transfer(send_transfer_);*/

    if(read_transfer_) {
        r = libusb_cancel_transfer(read_transfer_);
    }

    while(r >= 0 && read_transfer_) {
        struct timeval tv = {0, 1};
        r = libusb_handle_events_timeout(ctx_, &tv);
    }

    libusb_free_transfer(read_transfer_);

    libusb_release_interface(devh_, 0);
    libusb_close(devh_);
    libusb_exit(ctx_);
}

void DrvPicoGraphic::InitiateContrastBacklight() {
    DrvContrast(contrast_);
    DrvBacklight(backlight_);
}

// libusb_handle_events thread
void DrvPicoGraphic::DrvPoll() {
    while(connected_) {
        struct timeval tv = {1, 0};
        int r = libusb_handle_events_timeout(ctx_, &tv);
        if(r < 0) {
            LCDError("PollThread: Error %d", r);
        }
    }
}

// ENDPOINT_IN -- only used by GPI/keypad
void DrvPicoGraphic::DrvRead(unsigned char *data, int size, 
    void (*cb)(libusb_transfer*)) {
    while(read_locked_);
    read_locked_ = true;
    libusb_fill_interrupt_transfer(read_transfer_, devh_,
        LIBUSB_ENDPOINT_IN + 1, data, size, cb, this, 0);
    libusb_submit_transfer(read_transfer_);
}

// ENDPOINT_OUT -- called from DrvClear and DrvUpdateImg
void DrvPicoGraphic::DrvSend(unsigned char *data, int size) {
    class DrvSendCB : public PGSendCallback {
        struct libusb_device_handle *devh_;
        struct libusb_transfer *send_transfer_;
        DrvPicoGraphic *visitor_;
        public:
        DrvSendCB(DrvPicoGraphic *v, struct libusb_device_handle *devh, 
            struct libusb_transfer *transfer, unsigned char *data, int size ) : 
            PGSendCallback(data, size) {
            visitor_ = v;
            devh_ = devh;
                send_transfer_ = transfer;
        }
        void operator()() { // fill and submit transfer
            struct libusb_transfer *transfer = libusb_alloc_transfer(0);
            libusb_fill_interrupt_transfer(transfer, devh_, 
                    LIBUSB_ENDPOINT_OUT + 1, data_, size_, 
                    DrvPicoGraphicSendCB, visitor_, 0);
            libusb_submit_transfer(transfer);
        }

    };

    DrvSendCB *cb = new DrvSendCB(this, devh_, send_transfer_, data, size);
    command_queue_.push_back(cb); //queue transfer
}

// Update the LCD's pixels
void DrvPicoGraphic::DrvUpdateImg() {

    for(int cs = 0; cs < 4; cs++) {
        int chipsel = cs << 2;
        for(int line = 0; line < 8; line++) {
            unsigned char *cmd3 = new unsigned char[44];
            unsigned char *cmd4 = new unsigned char[38];
            cmd3[0] = OUT_REPORT_CMD_DATA;
            cmd3[1] = chipsel;
            cmd3[2] = 0x02;
            cmd3[3] = 0x00;
            cmd3[4] = 0x00;
            cmd3[5] = 0xb8 | line;
            cmd3[6] = 0x00;
            cmd3[7] = 0x00;
            cmd3[8] = 0x40;
            cmd3[9] = 0x00;
            cmd3[10] = 0x00;
            cmd3[11] = 32;

            cmd4[0] = OUT_REPORT_DATA;
            cmd4[1] = chipsel | 0x01;
            cmd4[2] = 0x00;
            cmd4[3] = 0x00;
            cmd4[4] = 32;

            for(int index = 0; index < 32; index++) {
                int pixel = 0x00;

                for(int bit = 0; bit < 8; bit++) {
                    int x = cs * 64 + index;
                    int y = (line * 8 + bit + 0) % SCREEN_H;
                    if(drvFB[y * 256 + x] ^ inverted_)
                        pixel |= (1 << bit);
                }
                cmd3[12 + index] = pixel;
            }
            for(int index = 32; index < 64; index++) {
                int pixel = 0x00;

                for(int bit = 0; bit < 8; bit++) {
                    int x = cs * 64 + index;
                    int y = (line * 8 + bit + 0) % SCREEN_H;
                    if(drvFB[y * 256 + x] ^ inverted_)
                        pixel |= (1 << bit);
                }
                cmd4[5 + (index - 32)] = pixel;
            }

            DrvSend(cmd3, 44);
            DrvSend(cmd4, 38);
        }
    }
}

// Driver-side blit method
void DrvPicoGraphic::DrvBlit(const int row, const int col, 
    const int height, const int width) {
    for(int r = row; r < row + height; r++) {
        for(int c = col; c < col + width; c++) {
            drvFB[r * 256 + c] = GraphicBlack(r, c);
        }
    }
}

// Clear the LCD
void DrvPicoGraphic::DrvClear() {
    unsigned char *cmd1 = new unsigned char[3];
    cmd1[0] = 0x93;
    cmd1[1] = 0x01;
    cmd1[2] = 0x00;
    DrvSend(cmd1, 3);

    for(int init = 0; init < 4; init++) {
        unsigned char *cmd2 = new unsigned char[9];
        unsigned char cs = ((init << 2) & 0xff);
        cmd2[0] = OUT_REPORT_CMD;
        cmd2[1] = cs;
        cmd2[2] = 0x02;
        cmd2[3] = 0x00;
        cmd2[4] = 0x64;
        cmd2[5] = 0x3F;
        cmd2[6] = 0x00;
        cmd2[7] = 0x64;
        cmd2[8] = 0xC0;
        DrvSend(cmd2, 9);
    }

    for(unsigned char cs = 0; cs < 4; cs++) {
        unsigned char chipsel = (cs << 2);
        for(unsigned char line = 0; line < 8; line++) {
            unsigned char *cmd3 = new unsigned char[44];
            unsigned char *cmd4 = new unsigned char[38];
            cmd3[0] = OUT_REPORT_CMD_DATA;
            cmd3[1] = chipsel;
            cmd3[2] = 0x02;
            cmd3[3] = 0x00;
            cmd3[4] = 0x00;
            cmd3[5] = 0xb8 | line;
            cmd3[6] = 0x00;
            cmd3[7] = 0x00;
            cmd3[8] = 0x40;
            cmd3[9] = 0x00;
            cmd3[10] = 0x00;
            cmd3[11] = 32;

            unsigned char temp = 0;

            for(int index = 0; index < 32; index++)
                cmd3[12 + index] = temp;

            DrvSend(cmd3, 44);

            cmd4[0] = OUT_REPORT_DATA;
            cmd4[1] = chipsel | 0x01;
            cmd4[2] = 0x00;
            cmd4[3] = 0x00;
            cmd4[4] = 32;

            for(int index = 32; index < 64; index++) {
                temp = 0x00;
                cmd4[5 + (index - 32)] = temp;
            }

            DrvSend(cmd4, 38);
        }
    }
}

// LCD Contrast
void DrvPicoGraphic::DrvContrast(const int contrast) {
    unsigned char *cmd = new unsigned char[2];
    cmd[0] = OUT_REPORT_LCD_CONTRAST;
    int val = contrast;
    if(val < 0)
        val = 0;
    if(val > 255)
        val = 255;
    cmd[1] = val;
    DrvSend(cmd, 2);
}

// LCD Backlight
void DrvPicoGraphic::DrvBacklight(const int backlight) {
    unsigned char *cmd = new unsigned char[2];
    cmd[0] = OUT_REPORT_LCD_BACKLIGHT;
    int val = backlight;
    if(val < 0)
        val = 0;
    if(val > 255)
        val = 255;
    cmd[1] = val;
    DrvSend(cmd, 2);
}

// Keypad event
void DrvPicoGraphic::DrvKeypadEvent(const int key) {
    LCDError("KeypadEvent: 0x%02x", key);
    emit static_cast<LCDEvents *>(visitor_->GetWrapper())->_KeypadEvent(key);
}

// GPI/keypad
void DrvPicoGraphic::DrvGPI() {
    unsigned char *data = new unsigned char[_USBLCD_MAX_DATA_LEN];
    DrvRead(data, _USBLCD_MAX_DATA_LEN, DrvPicoGraphicReadGPICB);
}

// GPO/LEDs
void DrvPicoGraphic::DrvGPO(int num, int val) {
    unsigned char *cmd = new unsigned char[2];
    cmd[0] = OUT_REPORT_LED_STATE;
    if(num < 0)
        num = 0;
    if(num > 7)
        num = 7;

    if(val < 0)
        val = 0;
    if(val > 1)
        val = 1;

    if(val)
        gpo_ |= 1 << num;
    else
        gpo_ &= ~(1 << num);

    cmd[1] = gpo_;
    DrvSend(cmd, 2);
}
