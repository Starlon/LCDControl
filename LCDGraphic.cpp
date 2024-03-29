/* $Id$
 * $URL$
 *
 * Copyright (C) 1999, 2000 Michael Reinelt <michael@reinelt.co.at>
 * Copyright (C) 2004 The LCD4Linux Team <lcd4linux-devel@users.sourceforge.net>
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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <json/json.h>

#include "WidgetVisualization.h"
#include "LCDCore.h"
#include "LCDGraphic.h"
#include "RGBA.h"
#include "CFG.h"
#include "WidgetText.h"
#include "WidgetBar.h"
#include "WidgetIcon.h"
#include "WidgetHistogram.h"
#include "WidgetBignums.h"
#include "WidgetGif.h"
//#include "WidgetNifty.h"
#include "debug.h"

#include <libvisual/libvisual.h>

#undef NULL
#define NULL 0

using namespace LCD;
using namespace std;

extern int Font_6x8[256][8];
extern int Font_6x8_bold[256][8];
extern int VISUALIZATION_CHARS[6][9];

LCDGraphic::LCDGraphic(LCDCore *v) : 
    FG_COL(0x00, 0x00, 0x00, 0xFF),
    BG_COL(0xFF, 0xFF, 0xFF, 0xFF),
    BL_COL(0xFF, 0xFF, 0xFF, 0x00),
    NO_COL(0x00, 0x00, 0x00, 0x00) {

    visitor_ = v;

    Json::Value *val = v->CFG_Fetch_Raw(v->CFG_Get_Root(), 
        v->GetName() + ".foreground", new Json::Value("000000ff"));

    if(color2RGBA(val->asCString(), &FG_COL) < 0 ) {
        LCDError("%s: ignoring illegal color '%s'", 
            v->GetName().c_str(), val->asCString());
    }
    delete val;

    val = v->CFG_Fetch_Raw(v->CFG_Get_Root(),
        v->GetName() + ".background", new Json::Value("ffffffff"));

    if(color2RGBA(val->asCString(), &BG_COL) < 0 ) {
        LCDError("%s: ignoring illegal color '%s'", 
            v->GetName().c_str(), val->asCString());
    }
    delete val;

    val = v->CFG_Fetch_Raw(v->CFG_Get_Root(),
        v->GetName() + ".basecolor", new Json::Value("ffffff"));

    if(color2RGBA(val->asCString(), &BL_COL) < 0) {
        LCDError("%s: ignoring illegal color '%s'", 
            v->GetName().c_str(), val->asCString());
    }
    delete val;

    val = v->CFG_Fetch(v->CFG_Get_Root(),
        v->GetName() + ".fill", new Json::Value(0));
    fill_ = val->asInt();
    delete val;

    val = v->CFG_Fetch(v->CFG_Get_Root(), v->GetName() + ".refresh-rate", 
        new Json::Value(10));
    refresh_rate_ = val->asInt();
    delete val;

    val = v->CFG_Fetch(v->CFG_Get_Root(),
        v->GetName() + ".inverted", new Json::Value(0));
    INVERTED = val->asInt();
    delete val;
    
    GraphicRealBlit = NULL;

    transition_tick_ = 0;
    transitioning_ = false;

    //QObject::connect(&wrapper_, SIGNAL(_GraphicUpdate(int, int, int, int)),
    //    &wrapper_, SLOT(GraphicUpdate(int, int, int, int)));

    update_thread_ = new LCDGraphicUpdateThread(this);

    graphic_wrapper_ = new LCDGraphicWrapper(this);

    QObject::connect(v->GetWrapper(), SIGNAL(_ResizeBefore(int, int)),
        graphic_wrapper_, SLOT(ResizeBefore(int, int)));
    QObject::connect(v->GetWrapper(), SIGNAL(_ResizeAfter()),
        graphic_wrapper_, SLOT(ResizeAfter()));
    QObject::connect(v->GetWrapper(), SIGNAL(_LayoutChangeBefore()), 
        graphic_wrapper_, SLOT(LayoutChangeBefore()));
    QObject::connect(v->GetWrapper(), SIGNAL(_LayoutChangeAfter()),
        graphic_wrapper_, SLOT(LayoutChangeAfter()));

}

LCDGraphic::~LCDGraphic() {
    update_thread_->wait();
    for(int l = 0; l < LAYERS; l++ ) {
        free(DisplayFB[l]);
        free(LayoutFB[l]);
        free(TransitionFB[l]);
    }
    free(DisplayFB);
    free(LayoutFB);
    free(TransitionFB);
}

void LCDGraphic::LayoutChangeBefore() {
	//GraphicClear();
}

void LCDGraphic::LayoutChangeAfter() {
}

void LCDGraphic::GraphicStart() {
    is_resizing_ = false;
    update_thread_->start();
}

void LCDGraphic::GraphicInit(const int rows, const int cols,
    const int yres, const int xres, const int layers, const bool clear_on_layout_change) {
cout << "rows " << rows << " cols " << cols << "-----------======================\n";
    LROWS = rows;
    LCOLS = cols;
    YRES = yres;
    XRES = xres;
    LAYERS = layers;
    clear_on_layout_change_ = clear_on_layout_change;

    DisplayFB = (RGBA **)malloc(sizeof(RGBA) * layers * rows * cols);

    for(int l = 0; l < layers; l++) {
        DisplayFB[l] = (RGBA *)malloc(sizeof(RGBA) * rows * cols);
    }

    LayoutFB = (RGBA **)malloc(sizeof(RGBA) * layers * rows * cols);

    for(int l = 0; l < layers; l++) {
        LayoutFB[l] = (RGBA *)malloc(sizeof(RGBA) * rows * cols);
    }

    TransitionFB = (RGBA **)malloc(sizeof(RGBA) * layers * rows * cols);

    for( int l = 0; l < layers; l++) {
        TransitionFB[l] = (RGBA *)malloc(sizeof(RGBA) * rows * cols);
    }

    /*
    DisplayFB.resize(layers);
    LayoutFB.resize(layers);
    TransitionFB.resize(layers);
    */

    for(int l = 0; l < layers; l++) {
        /*DisplayFB[l].resize(rows * cols);
        LayoutFB[l].resize(rows * cols);
        TransitionFB[l].resize(row * cols);
        */
        for(int i = 0; i < cols * rows; i++) {
            DisplayFB[l][i] = NO_COL;
            LayoutFB[l][i] = NO_COL;
            TransitionFB[l][i] = NO_COL;
        }
    }

    update_window_.R = rows - 1;
    update_window_.C = cols - 1;
    update_window_.H = 0;
    update_window_.W = 0;
}

