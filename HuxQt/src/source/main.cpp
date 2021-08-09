#include "stdafx.h"
#include "UI/HuxQt.h"
#include <QtWidgets/QApplication>

#ifdef Q_OS_WIN
#include <shobjidl_core.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    DWORD currentProcessId = GetCurrentProcessId();
    WCHAR AppID[100];
    swprintf(AppID, sizeof(AppID) / sizeof(AppID[0]), L"HuxAppID%u", currentProcessId);
    SetCurrentProcessExplicitAppUserModelID(AppID);
#endif
    QApplication a(argc, argv);
    HuxApp::HuxQt w;
    w.setWindowIcon(QIcon(":/HuxQt/fat_hux.ico"));
    w.show();
    return a.exec();
}