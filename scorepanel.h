#ifndef SCOREPANEL_H
#define SCOREPANEL_H

#include <QObject>
#include <QWidget>
#include <QProcess>
#include <QFileInfoList>

#include "slidewindow.h"
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
    explicit ScorePanel(QWebSocket *_pWebSocket, QFile *_logFile, QWidget *parent = 0);
    virtual ~ScorePanel();
    void keyPressEvent(QKeyEvent *event);
    void closeEvent(QCloseEvent *event);

signals:
    void updateFiles();

public slots:
    void resizeEvent(QResizeEvent *event);
    void onFileUpdaterClosed(bool bError);
    void onTextMessageReceived(QString sMessage);
    void onBinaryMessageReceived(QByteArray baMessage);

private slots:
    void onSpotClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onLiveClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onStartNextSpot(int exitCode, QProcess::ExitStatus exitStatus);
    void onAskNewImage();

protected slots:

protected:
    void logMessage(QString sFunctionName, QString sMessage);
    QString XML_Parse(QString input_string, QString token);
    virtual QGridLayout* createPanel();

protected:
    QString            sNoData;
    QSize              mySize;
    bool               isMirrored;

    QSettings         *pSettings;
    QTextStream        sDebugInformation;
    QString            sDebugMessage;
    QDateTime          dateTime;
    QWebSocket        *pWebSocket;
    bool               bWaitingNextImage;
    QProcess          *videoPlayer;
    QProcess          *cameraPlayer;
    QThread           *pUpdaterThread;
    FileUpdater       *pFileUpdater;
    QString            sProcess;
    QString            sProcessArguments;
    QString            sSpotDir;
    QFileInfoList      spotList;
    struct spot {
        QString spotFilename;
        qint64  spotFileSize;
    };
    QList<spot>        availabeSpotList;
    int                iCurrentSpot;
    QFile             *logFile;
    QString            logFileName;
    SlideWindow       *pMySlideWindow;
    quint16            fileUpdatePort;

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
    void askSpotList();
    void updateSpots();
};

#endif // SCOREPANEL_H
