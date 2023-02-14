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

#include "segnapuntivolley.h"
#include "timeoutwindow.h"
#include "utility.h"


/*!
 * \brief SegnapuntiVolley::SegnapuntiVolley
 * \param myServerUrl
 * \param myLogFile
 */
SegnapuntiVolley::SegnapuntiVolley(const QString &myServerUrl, QFile *myLogFile)
    : ScorePanel(myServerUrl, myLogFile, Q_NULLPTR)
    , iServizio(0)
    , pTimeoutWindow(Q_NULLPTR)
{
    connect(pPanelServerSocket, SIGNAL(textMessageReceived(QString)),
            this, SLOT(onTextMessageReceived(QString)));
    connect(pPanelServerSocket, SIGNAL(binaryMessageReceived(QByteArray)),
            this, SLOT(onBinaryMessageReceived(QByteArray)));

    pSettings = new QSettings("Gabriele Salvato", "Segnapunti Volley");

    // QWidget propagates explicit palette roles from parent to child.
    // If you assign a brush or color to a specific role on a palette and
    // assign that palette to a widget, that role will propagate to all
    // the widget's children, overriding any system defaults for that role.
    pal = QWidget::palette();
    QLinearGradient myGradient = QLinearGradient(0.0, 0.0, 0.0, height());
    myGradient.setColorAt(0, QColor(0, 0, 16));
    myGradient.setColorAt(1, QColor(0, 0, 48));
    QBrush WinBrush(myGradient);
    pal.setBrush(QPalette::Active, QPalette::Window, WinBrush);
//    pal.setColor(QPalette::Window,        Qt::black);
    pal.setColor(QPalette::WindowText,    Qt::yellow);
    pal.setColor(QPalette::Base,          Qt::black);
    pal.setColor(QPalette::AlternateBase, Qt::blue);
    pal.setColor(QPalette::Text,          Qt::yellow);
    pal.setColor(QPalette::BrightText,    Qt::white);
    setPalette(pal);

    maxTeamNameLen = 15;

    buildFontSizes();

    pTimeoutWindow = new TimeoutWindow(Q_NULLPTR);
    createPanelElements();
    buildLayout();
}


/*!
 * \brief SegnapuntiVolley::buildFontSizes
 */
void
SegnapuntiVolley::buildFontSizes() {
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
    for(int i=12; i<100; i++) {
        QFontMetrics f(QFont("Arial", i, QFont::Black));
        int rW = f.horizontalAdvance("Timeout");
        if(rW > width/2) {
            iTimeoutFontSize = i-1;
            break;
        }
    }
    iSetFontSize = 100;
    for(int i=12; i<100; i++) {
        QFontMetrics f(QFont("Arial", i, QFont::Black));
        int rW = f.horizontalAdvance("Set Vinti");
        if(rW > width/2) {
            iSetFontSize = i-1;
            break;
        }
    }
    iServiceFontSize = 100;
    for(int i=12; i<300; i++) {
        QFontMetrics f(QFont("Arial", i, QFont::Black));
        int rW = f.horizontalAdvance(" * ");
        if(rW > width/10) {
            iServiceFontSize = i-1;
            break;
        }
    }
    iScoreFontSize   = 100;
    for(int i=12; i<300; i++) {
        QFontMetrics f(QFont("Arial", i, QFont::Black));
        int rW = f.horizontalAdvance("Punti");
        if(rW > width/6) {
            iScoreFontSize = i-1;
            break;
        }
    }
    int minFontSize = qMin(iScoreFontSize, iTimeoutFontSize);
    minFontSize = qMin(minFontSize, iSetFontSize);
    iScoreFontSize = iTimeoutFontSize = iSetFontSize = minFontSize;
}


/*!
 * \brief SegnapuntiVolley::~SegnapuntiVolley
 */
SegnapuntiVolley::~SegnapuntiVolley() {
    if(pSettings) delete pSettings;
}


/*!
 * \brief SegnapuntiVolley::closeEvent
 * \param event
 */
void
SegnapuntiVolley::closeEvent(QCloseEvent *event) {
    if(pSettings != Q_NULLPTR) delete pSettings;
    pSettings = Q_NULLPTR;
    ScorePanel::closeEvent(event);
    event->accept();
}


