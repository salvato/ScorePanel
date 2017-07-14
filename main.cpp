#include "chooserwidget.h"
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    #include "slidewindow2.h"
    #include "slidewindow_adaptor.h"
    #include <QtDBus/QDBusConnection>
#endif

#include <QApplication>
#include <QScreen>
#include <QRect>
#include <QFont>
#include <QFontMetrics>


int
main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    // We want to have a cursor set for all widgets,
    // even when outside the window then :
    QApplication::setOverrideCursor(Qt::BlankCursor);

    chooserWidget w;
    w.setVisible(false);

#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    SlideWindow *pSlideWindow = new SlideWindow();
    new SlideShowInterfaceAdaptor(pSlideWindow);
    QDBusConnection connection = QDBusConnection::sessionBus();
    connection.registerObject("/SlideShow", pSlideWindow);
    connection.registerService("org.salvato.gabriele.SlideShow");
#endif

    return a.exec();
}
