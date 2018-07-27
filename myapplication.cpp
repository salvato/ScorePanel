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
        bool bResult = QApplication::notify(receiver, event);
        return bResult;
    } catch(std::exception& e) {
        qCritical() << tr("Emessa un'eccezione:") << e.what();
    }
    return false;
}
