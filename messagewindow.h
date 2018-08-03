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
#ifndef MESSAGEWINDOW_H
#define MESSAGEWINDOW_H

#include <QTimer>
#include <QWidget>
#include <QLabel>
#include <qevent.h>


class MessageWindow : public QWidget
{
    Q_OBJECT

public:
    MessageWindow(QWidget *parent = Q_NULLPTR);
    ~MessageWindow();
    void keyPressEvent(QKeyEvent *event);

public slots:
    void onTimeToMoveLabel();

signals:

public:
    void setDisplayedText(QString sNewText);

private:
    QString sDisplayedText;
    QLabel *pMyLabel;
    QTimer moveTimer;
};

#endif // MESSAGEWINDOW_H
