#ifndef MYAPPLICATION_H
#define MYAPPLICATION_H

#include <QApplication>

QT_FORWARD_DECLARE_CLASS(chooserWidget)

class MyApplication : public QApplication
{
public:
    MyApplication(int& argc, char ** argv);

private:
    chooserWidget* pW;
};

#endif // MYAPPLICATION_H
