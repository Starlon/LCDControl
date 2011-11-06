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
#include <string>
#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QWidget>
#include <Magick++.h>

#include "QtDisplay.h"
#include "LCDCore.h"
#include "debug.h"

#define FONT_LEN 256

extern int Font_6x8[256][8];

using namespace LCD;

unsigned char icons[8][8] {
    {0, 8,12,14,15,14,12, 8}, //right arrow
    {0, 1, 3, 7,15, 7, 3, 1},  //left arrow
    {0, 4, 4,14,14,31,31, 0},  //up arrow  
    {0,31,31,14,14, 4, 4, 0},  //down arrow  
    {0, 0,14,31,31,31,14, 0},  //circle  
    {0, 1, 3, 3, 6,22,28,12},  //check  
    {0,17,27,14, 4,14,27,17},  //cancel  
    {4,14,63, 4, 4,63,14, 4}}; //Up/Dn Arrow

QPixmap *CreatePixmapFromData(QtDisplay *, int *);

void LCD::Blit(LCDText *obj, int row, int col, 
    unsigned char *data, int len) {
    QtDisplay *self = dynamic_cast<QtDisplay *>(obj);

    if( col + len > self->LCOLS ) {
        len = len - ((col + len) - self->LCOLS); 
    }

    for(int i = 0; i < len; i++) {
        self->ch_data[row * self->LCOLS + col + i] = data[i];
    }

    self->repaint((col * self->GetCharWidth()) + self->border, 
        (row * self->GetCharHeight()) + self->border, 
        len * self->GetCharWidth(), self->GetCharHeight());
}

void LCD::DefineChar(LCDText *obj, int i, SpecialChar matrix) {
    QtDisplay *self = dynamic_cast<QtDisplay *>(obj);
    self->lcdFonts[i] = matrix.Vector();
    self->fontP->UpdateFont(i, matrix);
    self->update();
}

// LCDFont
//
LCDFont::LCDFont(int num, QtDisplay *p) {
    num_elements = num;
    lcdP = p;
    pixmaps.resize(num);
    Setup();
}

LCDFont::~LCDFont() {
    for(int i = 0; i < num_elements; i++) {
        if(pixmaps[i])
            delete pixmaps[i];
    }
}

void LCDFont::Setup() {
    for(int i = 0; i < num_elements; i++ ) {
        pixmaps[i] = CreatePixmapFromData(lcdP, Font_6x8[i]);
    }
}

void LCDFont::UpdateFont(int i, SpecialChar font) {
    if(i >= 8) return;
    int data[font.Size()];
    font.Data(data);
    delete pixmaps[i];
    pixmaps[i] = CreatePixmapFromData(lcdP, data);
}

// QtDisplay

QtDisplay::QtDisplay(LCDCore *visitor, Json::Value *config, 
    int rows, int cols, 
    int yres, int xres, int layers) : 
    LCDText(visitor) {
    this->setAttribute(Qt::WA_StaticContents);
    //this->setAttribute(Qt::WA_OpaquePaintEvent);

    visitor_ = visitor;
    TextInit(rows, cols, yres, xres, 0, 8, 0, layers);
    bg_color = new QColor(0x78a878);
    fg_color = new QColor(0x113311);
    dots.y = 8;
    dots.x = 6;
    pixels.y = 10;
    pixels.x = 10;
    border = 10;
    fontP = new LCDFont(FONT_LEN, this);
    lcdFonts.resize(FONT_LEN);
    ch_data.resize(LROWS * LCOLS);
    resize(LCOLS * GetCharWidth() + border * 2, LROWS * GetCharHeight() + border * 2);
    setPalette(QPalette(*bg_color));
    setAutoFillBackground(true);
    ClearDisplay();
    this->show();
    TextRealDefChar = DefineChar;
    TextRealBlit = Blit;

    Json::Value *val = visitor_->CFG_Fetch_Raw(config, 
        visitor->GetName() + ".gif-file", new Json::Value(""));
    gif_file_ = val->asString();
    delete val;

    ani_speed_ = 31;

    image_timer_ = new QTimer();
    image_timer_->setInterval(ani_speed_);
    QObject::connect(image_timer_, SIGNAL(timeout()), this, SLOT(UpdateImage()));
    if(gif_file_ != "")
        image_timer_->start();

}

QtDisplay::~QtDisplay() {
    delete bg_color;
    delete fg_color;
    delete fontP;
    delete image_timer_;
    if(gif_file_ != "") {
        for_each( image_.begin(), image_.end(), Magick::animationDelayImage(ani_speed_*60/100));
        Magick::writeImages(image_.begin(), image_.end(), gif_file_);
    }
}

void QtDisplay::UpdateImage() {
    QImage snapshot(size(), QImage::Format_RGB32);
    render(&snapshot);
    Magick::Image image;
    image.magick("GIF");
    image.size(Magick::Geometry(snapshot.width(), snapshot.height()));
    Magick::PixelPacket *pixel_cache = image.getPixels(0, 0, snapshot.width(), snapshot.height());
    for(int row = 0; row < snapshot.height(); row++) {
        for(int col = 0; col < snapshot.width(); col++) {
            QColor ss_pixel = QColor::fromRgb(snapshot.pixel(col, row));
            Magick::PixelPacket *gif_pixel = pixel_cache + row * snapshot.width() + col;
            *gif_pixel = Magick::ColorRGB(ss_pixel.red() / 256.0, 
                ss_pixel.green() / 256.0, ss_pixel.blue() / 256.0);
        }
    }
    image.syncPixels();
    image_.push_back(image);
}

void QtDisplay::paintEvent(QPaintEvent *event) {
    QPainter qpainter (this);
    int cw = GetCharWidth();
    int ch = GetCharHeight();
    for(int j = 0; j < LROWS; j++) {
        for(int i = 0; i < LCOLS; i++) {
            QPixmap *pixmap = GetPixmap(j, i);
            if(!pixmap) {LCDError("QtDisplay: No pixmap."); continue; }
            qpainter.drawPixmap(border+i*cw, border+j*ch, *GetPixmap(j, i));
        }
    }
}

QPixmap *QtDisplay::GetPixmap(int row, int col) {
    return fontP->pixmaps[(int)ch_data[row * LCOLS + col]];
}

int QtDisplay::GetCharWidth() {
    return dots.x * pixels.x;
}

int QtDisplay::GetCharHeight() {
    return dots.y * pixels.y;
}

void QtDisplay::ClearDisplay() {
    for(int i = 0; i < LROWS * LCOLS; i++ ) {
        ch_data[i] = ' ';
    }
    update();
}

QPixmap *CreatePixmapFromData(QtDisplay *lcdP, int *ch) {
    int width = lcdP->GetCharWidth();
    int height = lcdP->GetCharHeight();
    int k, m, mask;
    QPixmap *pixmap = new QPixmap( width, height );
    pixmap->fill(*lcdP->bg_color);

    QPainter *painter = new QPainter(pixmap);

    painter->setPen( *lcdP->fg_color );

    for(int j = 0; j < lcdP->dots.y; j++ ) {
        k = j * lcdP->pixels.y;
        mask = 1<<lcdP->dots.x;
        for(int i = 0; i < lcdP->dots.x; i++ ) {
            mask>>= 1;
            if( (mask & ch[j]) == 0 )
                continue;

            m = i * lcdP->pixels.y;

            for(int jj = k; jj < k+lcdP->pixels.y-1; jj++)
            {
                for(int ii = m+1; ii < m+lcdP->pixels.x; ii++)
                {
                    painter->drawPoint(ii, jj);
                }
            }
        }
    }

    delete painter;
    return pixmap;
}
