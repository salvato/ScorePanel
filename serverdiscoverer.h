#ifndef SERVERDISCOVERER_H
#define SERVERDISCOVERER_H

#include <QObject>
#include <QList>
#include <QVector>
#include <QHostAddress>
#include <QSslError>

QT_FORWARD_DECLARE_CLASS(QUdpSocket)
QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(QFile)

class ServerDiscoverer : public QObject
{
    Q_OBJECT
public:
    explicit ServerDiscoverer(QFile *_logFile=Q_NULLPTR, QObject *parent=Q_NULLPTR);

signals:
//    void serverConnected(QWebSocket* pWebSocket);
    void serverFound(QString serverUrl);

public slots:

private slots:
    void onProcessDiscoveryPendingDatagrams();
    void onDiscoverySocketError(QAbstractSocket::SocketError error);
//    void onWebSocketError(QAbstractSocket::SocketError error);
//    void onSslErrors(const QList<QSslError> &errors);
//    void onWebSocketConnected();

public:
    void Discover();

private:
    QFile               *logFile;
    QList<QHostAddress>  broadcastAddress;
//    QList<QWebSocket*>   webSocketList;
    QVector<QUdpSocket*> discoverySocketArray;
    QStringList          ipList;
    quint16              discoveryPort;
    quint16              serverPort;
    QHostAddress         discoveryAddress;
    QString              serverUrl;
};

#endif // SERVERDISCOVERER_H
