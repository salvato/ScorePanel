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


#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    #include "pigpiod_if2.h"
#endif

#include "slidewindow.h"
#include "fileupdater.h"
#include "scorepanel.h"
#include "utility.h"

#define FILE_UPDATE_PORT      45455

//#define QT_DEBUG
#define LOG_MESG


// For Raspberry Pi GPIO pin numbering see https://pinout.xyz/
//
// +5V pins 2 or 4 in the 40 pin GPIO connector.
// GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
// in the 40 pin GPIO connector.
//
// Samwa servo pinout
// 1) PWM Signal
// 2) GND
// 3) +5V


ScorePanel::ScorePanel(QUrl serverUrl, QFile *_logFile, QWidget *parent)
    : QWidget(parent)
    , pServerSocket(Q_NULLPTR)
    , videoPlayer(NULL)
    , cameraPlayer(NULL)
    , iCurrentSpot(0)
    , logFile(_logFile)
    , panPin(14) // GPIO Numbers are Broadcom (BCM) numbers
                 // BCM14 is Pin 8 in the 40 pin GPIO connector.
    , tiltPin(26)// GPIO Numbers are Broadcom (BCM) numbers
                 // BCM26 IS Pin 37 in the 40 pin GPIO connector.
    , gpioHostHandle(-1)
{
    QString sFunctionName = " ScorePanel::ScorePanel ";
    Q_UNUSED(sFunctionName)

    pSettings = new QSettings(tr("Gabriele Salvato"), tr("Score Panel"));

    pUpdaterThread = Q_NULLPTR;
    pFileUpdater   = Q_NULLPTR;
    fileUpdatePort = FILE_UPDATE_PORT;
    mySize         = size();

    QTime time(QTime::currentTime());
    qsrand(time.msecsSinceStartOfDay());

    QString sBaseDir;
#ifdef Q_OS_ANDROID
    sBaseDir = QString("/storage/extSdCard/");
#else
    sBaseDir = QDir::homePath();
    if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");
#endif
    sSpotDir    = QString("%1spots/").arg(sBaseDir);

    initCamera();
    // Turns off the default window title hints.
    setWindowFlags(Qt::CustomizeWindowHint);

    // Slide Window
    pMySlideWindow = new SlideWindow();
    pMySlideWindow->stopSlideShow();
    pMySlideWindow->hide();
    connect(pMySlideWindow, SIGNAL(getNextImage()),
            this, SLOT(onAskNewImage()));
    bWaitingNextImage = false;

    // Connect to the server
    pServerSocket = new QWebSocket();
    connect(pServerSocket, SIGNAL(connected()),
            this, SLOT(onServerConnected()));
    connect(pServerSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onServerSocketError(QAbstractSocket::SocketError)));

    pServerSocket->open(QUrl(serverUrl));
}


void
ScorePanel::onServerConnected() {
    QString sFunctionName = " ScorePanel::onServerConnected ";
    Q_UNUSED(sFunctionName)

    QString sMessage;
    sMessage = QString("<getStatus>1</getStatus>");
    qint64 bytesSent = pServerSocket->sendTextMessage(sMessage);
    if(bytesSent != sMessage.length()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Unable to ask the initial status"));
    }

    // Create the File Updater Thread
    pUpdaterThread = new QThread();
    connect(pUpdaterThread, SIGNAL(finished()),
            this, SLOT(onUpdaterThreadDone()));
    // And the File Update Server
    QString updateServer;
    updateServer= QString("ws://%1:%2").arg(pServerSocket->peerAddress().toString()).arg(fileUpdatePort);
    pFileUpdater = new FileUpdater(updateServer, logFile);
    pFileUpdater->moveToThread(pUpdaterThread);
    connect(pFileUpdater, SIGNAL(connectionClosed(bool)),
            this, SLOT(onFileUpdaterClosed(bool)));
    pUpdaterThread->start();
    logMessage(logFile,
               sFunctionName,
               QString("File Update thread started"));
    // Query the spot's list
    askSpotList();

    // Prepare for the slide show
    if(!pMySlideWindow->isReady()) {
        onAskNewImage();// Get prepared for the slideshow...
    }
    else {
        pMySlideWindow->stopSlideShow();
        connect(pMySlideWindow, SIGNAL(getNextImage()),
                this, SLOT(onAskNewImage()));
    }
}


