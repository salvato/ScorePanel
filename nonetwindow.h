/*
 *
Copyright (C) 2016  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/
#ifndef NONETWINDOW_H
#define NONETWINDOW_H

#include <QTimer>
#include <QWidget>
#include <QLabel>
#include <qevent.h>


class NoNetWindow : public QWidget
{
    Q_OBJECT

public:
    NoNetWindow(QWidget *parent = 0);
    ~NoNetWindow();
    void keyPressEvent(QKeyEvent *event);

public:
    void setDisplayedText(QString sNewText);
private:

public slots:
    void resizeEvent(QResizeEvent *event);

signals:

private:
    QSize mySize;
    QString sDisplayedText;
    QLabel myLabel;
};

#endif // NONETWINDOW_H