int LCDGraphic::ResizeLCD(int rows, int cols) {
    RGBA *tmp;

    for(int l = 0; l < LAYERS; l++) {
        free(DisplayFB[l]);
        free(LayoutFB[l]);
        free(TransitionFB[l]);
        tmp = (RGBA *)malloc(rows * cols * sizeof(RGBA));
        if(tmp) {
            DisplayFB[l] = tmp;
        } else {
            return -1;
        }
        tmp = (RGBA *)malloc(rows * cols * sizeof(RGBA));
        if(tmp) {
            LayoutFB[l] = tmp;
        } else {
            return -1;
        }
        tmp = (RGBA *)malloc(rows * cols * sizeof(RGBA));
        if(tmp) {
            TransitionFB[l] = tmp;
        } else {
            return -1;
        }
        for(int n = 0; n  < rows * cols; n++) {
            if(l == 0 && fill_)
                DisplayFB[l][n] = BG_COL;
            else
                DisplayFB[l][n] = NO_COL;
            LayoutFB[l][n] = NO_COL;
            TransitionFB[l][n] = NO_COL;
        }
    }
    LROWS = update_window_.H = rows;
    DROWS = rows;
    LCOLS = update_window_.W = cols;
    DCOLS = cols;
    update_window_.R = 0;
    update_window_.C = 0;
    return 0;
}

void LCDGraphic::ResizeBefore(int rows, int cols) {
    is_resizing_ = true;
}

void LCDGraphic::ResizeAfter() {
    is_resizing_ = false;
}

#define max(a, b) ((a>b)?a:b)
#define min(a, b) ((a<b)?a:b)

void LCDGraphic::GraphicUpdate(int row, int col, int height, int width) {

    int y1 = row + height;
    int x1 = col + width;
    int y2 = update_window_.R + update_window_.H;
    int x2 = update_window_.C + update_window_.W;

    //row = max((int)LROWS - 1, row);
    //col = max((int)LCOLS - 1, col);

    if(y1 > y2)
        update_window_.H = (y1 - y2) + height;
    if(x1 > x2)
        update_window_.W = (x1 - x2) + width;;

    if(row < update_window_.R) {
        update_window_.R = row;
    }
    if(col < update_window_.C) {
        update_window_.C = col;
    }

/*
    int tmp = max((int)LROWS - 1, row + height);
    int delta = LROWS - tmp;
    update_window_.H-=delta;

    tmp = max((int)LCOLS - 1, col + width);
    delta = LCOLS - tmp;
    update_window_.W-=delta;
*/
}

void LCDGraphic::GraphicDraw() {
    while(visitor_->IsActive()) {
        if(update_window_.H == 0 || update_window_.W == 0 || 
            is_resizing_ || transitioning_) {
            usleep(refresh_rate_ * 1000);
            continue;
        }
	
        graphic_mutex_.lock();
        GraphicBlit(update_window_.R, update_window_.C, update_window_.H, update_window_.W);
        graphic_mutex_.unlock();
        update_window_.R = LROWS - 1;
        update_window_.C = LCOLS - 1;
        update_window_.H = 0;
        update_window_.W = 0;
        usleep(refresh_rate_ * 1000);
    }
}

void LCDGraphic::GraphicWindow(int pos, int size, int max, int *wpos, int *wsize)
{
    int p1 = pos;
    int p2 = pos + size;

    *wpos = 0;
    *wsize = 0;

    if (p1 > max || p2 < 0 || size < 1)
        return;

    if (p1 < 0)
        p1 = 0;

    if (p2 > max)
        p2 = max;

    *wpos = p1;
    *wsize = p2 - p1;
}

void LCDGraphic::GraphicBlit(const int row, const int col, const int height, const int width)
{
    if (GraphicRealBlit) {
        int r, c, h, w;
        GraphicWindow(row, height, LROWS, &r, &h);
        GraphicWindow(col, width, LCOLS, &c, &w);
        if (h > 0 && w > 0) {
            for(int rr = r; rr < r + h; rr++) {
                for(int cc = c; cc < c + w; cc++) {
                    for(int l = LAYERS - 1; l >= 0; l-- ) {
                        if(LayoutFB[l][rr * LCOLS + cc] != NO_COL)
                            DisplayFB[l][rr * LCOLS + cc] = 
                                LayoutFB[l][rr * LCOLS + cc];
                    }
                }
            }
            GraphicRealBlit(this, r, c, h, w);
        }
    }
}

inline RGBA LCDGraphic::GraphicBlend(const int row, const int col, RGBA **buffer)
{
    int l, o;
    RGBA p;
    RGBA ret;

    if(buffer == NULL)
        buffer = DisplayFB;

    ret.R = BL_COL.R;
    ret.G = BL_COL.G;
    ret.B = BL_COL.B;
    ret.A = 0x00;

    /* find first opaque layer */
    /* layers below are fully covered */
    o = LAYERS - 1;
    for (l = 0; l < (int)LAYERS; l++) {
        p = buffer[l][row * LCOLS + col];
        if (p.A == 255) {
            o = l;
            break;
        }
    }

    for (l = o; l >= 0; l--) {
        p = buffer[l][row * LCOLS + col];
        switch(p.A) {
        //if(p.A == 255) {
        case 0:
        break;
        case 255:
            ret.R = p.R;
            ret.G = p.G;
            ret.B = p.B;
            ret.A = 0xff;
            break;
        //} else if(p.A != 0) {
        default:
            ret.R = (p.R * p.A + ret.R * (255 - p.A)) / 255;
            ret.G = (p.G * p.A + ret.G * (255 - p.A)) / 255;
            ret.B = (p.B * p.A + ret.B * (255 - p.A)) / 255;
            ret.A = 0xff;
            break;
        //}
        }
    }
    if (INVERTED) {
        ret.R = 255 - ret.R;
        ret.G = 255 - ret.G;
        ret.B = 255 - ret.B;
    }

    return ret;
}