void
ScorePanel::onUpdaterThreadDone() {
    QString sFunctionName = " ScorePanel::onUpdaterThreadDone ";
    Q_UNUSED(sFunctionName)
    logMessage(logFile,
               sFunctionName,
               QString("File Update Thread regularly closed"));
    disconnect(pFileUpdater, 0, 0, 0);
    pUpdaterThread->deleteLater();
    pFileUpdater->deleteLater();
    pFileUpdater = Q_NULLPTR;
    pUpdaterThread = Q_NULLPTR;
}


// Da controllare... viene mai chiamata ? <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
void
ScorePanel::onServerDisconnected() {
    QString sFunctionName = " ScorePanel::onServerDisconnected ";
    Q_UNUSED(sFunctionName)
    if(pUpdaterThread) {
        if(pUpdaterThread->isRunning()) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Closing File Update Thread"));
            pUpdaterThread->requestInterruption();
            if(pUpdaterThread->wait(30000)) {
                logMessage(logFile,
                           sFunctionName,
                           QString("File Update Thread regularly closed"));
            }
            else {
                logMessage(logFile,
                           sFunctionName,
                           QString("File Update Thread forced to close"));
            }
        }
    }
    logMessage(logFile,
               sFunctionName,
               QString("emitting panelClosed()"));
    emit panelClosed();
}


void
ScorePanel::onServerSocketError(QAbstractSocket::SocketError error) {
    QString sFunctionName = " ScorePanel::onServerSocketError ";
    logMessage(logFile,
               sFunctionName,
               QString("%1 %2 Error %3")
               .arg(pServerSocket->peerAddress().toString())
               .arg(pServerSocket->errorString())
               .arg(error));
    if(pUpdaterThread) {
        if(pUpdaterThread->isRunning()) {
            pUpdaterThread->requestInterruption();
            if(pUpdaterThread->wait(3000)) {
                logMessage(logFile,
                           sFunctionName,
                           QString("File Update Thread regularly closed"));
            }
            else {
                logMessage(logFile,
                           sFunctionName,
                           QString("File Update Thread forced to close"));
            }
        }
    }
    if(!disconnect(pServerSocket, 0, 0, 0)) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Unable to disconnect signals from Sever Socket"));
    }
    emit panelClosed();
}


ScorePanel::~ScorePanel() {
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle>=0) {
        pigpio_stop(gpioHostHandle);
    }
#endif
}


void
ScorePanel::initCamera() {
    QString sFunctionName = " ScorePanel::initCamera ";
    Q_UNUSED(sFunctionName)
    // Get the initial camera position from the past stored values
    cameraPanAngle  = pSettings->value(tr("camera/panAngle"),  0.0).toDouble();
    cameraTiltAngle = pSettings->value(tr("camera/tiltAngle"), 0.0).toDouble();

    PWMfrequency    = 50;     // in Hz
    pulseWidthAt_90 = 600.0;  // in us
    pulseWidthAt90  = 2200;   // in us

#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    gpioHostHandle = pigpio_start((char*)"localhost", (char*)"8888");
    if(gpioHostHandle < 0) {
        logMessage(sFunctionName, QString("Non riesco ad inizializzare la GPIO."));
    }
    int iResult;
    if(gpioHostHandle >= 0) {
        iResult = set_PWM_frequency(gpioHostHandle, panPin, PWMfrequency);
        if(iResult < 0) {
            logMessage(sFunctionName, QString("Non riesco a definire la frequenza del PWM per il Pan."));
        }
        double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraPanAngle+90.0);// In us
        iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
            logMessage(sFunctionName, QString("Non riesco a far partire il PWM per il Pan."));
        }
        iResult = set_PWM_frequency(gpioHostHandle, tiltPin, PWMfrequency);
        if(iResult < 0) {
            logMessage(sFunctionName, QString("Non riesco a definire la frequenza del PWM per il Tilt."));
        }
        pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraTiltAngle+90.0);// In us
        iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
            logMessage(sFunctionName, QString("Non riesco a far partire il PWM per il Tilt."));
        }
    }
