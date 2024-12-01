#include <HuxQt/UI/HuxQt.h>
#include <QtWidgets/QApplication>

#include <QStyleFactory>

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
    QApplication application(argc, argv);

    // Use fusion style (allows us to add dark mode)
    application.setStyle(QStyleFactory::create("Fusion"));

    HuxApp::HuxQt w;
    w.setWindowIcon(QIcon(":/HuxQt/fat_hux.ico"));
    w.show();

    return application.exec();
}