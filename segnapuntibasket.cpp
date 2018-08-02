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
#include <QStringList>

#include "utility.h"
#include "segnapuntibasket.h"


SegnapuntiBasket::SegnapuntiBasket(const QString &_serverUrl, QFile *_logFile)
    : TimedScorePanel(_serverUrl, _logFile, Q_NULLPTR)
{
    connect(this, SIGNAL(arduinoFound()),
            this, SLOT(onArduinoFound()));
    connect(this, SIGNAL(newTimeValue(QString)),
            this, SLOT(onNewTimeValue(QString)));

    connect(pPanelServerSocket, SIGNAL(textMessageReceived(QString)),
            this, SLOT(onTextMessageReceived(QString)));
    connect(pPanelServerSocket, SIGNAL(binaryMessageReceived(QByteArray)),
            this, SLOT(onBinaryMessageReceived(QByteArray)));

    pSettings = new QSettings("Gabriele Salvato", "Segnapunti Basket");

    pal = QWidget::palette();
    pal.setColor(QPalette::Window,        Qt::black);
    pal.setColor(QPalette::WindowText,    Qt::yellow);
    pal.setColor(QPalette::Base,          Qt::black);
    pal.setColor(QPalette::AlternateBase, Qt::blue);
    pal.setColor(QPalette::Text,          Qt::yellow);
    pal.setColor(QPalette::BrightText,    Qt::white);
    setPalette(pal);

    maxTeamNameLen = 15;

#ifdef Q_OS_ANDROID
    iTimeoutFontSize   = 28;
    iTimeFontSize      = 28;
    iTeamFontSize      = 28;
    iTeamFoulsFontSize = 28;
    iBonusFontSize     = 28;
#else
    buildFontSizes();
#endif
    createPanelElements();
    buildLayout();
}


SegnapuntiBasket::~SegnapuntiBasket() {
    if(pSettings != Q_NULLPTR) delete pSettings;
}


void
SegnapuntiBasket::closeEvent(QCloseEvent *event) {
    if(pSettings != Q_NULLPTR) delete pSettings;
    pSettings = Q_NULLPTR;
    TimedScorePanel::closeEvent(event);
    event->accept();
}


void
SegnapuntiBasket::resizeEvent(QResizeEvent *event) {
    event->accept();
}


void
SegnapuntiBasket::onArduinoFound() {
    requestData.clear();
    requestData.append(quint8(startMarker));
    requestData.append(quint8(11));
    requestData.append(quint8(Configure));
    requestData.append(quint8(BASKET_PANEL));
    quint16 iTime   = 10*60;// Durata del periodo in secondi
    quint16 iPoss24 = 24;
    quint16 iPoss14 = 14;
    requestData.append(quint8(iTime & 0xFF));// LSB first
    requestData.append(quint8(iTime >> 8));  // then MSB
    requestData.append(quint8(iPoss24 & 0xFF));
    requestData.append(quint8(iPoss24 >> 8));
    requestData.append(quint8(iPoss14 & 0xFF));
    requestData.append(quint8(iPoss14 >> 8));
    requestData.append(quint8(endMarker));
    writeSerialRequest(requestData);
}


void
SegnapuntiBasket::buildFontSizes() {
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect  screenGeometry = screen->geometry();
    int width = screenGeometry.width();
    iTeamFontSize = 100;
    for(int i=12; i<100; i++) {
        QFontMetrics f(QFont("Arial", i, QFont::Black));
        int rW = f.maxWidth()*maxTeamNameLen;
        if(rW > width/2) {
            iTeamFontSize = i-1;
            break;
        }
    }
    iTimeoutFontSize = 100;
    for(int i=12; i<300; i++) {
        QFontMetrics f(QFont("Arial", i, QFont::Black));
        int rW = f.horizontalAdvance("* * * ");
        if(rW > width/6) {
            iTimeoutFontSize = i-1;
            break;
        }
    }
    iBonusFontSize = 300;
    for(int i=12; i<300; i++) {
        QFontMetrics f(QFont("Arial", i, QFont::Black));
        int rW = f.horizontalAdvance(" Bonus ");
        if(rW > width/6) {
            iBonusFontSize = i-1;
            break;
        }
    }
    iTimeFontSize = 300;
    for(int i=12; i<300; i++) {
        QFontMetrics f(QFont("Helvetica", i, QFont::Black));
        int rW = f.horizontalAdvance("00:00");
        if(rW > width/2) {
            iTimeFontSize = i-1;
            break;
        }
    }
    iTeamFoulsFontSize = 100;
    for(int i=12; i<300; i++) {
        QFontMetrics f(QFont("Arial", i, QFont::Black));
        int rW = f.horizontalAdvance("Team Fouls");
        if(rW > width/3) {
            iTeamFoulsFontSize = i-1;
            break;
        }
    }
}


