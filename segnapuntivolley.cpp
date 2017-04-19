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

#include <QtGlobal>
#include <QtNetwork>
#include <QtWidgets>
#include <QProcess>
#include <QWebSocket>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QTime>
#include <QSettings>

#include "segnapuntivolley.h"

//#define QT_DEBUG
#define LOG_MESG


SegnapuntiVolley::SegnapuntiVolley(QWebSocket *_pWebSocket, QFile *_logFile, bool bReflected)
    : ScorePanel(_pWebSocket, _logFile, Q_NULLPTR)
    , isMirrored(bReflected)
    , iServizio(0)
{
    QString sFunctionName = " SegnapuntiVolley::SegnapuntiVolley ";
    Q_UNUSED(sFunctionName)

    connect(pWebSocket, SIGNAL(textMessageReceived(QString)),
            this, SLOT(onTextMessageReceived(QString)));
    connect(pWebSocket, SIGNAL(binaryMessageReceived(QByteArray)),
            this, SLOT(onBinaryMessageReceived(QByteArray)));

    pSettings = new QSettings(tr("Gabriele Salvato"), tr("Segnapunti Volley"));
    mySize = size();

    pal = QWidget::palette();
    pal.setColor(QPalette::Window,        Qt::black);
    pal.setColor(QPalette::WindowText,    Qt::yellow);
    pal.setColor(QPalette::Base,          Qt::black);
    pal.setColor(QPalette::AlternateBase, Qt::blue);
    pal.setColor(QPalette::Text,          Qt::yellow);
    pal.setColor(QPalette::BrightText,    Qt::white);
    setPalette(pal);

#ifdef Q_OS_ANDROID
    iTimeoutFontSize = 28;
    iSetFontSize     = 28;
    iServiceFontSize = 28;
    iScoreFontSize   = 28;
    iTeamFontSize    = 28;
#else
    iTimeoutFontSize = 66;
    iSetFontSize     = 66;
    iServiceFontSize = 96;
    iScoreFontSize   = 66;
    iTeamFontSize    = 66;
#endif

    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addLayout(createPanel());
    setLayout(mainLayout);
}


SegnapuntiVolley::~SegnapuntiVolley() {
}


void
SegnapuntiVolley::onBinaryMessageReceived(QByteArray baMessage) {
    QString sFunctionName = " SegnapuntiVolley::onBinaryMessageReceived ";
    Q_UNUSED(sFunctionName)
    logMessage(sFunctionName, QString("Received %1 bytes").arg(baMessage.size()));
    ScorePanel::onBinaryMessageReceived(baMessage);
}


void
SegnapuntiVolley::onTextMessageReceived(QString sMessage) {
    QString sFunctionName = " SegnapuntiVolley::onTextMessageReceived ";
    Q_UNUSED(sFunctionName)
    QString sToken;
    bool ok;
    int iVal;

    sToken = XML_Parse(sMessage, "team0");
    if(sToken != sNoData){
      team[0]->setText(sToken);
    }// team0

    sToken = XML_Parse(sMessage, "team1");
    if(sToken != sNoData){
      team[1]->setText(sToken);
    }// team1

    sToken = XML_Parse(sMessage, "set0");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>3)
        iVal = 8;
      set[0]->display(iVal);
    }// set0

    sToken = XML_Parse(sMessage, "set1");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>3)
        iVal = 8;
      set[1]->display(iVal);
    }// set1

    sToken = XML_Parse(sMessage, "timeout0");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>2)
        iVal = 8;
      timeout[0]->display(iVal);
    }// timeout0

    sToken = XML_Parse(sMessage, "timeout1");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>2)
        iVal = 8;
      timeout[1]->display(iVal);
    }// timeout1

    sToken = XML_Parse(sMessage, "score0");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>99)
        iVal = 99;
      score[0]->display(iVal);
    }// score0

    sToken = XML_Parse(sMessage, "score1");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>99)
        iVal = 99;
      score[1]->display(iVal);
    }// score1

    sToken = XML_Parse(sMessage, "servizio");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<-1 || iVal>1)
        iVal = 0;
      iServizio = iVal;
      if(iServizio == -1) {
        servizio[0]->setText(tr(" "));
        servizio[1]->setText(tr(" "));
      } else if(iServizio == 0) {
        servizio[0]->setText(tr("*"));
        servizio[1]->setText(tr(" "));
      } else if(iServizio == 1) {
        servizio[0]->setText(tr(" "));
        servizio[1]->setText(tr("*"));
      }
    }// servizio

    ScorePanel::onTextMessageReceived(sMessage);
}


