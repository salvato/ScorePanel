#include "chooserwidget.h"
#include <QApplication>

#include <QScreen>
#include <QRect>
#include <QFont>
#include <QFontMetrics>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // Some window implementations will reset the cursor
    // if it leaves a widget even if the mouse is grabbed.
    // Since we want to have a cursor set for all widgets,
    // even when outside the window, we will use
    // QApplication::setOverrideCursor().
    QApplication::setOverrideCursor(Qt::BlankCursor);

    chooserWidget w;
    w.setVisible(false);
    return a.exec();
}
