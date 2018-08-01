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
    void serverFound(QUrl serverUrl, int panelType);
    void checkServerAddress();

public slots:

private slots:
    void onProcessDiscoveryPendingDatagrams();
    void onDiscoverySocketError(QAbstractSocket::SocketError error);
    void onCheckServerAddress();
    void onPanelServerConnected();
    void onPanelServerSocketError(QAbstractSocket::SocketError error);

public:
    bool Discover();

private:
    QFile               *logFile;
    QList<QHostAddress>  broadcastAddress;
    QVector<QUdpSocket*> discoverySocketArray;
    quint16              discoveryPort;
    quint16              serverPort;
    QHostAddress         discoveryAddress;
    QString              serverUrl;
    QStringList          serverList;
    QWebSocket          *pPanelServerSocket;
};

#endif // SERVERDISCOVERER_H
