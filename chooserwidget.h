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


class chooserWidget : public QObject
{
    Q_OBJECT

public:
    chooserWidget(QWidget *parent = Q_NULLPTR);
    ~chooserWidget();

private slots:
    void startServerDiscovery();
    void onServerFound(QString serverUrl, int panelType);
    void onTimeToCheckNetwork();
    void onConnectionTimerElapsed();
    void onExitProgram();
    void onPanelClosed();
    void onStart();

private:
    bool isConnectedToNetwork();
    bool PrepareLogFile();

private:
    QFile             *logFile;
    ServerDiscoverer  *pServerDiscoverer;
    NoNetWindow       *pNoNetWindow;
    QWebSocket        *pServerSocket;
    ScorePanel        *pScorePanel;
    QString            logFileName;
    QTimer             networkReadyTimer;
    QTimer             connectionTimer;
    QTimer             startTimer;
    int                connectionTime;
    QString            serverUrl;
};

#endif // CHOOSERWIDGET_H
