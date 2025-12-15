#include "gui/MainWindow.h"

#include <QApplication>

#ifdef __MINGW32__
#include <stdlib.h>
extern int __argc;
extern char **__argv;
// Patch for undefined reference to `__imp___argc` with newer MinGW / Qt6
__attribute__((__used__)) int *__imp___argc = &__argc;
__attribute__((__used__)) char ***__imp___argv = &__argv;
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
