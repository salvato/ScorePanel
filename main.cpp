#include "myapplication.h"


int
main(int argc, char *argv[]) {
    MyApplication a(argc, argv);
    int result = a.exec();
    return result;
}
