#include "messenger_window.h"
#include "single_instance.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QMessageBox>
#include <QSystemTrayIcon>

static QString singleInstanceKey()
{
    const QByteArray hash = QCryptographicHash::hash(
        QDir::homePath().toUtf8(),
        QCryptographicHash::Sha1);
    return QStringLiteral("yandex-messenger-qt-%1")
        .arg(QString::fromLatin1(hash.toHex()));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Yandex Messenger Qt"));
    QApplication::setOrganizationName(QStringLiteral("Local"));
    QApplication::setOrganizationDomain(QStringLiteral("local"));
    QApplication::setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::warning(
            nullptr,
            QStringLiteral("Yandex Messenger Qt"),
            QStringLiteral(
                "Системный трей не найден. Приложение запустится, "
                "но сворачивание в трей может не работать."));
    }

    SingleInstance singleInstance;
    if (!singleInstance.initialize(singleInstanceKey())) {
        singleInstance.sendActivateRequest();
        return 0;
    }

    MessengerWindow window;
    QObject::connect(&singleInstance, &SingleInstance::activateRequested,
                     &window, &MessengerWindow::activateFromSecondInstance);

    if (window.startHidden() && QSystemTrayIcon::isSystemTrayAvailable()) {
        window.hide();
    } else {
        window.showAndActivate();
    }

    return app.exec();
}
