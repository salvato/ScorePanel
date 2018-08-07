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
#include <QDebug>


#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    #include "pigpiod_if2.h"
#endif

#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    #include "slidewindow_interface.h"
#else
    #include "slidewindow.h"
#endif

#include "fileupdater.h"
#include "scorepanel.h"
#include "utility.h"
#include "panelorientation.h"



#define SPOT_UPDATE_PORT      45455
#define SLIDE_UPDATE_PORT     45456



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

/*!
 * \brief ScorePanel::ScorePanel The Base class constructor of all Score Panels
 * \param serverUrl The Panel Server URL to connect to
 * \param myLogFile The File for message logging (if any)
 * \param parent The parent Widget pointer
 */
ScorePanel::ScorePanel(const QString &serverUrl, QFile *myLogFile, QWidget *parent)
    : QWidget(parent)
    , isMirrored(false)
    , isScoreOnly(false)
    , pPanelServerSocket(Q_NULLPTR)
    , logFile(myLogFile)
    , slidePlayer(Q_NULLPTR)
    , videoPlayer(Q_NULLPTR)
    , cameraPlayer(Q_NULLPTR)
    , panPin(14) // GPIO Numbers are Broadcom (BCM) numbers
                 // BCM14 is Pin 8 in the 40 pin GPIO connector.
    , tiltPin(26)// GPIO Numbers are Broadcom (BCM) numbers
                 // BCM26 IS Pin 37 in the 40 pin GPIO connector.
    , gpioHostHandle(-1)
{
    iCurrentSpot = 0;
    iCurrentSlide = 0;
    pPanel = new QWidget(this);

    // Turns off the default window title hints.
    setWindowFlags(Qt::CustomizeWindowHint);

    pSettings = new QSettings("Gabriele Salvato", "Score Panel");
#if defined(Q_OS_ANDROID)
    setScoreOnly(true);
#else
    isScoreOnly = pSettings->value("panel/scoreOnly",  false).toBool();
#endif
    isMirrored  = pSettings->value("panel/orientation",  false).toBool();

    QString sBaseDir;
#ifdef Q_OS_ANDROID
    sBaseDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#else
    sBaseDir = QDir::homePath();
#endif
    if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");

    // Spot management
    pSpotUpdaterThread = Q_NULLPTR;
    pSpotUpdater   = Q_NULLPTR;
    spotUpdatePort = SPOT_UPDATE_PORT;
    sSpotDir = QString("%1spots/").arg(sBaseDir);

    // Slide management
    pSlideUpdaterThread = Q_NULLPTR;
    pSlideUpdater   = Q_NULLPTR;
    slideUpdatePort = SLIDE_UPDATE_PORT;
    sSlideDir= QString("%1slides/").arg(sBaseDir);

    // Camera management
    initCamera();

    // We are ready to  connect to the remote Panel Server
    pPanelServerSocket = new QWebSocket();
    connect(pPanelServerSocket, SIGNAL(connected()),
            this, SLOT(onPanelServerConnected()));
    connect(pPanelServerSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onPanelServerSocketError(QAbstractSocket::SocketError)));

    // Slide Window
#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    pMySlideWindow = new org::salvato::gabriele::SlideShowInterface
            ("org.salvato.gabriele.slideshow",// Service name
             "/SlideShow",                    // Path
             QDBusConnection::sessionBus(),   // Bus
             this);

    slidePlayer = new QProcess(this);
    connect(slidePlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(onSlideShowClosed(int, QProcess::ExitStatus)));
    QString sHomeDir = QDir::homePath();
    if(!sHomeDir.endsWith("/")) sHomeDir += "/";
    QString sCommand = QString("%1SlideShow")
                              .arg(sHomeDir);
    slidePlayer->start(sCommand);
    if(!slidePlayer->waitForStarted(3000)) {
        slidePlayer->terminate();
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Impossibile mandare lo Slide Show."));
        delete slidePlayer;
        slidePlayer = Q_NULLPTR;
    }
#endif

