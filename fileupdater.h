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


struct files {
    QString fileName;
    qint64  fileSize;
};


class FileUpdater : public QObject
{
    Q_OBJECT
public:
    explicit FileUpdater(QString sName, QUrl _serverUrl, QFile *_logFile = Q_NULLPTR, QObject *parent = 0);
    ~FileUpdater();
    bool setDestination(QString _destinationDir, QString sExtensions);
    void askFileList();

signals:
    void connectionClosed(bool bError);
    void writeFileError();
    void openFileError();

public slots:
    void startUpdate();

private slots:
    void onUpdateSocketError(QAbstractSocket::SocketError error);
    void onUpdateSocketConnected();
    void onUpdateSocketChangedState(QAbstractSocket::SocketState newSocketState);
    void onServerDisconnected();
    void onProcessTextMessage(QString sMessage);
    void onProcessBinaryFrame(QByteArray baMessage, bool isLastFrame);
    void onOpenFileError();
    void onWriteFileError();
    void retryReconnection();
    void connectToServer();

private:
    bool isConnectedToNetwork();
    void updateFiles();
    void askFirstFile();

private:
    QString      sMyName;
    QFile       *logFile;
    QFile        file;
    QUrl         serverUrl;
    QString      destinationDir;
    QString      sFileExtensions;
    QWebSocket  *pUpdateSocket;
    quint32      bytesReceived;
    QString      sFileName;
    bool         bTrasferError;
    QTimer      *pReconnectionTimer;

    QAbstractSocket::SocketState previousSocketState;

    QList<files>       queryList;
    QList<files>       remoteFileList;
};

#endif // FILEUPDATER_H
