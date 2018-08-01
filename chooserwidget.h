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


class chooserWidget : public QObject
{
    Q_OBJECT

public:
    chooserWidget(QWidget *parent = Q_NULLPTR);
    ~chooserWidget();
    void start();

private slots:
    void onTimeToCheckNetwork();
//    void onServerFound(QString serverUrl, int panelType);
//    void onConnectionTimerElapsed();
//    void onExitProgram();
//    void onPanelClosed();

private:
    bool isConnectedToNetwork();
    bool PrepareLogFile();

private:
    QFile             *logFile;
    ServerDiscoverer  *pServerDiscoverer;
    NoNetWindow       *pNoNetWindow;
    QString            logFileName;
    QTimer             networkReadyTimer;
};

#endif // CHOOSERWIDGET_H
