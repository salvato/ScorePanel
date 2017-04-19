#ifndef FILEUPDATER_H
#define FILEUPDATER_H

#include <QObject>
#include <QUrl>
#include <QWebSocket>
#include <QWidget>
#include <QFile>
#include <QFileInfoList>



QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(QTimer)


struct spot {
    QString spotFilename;
    qint64  spotFileSize;
};


class FileUpdater : public QObject
{
    Q_OBJECT
public:
    explicit FileUpdater(QUrl _serverUrl, QFile *_logFile = Q_NULLPTR, QObject *parent = 0);
    ~FileUpdater();
    void setFileList(QList<spot> _fileList);
    void setDestinationDir(QString _destinationDir);
    void stop();

signals:
    void connectionClosed(bool bError);
    void writeFileError();
    void openFileError();

public slots:
    void updateFiles();

private slots:
    void connectToServer();
    void onUpdateSocketError(QAbstractSocket::SocketError error);
    void onUpdateSocketConnected();
    void onUpdateSocketChangedState(QAbstractSocket::SocketState newSocketState);
    void onServerDisconnected();
    void onProcessTextMessage(QString sMessage);
    void onProcessBinaryFrame(QByteArray baMessage, bool isLastFrame);
    void onOpenFileError();
    void onWriteFileError();
    void retryReconnection();

private:
    void logMessage(QString sFunctionName, QString sMessage);
    QString XML_Parse(QString input_string, QString token);
    bool isConnectedToNetwork();
    void askSpotList();
    void updateSpots();
    void askFirstFile();

private:
    QFile       *logFile;
    QFile        file;
    QUrl         serverUrl;
    QList<spot>  fileList;
    QString      destinationDir;
    QWebSocket  *pUpdateSocket;
    QDateTime    dateTime;
    QTextStream  sDebugInformation;
    QString      sDebugMessage;
    QString      sNoData;
    quint32      bytesReceived;
    QString      sFileName;
    bool         bTrasferError;
    QTimer      *pReconnectionTimer;

    QAbstractSocket::SocketState previousSocketState;

    QFileInfoList      spotList;
    QList<spot>        availabeSpotList;
};

#endif // FILEUPDATER_H
