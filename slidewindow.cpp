#include <QDir>
#include <QDebug>
#include <QPainter>

#include "slidewindow.h"


#define STEADY_SHOW_TIME       12000// Change slide time
#define TRANSITION_TIME        3000 // Transition duration
#define TRANSITION_GRANULARITY 30   // Steps to complete transition


SlideWindow::SlideWindow(QWidget *parent)
    : QLabel("In Attesa della Connessione con la Rete")
    , pPresentImage(NULL)
    , pNextImage(NULL)
    , pPresentImageToShow(NULL)
    , pNextImageToShow(NULL)
    , pShownImage(NULL)
    , iCurrentSlide(0)
    , steadyShowTime(STEADY_SHOW_TIME)
    , transitionTime(TRANSITION_TIME)
    , transitionGranularity(TRANSITION_GRANULARITY)
    , transitionStepNumber(0)
#ifdef Q_OS_ANDROID
    , transitionType(transition_Abrupt)
#else
    , transitionType(transition_FromLeft)
#endif
    , bRunning(false)
{
    Q_UNUSED(parent);
    setAlignment(Qt::AlignCenter);
    setMinimumSize(QSize(320, 240));

    connect(&transitionTimer, SIGNAL(timeout()),
            this, SLOT(onTransitionTimeElapsed()));
    connect(&showTimer, SIGNAL(timeout()),
            this, SLOT(onNewSlideTimer()));
}


SlideWindow::~SlideWindow() {

}


bool
SlideWindow::isReady() {
    return (pPresentImage != NULL && pNextImage != NULL);
}


void
SlideWindow::addNewImage(QByteArray baMessage) {
    QImage* pImage = new QImage();
    bool bResult = pImage->loadFromData(baMessage);
    if(!bResult) {
#ifdef QT_DEBUG
        qDebug() << "Error receiving image data";
#endif
        return;
    }
    if(pPresentImage == NULL) {// That's the first image...
        pPresentImage = pImage;
        pNextImage    = NULL;
        emit getNextImage();
    }
    else if(pNextImage == NULL) {
        pNextImage    = pImage;

        QImage scaledPresentImage = pPresentImage->scaled(size(), Qt::KeepAspectRatio);
        QImage scaledNextImage    = pNextImage->scaled(size(), Qt::KeepAspectRatio);

        if(pPresentImageToShow) delete pPresentImageToShow;
        if(pNextImageToShow)    delete pNextImageToShow;
        if(pShownImage)         delete pShownImage;

        pPresentImageToShow = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
        pNextImageToShow    = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
        pShownImage         = new QImage(size(), QImage::Format_ARGB32_Premultiplied);

        int x = (size().width()-scaledPresentImage.width())/2;
        int y = (size().height()-scaledPresentImage.height())/2;

        QPainter presentPainter(pPresentImageToShow);
        presentPainter.setCompositionMode(QPainter::CompositionMode_Source);
        presentPainter.fillRect(rect(), Qt::white);
        presentPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        presentPainter.drawImage(x, y, scaledPresentImage);
        presentPainter.end();

        x = (size().width()-scaledNextImage.width())/2;
        y = (size().height()-scaledNextImage.height())/2;

        QPainter nextPainter(pNextImageToShow);
        nextPainter.setCompositionMode(QPainter::CompositionMode_Source);
        nextPainter.fillRect(rect(), Qt::white);
        nextPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        nextPainter.drawImage(x, y, scaledNextImage);
        nextPainter.end();

        computeRegions(&rectSourcePresent, &rectDestinationPresent,
                       &rectSourceNext,    &rectDestinationNext);

        QPainter painter(pShownImage);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(rectDestinationNext, *pNextImageToShow, rectSourceNext);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(rectDestinationPresent, *pPresentImageToShow, rectSourcePresent);
        painter.end();

        setPixmap(QPixmap::fromImage(*pShownImage));
    }
    else {
        delete pPresentImage;
        pPresentImage = pNextImage;
        pNextImage = pImage;
    }
}


void
SlideWindow::startSlideShow() {
    emit getNextImage();
    showTimer.start(steadyShowTime);
    bRunning = true;
}


void
SlideWindow::stopSlideShow() {
    showTimer.stop();
    transitionTimer.stop();
    bRunning = false;
}


void
SlideWindow::pauseSlideShow() {
    showTimer.stop();
    transitionTimer.stop();
    bRunning = false;
}


bool
SlideWindow::isRunning() {
    return bRunning;
}

void
SlideWindow::computeRegions(QRect* sourcePresent, QRect* destinationPresent,
               QRect* sourceNext,    QRect* destinationNext) {
    double percent = double(transitionStepNumber)/double(transitionGranularity);
    *sourcePresent = QRect(0, 0,
                           int(width()*(1.0-percent)+0.5), height());
    *destinationPresent = *sourcePresent;
    destinationPresent->translate(width()*percent, 0);

    *sourceNext = QRect(int(double(width())*(1.0-percent)+0.5), 0,
                        int(width()*percent+0.5), height());
    *destinationNext = QRect(0, 0,
                             int(width()*percent+0.5), height());
}


void
SlideWindow::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Escape) {
#ifndef Q_OS_ANDROID
        qDebug() << "Exit requested";
#endif
        close();
    }
    if(event->key() == Qt::Key_F1) {
        showNormal();
        event->accept();
        return;
    }
}