#if !defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    pMySlideWindow = new SlideWindow();
#endif
    pPanelServerSocket->ignoreSslErrors();
    pPanelServerSocket->open(QUrl(serverUrl));
}


/*!
 * \brief ScorePanel::~ScorePanel The Score Panel destructor
 */
ScorePanel::~ScorePanel() {
    if(pPanelServerSocket)
        pPanelServerSocket->disconnect();
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle>=0) {
        pigpio_stop(gpioHostHandle);
    }
#endif
    if(pSettings) delete pSettings;
    pSettings = Q_NULLPTR;

    doProcessCleanup();

    if(pPanelServerSocket) delete pPanelServerSocket;
    pPanelServerSocket = Q_NULLPTR;
}


/*!
 * \brief ScorePanel::buildLayout Utility function to build the ScorePanel layout
 */
void
ScorePanel::buildLayout() {
    QWidget* oldPanel = pPanel;
    pPanel = new QWidget(this);
    QVBoxLayout *panelLayout = new QVBoxLayout();
    panelLayout->addLayout(createPanel());
    pPanel->setLayout(panelLayout);
    if(!layout()) {
        QVBoxLayout *mainLayout = new QVBoxLayout();
        setLayout(mainLayout);
     }
    layout()->addWidget(pPanel);
    if(oldPanel != Q_NULLPTR)
        delete oldPanel;
}

//////////////////////////////////
// Spot Server Management routines
//////////////////////////////////

/*!
 * \brief ScorePanel::onSpotUpdaterClosed Invoked Asynchronously when the spot-updater Thread has been closed.
 * \param bError True if the thread closed with errors.
 */
void
ScorePanel::onSpotUpdaterClosed(bool bError) {
    if(pSpotUpdater)
        pSpotUpdater->disconnect();
    if(pSpotUpdaterThread) {
        pSpotUpdaterThread->exit(0);
        if(pSpotUpdaterThread->wait(5000)) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Spot Update Thread regularly closed"));
        }
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Spot Update Thread forced to close"));
        }
        if(bError) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Spot Updater closed with errors"));
        }
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Spot Updater closed without errors"));
        }
    }
    if(pSpotUpdater)
        delete pSpotUpdater;
    pSpotUpdater = Q_NULLPTR;
    if(pSpotUpdaterThread)
        delete pSpotUpdaterThread;
    pSpotUpdaterThread = Q_NULLPTR;
}


/*!
 * \brief ScorePanel::onSpotUpdaterThreadDone Invoked Asynchronously when the spot-updater thread is done.
 */
void
ScorePanel::onSpotUpdaterThreadDone() {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Spot Update Thread regularly closed"));
    if(pSpotUpdater) {
        pSpotUpdater->disconnect();
        delete pSpotUpdater;
    }
    pSpotUpdater = Q_NULLPTR;
    if(pSpotUpdaterThread)
        delete pSpotUpdaterThread;
    pSpotUpdaterThread = Q_NULLPTR;
}


/*!
 * \brief ScorePanel::closeSpotUpdaterThread Called to close the spot updater Thread
 */
void
ScorePanel::closeSpotUpdaterThread() {
    if(pSpotUpdaterThread) {
        if(pSpotUpdaterThread->isRunning()) {
            pSpotUpdaterThread->disconnect();
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Closing Spot Update Thread"));
            pSpotUpdaterThread->requestInterruption();
            if(pSpotUpdaterThread->wait(3000)) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Spot Update Thread regularly closed"));
                delete pSpotUpdaterThread;
            }
            else {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Spot Update Thread forced to close"));
                pSpotUpdaterThread->terminate();
            }
            pSpotUpdaterThread = Q_NULLPTR;
        }
        if(pSpotUpdater) {
            pSpotUpdater->disconnect();
            delete pSpotUpdater;
            pSpotUpdater = Q_NULLPTR;
        }
    }
}
// End of Spot Server Management routines


//////////////////////////////////
// Slide Server Management routines
//////////////////////////////////

/*!
 * \brief ScorePanel::onSlideUpdaterClosed Invoked Asynchronously when the splide-updater Thread has been closed.
 * \param bError
 */
