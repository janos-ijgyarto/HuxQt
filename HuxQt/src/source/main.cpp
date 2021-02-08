#include "stdafx.h"
#include "UI/HuxQt.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    HuxApp::HuxQt w;
    w.setWindowIcon(QIcon(":/HuxQt/fat_hux.ico"));
    w.show();
    return a.exec();
}
