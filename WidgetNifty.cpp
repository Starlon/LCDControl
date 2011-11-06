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

#include <json/json.h>
#include <QTimer>

#include "WidgetNifty.h"
#include "LCDText.h"
#include "LCDGraphic.h"
#include "LCDCore.h"
#include "niftyeventsource.h"

#define EX_HANDLER_BEGIN(ok)                    \
    { ok = false; NL_X_TRY {

#define EX_HANDLER_END(ok, err_str)                                     \
    ok = true; } NL_X_CATCH(e) { LCDError("%s:%s (file:%s line:%d)", e->reason, exception_frame.message, exception_frame.file, exception_frame.line); } NL_X_END; }

#define EX_HANDLER_END_W_ERR(ok)                         \
    ok = true; } NL_X_CATCH(e) { LCDError("%s:%s (file:%s line:%d)", e->reason, exception_frame.message, exception_frame.file, exception_frame.line); } NL_X_END; }

using namespace LCD;

static void _frame_callback(nlRemoteOutput *output,
                            nlFrame *frame, void *user_data)
{
    if(!output)
        return;
    if(!frame)
        return;
    WidgetNifty *w = static_cast<WidgetNifty*>(user_data);
    if(!w)
        return;

    w->FrameReceived(frame);
}

extern void GraphicNiftyDraw(WidgetNifty *w);

WidgetNifty::WidgetNifty(LCDCore *v, std::string name, Json::Value *config,
        int row, int col, int layer) : Widget(v, name, config, row, col, layer, 
        WIDGET_TYPE_NIFTY | WIDGET_TYPE_RC), 
        zoomFac_(0), nifty_(0), remote_(0), output_(0) {

    if(lcd_type_ == LCD_GRAPHIC) {
        Draw = GraphicNiftyDraw;
    } else if(lcd_type_ == LCD_TEXT) {
        LCDError("WidgetNifty: Only graphic displays are supported.");
        update_ = -1;
        return;
    } else {
        Draw = NULL;
    }

    update_ = 0;

    Json::Value *val = v->CFG_Fetch(config, "height", new Json::Value(0));
    rows_ = val->asInt();
    delete val;

    val = v->CFG_Fetch(config, "width", new Json::Value(0));
    cols_ = val->asInt();
    delete val;

    val = v->CFG_Fetch_Raw(config, "host", new Json::Value("127.0.0.1"));
    telnet_host_ = val->asString();
    delete val;

    val = v->CFG_Fetch(config, "port", new Json::Value(8766));
    telnet_port_ = val->asInt();
    delete val;

    pixmap_ = new RGBA[rows_ * cols_];

    nlBool ok;
    EX_HANDLER_BEGIN(ok) {
        nlEventContext *event_context = NiftyEventSource::allocate();
        nl_assert(event_context);

        nifty_ = nl_client_init("/home/starlon/.widgetNifty.xml", 0, NULL, event_context);

        event_context = NULL;

        remote_ = nl_remote_alloc(nifty_);
        nl_remote_host_set(remote_, telnet_host_.c_str(), telnet_port_);

        output_ = nl_remote_output_alloc(remote_, "widgetnifty", -1);

        frameCallback_ = nl_remote_output_frame_callback_register(output_,
            _frame_callback, this);

        mutex_ = nl_mutex_alloc(NL_MUTEX_NORMAL, "widgetnifty_mutex");
        nl_assert(mutex_);

        nl_remote_output_connect(output_);

        nl_remote_output_frame_format_set(output_, cols_, rows_, 24, 3);

    } EX_HANDLER_END_W_ERR(ok);

    if(!ok)
        update_ = -1;

    timer_ = new QTimer();
    timer_->setSingleShot(false);
    timer_->setInterval(update_);
    QObject::connect(timer_, SIGNAL(timeout()), this, SLOT(Update()));

    QObject::connect(visitor_->GetWrapper(), SIGNAL(_ResizeLCD(int, int, int, int)),
        this, SLOT(Resize(int, int, int, int)));
}


WidgetNifty::~WidgetNifty() {
    Stop();
    if(pixmap_) delete []pixmap_;
    if(timer_) delete timer_;
    nlBool ok;
    EX_HANDLER_BEGIN(ok) {
        nl_callback_unregister(frameCallback_);
        nl_remote_output_free(output_);
        nl_remote_free(remote_);
        nl_client_deinit(nifty_);
        nl_mutex_free(mutex_);
    } EX_HANDLER_END_W_ERR(ok);
    remote_ = 0;
    nifty_ = 0;
}

void WidgetNifty::Resize(int rows, int cols, int old_rows, int old_cols) {
    float y = old_rows / (float)rows_;
    float x = old_cols / (float)cols_;
    float r = old_rows / (float)row_;
    float c = old_cols / (float)col_;
    rows_ = round(rows * y);
    cols_ = round(cols * x);
    row_ = round(rows * r);
    col_ = round(cols * c);

    delete [] pixmap_;
    pixmap_ = new RGBA[rows_ * cols_];

    nl_remote_output_frame_format_set(output_, cols_, rows_, 24, 3);
    Update();
}

void WidgetNifty::FrameReceived(nlFrame *frame) {
    if(!frame)
        return;

    NL_LOCK_PUSH(mutex_);

    nlBool ok = false;
    EX_HANDLER_BEGIN(ok) {
        const int w = nl_frame_width_get(frame);
        const int h = nl_frame_height_get(frame);

        if(w != cols_ || h != rows_) {
            LCDError("WidgetNifty: Height or width doesn't match up: %dx%d, %dx%d", w, h, cols_, rows_);
            update_ = -1;
            return;
        }

        const int pixelNum = w*h;
        const size_t buf_size = nl_frame_pixbuf_size_get(frame);
        char *buf = (char*) nl_frame_pixbuf_get(frame);

        for(int i = 0; i < pixelNum && i*3+2 < (int)buf_size; i++) {
            pixmap_[i].R = buf[i*3];
            pixmap_[i].G = buf[i*3+1];
            pixmap_[i].B = buf[i*3+2];
            pixmap_[i].A = 255;
        }


    } EX_HANDLER_END_W_ERR(ok);

    NL_UNLOCK_POP(mutex_);
}

void WidgetNifty::Update() {
    if(update_ < 0)
        return;

    NL_LOCK_PUSH(mutex_);

    if(Draw)
        Draw(this);
    else
        LCDError("WidgetNifty: No draw method");

    NL_UNLOCK_POP(mutex_);
}

void WidgetNifty::Start() {
    if(update_ < 0)
        return;
    timer_->start();
    started_ = true;
    Update();
}

void WidgetNifty::Stop() {
    timer_->stop();
    started_ = false;
}