void
SegnapuntiBasket::createPanelElements() {
    // Teams
    for(int i=0; i<2; i++) {
        team[i] = new QLabel(QString(maxTeamNameLen, 'W'));
        team[i]->setFont(QFont("Arial", iTeamFontSize, QFont::Black));
        team[i]->setPalette(pal);
        team[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    team[0]->setText(tr("Locali"));
    team[1]->setText(tr("Ospiti"));
    // Score
    for(int i=0; i<2; i++){
        score[i] = new QLCDNumber(3);
        score[i]->setSegmentStyle(QLCDNumber::Filled);
        score[i]->setFrameStyle(QFrame::NoFrame);
        score[i]->setPalette(pal);
        score[i]->display(188);
    }
    // Period
    period = new QLCDNumber(2);
    period->setFrameStyle(QFrame::NoFrame);
    period->setPalette(pal);
    period->display(88);
    // Timeouts
    for(int i=0; i<2; i++) {
        timeout[i] = new QLabel();
        timeout[i]->setFont(QFont("Arial", iTimeoutFontSize, QFont::Black));
        timeout[i]->setPalette(pal);
        timeout[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        timeout[i]->setText("* * *");
    }
    // Possess
    possess[0] = new QLabel("<==");
    possess[0]->setFont(QFont("Times", iTimeoutFontSize, QFont::Black));
    possess[0]->setPalette(pal);
    possess[0]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    possess[1] = new QLabel("==>");
    possess[1]->setFont(QFont("Times", iTimeoutFontSize, QFont::Black));
    possess[1]->setPalette(pal);
    possess[1]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    // Bonus
    for(int i=0; i<2; i++) {
        bonus[i] = new QLabel();
        bonus[i]->setFont(QFont("Arial", iBonusFontSize, QFont::Black));
        bonus[i]->setPalette(pal);
        bonus[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        bonus[i]->setText(" Bonus ");
    }
    // Time
    timeLabel = new QLabel("00:00");
    timeLabel->setFont(QFont("Helvetica", iTimeFontSize, QFont::Black));
    timeLabel->setPalette(pal);
    timeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    // Team Fouls
    foulsLabel = new QLabel("Team Fouls");
    foulsLabel->setFont(QFont("Arial", iTeamFoulsFontSize, QFont::Black));
    foulsLabel->setPalette(pal);
    foulsLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    for(int i=0; i<2; i++) {
        teamFouls[i] = new QLCDNumber(2);
        teamFouls[i]->setFrameStyle(QFrame::NoFrame);
        teamFouls[i]->setPalette(pal);
        teamFouls[i]->display(0);
    }
}


QGridLayout*
SegnapuntiBasket::createPanel() {
    // The panel is a (22x24) grid
    QGridLayout *layout = new QGridLayout();

    if(isMirrored) {// Reflect horizontally to respect teams position on the field
        // Teams
        layout->addWidget(team[1],       0,  0,  4, 12, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(team[0],       0, 12,  4, 12, Qt::AlignHCenter|Qt::AlignVCenter);
        // Score
        layout->addWidget(score[1],      4,  0,  6,  6);
        layout->addWidget(score[0],      4, 18,  6,  6);
        // Possess
        possess[0]->setText("==>");
        possess[1]->setText("<==");
        layout->addWidget(possess[1],    4,  6,  6,  4, Qt::AlignLeft|Qt::AlignVCenter);
        layout->addWidget(possess[0],    4, 14,  6,  4, Qt::AlignRight|Qt::AlignVCenter);
        // Timeouts
        layout->addWidget(timeout[1],   12,  0,  3,  5, Qt::AlignRight|Qt::AlignVCenter);
        layout->addWidget(timeout[0],   12, 19,  3,  5, Qt::AlignLeft|Qt::AlignVCenter);
        // Bonus
        layout->addWidget(bonus[1],     15,  0,  3,  5, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(bonus[0],     15, 19,  3,  5, Qt::AlignHCenter|Qt::AlignVCenter);
        // Team Fouls
        layout->addWidget(teamFouls[1], 19,  3,  3,  2);
        layout->addWidget(foulsLabel,   20,  5,  2, 15, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(teamFouls[0], 19, 20,  3,  2);
    }
    else {
        // Teams
        layout->addWidget(team[0],       0,  0,  4, 12, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(team[1],       0, 12,  4, 12, Qt::AlignHCenter|Qt::AlignVCenter);
        // Score
        layout->addWidget(score[0],      4,  0,  6,  6);
        layout->addWidget(score[1],      4, 18,  6,  6);
        // Possess
        possess[0]->setText("<==");
        possess[1]->setText("==>");
        layout->addWidget(possess[0],    4,  6,  6,  4, Qt::AlignLeft|Qt::AlignVCenter);
        layout->addWidget(possess[1],    4, 14,  6,  4, Qt::AlignRight|Qt::AlignVCenter);
        // Timeouts
        layout->addWidget(timeout[0],   12,  0,  3,  5, Qt::AlignRight|Qt::AlignVCenter);
        layout->addWidget(timeout[1],   12, 19,  3,  5, Qt::AlignLeft|Qt::AlignVCenter);
        // Bonus
        layout->addWidget(bonus[0],     15,  0,  3,  5, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(bonus[1],     15, 19,  3,  5, Qt::AlignHCenter|Qt::AlignVCenter);
        // Team Fouls
        layout->addWidget(teamFouls[0], 19,  3,  3,  2);
        layout->addWidget(foulsLabel,   20,  5,  2, 15, Qt::AlignHCenter|Qt::AlignVCenter);
        layout->addWidget(teamFouls[1], 19, 20,  3,  2);
    }
    // Period
    layout->addWidget(period,            4, 10,  6,  4);
    // Time
    layout->addWidget(timeLabel,        10,  5, 10, 14, Qt::AlignHCenter|Qt::AlignVCenter);

    return layout;
}


void
SegnapuntiBasket::onNewTimeValue(QString sTimeValue) {
    timeLabel->setText(sTimeValue);
}


void
SegnapuntiBasket::onBinaryMessageReceived(QByteArray baMessage) {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Received %1 bytes").arg(baMessage.size()));
    ScorePanel::onBinaryMessageReceived(baMessage);
}


void
SegnapuntiBasket::onTextMessageReceived(QString sMessage) {
    QString sToken;
    bool ok;
    int iVal;
    QString sNoData = QString("NoData");

    sToken = XML_Parse(sMessage, "team0");
    if(sToken != sNoData){
      team[0]->setText(sToken.left(maxTeamNameLen));
      int width = QGuiApplication::primaryScreen()->geometry().width();
      iVal = 100;
      for(int i=12; i<100; i++) {
          QFontMetrics f(QFont("Arial", i, QFont::Black));
          int rW = f.horizontalAdvance(team[0]->text()+"  ");
          if(rW > width/2) {
              iVal = i-1;
              break;
          }
      }
      team[0]->setFont(QFont("Arial", iVal, QFont::Black));
    }// team0

    sToken = XML_Parse(sMessage, "team1");
    if(sToken != sNoData){
      team[1]->setText(sToken.left(maxTeamNameLen));
      int width = QGuiApplication::primaryScreen()->geometry().width();
      iVal = 100;
      for(int i=12; i<100; i++) {
          QFontMetrics f(QFont("Arial", i, QFont::Black));
          int rW = f.horizontalAdvance(team[1]->text()+"  ");
          if(rW > width/2) {
              iVal = i-1;
              break;
          }
      }
      team[1]->setFont(QFont("Arial", iVal, QFont::Black));
    }// team1

    sToken = XML_Parse(sMessage, "period");
    if(sToken != sNoData) {
        QStringList sArgs = sToken.split(",", QString::SkipEmptyParts);
        iVal = sArgs.at(0).toInt(&ok);
        if(!ok || iVal<0 || iVal>99)
            iVal = 99;
        period->display(iVal);
        iVal = sArgs.at(1).toInt(&ok);
        if(!ok || iVal<0 || iVal>10)
            iVal = 10;
        requestData.clear();
        requestData.append(quint8(startMarker));
        requestData.append(quint8(11));
        requestData.append(quint8(Configure));
        requestData.append(quint8(BASKET_PANEL));
        quint16 iTime   = iVal*60;// Durata del periodo in secondi
        quint16 iPoss24 = 24;
        quint16 iPoss14 = 14;
        requestData.append(quint8(iTime & 0xFF));// LSB first
        requestData.append(quint8(iTime >> 8));  // then MSB
        requestData.append(quint8(iPoss24 & 0xFF));
        requestData.append(quint8(iPoss24 >> 8));
        requestData.append(quint8(iPoss14 & 0xFF));
        requestData.append(quint8(iPoss14 >> 8));
        requestData.append(quint8(endMarker));
        writeSerialRequest(requestData);
    }// period

    sToken = XML_Parse(sMessage, "timeout0");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(ok && iVal>=0 && iVal<4) {
            timeout[0]->clear();
            QString sTimeout = QString();
            for(int i=0; i<iVal; i++)
                sTimeout += QString("* ");
            timeout[0]->setText(sTimeout);
        }
    }// timeout0

    sToken = XML_Parse(sMessage, "timeout1");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(ok && iVal>=0 && iVal<4) {
            timeout[1]->clear();
            QString sTimeout = QString();
            for(int i=0; i<iVal; i++)
                sTimeout += QString("* ");
            timeout[1]->setText(sTimeout);
        }
    }// timeout1

    sToken = XML_Parse(sMessage, "score0");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>999)
        iVal = 999;
      score[0]->display(iVal);
    }// score0

    sToken = XML_Parse(sMessage, "score1");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>999)
        iVal = 999;
      score[1]->display(iVal);
    }// score1

    sToken = XML_Parse(sMessage, "possess");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(ok) {
          iPossess = iVal;
          if(iPossess == 0) {
              possess[0]->setStyleSheet("background:black;color:yellow;");
              possess[1]->setStyleSheet("background:black;color:black;");
          }
          else {
              possess[0]->setStyleSheet("background:black;color:black;");
              possess[1]->setStyleSheet("background:black;color:yellow;");
          }
      }
    }// possess

    sToken = XML_Parse(sMessage, "fauls0");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>99)
        iVal = 99;
      teamFouls[0]->display(iVal);
    }// fauls0

    sToken = XML_Parse(sMessage, "fauls1");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>99)
        iVal = 99;
      teamFouls[1]->display(iVal);
    }// fauls1

    sToken = XML_Parse(sMessage, "bonus0");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(ok) {
          if(iVal == 0)
              bonus[0]->setStyleSheet("background:black;color:black;");
          else
              bonus[0]->setStyleSheet("background:red;color:white;");
      }
    }// bonus0

    sToken = XML_Parse(sMessage, "bonus1");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(ok) {
          if(iVal == 0)
              bonus[1]->setStyleSheet("background:black;color:black;");
          else
              bonus[1]->setStyleSheet("background:red;color:white;");
      }
    }// bonus1

    ScorePanel::onTextMessageReceived(sMessage);
}