#endif
}


void
ScorePanel::resizeEvent(QResizeEvent *event) {
    mySize = event->size();
    event->accept();
}


void
ScorePanel::closeEvent(QCloseEvent *event) {
    pSettings->setValue(tr("camera/panAngle"),  cameraPanAngle);
    pSettings->setValue(tr("camera/tiltAngle"), cameraTiltAngle);
    if(pUpdaterThread)
        pUpdaterThread->requestInterruption();
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle>=0) {
        pigpio_stop(gpioHostHandle);
        gpioHostHandle = -1;
    }
#endif
    if(videoPlayer)
        videoPlayer->kill();
    if(cameraPlayer)
        cameraPlayer->kill();
    if(pMySlideWindow)
        delete pMySlideWindow;
    event->accept();
}


void
ScorePanel::askSpotList() {
    QString sFunctionName = " ScorePanel::askSpotList ";
    if(pServerSocket->isValid()) {
        QString sMessage;
        sMessage = QString("<send_spot_list>1</send_spot_list>");
        qint64 bytesSent = pServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Unable to ask for spot list"));
        }
    }
}


void
ScorePanel::onAskNewImage() {
    QString sFunctionName = " ScorePanel::onAskNewImage ";
    Q_UNUSED(sFunctionName)
    if(bWaitingNextImage) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Still waiting for previous image"));
        return;
    }
    if(pServerSocket->isValid()) {
        QString sMessage;
        if(pMySlideWindow->isReady()) {
            sMessage = QString("<send_image>%1,%2</send_image>").arg(pMySlideWindow->size().width()).arg(pMySlideWindow->size().height());
        }
        else {
            sMessage = QString("<send_image>%1,%2</send_image>").arg(size().width()).arg(size().height());
        }
        qint64 bytesSent = pServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Unable to ask for a new image"));
        }
        else {
            bWaitingNextImage = true;
            logMessage(logFile,
                       sFunctionName,
                       sMessage);
        }
    }
}


void
ScorePanel::keyPressEvent(QKeyEvent *event) {
    QString sFunctionName = " ScorePanel::keyPressEvent ";
    Q_UNUSED(sFunctionName);
    if(event->key() == Qt::Key_Escape) {
        pMySlideWindow->hide();
        if(pServerSocket) {
            disconnect(pServerSocket, 0, 0, 0);
            pServerSocket->close(QWebSocketProtocol::CloseCodeNormal, "Client switched off");
        }
        if(videoPlayer) {
            disconnect(videoPlayer, 0, 0, 0);
            videoPlayer->write("q", 1);
            videoPlayer->waitForFinished(3000);
        }
        if(cameraPlayer) {
            disconnect(cameraPlayer, 0, 0, 0);
            cameraPlayer->kill();
            cameraPlayer->waitForFinished(3000);
        }
        if(logFile)
            logFile->close();
        close();
    }
}


void
ScorePanel::onSpotClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    QString sFunctionName = " ScorePanel::onSpotClosed ";
    Q_UNUSED(sFunctionName)
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    if(videoPlayer) {
        delete videoPlayer;
        videoPlayer = NULL;
        //To avoid a blank screen that sometime appear at the end of omxplayer
        int exitCode = system("xrefresh -display :0");
        Q_UNUSED(exitCode);
        QString sMessage = "<closed_spot>1</closed_spot>";
        qint64 bytesSent = pServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
    }
}


void
ScorePanel::onLiveClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    QString sFunctionName = " ScorePanel::onLiveClosed ";
    Q_UNUSED(sFunctionName)
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    if(cameraPlayer) {
        delete cameraPlayer;
        cameraPlayer = NULL;
        //To avoid a blank screen that sometime appear at the end of omxplayer
        int exitCode = system("xrefresh -display :0");
        Q_UNUSED(exitCode);
        QString sMessage = "<closed_live>1</closed_live>";
        qint64 bytesSent = pServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
    }
}


