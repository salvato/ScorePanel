#include "myapplication.h"
#include "chooserwidget.h"


MyApplication::MyApplication(int& argc, char ** argv)
    : QApplication(argc, argv)
{
    // We want the cursor set for all widgets,
    // even when outside the window then:
    setOverrideCursor(Qt::BlankCursor);
    pW = new chooserWidget();
}


bool
MyApplication::notify(QObject * receiver, QEvent * event) {
    try {
        return QApplication::notify(receiver, event);
    } catch(std::exception& e) {
        qCritical() << "Exception thrown:" << e.what();
    }
    return false;
}