void
ScorePanel::onSlideUpdaterClosed(bool bError) {
    if(pSlideUpdater)
        pSlideUpdater->disconnect();
    if(pSlideUpdaterThread) {
        pSlideUpdaterThread->exit(0);
        if(pSlideUpdaterThread->wait(3000)) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Slide Update Thread regularly closed"));
            delete pSlideUpdaterThread;
        }
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Slide Update Thread forced to close"));
            pSlideUpdaterThread->deleteLater();
        }
        if(bError) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Slide Updater closed with errors"));
        }
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Slide Updater closed without errors"));
        }
    }
    if(pSlideUpdater)
        delete pSlideUpdater;
    pSlideUpdater = Q_NULLPTR;
    pSlideUpdaterThread = Q_NULLPTR;
}


/*!
 * \brief ScorePanel::onSlideUpdaterThreadDone Invoked Asynchronously when the slide-updater thread is done.
 */
void
ScorePanel::onSlideUpdaterThreadDone() {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Slide Update Thread regularly closed"));
    if(pSlideUpdater) {
        pSlideUpdater->disconnect();
        delete pSlideUpdater;
    }
    pSlideUpdater = Q_NULLPTR;
    if(pSlideUpdaterThread)
        delete pSlideUpdaterThread;
    pSlideUpdaterThread = Q_NULLPTR;
}


/*!
 * \brief ScorePanel::closeSlideUpdaterThread Called to close the slide updater Thread
 */
void
ScorePanel::closeSlideUpdaterThread() {
    if(pSlideUpdaterThread) {
        if(pSlideUpdaterThread->isRunning()) {
            pSlideUpdaterThread->disconnect();
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Closing Slide Update Thread"));
            pSlideUpdaterThread->requestInterruption();
            if(pSlideUpdaterThread->wait(3000)) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Slide Update Thread regularly closed"));
                delete pSlideUpdaterThread;
            }
            else {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Slide Update Thread forced to close"));
                pSlideUpdaterThread->terminate();
                pSlideUpdaterThread->deleteLater();
            }
        }
        pSlideUpdaterThread = Q_NULLPTR;
    }
    if(pSlideUpdater) {
        pSlideUpdater->disconnect();
        delete pSlideUpdater;
        pSlideUpdater = Q_NULLPTR;
    }
}
// End of Slide Server Management routines


//==================
// Panel management
//==================
/*!
 * \brief ScorePanel::setScoreOnly To set or reset the "Score Only" mode for the panel
 * \param bScoreOnly True if the Panel has to show only the Score (no Slides, Spots or Camera)
 */
void
ScorePanel::setScoreOnly(bool bScoreOnly) {
    isScoreOnly = bScoreOnly;
    if(isScoreOnly) {
        // Terminate, if running, videos, Slides and Camera
        if(videoPlayer) {
            videoPlayer->disconnect();
    #if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
            videoPlayer->write("q", 1);
            system("xrefresh -display :0");
    #else
            videoPlayer->close();
    #endif
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Closing Video Player..."));
            videoPlayer->waitForFinished(3000);
            videoPlayer->deleteLater();
            videoPlayer = Q_NULLPTR;
        }
        if(cameraPlayer) {
            cameraPlayer->close();
            cameraPlayer->waitForFinished(3000);
            cameraPlayer->deleteLater();
            cameraPlayer = Q_NULLPTR;
        }
    }
}


/*!
 * \brief ScorePanel::getScoreOnly
 * \return true if the Panel shows only the score (no Slides, Spots or Camera)
 */
bool
ScorePanel::getScoreOnly() {
    return isScoreOnly;
}


/*!
 * \brief ScorePanel::onPanelServerConnected Invoked asynchronously upon the Server connection
 */