void
ScorePanel::onStartNextSpot(int exitCode, QProcess::ExitStatus exitStatus) {
    QString sFunctionName = " ScorePanel::onStartNextSpot ";
    Q_UNUSED(sFunctionName)
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    // Update spot list just in case we are updating the spot list...
    QDir spotDir(sSpotDir);
    spotList = QFileInfoList();
    QStringList nameFilter(QStringList() << "*.mp4");
    spotDir.setNameFilters(nameFilter);
    spotDir.setFilter(QDir::Files);
    spotList = spotDir.entryInfoList();
    if(spotList.count() == 0) {
        return;
    }

    iCurrentSpot = iCurrentSpot % spotList.count();
    if(!videoPlayer) {
        videoPlayer = new QProcess(this);
        connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                this, SLOT(onStartNextSpot(int, QProcess::ExitStatus)));
    }
    QString sCommand;
    #ifdef Q_PROCESSOR_ARM
        sCommand = "/usr/bin/omxplayer -o hdmi -r " + spotList.at(iCurrentSpot).absoluteFilePath();
    #else
        sCommand = "/usr/bin/cvlc --no-osd -f " + spotList.at(iCurrentSpot).absoluteFilePath() + " vlc://quit";
    #endif
    videoPlayer->start(sCommand);
    logMessage(logFile,
               sFunctionName,
               QString("Now playing: %1")
               .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
    iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
    if(!videoPlayer->waitForStarted(3000)) {
        videoPlayer->kill();
        logMessage(logFile,
                   sFunctionName,
                   QString("Impossibile mandare lo spot"));
        delete videoPlayer;
        videoPlayer = NULL;
    }
}


void
ScorePanel::onBinaryMessageReceived(QByteArray baMessage) {
    QString sFunctionName = " ScorePanel::onBinaryMessageReceived ";
    Q_UNUSED(sFunctionName)
    logMessage(logFile,
               sFunctionName,
               QString("Received %1 bytes").arg(baMessage.size()));
    bWaitingNextImage = false;
    pMySlideWindow->addNewImage(baMessage);
    if(pServerSocket->isValid()) {
        QString sMessage = QString("<image_size>%1</image_size>").arg(baMessage.size());
        qint64 bytesSent = pServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Unable to send back received image size"));
        }
    }
}


