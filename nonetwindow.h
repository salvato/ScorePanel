#ifndef NONETWINDOW_H
#define NONETWINDOW_H

#include <QTimer>
#include <QWidget>
#include <QLabel>
#include <qevent.h>


class NoNetWindow : public QWidget
{
    Q_OBJECT

public:
    NoNetWindow(QWidget *parent = 0);
    ~NoNetWindow();
    void keyPressEvent(QKeyEvent *event);

public:
    void setDisplayedText(QString sNewText);
private:

public slots:
    void resizeEvent(QResizeEvent *event);

signals:

private:
    QSize mySize;
    QString sDisplayedText;
    QLabel myLabel;
};

#endif // NONETWINDOW_H