void
ScorePanel::onPanelServerConnected() {
    connect(pPanelServerSocket, SIGNAL(disconnected()),
            this, SLOT(onPanelServerDisconnected()));

    QString sMessage;
    sMessage = QString("<getStatus>%1</getStatus>").arg(QHostInfo::localHostName());
    qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
    if(bytesSent != sMessage.length()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Unable to ask the initial status"));
    }
#if !defined(Q_OS_ANDROID)
    // Create the Spot Updater Thread
    pSpotUpdaterThread = new QThread();
    connect(pSpotUpdaterThread, SIGNAL(finished()),
            this, SLOT(onSpotUpdaterThreadDone()));
    // And the Spot Update Server
    QString spotUpdateServer;
    spotUpdateServer= QString("ws://%1:%2").arg(pPanelServerSocket->peerAddress().toString()).arg(spotUpdatePort);
    pSpotUpdater = new FileUpdater(QString("SpotUpdater"), spotUpdateServer, logFile);
    pSpotUpdater->moveToThread(pSpotUpdaterThread);
    connect(pSpotUpdater, SIGNAL(connectionClosed(bool)),
            this, SLOT(onSpotUpdaterClosed(bool)));
    connect(this, SIGNAL(updateSpots()),
            pSpotUpdater, SLOT(startUpdate()));
    pSpotUpdaterThread->start();
    pSpotUpdater->setDestination(sSpotDir, QString("*.mp4 *.MP4"));
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Spot Update thread started"));

    // Create the Slide Updater Thread
    pSlideUpdaterThread = new QThread();
    connect(pSlideUpdaterThread, SIGNAL(finished()),
            this, SLOT(onSlideUpdaterThreadDone()));
    // And the Slide Update Server
    QString slideUpdateServer;
    slideUpdateServer= QString("ws://%1:%2").arg(pPanelServerSocket->peerAddress().toString()).arg(slideUpdatePort);
    pSlideUpdater = new FileUpdater(QString("SlideUpdater"), slideUpdateServer, logFile);
    pSlideUpdater->moveToThread(pSlideUpdaterThread);
    connect(pSlideUpdater, SIGNAL(connectionClosed(bool)),
            this, SLOT(onSlideUpdaterClosed(bool)));
    connect(this, SIGNAL(updateSlides()),
            pSlideUpdater, SLOT(startUpdate()));
    pSlideUpdaterThread->start();
    pSlideUpdater->setDestination(sSlideDir, QString("*.jpg *.jpeg *.png *.JPG *.JPEG *.PNG"));
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Slide Update thread started"));

    emit updateSpots();
    emit updateSlides();
#endif
}


/*!
 * \brief ScorePanel::onPanelServerDisconnected Invoked asynchronously upon the Server disconnection
 */
void
ScorePanel::onPanelServerDisconnected() {
    doProcessCleanup();
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("emitting panelClosed()"));
#endif
    if(pPanelServerSocket)
        pPanelServerSocket->deleteLater();
    pPanelServerSocket =Q_NULLPTR;
    emit panelClosed();
}


/*!
 * \brief ScorePanel::doProcessCleanup responsible to clean all the running processes upon a Server disconnection.
 */
void
ScorePanel::doProcessCleanup() {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Cleaning all processes"));
    if(slidePlayer) {
        slidePlayer->disconnect();
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
        pMySlideWindow->exitShow();// This gently close the slidePlayer Process...
        system("xrefresh -display :0");
#else
        slidePlayer->close();
#endif
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Closing Slide Player..."));
        slidePlayer->waitForFinished(3000);
        slidePlayer->deleteLater();
        slidePlayer = Q_NULLPTR;
    }
    if(videoPlayer) {
        videoPlayer->disconnect();
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
        videoPlayer->write("q", 1);
        system("xrefresh -display :0");
#else
        videoPlayer->close();
#endif
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Closing Video Player..."));
        videoPlayer->waitForFinished(3000);
        videoPlayer->deleteLater();
        videoPlayer = Q_NULLPTR;
    }
    if(cameraPlayer) {
        cameraPlayer->close();
        cameraPlayer->waitForFinished(3000);
        cameraPlayer->deleteLater();
        cameraPlayer = Q_NULLPTR;
    }

    closeSpotUpdaterThread();
    closeSlideUpdaterThread();
}