/****************************************/
/*** generic text handling            ***/
/****************************************/

void LCDGraphic::GraphicRender(const int layer, const int row, const int col, 
    const RGBA fg, const RGBA bg, const char *txt, bool bold, int offset, std::string layout)
{
    int c, r, x, y, len;

    /* sanity checks */
    if (layer < 0 || layer >= (int)LAYERS) {
        LCDError("%s: layer %d out of bounds (0..%d)",
        visitor_->GetName().c_str(), layer, LAYERS - 1);
        return;
    }

    len = strlen(txt);

    r = row;
    c = col;

    if(c < 0)
        c = 0;
    else if(c >= LCOLS)
        c = LCOLS - 1;

    RGBA **fb;

    if(IsTransitioning() && layout == GetTransitionLayout())
        fb = TransitionFB;
    else
        fb = LayoutFB;

    if(offset < 0 && c < LCOLS - 2) {
        c+=XRES-(XRES+offset);
    } else if (offset > 0 && c < LCOLS - 2) {
        c-=offset;
    }

    /* render text into layout FB */

    while( *txt != '\0') {
        int *chr;

        if(bold) {
            chr = Font_6x8_bold[(int) *(unsigned char *) txt];
        } else {
            chr = Font_6x8[(int) *(unsigned char *) txt];
        }

        for (y = 0; y < YRES && y + r < LROWS; y++) {
            int mask = 1 << XRES;
            for (x = 0; x < XRES && x + c < LCOLS; x++) {
                mask >>= 1;
                if (chr[y] & mask)
                    fb[layer][(r + y) * LCOLS + c + x] = fg;
                else
                    fb[layer][(r + y) * LCOLS + c + x] = bg;
            }
        }
        c += XRES;
        if (offset > 0 && strlen(txt) == 1) {
            for(y = 0; y < YRES && y + r < LROWS; y++) {
                for(x = 0; x < XRES && x + c + offset < LCOLS; x++) {
                    fb[layer][(r + y) * LCOLS + c + x] = bg;
                }
            }
        }
        txt++;
    }

    /* flush area */
    if(!transitioning_)
        GraphicUpdate(row, col, YRES, XRES * len);
}

void LCDGraphic::GraphicClear() {
    for (int l = 0; l < LAYERS; l++) {
        for (int i = 0; i < LCOLS * LROWS; i++) {
            DisplayFB[l][i] = NO_COL;
            LayoutFB[l][i] = NO_COL;
            TransitionFB[l][i] = NO_COL;
        }
    }

    GraphicUpdate(0, 0, LROWS, LCOLS);
    GraphicBlit(0, 0, LROWS, LCOLS);
}

void LCDGraphic::GraphicFill() {
    for(int i = 0; i < LCOLS * LROWS; i++) {
        DisplayFB[0][i] = BG_COL;
        LayoutFB[0][i] = BG_COL;
        TransitionFB[0][i] = BG_COL;
    }

    GraphicBlit(0, 0, LROWS, LCOLS);
}


RGBA LCDGraphic::GraphicRGB(const int row, const int col)
{
    return GraphicBlend(row, col);
}


unsigned char LCDGraphic::GraphicGray(const int row, const int col)
{
    RGBA p = GraphicBlend(row, col);
    return (77 * p.R + 150 * p.G + 28 * p.B) / 255;
}


unsigned char LCDGraphic::GraphicBlack(const int row, const int col)
{
    return GraphicGray(row, col) < 127;
}

void LCDGraphic::DrawSpecialChar(const int row, const int col, 
    const int height, const int width, const int layer, 
    const RGBA fg, const RGBA bg, SpecialChar ch, std::string layout) {

   RGBA *fb;

    if(IsTransitioning() && layout == GetTransitionLayout())
        fb = TransitionFB[layer];
    else
        fb = LayoutFB[layer];

    for(int y = row; y < row + height; y++) {
        int mask = 1 << width;
        for(int x = col; x < col + width; x++) {
            mask >>= 1;
            if(ch[y - row] & mask) 
                fb[(row + y) * LCOLS + col + x] = fg;
            else
                fb[(row + y) * LCOLS + col + x] = bg;
        }
    }
}


void LCD::GraphicDraw(WidgetText *w) {
    LCDGraphic *lcd = (LCDGraphic *)w->GetVisitor()->GetLCD();
    RGBA fg, bg;

    fg = w->GetFGValid() ? w->GetFGColor() : lcd->FG_COL;
    bg = w->GetBGValid() ? w->GetBGColor() : lcd->BG_COL;

    lcd->graphic_mutex_.lock();
    lcd->GraphicRender(w->GetLayer(), lcd->YRES * w->GetRow(), 
        lcd->XRES * w->GetCol(), fg, bg, w->GetBuffer().c_str(), 
        w->GetBold(), w->GetOffset(), w->GetLayoutBase());
    lcd->graphic_mutex_.unlock();

}

