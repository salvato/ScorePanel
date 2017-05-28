#include <QDir>
#include <QDebug>
//#include <QPainter>
#include <QVBoxLayout>

#include "nonetwindow.h"


NoNetWindow::NoNetWindow(QWidget *parent)
    : QWidget(parent)
{
    Q_UNUSED(parent);
    setMinimumSize(QSize(320, 240));
//    setAttribute(Qt::WA_TranslucentBackground);

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
}


NoNetWindow::~NoNetWindow() {
}


void
NoNetWindow::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Escape) {
        close();
    }
    if(event->key() == Qt::Key_F1) {
        showNormal();
        event->accept();
        return;
    }
}


void
NoNetWindow::resizeEvent(QResizeEvent *event) {
    mySize = event->size();
}


void
NoNetWindow::setDisplayedText(QString sNewText) {
    sDisplayedText = sNewText;
    myLabel.setText(sDisplayedText);
}

