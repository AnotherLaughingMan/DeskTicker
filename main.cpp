#include "ClockWidget.h"

#include <QApplication>
#include <QDebug>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("DeskTicker"));
    QCoreApplication::setOrganizationName(QStringLiteral("DeskTicker"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DESKTICKER_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/DeskTicker.ico")));

    if (app.arguments().contains(QStringLiteral("--sync-startup"))) {
        QString errorMessage;
        if (!ClockWidget::synchronizeStartupRegistration(&errorMessage)) {
            qCritical().noquote() << errorMessage;
            return 1;
        }
        return 0;
    }

    ClockWidget clock;
    clock.show();

    return app.exec();
}
