#include "chooserwidget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    chooserWidget w;
    w.setVisible(false);
    return a.exec();
}