void LCD::GraphicIconDraw(WidgetIcon *w) {
    LCDGraphic *lcd = (LCDGraphic *)w->GetVisitor()->GetLCD();
    SpecialChar bitmap;
    RGBA fg, bg;
    int layer, row, col;
    int x, y;
    int visible;

    bitmap = w->GetBitmap();
    layer = w->GetLayer();
    row = lcd->YRES * w->GetRow();
    col = lcd->XRES * w->GetCol();

    fg = w->GetFGValid() ? w->GetFGColor() : lcd->FG_COL;
    bg = w->GetBGValid() ? w->GetBGColor() : lcd->BG_COL;

    /* sanity check */
    if (layer < 0 || layer >= lcd->LAYERS) {
        LCDError("%s: layer %d out of bounds (0..%d)", 
            lcd->GetVisitor()->GetName().c_str(), 
            layer, lcd->LAYERS - 1);
        return;
    }

    /* Icon visible? */
    visible = w->GetVisible()->P2INT();

    RGBA *fb;

    if(lcd->IsTransitioning() &&
        w->GetLayoutBase() == lcd->GetTransitionLayout())
        fb = lcd->TransitionFB[layer];
    else
        fb = lcd->LayoutFB[layer];

    lcd->graphic_mutex_.lock();

    /* render icon */
    for (y = 0; y < lcd->YRES; y++) {
        int mask = 1 << lcd->XRES;
        for (x = 0; x < lcd->XRES; x++) {
            int i = (row + y) * lcd->LCOLS + col + x;
            mask >>= 1;
            if (visible) {
                if (bitmap[y] & mask)
                    fb[i] = fg;
                else
                    fb[i] = bg;
            } else {
                fb[i] = lcd->BG_COL;
            }
        }
    }

    lcd->graphic_mutex_.unlock();

    /* flush area */
    if(!lcd->IsTransitioning())
        lcd->GraphicUpdate(row, col, lcd->YRES, lcd->XRES);
}

void LCD::GraphicBarDraw(WidgetBar *w) {
    LCDGraphic *lcd = (LCDGraphic *)w->GetVisitor()->GetLCD();
    RGBA fg, bg, bar[2];
    int layer, row, col, len, res, rev, max, val1, val2;
    int x, y;
    DIRECTION dir;
    STYLE style;

    layer = w->GetLayer();
    row = lcd->YRES * w->GetRow();
    col = lcd->XRES * w->GetCol();
    dir = w->GetDirection();
    style = w->GetStyle();
    len = w->GetCols();

    fg = w->GetFGValid() ? w->GetFGColor() : lcd->FG_COL;
    bg = w->GetBGValid() ? w->GetBGColor() : lcd->BG_COL;

    bar[0] = w->GetColorValid()[0] ? w->GetColor()[0] : fg;
    bar[1] = w->GetColorValid()[1] ? w->GetColor()[1] : fg;

    /* sanity check */
    if (layer < 0 || layer >= lcd->LAYERS) {
        LCDError("%s: layer %d out of bounds (0..%d)",
            lcd->GetVisitor()->GetName().c_str(), 
            layer, lcd->LAYERS - 1);
        return;
    }

    res = dir & (DIR_EAST | DIR_WEST) ? lcd->XRES : lcd->YRES;
    max = len * res;
    val1 = w->GetVal1() * (double) (max);
    val2 = w->GetVal2() * (double) (max);

    if (val1 < 1)
        val1 = 1;
    else if (val1 > max)
        val1 = max;

    if (val2 < 1)
        val2 = 1;
    else if (val2 > max)
        val2 = max;

    rev = 0;

    RGBA **fb;

    if(lcd->IsTransitioning() &&
        w->GetLayoutBase() == lcd->GetTransitionLayout())
        fb = lcd->TransitionFB;
    else
        fb = lcd->LayoutFB;

    switch (dir) {
    case DIR_WEST:
        val1 = max - val1;
        val2 = max - val2;
        rev = 1;

    case DIR_EAST:
	lcd->graphic_mutex_.lock();
        for (y = 0; y < lcd->YRES; y++) {
            int val = y < lcd->YRES / 2 ? val1 : val2;
            RGBA bc = y < lcd->YRES / 2 ? bar[0] : bar[1];

            for (x = 0; x < max; x++) {
                if (x < val)
                    fb[layer][(row + y) * lcd->LCOLS + col + x] = rev ? bg : bc;
                else
                    fb[layer][(row + y) * lcd->LCOLS + col + x] = rev ? bc : bg;

                if (style) {
                    fb[layer][(row + 0) * lcd->LCOLS + col + x] = fg;
                    fb[layer][(row + lcd->YRES - 1) * lcd->LCOLS + col + x] = fg;
                }
            }
            if (style) {
                fb[layer][(row + y) * lcd->LCOLS + col] = fg;
                fb[layer][(row + y) * lcd->LCOLS + col + max - 1] = fg;
            }
        }
	lcd->graphic_mutex_.unlock();
        break;

    case DIR_NORTH:
        val1 = max - val1;
        val2 = max - val2;
        rev = 1;

    case DIR_SOUTH:
	lcd->graphic_mutex_.lock();
        for (x = 0; x < lcd->XRES; x++) {
            int val = x < lcd->XRES / 2 ? val1 : val2;
            RGBA bc = x < lcd->XRES / 2 ? bar[0] : bar[1];
            for (y = 0; y < max; y++) {
                if (y < val)
                    fb[layer][(row + y) * lcd->LCOLS + col + x] = rev ? bg : bc;
                else
                    fb[layer][(row + y) * lcd->LCOLS + col + x] = rev ? bc : bg;
                if (style) {
                    fb[layer][(row + y) * lcd->LCOLS + col + 0] = fg;
                    fb[layer][(row + y) * lcd->LCOLS + col + lcd->XRES - 1] = fg;
                }
            }
            if (style) {
                fb[layer][(row + 0) * lcd->LCOLS + col + x] = fg;
                fb[layer][(row + max - 1) * lcd->LCOLS + col + x] = fg;
            }
        }
	lcd->graphic_mutex_.unlock();
        break;
    }

    /* flush area */
    if (dir & (DIR_EAST | DIR_WEST)) {
        if(!lcd->IsTransitioning())

            lcd->GraphicUpdate(row, col, lcd->YRES, lcd->XRES * len);
    } else {
        if(!lcd->IsTransitioning())
            lcd->GraphicUpdate(row, col, lcd->YRES * len, lcd->XRES);
    }

}

