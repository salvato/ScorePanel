#ifndef SCOREPANEL_H
#define SCOREPANEL_H

#include <QObject>
#include <QWidget>
#include <QProcess>
#include <QFileInfoList>
#include <QUrl>

#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    #include "slidewindow_interface.h"
#else
    #include "slidewindow.h"
#endif
#include "nonetwindow.h"
#include "serverdiscoverer.h"

QT_BEGIN_NAMESPACE
class QSettings;
class QFile;
class QUdpSocket;
class QWebSocket;
class SlideWindow;
class QGridLayout;
class UpdaterThread;
class FileUpdater;
QT_END_NAMESPACE


class ScorePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ScorePanel(QUrl _serverUrl, QFile *_logFile, QWidget *parent = 0);
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
    void resizeEvent(QResizeEvent *event);
    void onTextMessageReceived(QString sMessage);
    void onBinaryMessageReceived(QByteArray baMessage);

private slots:
    void onPanelServerConnected();
    void onPanelServerDisconnected();
    void onPanelServerSocketError(QAbstractSocket::SocketError error);
    void onSpotClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onLiveClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onStartNextSpot(int exitCode, QProcess::ExitStatus exitStatus);
    void onTimeToEmitPing();
    void onPongReceived(quint64 elapsed, QByteArray payload);
    void onTimeToCheckPong();
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

    QDateTime          dateTime;
    QWebSocket        *pPanelServerSocket;
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

    QFile             *logFile;
    QString            logFileName;
#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    org::salvato::gabriele::SlideShowInterface *pMySlideWindow;
#else
    SlideWindow       *pMySlideWindow;
#endif
    QTimer            *pTimerPing;
    QTimer            *pTimerCheckPong;
    int                pingPeriod;
    int                nPong;

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
    void initCamera();
    void closeSpotUpdaterThread();
    void closeSlideUpdaterThread();

private:
    QSettings         *pSettings;
    QWidget           *pPanel;
};

#endif // SCOREPANEL_H
