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
#include "timeoutwindow.h"
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QScreen>

#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
    #define horizontalAdvance width
#endif

TimeoutWindow::TimeoutWindow(QWidget *parent)
    : QWidget(parent)
{
    Q_UNUSED(parent);
    setMinimumSize(QSize(320, 240));

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect  screenGeometry = screen->geometry();
    int width = screenGeometry.width();
    int height = screenGeometry.height();
    int iFontSize = 400;
    for(int i=48; i<2000; i++) {
        QFontMetrics f(QFont("Arial", i, QFont::Black));
        int rW = f.horizontalAdvance("88");
        int rH = f.height();
        if((rW > width)  || (rH > height)){
            iFontSize = i-1;
            break;
        }
    }

    myLabel.setText(QString("No Text"));
    myLabel.setAlignment(Qt::AlignCenter);
    myLabel.setFont(QFont("Arial", iFontSize, QFont::Black));

    QPalette pal(QWidget::palette());
    pal.setColor(QPalette::Window,        Qt::black);
    pal.setColor(QPalette::WindowText,    Qt::yellow);
    pal.setColor(QPalette::Base,          Qt::black);
    pal.setColor(QPalette::AlternateBase, Qt::blue);
    pal.setColor(QPalette::Text,          Qt::yellow);
    pal.setColor(QPalette::BrightText,    Qt::white);
    setPalette(pal);
    myLabel.setPalette(pal);

    setWindowOpacity(0.8);
    sDisplayedText = tr("-- No Text --");
    myLabel.setText(sDisplayedText);
    QVBoxLayout *panelLayout = new QVBoxLayout();
    panelLayout->addWidget(&myLabel);
    setLayout(panelLayout);

    TimerUpdate.setTimerType(Qt::PreciseTimer);
    connect(&TimerUpdate, SIGNAL(timeout()),
            this, SLOT(updateTime()));
    TimerTimeout.setTimerType(Qt::PreciseTimer);
    TimerTimeout.setSingleShot(true);
    connect(&TimerTimeout, SIGNAL(timeout()),
            this, SLOT(updateTime()));
}


TimeoutWindow::~TimeoutWindow() {
}


void
TimeoutWindow::resizeEvent(QResizeEvent *event) {
    mySize = event->size();
}


void
TimeoutWindow::setDisplayedText(QString sNewText) {
    sDisplayedText = sNewText;
    myLabel.setText(sDisplayedText);
}


void
TimeoutWindow::updateTime() {
    int remainingTime = TimerTimeout.remainingTime();
    sDisplayedText = QString("%1").arg(1+(remainingTime/1000));
    myLabel.setText(sDisplayedText);
    if(remainingTime > 0)
        update();
    else {
        TimerUpdate.stop();
        TimerTimeout.stop();
        hide();
    }
}


void
TimeoutWindow::startTimeout(int msecTime) {
    TimerTimeout.start(msecTime);
    TimerUpdate.start(100);
    sDisplayedText = QString("%1").arg(int(0.999+(TimerTimeout.remainingTime()/1000.0)));
    myLabel.setText(sDisplayedText);
    show();
}


void
TimeoutWindow::stopTimeout() {
    TimerUpdate.stop();
    TimerTimeout.stop();
    hide();
}