/*!
 * \brief SegnapuntiVolley::onBinaryMessageReceived
 * \param baMessage
 */
void
SegnapuntiVolley::onBinaryMessageReceived(QByteArray baMessage) {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Received %1 bytes").arg(baMessage.size()));
    ScorePanel::onBinaryMessageReceived(baMessage);
}


/*!
 * \brief SegnapuntiVolley::onTextMessageReceived
 * \param sMessage
 */
void
SegnapuntiVolley::onTextMessageReceived(QString sMessage) {
    QString sToken;
    bool ok;
    int iVal, iVal0=100, iVal1=100;
    QString sNoData = QString("NoData");

    sToken = XML_Parse(sMessage, "team0");
    if(sToken != sNoData){
        team[0]->setText(sToken.left(maxTeamNameLen));
        int width = QGuiApplication::primaryScreen()->geometry().width();
        for(int i=12; i<100; i++) {
            QFontMetrics f(QFont("Arial", i, QFont::Black));
            int rW = f.horizontalAdvance(team[0]->text()+"  ");
            if(rW > width/2) {
                iVal0 = i-1;
                break;
            }
        }
    }// team0

    sToken = XML_Parse(sMessage, "team1");
    if(sToken != sNoData){
        team[1]->setText(sToken.left(maxTeamNameLen));
        int width = QGuiApplication::primaryScreen()->geometry().width();
        for(int i=12; i<100; i++) {
            QFontMetrics f(QFont("Arial", i, QFont::Black));
            int rW = f.horizontalAdvance(team[1]->text()+"  ");
            if(rW > width/2) {
                iVal1 = i-1;
                break;
            }
        }
    }// team1
    int iSize = std::min(iVal0, iVal1);
    team[0]->setFont(QFont("Arial", iSize, QFont::Black));
    team[1]->setFont(QFont("Arial", iSize, QFont::Black));

    sToken = XML_Parse(sMessage, "set0");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>3)
            iVal = 8;
        set[0]->setText(QString("%1").arg(iVal));
    }// set0

    sToken = XML_Parse(sMessage, "set1");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>3)
            iVal = 8;
        set[1]->setText(QString("%1").arg(iVal));
    }// set1

    sToken = XML_Parse(sMessage, "timeout0");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>2)
            iVal = 8;
        timeout[0]->setText(QString("%1"). arg(iVal));
    }// timeout0

    sToken = XML_Parse(sMessage, "timeout1");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>2)
            iVal = 8;
        timeout[1]->setText(QString("%1"). arg(iVal));
    }// timeout1

    sToken = XML_Parse(sMessage, "startTimeout");
    if(sToken != sNoData) {
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0)
            iVal = 30;
        pTimeoutWindow->startTimeout(iVal*1000);
        pTimeoutWindow->showFullScreen();
    }// timeout1

    sToken = XML_Parse(sMessage, "stopTimeout");
    if(sToken != sNoData) {
        pTimeoutWindow->stopTimeout();
        pTimeoutWindow->hide();
    }// timeout1

    sToken = XML_Parse(sMessage, "score0");
    if(sToken != sNoData) {
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>99)
            iVal = 99;
        score[0]->setText(QString("%1").arg(iVal));
    }// score0

    sToken = XML_Parse(sMessage, "score1");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>99)
            iVal = 99;
        score[1]->setText(QString("%1").arg(iVal));
    }// score1

    sToken = XML_Parse(sMessage, "servizio");
    if(sToken != sNoData){
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<-1 || iVal>1)
            iVal = 0;
        iServizio = iVal;
        if(iServizio == -1) {
            servizio[0]->setText(" ");
            servizio[1]->setText(" ");
        } else if(iServizio == 0) {
            servizio[0]->setText("*");
            servizio[1]->setText(" ");
        } else if(iServizio == 1) {
            servizio[0]->setText(" ");
            servizio[1]->setText("*");
        }
    }// servizio

    ScorePanel::onTextMessageReceived(sMessage);
}


/*!
 * \brief SegnapuntiVolley::createPanelElements
 */
