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
#include <QPaintEvent>
#include <Magick++.h>

#include "QtGraphicDisplay.h"
#include "LCDCore.h"
#include "RGBA.h"
#include "debug.h"

using namespace LCD;

void QtGraphicDisplayBlit(LCDGraphic *lcd, const int row, const int col,
    const int height, const int width) {
    QtGraphicDisplay *qt = dynamic_cast<QtGraphicDisplay *>(lcd);
    qt->QtGraphicBlit(row, col, height, width);
}
// QtGraphicDisplay

QtGraphicDisplay::QtGraphicDisplay(LCDCore *visitor, std::string name, 
    Json::Value *config, int rows, int cols, 
    int yres, int xres, int layers) : 
    LCDGraphic(visitor) {
    this->setAttribute(Qt::WA_StaticContents);
    //this->setAttribute(Qt::WA_OpaquePaintEvent);

    visitor_ = visitor;
    GraphicInit(rows, cols, 8, 6, layers);
    bg_color_ = new QColor(0x78a878);
    fg_color_ = new QColor(0x113311);
    pixels.y = 1;
    pixels.x = 1;
    border_ = 0;
    resize(LCOLS * pixels.x + border_ * 2, LROWS * pixels.y + border_ * 2);
    setPalette(QPalette(*bg_color_));
    setAutoFillBackground(true);
    this->show();

    GraphicRealBlit = QtGraphicDisplayBlit;

    displayFB_ = new RGBA[LROWS * LCOLS];

    ClearDisplay();

    Json::Value *val = visitor_->CFG_Fetch_Raw(config, 
        visitor->GetName() + ".gif-file", new Json::Value(""));
    gif_file_ = val->asString();
    delete val;

    ani_speed_ = 1;

    image_timer_ = new QTimer();
    image_timer_->setInterval(ani_speed_);
    QObject::connect(image_timer_, SIGNAL(timeout()), this, SLOT(UpdateImage()));
    if(gif_file_ != "")
        image_timer_->start();

    val = visitor->CFG_Fetch_Raw(visitor->CFG_Get_Root(), name + ".pixels");
    if(val) {
        sscanf(val->asCString(), "%dx%d", &pixels.x, &pixels.y);
        delete val;
    }

    thread_ = new QtGraphicThread(this);

    ClearDisplay();
    GraphicStart();
}

QtGraphicDisplay::~QtGraphicDisplay() {
    delete bg_color_;
    delete fg_color_;
    delete image_timer_;
    delete [] displayFB_;
    if(gif_file_ != "") {
        for_each( image_.begin(), image_.end(), Magick::animationDelayImage(ani_speed_*60/100));
        Magick::writeImages(image_.begin(), image_.end(), gif_file_);
    }
}

void QtGraphicDisplay::UpdateImage() {
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
            *gif_pixel = Magick::ColorRGB(ss_pixel.red() / 256.0, ss_pixel.green() / 256.0, ss_pixel.blue() / 256.0);
        }
    }
    image.syncPixels();
    image_.push_back(image);
}

void QtGraphicDisplay::paintEvent(QPaintEvent *event) {
    QRect rect = event->rect();
    QPainter qpainter (this);
    qpainter.setPen(*fg_color_);

    int y = rect.y();
    int x = rect.x();
    int height = rect.height();
    int width  = rect.width();

    for(int row = y; row + pixels.y < y + height; row+=pixels.y) {
    //for(int row = 0; row < LROWS; row++) {
        for(int col = x; col + pixels.x < x + width; col+=pixels.x) {
         //for(int col = 0; col < LCOLS; col++) {
            RGBA rgb = GraphicRGB(row / pixels.y, col / pixels.x);
            //RGBA rgb = GraphicRGB(row, col);
            QColor color(rgb.R, rgb.G, rgb.B, rgb.A);
            qpainter.setPen(color);
            qpainter.setBrush(color);
            qpainter.drawRect(col + border_, row + border_,
                pixels.x, pixels.y);
            //qpainter.drawRect(col * pixels.x + border_, row * pixels.y + border_,
            //    pixels.x, pixels.y);
        }
    }
}

void QtGraphicDisplay::ThreadRun() {

}

void QtGraphicDisplay::ClearDisplay() {
    GraphicClear();
    GraphicFill();
    QtGraphicBlit(0, 0, LROWS, LCOLS);
    /*
    for(int i = 0; i < LROWS * LCOLS; i++ ) {
        displayFB_[i] = BG_COL;
    }
    update(0, 0, LCOLS * pixels.x, LROWS * pixels.y);
    */
    //update();
}

void QtGraphicDisplay::QtGraphicBlit(const int row, const int col, 
    const int height, const int width) {
    update(col * pixels.x , row * pixels.y, width * pixels.y, height * pixels.x);
    //update();
}