/*!
 * \brief ScorePanel::onPanelServerSocketError Invoked asynchronously upon a Server socket error.
 * \param error
 */
void
ScorePanel::onPanelServerSocketError(QAbstractSocket::SocketError error) {
    doProcessCleanup();

    logMessage(logFile,
               Q_FUNC_INFO,
               QString("%1 %2 Error %3")
               .arg(pPanelServerSocket->peerAddress().toString())
               .arg(pPanelServerSocket->errorString())
               .arg(error));
    if(pPanelServerSocket) {
        if(!pPanelServerSocket->disconnect()) {
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to disconnect signals from Sever Socket"));
#endif
        }
        if(pPanelServerSocket->isValid())
            pPanelServerSocket->close();
        pPanelServerSocket->deleteLater();
    }
    pPanelServerSocket = Q_NULLPTR;
    close();
    emit panelClosed();
}


/*!
 * \brief ScorePanel::initCamera Initialize the PWM control of the pan-tilt camera servos
 */
void
ScorePanel::initCamera() {
    // Get the initial camera position from the past stored values
    cameraPanAngle  = pSettings->value("camera/panAngle",  0.0).toDouble();
    cameraTiltAngle = pSettings->value("camera/tiltAngle", 0.0).toDouble();

    PWMfrequency    = 50;     // in Hz
    pulseWidthAt_90 = 600.0;  // in us
    pulseWidthAt90  = 2200;   // in us

#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    gpioHostHandle = pigpio_start((char*)"localhost", (char*)"8888");
    if(gpioHostHandle < 0) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Non riesco ad inizializzare la GPIO."));
    }
    int iResult;
    if(gpioHostHandle >= 0) {
        iResult = set_PWM_frequency(gpioHostHandle, panPin, PWMfrequency);
        if(iResult < 0) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Non riesco a definire la frequenza del PWM per il Pan."));
        }
        double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraPanAngle+90.0);// In us
        iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Non riesco a far partire il PWM per il Pan."));
        }
        set_PWM_frequency(gpioHostHandle, panPin, 0);

        iResult = set_PWM_frequency(gpioHostHandle, tiltPin, PWMfrequency);
        if(iResult < 0) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Non riesco a definire la frequenza del PWM per il Tilt."));
        }
        pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraTiltAngle+90.0);// In us
        iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Non riesco a far partire il PWM per il Tilt."));
        }
        set_PWM_frequency(gpioHostHandle, tiltPin, 0);
    }
#endif
}


/*!
 * \brief ScorePanel::closeEvent Handle the Closing of the Panel
 * \param event The closing event
 */
void
ScorePanel::closeEvent(QCloseEvent *event) {
    pSettings->setValue("camera/panAngle",  cameraPanAngle);
    pSettings->setValue("camera/tiltAngle", cameraTiltAngle);
    pSettings->setValue("panel/orientation", isMirrored);

    doProcessCleanup();

#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle>=0) {
        pigpio_stop(gpioHostHandle);
        gpioHostHandle = -1;
    }
#endif
    event->accept();
}


/*!
 * \brief ScorePanel::keyPressEvent Close the window following and "Esc" key pressed
 * \param event The event descriptor
 */
void
ScorePanel::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Escape) {
        if(pPanelServerSocket) {
            pPanelServerSocket->disconnect();
            pPanelServerSocket->close(QWebSocketProtocol::CloseCodeNormal, tr("Il Client ha chiuso il collegamento"));
        }
        close();
    }
}


/*!
 * \brief ScorePanel::onSlideShowClosed Invoked asynchronously when the Slide Window closes
 * \param exitCode unused
 * \param exitStatus unused
 */
void
ScorePanel::onSlideShowClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    if(slidePlayer) {
        slidePlayer->deleteLater();
        slidePlayer = Q_NULLPTR;
        //To avoid a blank screen that sometime appear at the end of the slideShow
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
        int iDummy = system("xrefresh -display :0");
        Q_UNUSED(iDummy)
#endif
    }
}


