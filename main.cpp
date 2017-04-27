#include "chooserwidget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // Some underlying window implementations will reset the cursor if it leaves
    // a widget even if the mouse is grabbed.
    // If you want to have a cursor set for all widgets, even when outside the window,
    // consider QApplication::setOverrideCursor().
    QApplication::setOverrideCursor(Qt::BlankCursor);
    chooserWidget w;
    w.setVisible(false);
    return a.exec();
}