void
ScorePanel::onTextMessageReceived(QString sMessage) {
    QString sFunctionName = " ScorePanel::onTextMessageReceived ";
    Q_UNUSED(sFunctionName)
    QString sToken;
    bool ok;
    int iVal;
    QString sNoData = QString("NoData");

    sToken = XML_Parse(sMessage, "kill");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>1)
        iVal = 0;
      if(iVal == 1) {
          pMySlideWindow->stopSlideShow();
          pMySlideWindow->hide();
    #ifdef Q_PROCESSOR_ARM
              system("sudo halt");
    #endif
          close();
      }
    }// kill

    sToken = XML_Parse(sMessage, "endspot");
    if(sToken != sNoData) {
        if(videoPlayer) {
            #ifdef Q_PROCESSOR_ARM
                videoPlayer->write("q", 1);
            #else
                videoPlayer->kill();
            #endif
        }
    }// endspot

    sToken = XML_Parse(sMessage, "spot");
    if(sToken != sNoData) {
        QDir spotDir(sSpotDir);
        spotList = QFileInfoList();
        if(spotDir.exists()) {
            QStringList nameFilter(QStringList() << "*.mp4");
            spotDir.setNameFilters(nameFilter);
            spotDir.setFilter(QDir::Files);
            spotList = spotDir.entryInfoList();
        }
        logMessage(logFile,
                   sFunctionName,
                   QString("Found %1 spots").arg(spotList.count()));
        if(!spotList.isEmpty()) {
            iCurrentSpot = iCurrentSpot % spotList.count();
            if(!videoPlayer) {
                videoPlayer = new QProcess(this);
                connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                        this, SLOT(onSpotClosed(int, QProcess::ExitStatus)));
                QString sCommand;
                #ifdef Q_PROCESSOR_ARM
                    sCommand = "/usr/bin/omxplayer -o hdmi -r " + spotList.at(iCurrentSpot).absoluteFilePath();
                #else
                    sCommand = "/usr/bin/cvlc --no-osd -f " + spotList.at(iCurrentSpot).absoluteFilePath() + " vlc://quit";
                #endif
                videoPlayer->start(sCommand);
                logMessage(logFile,
                           sFunctionName,
                           QString("Now playing: %1")
                           .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
                iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
                if(!videoPlayer->waitForStarted(3000)) {
                    videoPlayer->kill();
                    logMessage(logFile,
                               sFunctionName,
                               QString("Impossibile mandare lo spot."));
                    delete videoPlayer;
                    videoPlayer = NULL;
                }
            }
        }
    }// spot

    sToken = XML_Parse(sMessage, "spotloop");
    if(sToken != sNoData) {
        QDir spotDir(sSpotDir);
        spotList = QFileInfoList();
        if(spotDir.exists()) {
            QStringList nameFilter(QStringList() << "*.mp4");
            spotDir.setNameFilters(nameFilter);
            spotDir.setFilter(QDir::Files);
            spotList = spotDir.entryInfoList();
        }
        logMessage(logFile,
                   sFunctionName,
                   QString("Found %1 spots").arg(spotList.count()));
        if(!spotList.isEmpty()) {
            iCurrentSpot = iCurrentSpot % spotList.count();
            if(!videoPlayer) {
                videoPlayer = new QProcess(this);
                connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                        this, SLOT(onStartNextSpot(int, QProcess::ExitStatus)));
                QString sCommand;
                #ifdef Q_PROCESSOR_ARM
                    sCommand = "/usr/bin/omxplayer -o hdmi -r " + spotList.at(iCurrentSpot).absoluteFilePath();
                #else
                    sCommand = "/usr/bin/cvlc --no-osd -f " + spotList.at(iCurrentSpot).absoluteFilePath() + " vlc://quit";
                #endif
                videoPlayer->start(sCommand);
                logMessage(logFile,
                           sFunctionName,
                           QString("Now playing: %1")
                           .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
                iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
                if(!videoPlayer->waitForStarted(3000)) {
                    videoPlayer->kill();
                    logMessage(logFile,
                               sFunctionName,
                               QString("Impossibile mandare lo spot."));
                    delete videoPlayer;
                    videoPlayer = NULL;
                }
            }
        }
    }// spotloop

    sToken = XML_Parse(sMessage, "endspotloop");
    if(sToken != sNoData) {
        if(videoPlayer) {
            disconnect(videoPlayer, 0, 0, 0);
            connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                    this, SLOT(onSpotClosed(int, QProcess::ExitStatus)));
            #ifdef Q_PROCESSOR_ARM
                videoPlayer->write("q", 1);
            #else
                videoPlayer->kill();
            #endif
        }
    }// endspoloop

    sToken = XML_Parse(sMessage, "slideshow");
    if(sToken != sNoData){
        if(videoPlayer || cameraPlayer)
            return;// No Slide Show if movies are playing or camera is active
        if(!pMySlideWindow->isReady())
            return;// No slides are ready to be shown
#ifdef QT_DEBUG
        pMySlideWindow->showMaximized();
#else
        pMySlideWindow->showFullScreen();
#endif
        pMySlideWindow->startSlideShow();
    }// slideshow

    sToken = XML_Parse(sMessage, "endslideshow");
    if(sToken != sNoData){
        pMySlideWindow->stopSlideShow();
        pMySlideWindow->hide();
    }// endslideshow

    sToken = XML_Parse(sMessage, "live");
    if(sToken != sNoData) {
        if(!cameraPlayer) {
            cameraPlayer = new QProcess(this);
            connect(cameraPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                    this, SLOT(onLiveClosed(int, QProcess::ExitStatus)));
            QString sCommand = QString();
            #ifdef Q_PROCESSOR_ARM
                sCommand = tr("/usr/bin/raspivid -f -t 0 -awb auto --vflip");
            #else
                if(!spotList.isEmpty()) {
                    sCommand = "/usr/bin/cvlc --no-osd -f " + spotList.at(iCurrentSpot).absoluteFilePath() + " vlc://quit";
                    iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
                }
            #endif
            if(sCommand != QString()) {
                cameraPlayer->start(sCommand);
                if(!cameraPlayer->waitForStarted(3000)) {
                    cameraPlayer->kill();
                    logMessage(logFile,
                               sFunctionName,
                               QString("Impossibile mandare lo spot."));
                    delete cameraPlayer;
                    cameraPlayer = NULL;
                }
                else {
                    logMessage(logFile,
                               sFunctionName,
                               QString("Live Show is started."));
                }
            }
        }// live
    }

    sToken = XML_Parse(sMessage, "endlive");
    if(sToken != sNoData) {
        if(cameraPlayer) {
            cameraPlayer->kill();
            logMessage(logFile,
                       sFunctionName,
                       QString("Live Show has been closed."));
        }
    }// endlive

    sToken = XML_Parse(sMessage, "pan");
    if(sToken != sNoData) {
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle >= 0) {
        cameraPanAngle = sToken.toDouble();
        pSettings->setValue(tr("camera/panAngle"),  cameraPanAngle);
        double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraPanAngle+90.0);// In ms
        int iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
            logMessage(sFunctionName, QString("Non riesco a far partire il PWM per il Pan."));
        }
    }