/*!
 * \brief ScorePanel::onSpotClosed Invoked asynchronously when the Spot Window closes
 * \param exitCode Unused
 * \param exitStatus Unused
 */
void
ScorePanel::onSpotClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    if(videoPlayer) {
        videoPlayer->disconnect();
        videoPlayer->close();// Closes all communication with the process and kills it.
        delete videoPlayer;
        videoPlayer = Q_NULLPTR;
        //To avoid a blank screen that sometime appear at the end of omxplayer
        int iDummy = system("xrefresh -display :0");
        Q_UNUSED(iDummy)
        QString sMessage = "<closed_spot>1</closed_spot>";
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
    }
}


/*!
 * \brief ScorePanel::onLiveClosed Invoked asynchronously when the Camera Window closes
 * \param exitCode Unused
 * \param exitStatus Unused
 */
void
ScorePanel::onLiveClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    if(cameraPlayer) {
        delete cameraPlayer;
        cameraPlayer = Q_NULLPTR;
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
        //To avoid a blank screen that sometime appear at the end of omxplayer
        int iDummy =system("xrefresh -display :0");
        Q_UNUSED(iDummy)
#endif
        QString sMessage = "<closed_live>1</closed_live>";
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
    }
}


/*!
 * \brief ScorePanel::onStartNextSpot
 * \param exitCode
 * \param exitStatus
 */
void
ScorePanel::onStartNextSpot(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    // Update spot list just in case we are updating the spot list...
    QDir spotDir(sSpotDir);
    spotList = QFileInfoList();
    QStringList nameFilter(QStringList() << "*.mp4" << "*.MP4");
    spotDir.setNameFilters(nameFilter);
    spotDir.setFilter(QDir::Files);
    spotList = spotDir.entryInfoList();
    if(spotList.count() == 0) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("No spots available !"));
#endif
        if(videoPlayer) {
            videoPlayer->disconnect();
            delete videoPlayer;
            videoPlayer = Q_NULLPTR;
            QString sMessage = "<closed_spot>1</closed_spot>";
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to send %1")
                           .arg(sMessage));
            }
        }
        //To avoid a blank screen that sometime appear at the end of omxplayer
        int iDummy = system("xrefresh -display :0");
        Q_UNUSED(iDummy)
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
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Now playing: %1")
               .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
#endif
    iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
    if(!videoPlayer->waitForStarted(3000)) {
        videoPlayer->close();
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Impossibile mandare lo spot"));
        videoPlayer->disconnect();
        delete videoPlayer;
        videoPlayer = Q_NULLPTR;
        //To avoid a blank screen that sometime appear at the end of omxplayer
        int iDummy = system("xrefresh -display :0");
        Q_UNUSED(iDummy)
    }
}


/*!
 * \brief ScorePanel::onBinaryMessageReceived Invoked asynchronously upon a binary message has been received
 * \param baMessage The received message
 */
void
ScorePanel::onBinaryMessageReceived(QByteArray baMessage) {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Received %1 bytes").arg(baMessage.size()));
}


/*!
 * \brief ScorePanel::onTextMessageReceived Invoked asynchronously upon a text message has been received
 * \param sMessage The received message
 *
 * The XML message is processed and the contained command will be executed
 */