void
SlideWindow::resizeEvent(QResizeEvent *event) {
    mySize = event->size();
    if(!pPresentImage || !pNextImage) {
        event->accept();
        return;
    }
    QImage scaledPresentImage = pPresentImage->scaled(size(), Qt::KeepAspectRatio);
    QImage scaledNextImage    = pNextImage->scaled(size(), Qt::KeepAspectRatio);

    if(pPresentImageToShow) delete pPresentImageToShow;
    if(pNextImageToShow)    delete pNextImageToShow;
    if(pShownImage)         delete pShownImage;

    pPresentImageToShow = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
    pNextImageToShow    = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
    pShownImage         = new QImage(size(), QImage::Format_ARGB32_Premultiplied);

    int x = (size().width()-scaledPresentImage.width())/2;
    int y = (size().height()-scaledPresentImage.height())/2;

    QPainter presentPainter(pPresentImageToShow);
    presentPainter.setCompositionMode(QPainter::CompositionMode_Source);
    presentPainter.fillRect(rect(), Qt::white);
    presentPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    presentPainter.drawImage(x, y, scaledPresentImage);
    presentPainter.end();

    x = (size().width()-scaledNextImage.width())/2;
    y = (size().height()-scaledNextImage.height())/2;

    QPainter nextPainter(pNextImageToShow);
    nextPainter.setCompositionMode(QPainter::CompositionMode_Source);
    nextPainter.fillRect(rect(), Qt::white);
    nextPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    nextPainter.drawImage(x, y, scaledNextImage);
    nextPainter.end();

    computeRegions(&rectSourcePresent, &rectDestinationPresent,
                   &rectSourceNext,    &rectDestinationNext);

    QPainter painter(pShownImage);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(rectDestinationNext, *pNextImageToShow, rectSourceNext);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(rectDestinationPresent, *pPresentImageToShow, rectSourcePresent);
    painter.end();

    setPixmap(QPixmap::fromImage(*pShownImage));
}


void
SlideWindow::onNewSlideTimer() {
    if(!pPresentImage || !pNextImage) return;
    if(transitionType == transition_FromLeft) {
        showTimer.stop();
        transitionStepNumber = 0;
        transitionTimer.start(int(double(transitionTime)/double(transitionGranularity)));
    } else if(transitionType == transition_Abrupt) {
        transitionStepNumber = 0;
        if(pPresentImageToShow) delete pPresentImageToShow;
        *pPresentImage = *pNextImage;
        pPresentImageToShow = pNextImageToShow;
        emit getNextImage();
        QImage scaledNextImage = pNextImage->scaled(size(), Qt::KeepAspectRatio);
        pNextImageToShow    = new QImage(size(), QImage::Format_ARGB32_Premultiplied);

        if(pShownImage) delete pShownImage;
        pShownImage = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
        int x = (size().width()-scaledNextImage.width())/2;
        int y = (size().height()-scaledNextImage.height())/2;

        QPainter nextPainter(pNextImageToShow);
        nextPainter.setCompositionMode(QPainter::CompositionMode_Source);
        nextPainter.fillRect(rect(), Qt::white);
        nextPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        nextPainter.drawImage(x, y, scaledNextImage);
        nextPainter.end();

        computeRegions(&rectSourcePresent, &rectDestinationPresent,
                       &rectSourceNext,    &rectDestinationNext);
        QPainter painter(pShownImage);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(rectDestinationNext, *pNextImageToShow, rectSourceNext);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(rectDestinationPresent, *pPresentImageToShow, rectSourcePresent);
        painter.end();

        setPixmap(QPixmap::fromImage(*pShownImage));
    }// else if (transitionType == other types...
}


void
SlideWindow::onTransitionTimeElapsed() {
    if(!pPresentImage || !pNextImage) return;
    transitionStepNumber++;
    if(transitionStepNumber > transitionGranularity) {
        transitionTimer.stop();
        transitionStepNumber = 0;
        if(pPresentImageToShow) delete pPresentImageToShow;
        *pPresentImage = *pNextImage;
        pPresentImageToShow = pNextImageToShow;
        emit getNextImage();
        QImage scaledNextImage = pNextImage->scaled(size(), Qt::KeepAspectRatio);
        pNextImageToShow    = new QImage(size(), QImage::Format_ARGB32_Premultiplied);

        if(pShownImage) delete pShownImage;
        pShownImage = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
        int x = (size().width()-scaledNextImage.width())/2;
        int y = (size().height()-scaledNextImage.height())/2;

        QPainter nextPainter(pNextImageToShow);
        nextPainter.setCompositionMode(QPainter::CompositionMode_Source);
        nextPainter.fillRect(rect(), Qt::white);
        nextPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        nextPainter.drawImage(x, y, scaledNextImage);
        nextPainter.end();
        showTimer.start(steadyShowTime);
    }
    computeRegions(&rectSourcePresent, &rectDestinationPresent,
                   &rectSourceNext,    &rectDestinationNext);
    QPainter painter(pShownImage);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(rectDestinationNext, *pNextImageToShow, rectSourceNext);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(rectDestinationPresent, *pPresentImageToShow, rectSourcePresent);
    painter.end();

    setPixmap(QPixmap::fromImage(*pShownImage));
}
