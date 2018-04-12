#include "timeoutwindow.h"
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QScreen>

TimeoutWindow::TimeoutWindow(QWidget *parent)
    : QWidget(parent)
{
    Q_UNUSED(parent);
    setMinimumSize(QSize(320, 240));

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect  screenGeometry = screen->geometry();
    int width = screenGeometry.width();
    int height = screenGeometry.height();
    int iFontSize = 400;
    for(int i=48; i<2000; i++) {
        QFontMetrics f(QFont("Arial", i, QFont::Black));
        int rW = f.width("88");
        int rH = f.height();
        if((rW > width)  || (rH > height)){
            iFontSize = i-1;
            break;
        }
    }

    myLabel.setText(QString("No Text"));
    myLabel.setAlignment(Qt::AlignCenter);
    myLabel.setFont(QFont("Arial", iFontSize, QFont::Black));

    QPalette pal(QWidget::palette());
    pal.setColor(QPalette::Window,        Qt::black);
    pal.setColor(QPalette::WindowText,    Qt::yellow);
    pal.setColor(QPalette::Base,          Qt::black);
    pal.setColor(QPalette::AlternateBase, Qt::blue);
    pal.setColor(QPalette::Text,          Qt::yellow);
    pal.setColor(QPalette::BrightText,    Qt::white);
    setPalette(pal);
    myLabel.setPalette(pal);

    setWindowOpacity(0.8);
    sDisplayedText = tr("-- No Text --");
    myLabel.setText(sDisplayedText);
    QVBoxLayout *panelLayout = new QVBoxLayout();
    panelLayout->addWidget(&myLabel);
    setLayout(panelLayout);

    connect(&TimerUpdate, SIGNAL(timeout()),
            this, SLOT(updateTime()));
    TimerTimeout.setSingleShot(true);
    connect(&TimerTimeout, SIGNAL(timeout()),
            this, SLOT(updateTime()));
}


TimeoutWindow::~TimeoutWindow() {
}


void
TimeoutWindow::resizeEvent(QResizeEvent *event) {
    mySize = event->size();
}


void
TimeoutWindow::setDisplayedText(QString sNewText) {
    sDisplayedText = sNewText;
    myLabel.setText(sDisplayedText);
}


void
TimeoutWindow::updateTime() {
    int remainingTime = TimerTimeout.remainingTime();
    sDisplayedText = QString("%1").arg(1+(remainingTime/1000));
    myLabel.setText(sDisplayedText);
    if(remainingTime > 0)
        update();
    else {
        TimerUpdate.stop();
        TimerTimeout.stop();
        hide();
    }
}


void
TimeoutWindow::startTimeout(int msecTime) {
    TimerTimeout.start(msecTime);
    TimerUpdate.start(100);
    sDisplayedText = QString("%1").arg(int(0.999+(TimerTimeout.remainingTime()/1000.0)));
    myLabel.setText(sDisplayedText);
    show();
}


void
TimeoutWindow::stopTimeout() {
    TimerUpdate.stop();
    TimerTimeout.stop();
    hide();
}

