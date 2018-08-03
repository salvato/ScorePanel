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
#ifndef FILEUPDATER_H
#define FILEUPDATER_H

#include <QObject>
#include <QUrl>
#include <QWebSocket>
#include <QWidget>
#include <QFile>
#include <QFileInfoList>



QT_FORWARD_DECLARE_CLASS(QWebSocket)


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
    void onServerDisconnected();
    void onProcessTextMessage(QString sMessage);
    void onProcessBinaryFrame(QByteArray baMessage, bool isLastFrame);
    void onOpenFileError();
    void onWriteFileError();
    void connectToServer();

private:
    bool isConnectedToNetwork();
    void updateFiles();
    void askFirstFile();

private:
    QFile       *logFile;
    QWebSocket  *pUpdateSocket;
    QString      sMyName;
    QFile        file;
    QUrl         serverUrl;
    QString      destinationDir;
    QString      sFileExtensions;
    quint32      bytesReceived;
    QString      sFileName;
    bool         bTrasferError;

    QList<files>       queryList;
    QList<files>       remoteFileList;
};

#endif // FILEUPDATER_H