#endif
    }// pan

    sToken = XML_Parse(sMessage, "tilt");
    if(sToken != sNoData) {
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle >= 0) {
        cameraTiltAngle = sToken.toDouble();
        pSettings->setValue(tr("camera/tiltAngle"), cameraTiltAngle);
        double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraTiltAngle+90.0);// In ms
        int iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
          logMessage(sFunctionName, QString("Non riesco a far partire il PWM per il Tilt."));
        }
    }
#endif
    }// tilt

    sToken = XML_Parse(sMessage, "getPanTilt");
    if(sToken != sNoData) {
        if(pServerSocket->isValid()) {
            QString sMessage;
            sMessage = QString("<pan_tilt>%1,%2</pan_tilt>").arg(int(cameraPanAngle)).arg(int(cameraTiltAngle));
            qint64 bytesSent = pServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           sFunctionName,
                           QString("Unable to send pan & tilt values."));
            }
        }
    }// getPanTilt

    sToken = XML_Parse(sMessage, "spot_list");
    if(sToken != sNoData) {
        QStringList spotFileList = QStringList(sToken.split(tr(","), QString::SkipEmptyParts));
        availabeSpotList.clear();
        QStringList tmpList;
        for(int i=0; i< spotFileList.count(); i++) {
            tmpList =   QStringList(spotFileList.at(i).split(tr(";"), QString::SkipEmptyParts));
            if(tmpList.count() > 1) {
                spot* newSpot = new spot();
                newSpot->spotFilename = tmpList.at(0);
                newSpot->spotFileSize = tmpList.at(1).toLong();
                availabeSpotList.append(*newSpot);
            }
        }
        updateSpots();
    }// spot_list
}


void
ScorePanel::updateSpots() {
    QString sFunctionName = " ScorePanel::updateSpots ";
    Q_UNUSED(sFunctionName)

    pFileUpdater->setDestinationDir(sSpotDir);
    connect(this, SIGNAL(updateFiles()),
            pFileUpdater, SLOT(updateFiles()));
    emit updateFiles();
}


void
ScorePanel::onFileUpdaterClosed(bool bError) {
    QString sFunctionName = " ScorePanel::onFileUpdaterClosed ";
    Q_UNUSED(sFunctionName)
    disconnect(pFileUpdater, 0, 0, 0);
    pUpdaterThread->exit(0);
    if(pUpdaterThread->wait(3000)) {
        logMessage(logFile,
                   sFunctionName,
                   QString("File Update Thread regularly closed"));
    }
    else {
        logMessage(logFile,
                   sFunctionName,
                   QString("File Update Thread forced to close"));
    }
    if(bError) {
        logMessage(logFile,
                   sFunctionName,
                   QString("File Updater closed with errors"));
    }
    else {
        logMessage(logFile,
                   sFunctionName,
                   QString("File Updater closed without errors"));
    }
    pFileUpdater = Q_NULLPTR;
    pUpdaterThread = Q_NULLPTR;
}


QGridLayout*
ScorePanel::createPanel() {
    return new QGridLayout();
}
