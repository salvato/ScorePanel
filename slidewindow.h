#ifndef SLIDEWINDOW_H
#define SLIDEWINDOW_H

#include <QTimer>
#include <QLabel>
#include<QFileInfoList>

#include <qevent.h>


class SlideWindow : public QLabel
{
    Q_OBJECT

public:
    SlideWindow(QWidget *parent = 0);
    ~SlideWindow();
    void setSlideDir(QString sNewDir);
    void keyPressEvent(QKeyEvent *event);
    void addNewImage(QImage image);
    void startSlideShow();
    void stopSlideShow();
    void pauseSlideShow();
    bool isReady();
    bool isRunning();

public:
    enum transitionMode {
        transition_Abrupt,
        transition_FromLeft,
        transition_Fade
    };

private:
    void computeRegions(QRect* sourcePresent, QRect* destinationPresent, QRect* sourceNext, QRect* destinationNext);

public slots:
    void onNewSlideTimer();
    void onTransitionTimeElapsed();
    void resizeEvent(QResizeEvent *event);

private:
    QString sSlideDir;
    QFileInfoList slideList;
    QImage* pPresentImage;
    QImage* pNextImage;
    QImage* pPresentImageToShow;
    QImage* pNextImageToShow;
    QImage* pShownImage;

    QTimer showTimer;
    QTimer transitionTimer;

    int iCurrentSlide;
    int steadyShowTime;
    int transitionTime;
    int transitionGranularity;
    int transitionStepNumber;
    QSize mySize;
    QRect rectSourcePresent;
    QRect rectSourceNext;
    QRect rectDestinationPresent;
    QRect rectDestinationNext;

    transitionMode transitionType;
    bool bRunning;
};

#endif // SLIDEWINDOW_H