void LCD::GraphicHistogramDraw(WidgetHistogram *w) {
    LCDGraphic *lcd = (LCDGraphic *)w->GetVisitor()->GetLCD();
    int layer, row, col, width, height;
    int offset = w->GetOffset();


    layer = w->GetLayer();
    row = w->GetRow() * lcd->YRES;
    col = w->GetCol() * lcd->XRES;
    width = w->GetCols() * lcd->XRES;
    height = w->GetRows() * lcd->YRES;

    if (layer < 0 || layer >= lcd->LAYERS ) {
        LCDError("%s: layer %d out of bounds (0..%d)",
        lcd->GetVisitor()->GetName().c_str(), layer, lcd->LAYERS - 1);
        return;
    }

    RGBA fg = w->GetFGValid() ? w->GetFGColor() : lcd->FG_COL;
    RGBA bg = w->GetBGValid() ? w->GetBGColor() : lcd->BG_COL;

    RGBA *fb;

    if(lcd->IsTransitioning() &&
        w->GetLayoutBase() == lcd->GetTransitionLayout())
        fb = lcd->TransitionFB[layer];
    else
        fb = lcd->LayoutFB[layer];

    lcd->graphic_mutex_.lock();
    for(int c = 0; c < width / lcd->XRES; c++) {
        int base = (int)(w->GetHistory()[c] * height);
        int top = round((w->GetHistory()[c] * height - base) * lcd->YRES);
        for(int y = 0; y < height && row + y < lcd->LROWS; y++) {
            for(int x = 0; x < lcd->XRES && x + col + c * lcd->XRES + offset < lcd->LCOLS; x++) {
                int n = (row + y) * lcd->LCOLS + col + c * lcd->XRES + x + offset;

                if(n < 0)
                    n = 0;

                if(y < height - base - top) {
                    fb[n] = bg;
                } else if( y < height - base ) {
                    if(y < height - base + top) {
                        fb[n] = bg;
                    } else {
                        fb[n] = fg;
                    }
                } else {
                    fb[n] = fg;
                }
            }
        }
    }
    lcd->graphic_mutex_.unlock();

    if(!lcd->IsTransitioning())
        lcd->GraphicUpdate(row, col, height, width);
}

void LCD::GraphicBignumsDraw(WidgetBignums *w) {
    LCDGraphic *lcd = (LCDGraphic *)w->GetVisitor()->GetLCD();
    int layer, row, col;

    layer = w->GetLayer();
    row = w->GetRow() * lcd->YRES;
    col = w->GetCol() * lcd->XRES;

    if (layer < 0 || layer >= lcd->LAYERS ) {
        LCDError("%s: layer %d out of bounds (0..%d)",
        lcd->GetVisitor()->GetName().c_str(), layer, lcd->LAYERS - 1);
        return;
    }

    RGBA fg = w->GetFGValid() ? w->GetFGColor() : lcd->FG_COL;
    RGBA bg = w->GetBGValid() ? w->GetBGColor() : lcd->BG_COL;

    RGBA *fb;

    if(lcd->IsTransitioning() &&
        w->GetLayoutBase() == lcd->GetTransitionLayout())
        fb = lcd->TransitionFB[layer];
    else
        fb = lcd->LayoutFB[layer];

    lcd->graphic_mutex_.lock();
    for(int r = 0; r < 16 && row + r < lcd->LROWS; r++) {
        for(int c = 0; c < 24 && col + c < lcd->LCOLS; c++) {
            int n = (row + r) * lcd->LCOLS + col + c;
            if(w->GetFB()[r * 24 + c] == '.') {
                fb[n] = fg;
            } else {
                fb[n] = bg;
            }
        }
    }
    lcd->graphic_mutex_.unlock();

   if(!lcd->IsTransitioning())
        lcd->GraphicUpdate(row, col, 16, 24);
}

void LCD::GraphicGifDraw(WidgetGif *w) {
    LCDGraphic *lcd = (LCDGraphic *)w->GetVisitor()->GetLCD();
    int layer, row, col, width, height, x, y;
    int visible;

    layer = w->GetLayer();
    row = w->GetRow() * lcd->YRES;
    col = w->GetCol() * lcd->XRES;
    width = w->GetCols();
    height = w->GetRows();

    if (layer < 0 || layer >= lcd->LAYERS ) {
        LCDError("%s: layer %d out of bounds (0..%d)", 
        lcd->GetVisitor()->GetName().c_str(), layer, lcd->LAYERS - 1);
        return;
    }

    if (width <= 0 || height <= 0 || w->GetBitmap() == NULL) {
        return;
    }

    visible = w->GetVisible()->P2INT();

    RGBA **fb;

    if(lcd->IsTransitioning() &&
        w->GetLayoutBase() == lcd->GetTransitionLayout())
        fb = lcd->TransitionFB;
    else
        fb = lcd->LayoutFB;

    lcd->graphic_mutex_.lock();
    for( y = 0; y < height && row + y < lcd->LROWS; y++ ) {
        for ( x = 0; x < width && col + x < lcd->LCOLS; x++ ) {
            int i = (row + y) * lcd->LCOLS + col + x;
            if (visible) {
                fb[layer][i] = w->GetBitmap()[y * width + x];
            } else {
                fb[layer][i] = lcd->BG_COL;
            }
        }
    }
    lcd->graphic_mutex_.unlock();


    if(!lcd->IsTransitioning())
        lcd->GraphicUpdate(row, col, height, width);
}

