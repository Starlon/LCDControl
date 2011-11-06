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

#ifndef __QT_DISPLAY__
#define __QT_DISPLAY__

#include <vector>
#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QWidget>
#include <Magick++.h>
#include <string>

#include "LCDCore.h"
#include "LCDText.h"
#include "SpecialChar.h"

#define FONT_LEN 256

namespace LCD {

class QtDisplay;

class LCDFont {
    int num_elements;
    QWidget *parent_window;
    QtDisplay *lcdP;

    public:
    std::vector<QPixmap *> pixmaps;
    LCDFont(int, QtDisplay *);
    ~LCDFont();
    void Setup();
    void UpdateFont(int i, SpecialChar matrix);
};

class QtDisplay : public QWidget, public virtual LCDText {
    Q_OBJECT

    std::list<Magick::Image> image_;
    QTimer *image_timer_;
    std::string gif_file_;
    int ani_speed_;

    protected:
    void paintEvent(QPaintEvent *);
    LCDCore *visitor_;

    public:
    int rows;
    int cols;
    int border;
    std::vector<char> ch_data;
    std::vector<std::vector<int>> lcdFonts;
    LCDFont *fontP;
    QColor *bg_color;
    QColor *fg_color;
    struct _dots { int y; int x; } dots;
    struct _pixels { int y; int x; } pixels;
    QtDisplay(LCDCore *visitor, Json::Value *config, 
    int rows, int cols, 
        int yres, int xres, int layers);
    ~QtDisplay();
    void SetVisitor(LCDCore *visitor) { visitor_ = visitor; };
    int GetCharWidth();
    int GetCharHeight();
    int GetBorder() { return border; }
    QPixmap *GetPixmap(int, int);
    void ClearDisplay();

    public slots:
    void UpdateImage();
};


void DefineChar(LCDText *obj, int i, SpecialChar matrix);
void Blit(LCDText *obj, int row, int col, unsigned char *data, 
    int len);

}; // End namespace

#endif
