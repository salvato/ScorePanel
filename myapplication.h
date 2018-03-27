#ifndef MYAPPLICATION_H
#define MYAPPLICATION_H

#include <QApplication>

QT_FORWARD_DECLARE_CLASS(chooserWidget)

class MyApplication : public QApplication
{
public:
    MyApplication(int& argc, char ** argv);
    virtual ~MyApplication() { }
    // reimplemented from QApplication so we can throw exceptions in slots
    virtual bool notify(QObject * receiver, QEvent * event);
private:
    chooserWidget* pW;
};

#endif // MYAPPLICATION_H