void GraphicVisualizationPeakDraw(WidgetVisualization *widget) {
    LCDGraphic *lcd = (LCDGraphic *)widget->GetVisitor()->GetLCD();

    int row = widget->GetRow();
    int col = widget->GetCol();
    int width = widget->GetCols();
    int height = widget->GetRows();
    int layer = widget->GetLayer();
    char *buffer = widget->GetPeakBuffer();

    RGBA fg = widget->GetFGValid() ? widget->GetFGColor() : lcd->FG_COL;
    RGBA bg = widget->GetBGValid() ? widget->GetBGColor() : lcd->BG_COL;

    RGBA *fb;

    if(lcd->IsTransitioning() &&
        widget->GetLayoutBase() == lcd->GetTransitionLayout())
        fb = lcd->TransitionFB[layer];
    else
        fb = lcd->LayoutFB[layer];

    lcd->graphic_mutex_.lock();
    for(int y = 0; y < height && row + y < lcd->LROWS; y++) {
        int val = (int)(((double)widget->GetHistory()[y / lcd->YRES][0] /
            (double)SHRT_MAX) * (width / 2 - 1));
        for(int i = 0; i < (width / 2 - 1) - val && col + i < lcd->LCOLS; i++) {
            int n = y * width + i;
            if(buffer[n] == 0)
                buffer[n] = 1;
            else if(buffer[n] == 1)
                buffer[n] = 2;
            else if(buffer[n] == 2)
                buffer[n] = 3;
            else if(buffer[n] == 3)
                buffer[n] = 4;
            else
                buffer[n] = ' ';
        }

        for(int i = (width / 2 - 1) - val; i < (width / 2 - 1); i++) {
            buffer[y * width + i] = ' ';
        }

        buffer[y * width + (width / 2 - 1)] = 5;

        val = (int)(((double)widget->GetHistory()[y][1] /
            (double)SHRT_MAX) * ( width / 2 - 1)) + width / 2;

        for(int i = width / 2; i < val; i++) {
            buffer[y * width + i] = 0;
        }

        for(int i = val; i < width && i + col < lcd->LCOLS / lcd->XRES; i++) {
           int n = y * width + i;
           if(buffer[n] == 0)
                buffer[n] = 1;
            else if(buffer[n] == 1)
                buffer[n] = 2;
            else if(buffer[n] == 2)
                buffer[n] = 3;
            else if(buffer[n] == 3)
                buffer[n] = 4;
            else
                buffer[n] = ' ';
        }
    
        for(int i = 0; i < width; i++) {
            for(int yy = 0; yy < lcd->YRES && row + yy < lcd->LROWS; yy++) {
                for(int xx = 0; xx < lcd->XRES && col + xx < lcd->LCOLS; xx++) {
                    int n = ((row + y) * lcd->YRES + yy) * lcd->LCOLS + 
                        (col + i) * lcd->XRES + xx;
                    switch(buffer[y * width + i]) {
                        case 0:
                        case 1:
                        case 2:
                        case 3:
                        case 4:
                        case 5:
                            if( VISUALIZATION_CHARS[
                                    (int)buffer[y * width + i]][yy] & 
                                    (lcd->XRES - xx - 1) )
                                fb[n] = fg;
                            else
                                fb[n] = bg;
                            break;
                        case ' ':
                            fb[n] = bg;
                            break;
                   }
               }
           }
        }
    }
    lcd->graphic_mutex_.unlock();
}

template <class T>
void GraphicVisualizationPCMDraw(WidgetVisualization *widget) {
    LCDGraphic *lcd = (LCDGraphic *)widget->GetVisitor()->GetLCD();

    int row = widget->GetRow() * lcd->YRES;
    int col = widget->GetCol() * lcd->XRES;
    int width = widget->GetCols();
    int height = widget->GetRows();
    int layer = widget->GetLayer();
    T *buffer = (T *)widget->GetBuffer();
    proxy v = widget->GetProxy();
    int bpp = v.video->bpp;
    VisColor color;

    RGBA *fb;

    if(lcd->IsTransitioning() &&
        widget->GetLayoutBase() == lcd->GetTransitionLayout())
        fb = lcd->TransitionFB[layer];
    else
        fb = lcd->LayoutFB[layer];

    lcd->graphic_mutex_.lock();
    for(int r = 0; r < height && row + r < lcd->LROWS; r++) {
        for(int c = 0; c < width && col + c < lcd->LCOLS; c++) {
            int n = ((row + r) * lcd->LCOLS + col + c) * bpp;
            uint8_t pixel = buffer[r * width + c];
            if(sizeof(T) == sizeof(char)) {
	            fb[n].R = v.pal->colors[pixel].r;
        	    fb[n].G = v.pal->colors[pixel].g;
	            fb[n].B = v.pal->colors[pixel].b;
        	    fb[n].A = 255;
            } else {
		visual_color_from_uint32(&color, pixel);
		fb[n].R = color.r;
		fb[n].G = color.g;
		fb[n].B = color.b;
		fb[n].A = 255;
            }
        }
    }    
    lcd->graphic_mutex_.unlock();

   if(!lcd->IsTransitioning())
        lcd->GraphicUpdate(row, col, height, width);
}

void GraphicVisualizationSpectrumDraw(WidgetVisualization *w) {
    LCDGraphic *lcd = (LCDGraphic *)w->GetVisitor()->GetLCD();

    int row = w->GetRow() * lcd->YRES;
    int col = w->GetCol() * lcd->XRES;
    int width = w->GetCols() * lcd->XRES;
    int height = w->GetRows() * lcd->YRES;
    int layer = w->GetLayer();

    RGBA fg = w->GetFGValid() ? w->GetFGColor() : lcd->FG_COL;
    RGBA bg = w->GetBGValid() ? w->GetBGColor() : lcd->BG_COL;

    int base = SHRT_MAX / height;

    lcd->graphic_mutex_.lock();
    for(int x = 0; x < width && col + 
        x < lcd->LCOLS; x+=lcd->XRES) {
        int val = w->GetHistory()[0][x];

        for(int y = lcd->YRES; y <= height && row + y - lcd->YRES < 
            lcd->LROWS; y+=lcd->YRES) {
            if(val > (y * base))
                lcd->DrawSpecialChar(row + height - y, col + x, 8, 5, layer, fg, bg, 
                    SpecialChar(VISUALIZATION_CHARS[0], 8), w->GetLayoutBase());
            else if (val > ((y-1) * base))
                lcd->DrawSpecialChar(row + height - y, col + x, 8, 5, layer, fg, bg,
                    SpecialChar(VISUALIZATION_CHARS[1], 8), w->GetLayoutBase());
            else
                lcd->DrawSpecialChar(row + height - y, col + x, 8, 5, 
                    layer, lcd->NO_COL, lcd->NO_COL, SpecialChar(8), w->GetLayoutBase());
        }
    }
    lcd->graphic_mutex_.unlock();

   if(!lcd->IsTransitioning())
        lcd->GraphicUpdate(row * lcd->YRES, col * lcd->XRES, 
            height * lcd->YRES, width * lcd->XRES);

}