void
SegnapuntiVolley::createPanelElements() {
    // Timeout
    timeoutLabel = new QLabel("Timeout");
    timeoutLabel->setFont(QFont("Arial", iTimeoutFontSize, QFont::Black));
    timeoutLabel->setPalette(pal);
    timeoutLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    for(int i=0; i<2; i++) {
        timeout[i] = new QLabel("8");
        timeout[i]->setFrameStyle(QFrame::NoFrame);
        timeout[i]->setFont(QFont("Arial", 1.5*iTimeoutFontSize, QFont::Black));
    }

    // Set
    setLabel = new QLabel(tr("Set"));
    setLabel->setFont(QFont("Arial", iSetFontSize, QFont::Black));
    setLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    for(int i=0; i<2; i++) {
        set[i] = new QLabel("8");
        set[i]->setFrameStyle(QFrame::NoFrame);
        set[i]->setFont(QFont("Arial", 1.5*iSetFontSize, QFont::Black));
    }

    // Score
    scoreLabel = new QLabel(tr("Punti"));
    scoreLabel->setFont(QFont("Arial", iScoreFontSize, QFont::Black));
    scoreLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    for(int i=0; i<2; i++){
        score[i] = new QLabel("88");
        score[i]->setAlignment(Qt::AlignHCenter);
        score[i]->setFont(QFont("Arial", 3*iTimeoutFontSize, QFont::Black));
        score[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    // Servizio
    for(int i=0; i<2; i++){
        servizio[i] = new QLabel(" ");
        servizio[i]->setFont(QFont("Arial", iServiceFontSize, QFont::Black));
        servizio[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    // Teams
    pal.setColor(QPalette::WindowText, Qt::white);
    for(int i=0; i<2; i++) {
        team[i] = new QLabel();
        team[i]->setPalette(pal);
        team[i]->setFont(QFont("Arial", iTeamFontSize, QFont::Black));
        team[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    team[0]->setText(tr("Locali"));
    team[1]->setText(tr("Ospiti"));
}


/*!
 * \brief SegnapuntiVolley::createPanel
 * \return
 */
QGridLayout*
SegnapuntiVolley::createPanel() {
    QGridLayout *layout = new QGridLayout();

    int ileft  = 0;
    int iright = 1;
    if(isMirrored) {
        ileft  = 1;
        iright = 0;
    }
    QPixmap* pixmapLeftTop = new QPixmap(":/Logo_Unime.png");
    QLabel* leftTopLabel = new QLabel();
    leftTopLabel->setPixmap(*pixmapLeftTop);

    QPixmap* pixmapRightTop = new QPixmap(":/SSD_Unime.jpeg");
    QLabel* rightTopLabel = new QLabel();
    rightTopLabel->setPixmap(*pixmapRightTop);

    layout->addWidget(team[ileft],      0, 0, 2, 6, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(team[iright],     0, 6, 2, 6, Qt::AlignHCenter|Qt::AlignVCenter);

    layout->addWidget(score[ileft],     2, 1, 4, 3, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(servizio[ileft],  2, 4, 4, 1, Qt::AlignLeft   |Qt::AlignTop);
    layout->addWidget(scoreLabel,       2, 5, 4, 2, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(servizio[iright], 2, 7, 4, 1, Qt::AlignRight  |Qt::AlignTop);
    layout->addWidget(score[iright],    2, 8, 4, 3, Qt::AlignHCenter|Qt::AlignVCenter);

    layout->addWidget(set[ileft],       6, 2, 2, 1, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(setLabel,         6, 3, 2, 6, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(set[iright],      6, 9, 2, 1, Qt::AlignHCenter|Qt::AlignVCenter);

    layout->addWidget(leftTopLabel,     8, 0, 2, 2, Qt::AlignLeft   |Qt::AlignBottom);
    layout->addWidget(timeout[ileft],   8, 2, 2, 1, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(timeoutLabel,     8, 3, 2, 6, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(timeout[iright],  8, 9, 2, 1, Qt::AlignHCenter|Qt::AlignVCenter);
    layout->addWidget(rightTopLabel,    8,10, 2, 2, Qt::AlignRight  |Qt::AlignBottom);


    return layout;
}


void
SegnapuntiVolley::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("%1  %2")
                   .arg(setLabel->text())
                   .arg(scoreLabel->text()));
#endif
        setLabel->setText(tr("Set"));
        scoreLabel->setText(tr("Punti"));
    } else
        QWidget::changeEvent(event);
}

