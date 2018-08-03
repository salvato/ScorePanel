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
#include <QDir>
#include <QDebug>
#include <QVBoxLayout>
#include <QApplication>
#include <QDesktopWidget>

#include "messagewindow.h"


MessageWindow::MessageWindow(QWidget *parent)
    : QWidget(parent)
{
    Q_UNUSED(parent);
    setMinimumSize(QSize(320, 240));

    pMyLabel = new QLabel(this);
    pMyLabel->setFont(QFont("Arial", 24));
    pMyLabel->setAlignment(Qt::AlignCenter);
    QPalette pal(QWidget::palette());
    pal.setColor(QPalette::Window,        Qt::black);
    pal.setColor(QPalette::WindowText,    Qt::yellow);
    pal.setColor(QPalette::Base,          Qt::black);
    pal.setColor(QPalette::AlternateBase, Qt::blue);
    pal.setColor(QPalette::Text,          Qt::yellow);
    pal.setColor(QPalette::BrightText,    Qt::white);
    setPalette(pal);
    pMyLabel->setPalette(pal);
    setWindowOpacity(0.8);
    sDisplayedText = tr("No Text");
    pMyLabel->setText(sDisplayedText);
    QVBoxLayout *panelLayout = new QVBoxLayout();
    panelLayout->addWidget(pMyLabel);
    setLayout(panelLayout);
}


MessageWindow::~MessageWindow() {
}


void
MessageWindow::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Escape) {
        close();
    }
    if(event->key() == Qt::Key_F1) {
        showNormal();
        event->accept();
        return;
    }
}


void
MessageWindow::setDisplayedText(QString sNewText) {
    sDisplayedText = sNewText;
    pMyLabel->setText(sDisplayedText);
}

