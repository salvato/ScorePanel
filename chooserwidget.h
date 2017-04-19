#ifndef CHOOSERWIDGET_H
#define CHOOSERWIDGET_H


#include <QWidget>
#include <QTimer>
#include <QTextStream>
#include <QDateTime>
#include <QUrl>
#include <QAbstractSocket>


QT_FORWARD_DECLARE_CLASS(ServerDiscoverer)
QT_FORWARD_DECLARE_CLASS(NoNetWindow)
QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(ScorePanel)


class chooserWidget : public QWidget
{
    Q_OBJECT

public:
    chooserWidget(QWidget *parent = 0);
    ~chooserWidget();

protected:
    void logMessage(QString sFunctionName, QString sMessage);

private slots:
    bool isConnectedToNetwork();
    void onServerConnected(QWebSocket *pSocket);
    void onServerDisconnected();
    void onTimeToEmitPing();
    void onPongReceived(quint64 elapsed, QByteArray payload);
    void onTimeToCheckNetwork();
    void onTimeToCheckConnection();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onConnectionTimerElapsed();
    void onTextMessageReceived(QString sMessage);

private:
    QString XML_Parse(QString input_string, QString token);
    bool PrepareLogFile();

private:
    QTextStream        sDebugInformation;
    QString            sDebugMessage;
    QString            logFileName;
    QDateTime          dateTime;
    QFile             *logFile;
    ServerDiscoverer  *pServerDiscoverer;
    NoNetWindow       *pNoNetWindow;
    QTimer             networkReadyTimer;
    QTimer             connectionTimer;
    int                connectionTime;
    QWebSocket        *pWebSocket;
    QUrl               serverUrl;
    QString            sNoData;
    QTimer            *pTimerPing;
    int                pingPeriod;
    int                nPong;
    QTimer            *pTimerCheckConnection;
    ScorePanel        *pScorePanel;
};

#endif // CHOOSERWIDGET_H