void GraphicVisualizationDraw(WidgetVisualization *widget) {
    if(widget->GetStyle() == STYLE_PEAK)
        GraphicVisualizationPeakDraw(widget);
    else if(widget->GetStyle() == STYLE_PCM) {
        proxy p = widget->GetProxy();
        if(p.video->bpp == sizeof(char))
	        GraphicVisualizationPCMDraw<unsigned char>(widget);
	else
		GraphicVisualizationPCMDraw<unsigned int>(widget);
    } else if(widget->GetStyle() == STYLE_SPECTRUM)
        GraphicVisualizationSpectrumDraw(widget);
}
/*
void GraphicNiftyDraw(WidgetNifty *widget) {
    LCDGraphic *lcd = (LCDGraphic *)widget->GetVisitor()->GetLCD();

    int row = widget->GetRow() * lcd->YRES;
    int col = widget->GetCol() * lcd->XRES;
    int width = widget->GetCols();
    int height = widget->GetRows();
    int layer = widget->GetLayer();
    RGBA *pixmap = widget->GetPixmap();

    RGBA *fb;

    if(lcd->IsTransitioning() &&
        widget->GetLayoutBase() == lcd->GetTransitionLayout())
        fb = lcd->TransitionFB[layer];
    else
        fb = lcd->LayoutFB[layer];

    for(int r = 0; r < height && row + r < lcd->LROWS; r++) {
        for(int c = 0; c < width && col + c < lcd->LCOLS; c++) {
            int n = (row + r) * lcd->LCOLS + col + c;
            fb[n] = pixmap[n];
        }
    }

   if(!lcd->IsTransitioning())
        lcd->GraphicUpdate(row, col, height, width);

}
*/
void LCDGraphic::Transition() {
    switch(visitor_->GetDirection()) {
    case TRANSITION_LEFT:
    case TRANSITION_RIGHT:
    case TRANSITION_BOTH:
        TransitionLeftRight();
        break;
    case TRANSITION_UP:
    case TRANSITION_DOWN:
        TransitionUpDown();
        break;
    case TRANSITION_TENTACLE:
        TransitionTentacle();
        break;
    case TRANSITION_ALPHABLEND:
        TransitionAlphaBlend();
        break;
    default:
        TransitionLeftRight();
    }
}

void LCDGraphic::TransitionLeftRight() {
    int direction = visitor_->GetDirection();
    int col;
    RGBA **left, **right;

    for(int row = 0; row < LROWS / YRES; row++) {
        if( direction == TRANSITION_LEFT ||
            (direction == TRANSITION_BOTH && row % 2 == 0)) {
            col = LCOLS - transition_tick_ - XRES - 1;
            left = LayoutFB;
            right = TransitionFB;
        } else if( direction == TRANSITION_RIGHT || direction == TRANSITION_BOTH) {
            col = transition_tick_;
            left = TransitionFB;
            right = LayoutFB;
        } else {
            col = transition_tick_;
            left = TransitionFB;
            right = LayoutFB;
            LCDError("LCDGraphic: Bad transition direction: %d", direction);
        }

        if(col < 0)
            col = 0;
        
        if(col + XRES >= LCOLS)
            col-=LCOLS-col;

        for(int i = 0; i < YRES; i++) {
            int n = (row * YRES + i) * LCOLS;
            for(int l = 0; l < LAYERS; l++) {
                memcpy(DisplayFB[l] + n, left[l] + n + (LCOLS - col), 
                    col * sizeof(RGBA));
                memcpy(DisplayFB[l] + n + col + XRES, right[l] + n, 
                    (LCOLS - col - XRES) * sizeof(RGBA));
            }
        }
        GraphicRealBlit(this, row * YRES, 0, YRES, LCOLS);
    }

    transition_tick_+=XRES;
    if( transition_tick_ >= (int)LCOLS) {
        transition_tick_ = 0;
        emit static_cast<LCDEvents *>(
            visitor_->GetWrapper())->_TransitionFinished();
        for(int l = 0; l < LAYERS; l++) {
            memcpy(LayoutFB[l], TransitionFB[l], LCOLS * LROWS * sizeof(RGBA));
            for(int n = 0; n < LCOLS * LROWS; n++)
                TransitionFB[l][n] = NO_COL;
        }
        if(fill_) {
            for(int n = 0; n < LCOLS * LROWS; n++)
                TransitionFB[0][n] = BG_COL;
        }
        transitioning_ = false;
        GraphicBlit(0, 0, LROWS, LCOLS);
    }
    LCDError("TransitionLeftRight %d", transition_tick_);
}

void LCDGraphic::TransitionUpDown() {
    int direction = visitor_->GetDirection();
    int row;
    RGBA **top, **bottom;

    if(direction == TRANSITION_UP) {
        top = LayoutFB;
        bottom = TransitionFB;
        row = LROWS - transition_tick_ - YRES;
    } else {
        top = TransitionFB;
        bottom = LayoutFB;
        row = transition_tick_;
    }

    int o = row * LCOLS;

    LCDError("o: %d, row: %d", o, row);

    for(int r = 0; r < LROWS; r+=YRES) {
        int n = r * LCOLS;
        for(int rr = 0; rr < YRES && r + rr < LROWS; rr++) {
            int nn = n + rr * LCOLS;
            for(int l =0; l < LAYERS; l++) {
                if(row <= (int)r)
                    memcpy(DisplayFB[l] + nn, bottom[l] + nn - o, LCOLS * sizeof(RGBA));
                else
                    memcpy(DisplayFB[l] + nn, top[l] + nn - o, LCOLS * sizeof(RGBA));
            }
            GraphicRealBlit(this, r, 0, YRES, LCOLS);
        }
    }

    transition_tick_+=YRES;
    if( transition_tick_ >= (int)LROWS) {

        transition_tick_ = 0;
        emit static_cast<LCDEvents *>(
            visitor_->GetWrapper())->_TransitionFinished();
        for(int l = 0; l < LAYERS; l++) {
            memcpy(LayoutFB[l], TransitionFB[l], LCOLS * LROWS * sizeof(RGBA));
            for(int n = 0; n < LROWS * LCOLS; n++) {
                TransitionFB[l][n] = NO_COL;
            }
        }
        if(fill_) {
            for(int n = 0; n < LCOLS * LROWS; n++) 
                TransitionFB[0][n] = BG_COL;
        }
        transitioning_ = false;
        GraphicBlit(0, 0, LROWS, LCOLS);
        LCDError("Transition finished");
        return;
    }
    LCDError("Transition iteration finished");
}

