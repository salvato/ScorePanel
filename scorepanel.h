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
#ifndef SCOREPANEL_H
#define SCOREPANEL_H

#include <QObject>
#include <QWidget>
#include <QProcess>
#include <QFileInfoList>
#include <QUrl>
#include <QtGlobal>

#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    #include "slidewindow_interface.h"
#else
    #include "slidewindow.h"
#endif
#include "serverdiscoverer.h"

#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
    #define horizontalAdvance width
#endif


QT_BEGIN_NAMESPACE
QT_FORWARD_DECLARE_CLASS(QSettings)
QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QUdpSocket)
QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(SlideWindow)
QT_FORWARD_DECLARE_CLASS(QGridLayout)
QT_FORWARD_DECLARE_CLASS(UpdaterThread)
QT_FORWARD_DECLARE_CLASS(FileUpdater)
QT_END_NAMESPACE


class ScorePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ScorePanel(const QString& _serverUrl, QFile *myLogFile, QWidget *parent = Q_NULLPTR);
    ~ScorePanel();
    void keyPressEvent(QKeyEvent *event);
    void closeEvent(QCloseEvent *event);
    void setScoreOnly(bool bScoreOnly);
    bool getScoreOnly();

signals:
    void updateSpots();
    void updateSlides();
    void panelClosed();
    void exitRequest();

public slots:
    void onTextMessageReceived(QString sMessage);
    void onBinaryMessageReceived(QByteArray baMessage);

private slots:
    void onPanelServerConnected();
    void onPanelServerDisconnected();
    void onPanelServerSocketError(QAbstractSocket::SocketError error);
    void onSlideShowClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onSpotClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onLiveClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onStartNextSpot(int exitCode, QProcess::ExitStatus exitStatus);
    void onSpotUpdaterClosed(bool bError);
    void onSpotUpdaterThreadDone();
    void onSlideUpdaterClosed(bool bError);
    void onSlideUpdaterThreadDone();

protected:
    virtual QGridLayout* createPanel();
    void buildLayout();
    void doProcessCleanup();

protected:
    bool               isMirrored;
    bool               isScoreOnly;
    QWebSocket        *pPanelServerSocket;
    QFile             *logFile;

private:
    QDateTime          dateTime;
    QProcess          *slidePlayer;
    QProcess          *videoPlayer;
    QProcess          *cameraPlayer;
    QString            sProcess;
    QString            sProcessArguments;

    // Spots management
    quint16            spotUpdatePort;
    QThread           *pSpotUpdaterThread;
    FileUpdater       *pSpotUpdater;
    QString            sSpotDir;
    QFileInfoList      spotList;
    struct spot {
        QString spotFilename;
        qint64  spotFileSize;
    };
    QList<spot>        availabeSpotList;
    int                iCurrentSpot;

    // Slides management
    quint16            slideUpdatePort;
    QThread           *pSlideUpdaterThread;
    FileUpdater       *pSlideUpdater;
    QString            sSlideDir;
    QFileInfoList      slideList;
    struct slide {
        QString slideFilename;
        qint64  slideFileSize;
    };
    QList<slide>       availabeSlideList;
    int                iCurrentSlide;

    QString            logFileName;
#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    org::salvato::gabriele::SlideShowInterface *pMySlideWindow;
#else
    SlideWindow       *pMySlideWindow;
#endif

    unsigned           panPin;
    unsigned           tiltPin;
    double             cameraPanAngle;
    double             cameraTiltAngle;
    int                gpioHostHandle;
    unsigned           PWMfrequency;
    quint32            dutyCycle;
    double             pulseWidthAt_90;
    double             pulseWidthAt90;

private:
    void               initCamera();
    void               closeSpotUpdaterThread();
    void               closeSlideUpdaterThread();
    void               startLiveCamera();
    void               startSpotLoop();
    void               startSlideShow();
    void               getPanelScoreOnly();

private:
    QSettings         *pSettings;
    QWidget           *pPanel;
};

#endif // SCOREPANEL_H
