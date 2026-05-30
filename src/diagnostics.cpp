#include "diagnostics.h"

#include "app_version.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QStringList>

QString Diagnostics::text()
{
    const auto envValue = [](const char *name) {
        const QByteArray value = qgetenv(name);
        return value.isEmpty() ? QStringLiteral("unknown") : QString::fromUtf8(value);
    };

    QStringList lines;
    lines << QStringLiteral("Application: Yandex Messenger Qt");
    lines << QStringLiteral("Application version: %1").arg(QString::fromLatin1(APP_VERSION));
    lines << QStringLiteral("Qt version: %1").arg(QString::fromLatin1(qVersion()));
    lines << QStringLiteral("Session type: %1").arg(envValue("XDG_SESSION_TYPE"));
    lines << QStringLiteral("Desktop: %1").arg(envValue("XDG_CURRENT_DESKTOP"));
    lines << QStringLiteral("Platform plugin: %1").arg(envValue("QT_QPA_PLATFORM"));
    lines << QStringLiteral("Profile path: %1")
        .arg(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/webengine"));
    lines << QStringLiteral("Cache path: %1")
        .arg(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/webengine"));
    return lines.join('\n');
}