void LCDGraphic::SaneCoords(int *x, int *y1, int *y2) {
    if(*x >= (int)LCOLS)
        *x = LCOLS - 1;
    else if(*x < 0)
        *x = 0;

    if(*y1 >= (int)LROWS)
        *y1 = LROWS - 1;
    else if(*y1 < 0) 
        *y1 = 0;

    if(*y2 >= (int)LROWS)
        *y2 = LROWS - 1;
    else if(*y2 < 0)
        *y2 = 0;
}

void LCDGraphic::VlineFromBuffer(RGBA *dest, RGBA *src, int x, int y1, int y2) {
    
    SaneCoords(&x, &y1, &y2);

    for(int i = y1; i < y2; i++)
        dest[i * LCOLS + x] = src[i * LCOLS + x];
}

void LCDGraphic::TransitionTentacle() {
    int height1;
    int height2;
    int add1;
    int add2;

    float sinrate = tentacle_move_;
    float multiplier = 0;
    float multiadd = 1.000 / LCOLS;
    float rate = (LCOLS - transition_tick_) / (double)LCOLS;

    int i, l;

    for(l = 0; l < LAYERS; l++) {
        memcpy(DisplayFB[l], TransitionFB[l], LROWS * LCOLS * sizeof(RGBA));
    }

    for(i = 0; i < LCOLS; i++) {
        add1 = (LROWS / 2) - ((LROWS / 2) * (rate * 1.5));
        add2 = (LROWS / 2) + ((LROWS / 2) * (rate * 1.5));

        height1 = (sin(sinrate) * ((LROWS / 4) * multiplier)) + add1;
        height2 = (sin(sinrate) * ((LROWS / 4) * multiplier)) + add2;
        multiplier += multiadd;

        for(l = 0; l < LAYERS; l++) {
            VlineFromBuffer(DisplayFB[l], LayoutFB[l], i, height1, height2); 
        }
        sinrate += 0.02;
        tentacle_move_ += 0.0002;
    }

    transition_tick_+=XRES;
    if( transition_tick_ >= (int)LCOLS) {
        transition_tick_ = 0;
        emit static_cast<LCDEvents *>(
            visitor_->GetWrapper())->_TransitionFinished();
        for(int l = 0; l < LAYERS; l++) {
            memcpy(LayoutFB[l], TransitionFB[l], LCOLS * LROWS * sizeof(RGBA));
            for(int n = 0; n < LCOLS * LROWS; n++)
                TransitionFB[l][n] = NO_COL;
        }
        if(fill_) {
            for(int n = 0; n < LCOLS * LROWS; n++) 
                TransitionFB[0][n] = BG_COL;
        }
        transitioning_ = false;
        GraphicBlit(0, 0, LROWS, LCOLS);
        return;
    }
    GraphicRealBlit(this, 0, 0, LROWS, LCOLS);
}

RGBA LCDGraphic::BlendRGBA(RGBA col1, RGBA col2, uint8_t alpha) {
    RGBA ret;

    ret.R = (col1.R * alpha + col2.R * (255 - alpha)) / 255;
    ret.G = (col1.G * alpha + col2.G * (255 - alpha)) / 255;
    ret.B = (col1.B * alpha + col2.B * (255 - alpha)) / 255;
    ret.A = 0xff;

    return ret;
}

void LCDGraphic::AlphaBlendBuffer(RGBA **dest, RGBA **src1, RGBA **src2, float alpha) {
    uint8_t ialpha = (alpha * 255);

    for(int row = 0; row < LROWS; row++) {
        for(int col = 0; col < LCOLS; col++) {
            RGBA col1 = GraphicBlend(row, col, src1);
            RGBA col2 = GraphicBlend(row, col, src2);
            dest[0][row * LCOLS + col] = BlendRGBA(col1, col2, ialpha);
        }
    }
}

void LCDGraphic::TransitionAlphaBlend() {

    float rate = (LCOLS - transition_tick_) / (float)LCOLS;

    for(int n = 0; n < LROWS * LCOLS; n++)
        for(int l = 0; l < LAYERS; l++)
            DisplayFB[l][n] = NO_COL;

    AlphaBlendBuffer(DisplayFB, LayoutFB, TransitionFB, rate);

    transition_tick_+=XRES;
    if( transition_tick_ >= (int)LCOLS) {
        transition_tick_ = 0;
        emit static_cast<LCDEvents *>(
            visitor_->GetWrapper())->_TransitionFinished();
        for(int l = 0; l < LAYERS; l++) {
            memcpy(LayoutFB[l], TransitionFB[l], LCOLS * LROWS * sizeof(RGBA));
            for(int n = 0; n < LCOLS * LROWS; n++) {
                DisplayFB[l][n] = NO_COL;
                TransitionFB[l][n] = NO_COL;
            }
        }
        if(fill_) {
            for(int n = 0; n < LCOLS * LROWS; n++) 
                TransitionFB[0][n] = BG_COL;
        }
        transitioning_ = false;
        GraphicBlit(0, 0, LROWS, LCOLS);
        return;
    }
    GraphicRealBlit(this, 0, 0, LROWS, LCOLS);
}