void
ScorePanel::onTextMessageReceived(QString sMessage) {
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
          pPanelServerSocket->disconnect();
    #ifdef Q_PROCESSOR_ARM
              system("sudo halt");
    #endif
          close();// emit the QCloseEvent that is responsible
                  // to clean up all pending processes
      }
    }// kill

    sToken = XML_Parse(sMessage, "endspot");
    if(sToken != sNoData) {
        if(videoPlayer) {
            #ifdef Q_PROCESSOR_ARM
                videoPlayer->write("q", 1);
            #else
                videoPlayer->close();
            #endif
        }
    }// endspot

    sToken = XML_Parse(sMessage, "spotloop");
    if(sToken != sNoData && !isScoreOnly) {
        startSpotLoop();
    }// spotloop

    sToken = XML_Parse(sMessage, "endspotloop");
    if(sToken != sNoData) {
        if(videoPlayer) {
            videoPlayer->disconnect();
            connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                    this, SLOT(onSpotClosed(int, QProcess::ExitStatus)));
            #ifdef Q_PROCESSOR_ARM
                videoPlayer->write("q", 1);
            #else
                videoPlayer->close();
            #endif
        }
    }// endspoloop

    sToken = XML_Parse(sMessage, "slideshow");
    if(sToken != sNoData && !isScoreOnly){
        startSlideShow();
    }// slideshow

    sToken = XML_Parse(sMessage, "endslideshow");
    if(sToken != sNoData){
#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
        if(pMySlideWindow->isValid()) {
#else
        if(pMySlideWindow) {
            pMySlideWindow->hide();
#endif
            pMySlideWindow->stopSlideShow();
        }
    }// endslideshow

    sToken = XML_Parse(sMessage, "live");
    if(sToken != sNoData && !isScoreOnly) {
#if !defined(Q_OS_ANDROID)
        startLiveCamera();
#endif
    }// live

    sToken = XML_Parse(sMessage, "endlive");
    if(sToken != sNoData) {
#if !defined(Q_OS_ANDROID)
        if(cameraPlayer) {
            cameraPlayer->close();
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Live Show has been closed."));
#endif
        }
#endif
    }// endlive

    sToken = XML_Parse(sMessage, "pan");
    if(sToken != sNoData) {
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle >= 0) {
        cameraPanAngle = sToken.toDouble();
        pSettings->setValue("camera/panAngle",  cameraPanAngle);
        set_PWM_frequency(gpioHostHandle, panPin, PWMfrequency);
        double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraPanAngle+90.0);// In ms
        int iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Non riesco a far partire il PWM per il Pan."));
        }
        set_PWM_frequency(gpioHostHandle, panPin, 0);
    }
#endif
    }// pan

    sToken = XML_Parse(sMessage, "tilt");
    if(sToken != sNoData) {
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
        if(gpioHostHandle >= 0) {
            cameraTiltAngle = sToken.toDouble();
            pSettings->setValue("camera/tiltAngle", cameraTiltAngle);
            set_PWM_frequency(gpioHostHandle, tiltPin, PWMfrequency);
            double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraTiltAngle+90.0);// In ms
            int iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
            if(iResult < 0) {
              logMessage(logFile,
                         Q_FUNC_INFO,
                         QString("Non riesco a far partire il PWM per il Tilt."));
            }
            set_PWM_frequency(gpioHostHandle, tiltPin, 0);
        }
#endif
    }// tilt

    sToken = XML_Parse(sMessage, "getPanTilt");
    if(sToken != sNoData) {
        if(pPanelServerSocket->isValid()) {
            QString sMessage;
            sMessage = QString("<pan_tilt>%1,%2</pan_tilt>").arg(int(cameraPanAngle)).arg(int(cameraTiltAngle));
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to send pan & tilt values."));
            }
        }
    }// getPanTilt

    sToken = XML_Parse(sMessage, "getOrientation");
    if(sToken != sNoData) {
        if(pPanelServerSocket->isValid()) {
            QString sMessage;
            if(isMirrored)
                sMessage = QString("<orientation>%1</orientation>").arg(static_cast<int>(PanelOrientation::Reflected));
            else
                sMessage = QString("<orientation>%1</orientation>").arg(static_cast<int>(PanelOrientation::Normal));
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to send orientation value."));
            }
        }
    }// getOrientation

    sToken = XML_Parse(sMessage, "setOrientation");
    if(sToken != sNoData) {
        bool ok;
        int iVal = sToken.toInt(&ok);
        if(!ok) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Illegal orientation value received: %1")
                               .arg(sToken));
            return;
        }
        try {
            PanelOrientation newOrientation = static_cast<PanelOrientation>(iVal);
            if(newOrientation == PanelOrientation::Reflected)
                isMirrored = true;
            else
                isMirrored = false;
        } catch(...) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Illegal orientation value received: %1")
                               .arg(sToken));
            return;
        }
        pSettings->setValue("panel/orientation", isMirrored);
        buildLayout();
    }// setOrientation

    sToken = XML_Parse(sMessage, "getScoreOnly");
    if(sToken != sNoData) {
        getPanelScoreOnly();
    }// getScoreOnly

    sToken = XML_Parse(sMessage, "setScoreOnly");
    if(sToken != sNoData) {
#if !defined(Q_OS_ANDROID)
        bool ok;
        int iVal = sToken.toInt(&ok);
        if(!ok) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Illegal value fo ScoreOnly received: %1")
                               .arg(sToken));
            return;
        }
        if(iVal==0) {
            setScoreOnly(false);
        }
        else {
            setScoreOnly(true);
        }
        pSettings->setValue("panel/scoreOnly", isScoreOnly);
