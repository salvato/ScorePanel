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

private slots:
    void startServerDiscovery();
    void onServerFound(QString serverUrl, int panelType);
    void onTimeToCheckNetwork();
    void onConnectionTimerElapsed();
    void closePanels();

private:
    bool isConnectedToNetwork();
    bool PrepareLogFile();

private:
    QString            logFileName;
    QFile             *logFile;
    ServerDiscoverer  *pServerDiscoverer;
    NoNetWindow       *pNoNetWindow;
    QTimer             networkReadyTimer;
    QTimer             connectionTimer;
    int                connectionTime;
    QWebSocket        *pServerSocket;
    QUrl               serverUrl;
    ScorePanel        *pScorePanel;
};

#endif // CHOOSERWIDGET_H
