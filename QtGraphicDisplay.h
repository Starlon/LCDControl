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

#ifndef __QT_GRAPHIC_DISPLAY__
#define __QT_GRAPHIC_DISPLAY__

#include <vector>
#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QWidget>
#include <QThread>
#include <Magick++.h>
#include <string>

#include "LCDCore.h"
#include "LCDGraphic.h"
#include "RGBA.h"

namespace LCD {

class QtGraphicThread;

class QtGraphicDisplay : public QWidget, public virtual LCDGraphic {
    Q_OBJECT

    QtGraphicThread *thread_;

    std::list<Magick::Image> image_;
    QTimer *image_timer_;
    std::string gif_file_;
    int ani_speed_;

    protected:
    void paintEvent(QPaintEvent *);
    LCDCore *visitor_;

    public:
    int border_;
    QColor *bg_color_;
    QColor *fg_color_;
    RGBA *displayFB_;
    struct _dots { int y; int x; } dots;
    struct _pixels { int y; int x; } pixels;
    QtGraphicDisplay(LCDCore *visitor, std::string name, Json::Value *config, 
        int rows, int cols, 
        int yres, int xres, int layers);
    ~QtGraphicDisplay();
    void SetVisitor(LCDCore *visitor) { visitor_ = visitor; };
    int GetBorder() { return border_; }
    int YPixels() { return pixels.y; }
    int XPixels() { return pixels.x; }
    QPixmap *GetPixmap(int, int);
    void ClearDisplay();
    void QtGraphicBlit(const int row, const int col, const int height, const int width);
    void ThreadRun();

    public slots:
    void UpdateImage();
};

class QtGraphicThread: public QThread {
    Q_OBJECT
    QtGraphicDisplay *visitor_;

    protected:
    void run() { visitor_->ThreadRun(); }

    public:
    QtGraphicThread(QtGraphicDisplay *v) { visitor_ = v; }
};

void QtGraphicBlit(LCDText *obj, int row, int col, unsigned char *data, 
    int len);

}; // End namespace

#endif
