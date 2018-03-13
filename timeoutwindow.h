#ifndef TIMEOUTWINDOW_H
#define TIMEOUTWINDOW_H

#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QLabel>

class TimeoutWindow : public QWidget
{
    Q_OBJECT

public:
    explicit TimeoutWindow(QWidget *parent = nullptr);
    ~TimeoutWindow();

public:
    void startTimeout(int msecTime);
    void stopTimeout();

private:
    void setDisplayedText(QString sNewText);

public slots:
    void resizeEvent(QResizeEvent *event);
    void updateTime();

signals:

private:
    QSize mySize;
    QString sDisplayedText;
    QLabel myLabel;
    QTimer TimerTimeout;
    QTimer TimerUpdate;
};

#endif // TIMEOUTWINDOW_H
