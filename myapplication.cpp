#include "myapplication.h"
#include "chooserwidget.h"


MyApplication::MyApplication(int& argc, char ** argv)
    : QApplication(argc, argv)
{
    // We want the cursor set for all widgets,
    // even when outside the window then:
    setOverrideCursor(Qt::BlankCursor);
    pW = new chooserWidget();
    pW->start();
}

