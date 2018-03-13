#include "timeoutwindow.h"
#include <QVBoxLayout>
#include <QResizeEvent>

TimeoutWindow::TimeoutWindow(QWidget *parent)
    : QWidget(parent)
{
    Q_UNUSED(parent);
    setMinimumSize(QSize(320, 240));
//    setAttribute(Qt::WA_TranslucentBackground);

//    QScreen *screen = QGuiApplication::primaryScreen();
//    QRect  screenGeometry = screen->geometry();
//    int width = screenGeometry.width();
//    iTeamFontSize = 100;
//    for(int i=12; i<100; i++) {
//        QFontMetrics f(QFont("Arial", i, QFont::Black));
//        int rW = f.maxWidth()*maxTeamNameLen;
//        if(rW > width/2) {
//            iTeamFontSize = i-1;
//            break;
//        }
//    }

    myLabel.setText(QString("No Text"));
    myLabel.setAlignment(Qt::AlignCenter);
    myLabel.setFont(QFont("Arial", 24));

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
    sDisplayedText = tr("In Attesa della Connessione con la Rete");
    myLabel.setText(sDisplayedText);
    QVBoxLayout *panelLayout = new QVBoxLayout();
    panelLayout->addWidget(&myLabel);
    setLayout(panelLayout);

    connect(&TimerUpdate, SIGNAL(timeout()),
            this, SLOT(update()));
    TimerTimeout.setSingleShot(true);
    connect(&TimerTimeout, SIGNAL(timeout()),
            this, SLOT(update()));
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
    sDisplayedText = QString().arg(remainingTime/1000);
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
    sDisplayedText = QString().arg(TimerTimeout.remainingTime()/1000);
    myLabel.setText(sDisplayedText);
    show();
}


void
TimeoutWindow::stopTimeout() {
    TimerUpdate.stop();
    TimerTimeout.stop();
    hide();
}