#endif
    }// setScoreOnly
}


/*!
 * \brief ScorePanel::startLiveCamera
 */
void
ScorePanel::startLiveCamera() {
    if(!cameraPlayer) {
        cameraPlayer = new QProcess(this);
        connect(cameraPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                this, SLOT(onLiveClosed(int, QProcess::ExitStatus)));
        QString sCommand = QString();
        #ifdef Q_PROCESSOR_ARM
            sCommand = QString("/usr/bin/raspivid -f -t 0 -awb auto --vflip --hflip");
        #else
            if(!spotList.isEmpty()) {
                sCommand = "/usr/bin/cvlc --no-osd -f " + spotList.at(iCurrentSpot).absoluteFilePath() + " vlc://quit";
                iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
            }
        #endif
        if(sCommand != QString()) {
            cameraPlayer->start(sCommand);
            if(!cameraPlayer->waitForStarted(3000)) {
                cameraPlayer->close();
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Impossibile mandare lo spot."));
                delete cameraPlayer;
                cameraPlayer = Q_NULLPTR;
            }
#ifdef LOG_VERBOSE
            else {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Live Show is started."));
            }
#endif
        }
    }
}


/*!
 * \brief ScorePanel::getPanelScoreOnly
 * send a message indicating if the ScorePanel shows only the score
 */
void
ScorePanel::getPanelScoreOnly() {
    if(pPanelServerSocket->isValid()) {
        QString sMessage;
        sMessage = QString("<isScoreOnly>%1</isScoreOnly>").arg(static_cast<int>(getScoreOnly()));
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send scoreOnly configuration"));
        }
    }
}


/*!
 * \brief ScorePanel::startSpotLoop
 * Invoked to start a loop of Spots
 */
void
ScorePanel::startSpotLoop() {
    QDir spotDir(sSpotDir);
    spotList = QFileInfoList();
    if(spotDir.exists()) {
        QStringList nameFilter(QStringList() << "*.mp4" << "*.MP4");
        spotDir.setNameFilters(nameFilter);
        spotDir.setFilter(QDir::Files);
        spotList = spotDir.entryInfoList();
    }
    logMessage(logFile,
               Q_FUNC_INFO,
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
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Now playing: %1")
                       .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
#endif
            iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
            if(!videoPlayer->waitForStarted(3000)) {
                videoPlayer->close();
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Impossibile mandare lo spot."));
                videoPlayer->disconnect();
                delete videoPlayer;
                videoPlayer = Q_NULLPTR;
            }
        }
    }
}

/*!
 * \brief ScorePanel::startSlideShow
 * Invoked to start the SlideShow
 */
void
ScorePanel::startSlideShow() {
    if(videoPlayer || cameraPlayer)
        return;// No Slide Show if movies are playing or camera is active
#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    if(pMySlideWindow->isValid()) {
#else
    if(pMySlideWindow) {
        pMySlideWindow->showFullScreen();
#endif
        pMySlideWindow->setSlideDir(sSlideDir);
        pMySlideWindow->startSlideShow();
    }
    else {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Invalid Slide Window"));
    }
}


/*!
 * \brief ScorePanel::createPanel
 * \return
 */
QGridLayout*
ScorePanel::createPanel() {
    return new QGridLayout();
}
