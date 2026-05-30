#include "autostart_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

QString AutostartManager::autostartFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/autostart/yandex-messenger-qt.desktop");
}

bool AutostartManager::isEnabled()
{
    return QFileInfo::exists(autostartFilePath());
}

bool AutostartManager::setEnabled(bool enabled, QString *errorMessage)
{
    const QString path = autostartFilePath();
    const QFileInfo fileInfo(path);

    if (!enabled) {
        if (fileInfo.exists() && !QFile::remove(path)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Не удалось удалить файл автозапуска: %1").arg(path);
            }
            return false;
        }
        return true;
    }

    const QDir configDir(QFileInfo(path).absolutePath());
    if (!configDir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать каталог автозапуска: %1")
                .arg(configDir.absolutePath());
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось открыть файл автозапуска для записи: %1")
                .arg(path);
        }
        return false;
    }

    const QString execPath = QCoreApplication::applicationFilePath();
    QString desktopExec = execPath;
    desktopExec.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    desktopExec.replace(QStringLiteral(" "), QStringLiteral("\\ "));
    desktopExec.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    const QString content = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Yandex Messenger Qt\n"
        "Comment=Unofficial Qt wrapper for Yandex Messenger\n"
        "Exec=%1\n"
        "Icon=yandex-messenger-qt\n"
        "Terminal=false\n"
        "Categories=Network;InstantMessaging;\n"
        "StartupNotify=true\n")
        .arg(desktopExec);

    if (file.write(content.toUtf8()) == -1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось записать файл автозапуска: %1").arg(path);
        }
        return false;
    }

    return true;
}
