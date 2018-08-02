#ifndef SERVERDISCOVERER_H
#define SERVERDISCOVERER_H

#include <QObject>
#include <QList>
#include <QVector>
#include <QHostAddress>
#include <QSslError>
#include <QTimer>

QT_FORWARD_DECLARE_CLASS(QUdpSocket)
QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(NoNetWindow)
QT_FORWARD_DECLARE_CLASS(ScorePanel)

class ServerDiscoverer : public QObject
{
    Q_OBJECT
public:
    explicit ServerDiscoverer(QFile *_logFile=Q_NULLPTR, QObject *parent=Q_NULLPTR);

signals:
    void serverFound(QString serverUrl, int panelType);
    void checkNetwork();

public slots:

private slots:
    void onProcessDiscoveryPendingDatagrams();
    void onDiscoverySocketError(QAbstractSocket::SocketError error);
    void onPanelServerConnected();
    void onPanelServerSocketError(QAbstractSocket::SocketError error);
    void onServerConnectionTimeout();
    void onPanelClosed();
    void onExitProgram();

public:
    bool Discover();

protected:
    void checkServerAddresses();

private:
    QFile               *logFile;
    QList<QHostAddress>  broadcastAddress;
    QVector<QUdpSocket*> discoverySocketArray;
    QVector<QWebSocket*> serverSocketArray;
    quint16              discoveryPort;
    quint16              serverPort;
    QHostAddress         discoveryAddress;
    int                  panelType;
    QStringList          serverList;
    QWebSocket          *pPanelServerSocket;
    QString              serverUrl;
    QTimer               serverConnectionTimeoutTimer;
    NoNetWindow         *pNoServerWindow;
    ScorePanel          *pScorePanel;
};

#endif // SERVERDISCOVERER_H