QGridLayout*
SegnapuntiVolley::createPanel() {
    QGridLayout *layout = new QGridLayout();

    QFont *font;
    QLabel *label;

    font = new QFont("Arial", iTimeoutFontSize, QFont::Black);
    label = new QLabel("Timeout");
    label->setFont(*font);
    label->setPalette(pal);
    label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    for(int i=0; i<2; i++) {
        timeout[i] = new QLCDNumber(1);
        timeout[i]->setFrameStyle(QFrame::NoFrame);
        timeout[i]->setPalette(pal);
        timeout[i]->display(8);
    }
    if(isMirrored) {
        // Reflecting horizontally to respect
        // teams position on the field
        layout->addWidget(timeout[1], 0, 2, 2, 1);
        layout->addWidget(label,      0, 3, 1, 6, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(timeout[0], 0, 9, 2, 1);
    } else {
        layout->addWidget(timeout[0], 0, 2, 2, 1);
        layout->addWidget(label,      0, 3, 1, 6, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(timeout[1], 0, 9, 2, 1);
    }

    font = new QFont("Arial", iSetFontSize, QFont::Black);
    label = new QLabel("Set Vinti");
    label->setFont(*font);
    label->setPalette(pal);
    label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    for(int i=0; i<2; i++) {
        set[i] = new QLCDNumber(1);
        set[i]->setFrameStyle(QFrame::NoFrame);
        set[i]->setPalette(pal);
        set[i]->display(8);
    }
    if(isMirrored) {
        // Reflecting horizontally to respect
        // teams position on the field
        layout->addWidget(set[1], 2, 2, 2, 1);
        layout->addWidget(label,  2, 3, 1, 6, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(set[0], 2, 9, 2, 1);
    } else {
        layout->addWidget(set[0], 2, 2, 2, 1);
        layout->addWidget(label,  2, 3, 1, 6, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(set[1], 2, 9, 2, 1);
    }

    font = new QFont("Arial", iScoreFontSize, QFont::Black);
    scoreLabel = new QLabel(tr("Punti"));
    scoreLabel->setFont(*font);
    scoreLabel->setPalette(pal);
    scoreLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    font = new QFont("Arial", iServiceFontSize, QFont::Black);

    for(int i=0; i<2; i++){
        servizio[i] = new QLabel(tr(" "));
        servizio[i]->setFont(*font);
        servizio[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        score[i] = new QLCDNumber(2);
        score[i]->setSegmentStyle(QLCDNumber::Filled);
        score[i]->setFrameStyle(QFrame::NoFrame);
        score[i]->setPalette(pal);
        score[i]->display(88);
    }
    if(isMirrored) {
        // Reflect horizontally to respect
        // teams position on the field
        layout->addWidget(score[1],    4, 1, 4, 3);
        layout->addWidget(servizio[1], 4, 4, 4, 1, Qt::AlignLeft|Qt::AlignVCenter);
        layout->addWidget(scoreLabel,  4, 5, 4, 2, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(servizio[0], 4, 7, 4, 1, Qt::AlignRight|Qt::AlignVCenter);
        layout->addWidget(score[0],    4, 8, 4, 3);

    } else {
        layout->addWidget(score[0],    4, 1, 4, 3);
        layout->addWidget(servizio[0], 4, 4, 4, 1, Qt::AlignLeft|Qt::AlignVCenter);
        layout->addWidget(scoreLabel,  4, 5, 4, 2, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(servizio[1], 4, 7, 4, 1, Qt::AlignRight|Qt::AlignVCenter);
        layout->addWidget(score[1],    4, 8, 4, 3);
    }

    font = new QFont("Arial", iTeamFontSize, QFont::Black);
//    QFontMetrics fm(*font);
//    int width = fm.width(tr("888888888888888"));
//    qDebug() << "Width of team string" << width;
    for(int i=0; i<2; i++) {
        team[i] = new QLabel();
        team[i]->setFont(*font);
        team[i]->setPalette(pal);
        team[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    if(isMirrored) {
        // Reflect horizontally to respect
        // teams position on the field
        layout->addWidget(team[1],    8, 0, 1, 5, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(team[0],    8, 7, 1, 5, Qt::AlignHCenter|Qt::AlignVCenter);
        team[1]->setText(tr("Locali"));
        team[0]->setText(tr("Ospiti"));
    } else {
        layout->addWidget(team[0],    8, 0, 1, 5, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(team[1],    8, 7, 1, 5, Qt::AlignHCenter|Qt::AlignVCenter);
        team[0]->setText(tr("Locali"));
        team[1]->setText(tr("Ospiti"));
    }
    return layout;
}

