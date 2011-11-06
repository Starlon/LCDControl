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

#include "DrvNifty.h"
using namespace LCD;

// LCDGraphic RealBlit
void DrvNiftyBlit(LCDGraphic *lcd, const int row, const int col,
    const int height, const int width) {
    ((DrvNifty *)lcd)->DrvBlit(row, col, height, width);
}


// Constructor
DrvNifty::DrvNifty(std::string name, LCDControl *v,
    Json::Value *config, int layers) :
    LCDCore(v, name, config, LCD_GRAPHIC, (LCDGraphic *)this),
    LCDGraphic((LCDCore *)this) {
    LCDError("DrvNifty");

    GraphicRealBlit = DrvNiftyBlit;

    Json::Value *val = CFG_Fetch(config, name + ".cols", new Json::Value(SCREEN_W));
    cols_ = val->asInt();
    delete val;

    val = CFG_Fetch(config, name + ".rows", new Json::Value(SCREEN_H));
    rows_ = val->asInt();
    delete val;

    val = CFG_Fetch(config, name + ".update", new Json::Value(10));
    update_ = val->asInt();
    delete val;

    val = CFG_Fetch(config, name + ".depth", new Json::Value(8));
    depth_ = val->asInt();
    delete val;

    val = CFG_Fetch(config, name + ".bpp", new Json::Value(8*3));
    bpp_ = val->asInt();
    delete val;

    val = CFG_Fetch_Raw(config, name + ".host", new Json::Value("127.0.0.1"));
    udp_host_ = val->asString();
    delete val;

    val = CFG_Fetch(config, name + ".port", new Json::Value(8766));
    udp_port_ = val->asInt();
    delete val;

    GraphicInit(rows_, cols_, 8, 6, layers);

    drvFB = new RGBA[rows_*cols_];

    wrapper_ = new NiftyWrapper((NiftyInterface *)this);

    update_thread_ = new NiftyUpdateThread(this);
    
    nifty_ = NULL;
    frame_ = NULL;
    client_ = NULL;
    input_ = NULL;

    NL_X_TRY {
        char conf_filename[512];
        NL_SNPRINTF(conf_filename, sizeof(conf_filename), 
            "%s/.lcdcontrol_drvnifty.xml", getenv("HOME"));
    
        nifty_ = nl_client_init(conf_filename, 0, NULL, NULL);
        nl_exception_cleanup_push(nifty_, (nlExceptionCleanupFunc*)nl_client_deinit);
    
        frame_ = nl_frame_alloc(cols_, rows_, 3*8, 3);
        nl_exception_cleanup_push(frame_, (nlExceptionCleanupFunc*)nl_frame_free);

        client_ = nl_remote_alloc(nifty_);
        nl_exception_cleanup_push(client_, (nlExceptionCleanupFunc*)nl_remote_free);
    
        nl_remote_host_set(client_, udp_host_.c_str(), udp_port_);
    
        input_ = nl_remote_input_alloc(client_, "LCDControl_client", -1);
        nl_exception_cleanup_push(input_, (nlExceptionCleanupFunc*)nl_remote_input_free);
    
        nl_remote_input_connect(input_);

        mutex_ = nl_mutex_alloc(NL_MUTEX_NORMAL, "drvNifty_mutex");
        nl_assert(mutex_);
    
        nl_exception_cleanup_pop(input_, NL_CLEANUP_KEEP);
        nl_exception_cleanup_pop(client_, NL_CLEANUP_KEEP);
    
        nl_exception_cleanup_pop(frame_, NL_CLEANUP_KEEP);
        nl_exception_cleanup_pop(nifty_, NL_CLEANUP_KEEP);

    } NL_X_CATCH(e) {
        LCDError("DrvNifty: failed initializing libnifty: '%s:%s' (raised at %s:%d)", 
            e->reason, exception_frame.message, exception_frame.file, exception_frame.line);
        update_ = -1;
    } NL_X_END;

}

// Destructor
DrvNifty::~DrvNifty() {
    update_thread_->wait();
    delete []drvFB;
    delete wrapper_;
    delete update_thread_;

    NL_X_TRY {
        nl_frame_free(frame_);
        frame_ = NULL;

        nl_remote_input_disconnect(input_);

        nl_remote_input_free(input_);
        input_ = NULL;

        nl_remote_free(client_);
        client_ = NULL;

        nl_client_deinit(nifty_);
    } NL_X_CATCH(e) {
        LCDError("DrvNifty: failed deinitializing libnifty: '%s:%s' (raised at %s:%d)",
            e->reason, exception_frame.message, exception_frame.file, exception_frame.line);
    } NL_X_END;
}

// Initialize device and libusb
void DrvNifty::SetupDevice() {
    if(update_ < 0)
        return;
}

// Deinit driver
void DrvNifty::TakeDown() {
    Disconnect();
}

// Configuration setup
void DrvNifty::CFGSetup() {
    LCDCore::CFGSetup();
}

// Connect -- generic method called from main code
void DrvNifty::Connect() {
    if(update_ < 0)
        return;
    connected_ = true;
    GraphicClear();
    update_thread_->start();
    GraphicStart();
}

// Disconnect -- deinit
void DrvNifty::Disconnect() {
    connected_ = false;
}

void DrvNifty::UpdateThread() {
    while(connected_) {
        //NL_LOCK_PUSH(mutex_);
        DrvUpdateImg();
        //NL_UNLOCK_POP(mutex_);
        usleep(update_ * 1000);
    }
}

void DrvNifty::DrvUpdateImg() {
    NL_X_TRY {
        unsigned int size = cols_*rows_*3;
        uint8_t data[size];
    
        for(int i = 0; i < cols_*rows_; i++) {
            data[i*3] = drvFB[i].R;
                data[i*3+1] = drvFB[i].G;
                data[i*3+2] = drvFB[i].B;
        }
    
        if(nl_frame_width_get(frame_) != cols_ ||
            nl_frame_height_get(frame_) != rows_ ||
                nl_frame_depth_get(frame_) != depth_ ||
                nl_frame_pixelstride_get(frame_) != bpp_/8) {
    
            nl_frame_resize(frame_, NL_FORMAT_RGB, cols_, rows_, depth_, bpp_/8);
                nl_remote_input_mode_set(input_, frame_);
        }
    
        nl_assert(size == nl_frame_pixbuf_size_get(frame_));
        nl_frame_from_pixbuf_copy(frame_, data, size);
    
        nl_remote_input_frame_display(input_, frame_);

    } NL_X_CATCH(e) {
        LCDError("DrvNifty: failed trying to process frame: '%s:%s' (raised at %s:%d)",
            e->reason, exception_frame.message, exception_frame.file, exception_frame.line);
    } NL_X_END;
}

// Driver-side blit method
void DrvNifty::DrvBlit(const int row, const int col, 
    const int height, const int width) {
    //NL_LOCK_PUSH(mutex_);
    for(int r = row; r < row + height; r++) {
        for(int c = col; c < col + width; c++) {
            drvFB[r * cols_ + c] = GraphicRGB(r, c);
        }
    }
    //NL_UNLOCK_POP(mutex_);
}

// Clear the LCD
void DrvNifty::DrvClear() {
}

