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


class MyApplication : public QApplication {
public:
    MyApplication(int& argc, char ** argv)
        : QApplication(argc, argv) { }
    virtual ~MyApplication() { }

    // reimplemented from QApplication so we can throw exceptions in slots
    virtual bool
    notify(QObject * receiver, QEvent * event) {
        try {
            return QApplication::notify(receiver, event);
        } catch(std::exception& e) {
            qCritical() << "Exception thrown:" << e.what();
        }
        return false;
    }
};


int
main(int argc, char *argv[]) {
    MyApplication a(argc, argv);
    // We want to have a cursor set for all widgets,
    // even when outside the window then :
    MyApplication::setOverrideCursor(Qt::BlankCursor);

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
