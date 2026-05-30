#include "messenger_window.h"

#include "app_version.h"
#include "autostart_manager.h"
#include "diagnostics.h"
#include "page_patches.h"
#include "web_popup_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QFont>
#include <QDir>
#include <QDesktopServices>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QProcess>
#include <QPoint>
#include <QRect>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QSize>
#include <QUrl>
#include <QWebEngineCookieStore>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineNotification>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QWebEnginePermission>
#endif

#include <algorithm>
#include <memory>

namespace {
constexpr auto kAppName = "Yandex Messenger Qt";
constexpr auto kMessengerUrl = "https://yandex.ru/chat";
constexpr auto kRepoUrl = "https://github.com/glebrodionov94/yandex-messenger-qt";
constexpr auto kIssuesUrl = "https://github.com/glebrodionov94/yandex-messenger-qt/issues";

QIcon createWindowIcon()
{
    constexpr int size = 128;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#ff0000"));
    painter.drawEllipse(8, 8, 112, 112);

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(82);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(8, 5, 112, 112), Qt::AlignCenter, QStringLiteral("Я"));

    return QIcon(pixmap);
}

QIcon createTrayIcon(bool unread)
{
    constexpr int size = 64;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor foreground = QApplication::palette().color(QPalette::WindowText);
    if (!foreground.isValid()) {
        foreground = QApplication::palette().color(QPalette::Text);
    }
    if (!foreground.isValid()) {
        foreground = Qt::white;
    }

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(48);
    painter.setFont(font);
    painter.setPen(foreground);
    painter.drawText(QRect(2, -2, 58, 62), Qt::AlignCenter, QStringLiteral("Я"));

    if (unread) {
        painter.setPen(QPen(Qt::white, 3));
        painter.setBrush(QColor("#ff0000"));
        painter.drawEllipse(43, 3, 18, 18);
    }

    return QIcon(pixmap);
}

QString permissionDescription(int permissionType)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    using Type = QWebEnginePermission::PermissionType;
    switch (static_cast<Type>(permissionType)) {
    case Type::MediaAudioCapture:
        return QStringLiteral("доступ к микрофону");
    case Type::MediaVideoCapture:
        return QStringLiteral("доступ к камере");
    case Type::MediaAudioVideoCapture:
        return QStringLiteral("доступ к микрофону и камере");
    case Type::DesktopVideoCapture:
        return QStringLiteral("демонстрацию экрана");
    case Type::DesktopAudioVideoCapture:
        return QStringLiteral("демонстрацию экрана со звуком");
    default:
        return QStringLiteral("дополнительное разрешение");
    }
#else
    Q_UNUSED(permissionType);
    return QStringLiteral("дополнительное разрешение");
#endif
}

QString permissionDescription(QWebEnginePage::Feature feature)
{
    switch (feature) {
    case QWebEnginePage::MediaAudioCapture:
        return QStringLiteral("доступ к микрофону");
    case QWebEnginePage::MediaVideoCapture:
        return QStringLiteral("доступ к камере");
    case QWebEnginePage::MediaAudioVideoCapture:
        return QStringLiteral("доступ к микрофону и камере");
    case QWebEnginePage::DesktopVideoCapture:
        return QStringLiteral("демонстрацию экрана");
    case QWebEnginePage::DesktopAudioVideoCapture:
        return QStringLiteral("демонстрацию экрана со звуком");
    default:
        return QStringLiteral("дополнительное разрешение");
    }
}

bool geometryIsVisibleOnScreen(const QRect &geometry)
{
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen && screen->availableGeometry().intersects(geometry)) {
            return true;
        }
    }
    return false;
}

QString centerPositionFor(const QSize &size)
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return QString();
    }

    const QRect available = screen->availableGeometry();
    const QPoint topLeft = available.center() - QPoint(size.width() / 2, size.height() / 2);
    return QStringLiteral("%1,%2").arg(topLeft.x()).arg(topLeft.y());
}

QString shellEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', QStringLiteral("\\\""));
    escaped.replace('$', QStringLiteral("\\$"));
    escaped.replace('`', QStringLiteral("\\`"));
    return QStringLiteral("\"") + escaped + QStringLiteral("\"");
}

QString normalizedHost(const QUrl &url)
{
    return url.host().toLower();
}
} // namespace

MessengerWindow::MessengerWindow()
{
    setWindowTitle(QString::fromLatin1(kAppName));
    resize(1100, 760);

    m_windowIcon = createWindowIcon();
    m_normalTrayIcon = createTrayIcon(false);
    m_unreadTrayIcon = createTrayIcon(true);
    setWindowIcon(m_windowIcon);

    const QString profileRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/webengine");
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/webengine");
    QDir().mkpath(profileRoot);
    QDir().mkpath(cacheRoot);

    m_profile = new QWebEngineProfile(QStringLiteral("YandexMessenger"), this);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    m_profile->setPersistentStoragePath(profileRoot);
    m_profile->setCachePath(cacheRoot);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    m_profile->setPersistentPermissionsPolicy(
        QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
#endif

    m_view = new QWebEngineView(this);
    m_page = new QWebEnginePage(m_profile, m_view);
    m_view->setPage(m_page);
    setCentralWidget(m_view);

    configureWebPage(m_page);

    createActions();
    createTray();
    connectActions();
    loadSettings();
    restoreWindowState();

    connect(m_view, &QWebEngineView::loadFinished,
            this, &MessengerWindow::handlePageLoaded);

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(m_page, &QWebEnginePage::permissionRequested,
            this, [this](QWebEnginePermission permission) {
                using Type = QWebEnginePermission::PermissionType;
                const auto type = permission.permissionType();
                if (type == Type::Notifications) {
                    if (isTrustedYandexOrigin(permission.origin())) {
                        permission.grant();
                    } else {
                        permission.deny();
                    }
                    return;
                }

                const bool sensitive = type == Type::MediaAudioCapture
                    || type == Type::MediaVideoCapture
                    || type == Type::MediaAudioVideoCapture
                    || type == Type::DesktopVideoCapture
                    || type == Type::DesktopAudioVideoCapture;

                if (!sensitive) {
                    permission.deny();
                    return;
                }

                const QString question = QStringLiteral(
                    "Сайт %1 запрашивает %2.\n\nРазрешить?")
                    .arg(permission.origin().host(),
                         permissionDescription(static_cast<int>(type)));

                const auto result = QMessageBox::question(
                    this,
                    QStringLiteral("Разрешение сайта"),
                    question,
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);

                if (result == QMessageBox::Yes) {
                    permission.grant();
                } else {
                    permission.deny();
                }
            });
#else
    connect(m_page, &QWebEnginePage::featurePermissionRequested,
            this, [this](const QUrl &origin, QWebEnginePage::Feature feature) {
                if (feature == QWebEnginePage::Notifications) {
                    m_page->setFeaturePermission(
                        origin,
                        feature,
                        isTrustedYandexOrigin(origin)
                            ? QWebEnginePage::PermissionGrantedByUser
                            : QWebEnginePage::PermissionDeniedByUser);
                    return;
                }

                const bool sensitive = feature == QWebEnginePage::MediaAudioCapture
                    || feature == QWebEnginePage::MediaVideoCapture
                    || feature == QWebEnginePage::MediaAudioVideoCapture
                    || feature == QWebEnginePage::DesktopVideoCapture
                    || feature == QWebEnginePage::DesktopAudioVideoCapture;

                if (!sensitive) {
                    m_page->setFeaturePermission(
                        origin,
                        feature,
                        QWebEnginePage::PermissionDeniedByUser);
                    return;
                }

                const QString question = QStringLiteral(
                    "Сайт %1 запрашивает %2.\n\nРазрешить?")
                    .arg(origin.host(), permissionDescription(feature));

                const auto result = QMessageBox::question(
                    this,
                    QStringLiteral("Разрешение сайта"),
                    question,
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);

                m_page->setFeaturePermission(
                    origin,
                    feature,
                    result == QMessageBox::Yes
                        ? QWebEnginePage::PermissionGrantedByUser
                        : QWebEnginePage::PermissionDeniedByUser);
            });
#endif

    m_profile->setNotificationPresenter(
        [this](std::unique_ptr<QWebEngineNotification> notification) {
            presentNotification(std::move(notification));
        });

    connect(qApp, &QCoreApplication::aboutToQuit, this, &MessengerWindow::performProfileCleanupIfNeeded);

    m_view->setUrl(QUrl(QString::fromLatin1(kMessengerUrl)));
}

bool MessengerWindow::startHidden() const
{
    return m_startHidden;
}

void MessengerWindow::showAndActivate()
{
    if (m_savedMaximized) {
        showMaximized();
    } else if (isMinimized()) {
        showNormal();
    } else {
        show();
    }
    raise();
    activateWindow();
    clearUnread();
}

void MessengerWindow::activateFromSecondInstance()
{
    showAndActivate();
}

void MessengerWindow::closeEvent(QCloseEvent *event)
{
    saveWindowState();

    if (m_pendingProfileReset) {
        event->accept();
        return;
    }

    if (!m_tray || !m_tray->isVisible() || !QSystemTrayIcon::isSystemTrayAvailable()) {
        event->accept();
        return;
    }

    hide();
    event->ignore();

    if (!m_closeHintShown) {
        m_closeHintShown = true;
        showMessage(
            QString::fromLatin1(kAppName),
            QStringLiteral("Приложение продолжает работать в системном трее."));
    }
}

void MessengerWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        clearUnread();
    }
}

void MessengerWindow::createActions()
{
    m_openAction = new QAction(QStringLiteral("Открыть"), this);
    m_reloadAction = new QAction(QStringLiteral("Перезагрузить страницу"), this);
    m_autostartAction = new QAction(QStringLiteral("Запускать при входе в систему"), this);
    m_autostartAction->setCheckable(true);
    m_startHiddenAction = new QAction(QStringLiteral("Запускать скрытым в трее"), this);
    m_startHiddenAction->setCheckable(true);
    m_visualPatchesAction = new QAction(QStringLiteral("Включить визуальные исправления страницы"), this);
    m_visualPatchesAction->setCheckable(true);
    m_hideSidebarAction = new QAction(QStringLiteral("Скрыть левую панель сервисов"), this);
    m_hideSidebarAction->setCheckable(true);
    m_compactLayoutAction = new QAction(QStringLiteral("Убирать внешние отступы страницы"), this);
    m_compactLayoutAction->setCheckable(true);
    m_clearCookiesAction = new QAction(QStringLiteral("Очистить cookies"), this);
    m_resetProfileAction = new QAction(QStringLiteral("Сбросить профиль приложения"), this);
    m_copyDiagnosticsAction = new QAction(QStringLiteral("Скопировать диагностическую информацию"), this);
    m_aboutAction = new QAction(QStringLiteral("О программе"), this);
    m_quitAction = new QAction(QStringLiteral("Выход"), this);
}

void MessengerWindow::createTray()
{
    m_tray = new QSystemTrayIcon(m_normalTrayIcon, this);
    m_tray->setToolTip(QString::fromLatin1(kAppName));

    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction(m_openAction);
    m_trayMenu->addAction(m_reloadAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_startHiddenAction);
    m_trayMenu->addAction(m_autostartAction);
    m_trayMenu->addAction(m_visualPatchesAction);
    m_trayMenu->addAction(m_hideSidebarAction);
    m_trayMenu->addAction(m_compactLayoutAction);
    m_trayMenu->addSeparator();

    QMenu *dataMenu = m_trayMenu->addMenu(QStringLiteral("Данные приложения"));
    dataMenu->addAction(m_clearCookiesAction);
    dataMenu->addAction(m_resetProfileAction);

    m_trayMenu->addAction(m_copyDiagnosticsAction);
    m_trayMenu->addAction(m_aboutAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_quitAction);
    m_tray->setContextMenu(m_trayMenu);

    connect(m_tray, &QSystemTrayIcon::activated,
            this, [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                    if (isVisible() && isActiveWindow()) {
                        hide();
                    } else {
                        showAndActivate();
                    }
                }
            });

    connect(m_tray, &QSystemTrayIcon::messageClicked,
            this, [this] {
                showAndActivate();
                openLatestNotification();
            });

    m_tray->show();
}

void MessengerWindow::connectActions()
{
    connect(m_openAction, &QAction::triggered, this, &MessengerWindow::showAndActivate);
    connect(m_reloadAction, &QAction::triggered, this, &MessengerWindow::reloadPage);
    connect(m_startHiddenAction, &QAction::toggled,
            this, [this](bool enabled) {
                m_startHidden = enabled;
                m_settings.setValue(QStringLiteral("startHidden"), enabled);
            });
    connect(m_autostartAction, &QAction::toggled,
            this, &MessengerWindow::setAutostartEnabled);
    connect(m_visualPatchesAction, &QAction::toggled,
            this, &MessengerWindow::setVisualPatchesEnabled);
    connect(m_hideSidebarAction, &QAction::toggled,
            this, &MessengerWindow::setSidebarEnabled);
    connect(m_compactLayoutAction, &QAction::toggled,
            this, &MessengerWindow::setCompactLayoutEnabled);
    connect(m_clearCookiesAction, &QAction::triggered,
            this, &MessengerWindow::clearCookies);
    connect(m_resetProfileAction, &QAction::triggered,
            this, &MessengerWindow::resetProfile);
    connect(m_copyDiagnosticsAction, &QAction::triggered,
            this, &MessengerWindow::copyDiagnosticsToClipboard);
    connect(m_aboutAction, &QAction::triggered,
            this, &MessengerWindow::showAboutDialog);
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);
}

void MessengerWindow::loadSettings()
{
    m_startHidden = m_settings.value(QStringLiteral("startHidden"), false).toBool();
    m_visualPatchesEnabled = m_settings.value(QStringLiteral("visualPatches"), true).toBool();
    m_sidebarHidden = m_settings.value(QStringLiteral("hideServiceSidebar"), true).toBool();
    m_compactLayout = m_settings.value(QStringLiteral("compactLayout"), true).toBool();

    const QSignalBlocker blockStartHidden(m_startHiddenAction);
    const QSignalBlocker blockVisualPatches(m_visualPatchesAction);
    const QSignalBlocker blockSidebar(m_hideSidebarAction);
    const QSignalBlocker blockCompact(m_compactLayoutAction);
    const QSignalBlocker blockAutostart(m_autostartAction);

    m_startHiddenAction->setChecked(m_startHidden);
    m_visualPatchesAction->setChecked(m_visualPatchesEnabled);
    m_hideSidebarAction->setChecked(m_sidebarHidden);
    m_compactLayoutAction->setChecked(m_compactLayout);
    m_autostartAction->setChecked(AutostartManager::isEnabled());

    updatePatchActionStates();
}

void MessengerWindow::restoreWindowState()
{
    const QSize defaultSize(1100, 760);
    const QPoint defaultPosition = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry().center()
            - QPoint(defaultSize.width() / 2, defaultSize.height() / 2)
        : QPoint(50, 50);

    m_savedSize = m_settings.value(QStringLiteral("windowSize"), defaultSize).toSize();
    m_savedPosition = m_settings.value(QStringLiteral("windowPosition"), defaultPosition).toPoint();
    m_savedMaximized = m_settings.value(QStringLiteral("windowMaximized"), false).toBool();

    const QRect geometry(m_savedPosition, m_savedSize);
    const bool visible = geometryIsVisibleOnScreen(geometry);

    if (visible) {
        resize(m_savedSize);
        move(m_savedPosition);
    } else {
        resize(defaultSize);
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            const QRect available = screen->availableGeometry();
            const QPoint fallback = available.center()
                - QPoint(defaultSize.width() / 2, defaultSize.height() / 2);
            move(fallback);
        }
    }

}

void MessengerWindow::saveWindowState()
{
    const QRect normalGeometry = isMaximized() ? this->normalGeometry() : geometry();
    m_settings.setValue(QStringLiteral("windowSize"), normalGeometry.size());
    m_settings.setValue(QStringLiteral("windowPosition"), normalGeometry.topLeft());
    m_settings.setValue(QStringLiteral("windowMaximized"), isMaximized());
    m_settings.setValue(QStringLiteral("startHidden"), m_startHidden);
    m_settings.setValue(QStringLiteral("visualPatches"), m_visualPatchesEnabled);
    m_settings.setValue(QStringLiteral("hideServiceSidebar"), m_sidebarHidden);
    m_settings.setValue(QStringLiteral("compactLayout"), m_compactLayout);
}

void MessengerWindow::updatePatchActionStates()
{
    m_hideSidebarAction->setEnabled(m_visualPatchesEnabled);
    m_compactLayoutAction->setEnabled(m_visualPatchesEnabled);
}

void MessengerWindow::applyVisualPatches()
{
    if (!m_visualPatchesEnabled) {
        return;
    }

    m_page->runJavaScript(sidebarPatchScript(m_sidebarHidden));
    m_page->runJavaScript(compactLayoutPatchScript(m_compactLayout));
}

void MessengerWindow::restoreVisualPatches()
{
    m_page->runJavaScript(sidebarPatchScript(false));
    m_page->runJavaScript(compactLayoutPatchScript(false));
}

void MessengerWindow::reloadPage()
{
    m_view->reload();
}

void MessengerWindow::configureWebPage(QWebEnginePage *page)
{
    if (!page) {
        return;
    }

    connect(page, &QWebEnginePage::urlChanged, this, [this, page](const QUrl &url) {
        qInfo() << "WebEngine URL changed to" << url;

        if (page == m_page) {
            if (m_visualPatchesEnabled && isMessengerPage(url)) {
                applyVisualPatches();
            }
            return;
        }

        if (shouldHandlePopupUrl(url)) {
            closePopupIfNeeded(url);
        }
    });

    connect(page, &QWebEnginePage::newWindowRequested,
            this, [this, page](QWebEngineNewWindowRequest &request) {
                qInfo() << "newWindowRequested" << request.requestedUrl()
                         << "userInitiated=" << request.isUserInitiated();

                if (!request.isUserInitiated() && !isTrustedYandexOrigin(request.requestedUrl())) {
                    qWarning() << "Blocked non-user-initiated popup:" << request.requestedUrl();
                    return;
                }

                if (!isTrustedYandexOrigin(request.requestedUrl()) && !request.requestedUrl().isEmpty()) {
                    const auto result = QMessageBox::question(
                        this,
                        QStringLiteral("Открыть ссылку"),
                        QStringLiteral("Открыть внешнюю ссылку в браузере?\n%1")
                            .arg(request.requestedUrl().toString()),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No);
                    if (result == QMessageBox::Yes) {
                        QDesktopServices::openUrl(request.requestedUrl());
                    }
                    return;
                }

                openPopupForRequest(request.requestedUrl(), request.isUserInitiated());
                if (m_popupWindow && m_popupWindow->page()) {
                    request.openIn(m_popupWindow->page());
                } else {
                    qWarning() << "Unable to create popup window for" << request.requestedUrl();
                }
            });

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(page, &QWebEnginePage::permissionRequested,
            this, [this, page](QWebEnginePermission permission) {
                Q_UNUSED(page);
                using Type = QWebEnginePermission::PermissionType;
                const auto type = permission.permissionType();
                const QUrl origin = permission.origin();
                if (isMessengerPage(origin) && !isTrustedYandexOrigin(origin)) {
                    permission.deny();
                    return;
                }

                if (type == Type::Notifications) {
                    if (isTrustedYandexOrigin(origin)) {
                        permission.grant();
                    } else {
                        permission.deny();
                    }
                    return;
                }

                const bool sensitive = type == Type::MediaAudioCapture
                    || type == Type::MediaVideoCapture
                    || type == Type::MediaAudioVideoCapture
                    || type == Type::DesktopVideoCapture
                    || type == Type::DesktopAudioVideoCapture;

                if (!sensitive) {
                    permission.deny();
                    return;
                }

                const QString question = QStringLiteral(
                    "Сайт %1 запрашивает %2.\n\nРазрешить?")
                    .arg(permission.origin().host(), permissionDescription(static_cast<int>(type)));

                const auto result = QMessageBox::question(
                    this,
                    QStringLiteral("Разрешение сайта"),
                    question,
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);

                if (result == QMessageBox::Yes) {
                    permission.grant();
                } else {
                    permission.deny();
                }
            });
#else
    connect(page, &QWebEnginePage::featurePermissionRequested,
            this, [this, page](const QUrl &origin, QWebEnginePage::Feature feature) {
                if (page != m_page && !isTrustedYandexOrigin(origin)) {
                    page->setFeaturePermission(origin, feature, QWebEnginePage::PermissionDeniedByUser);
                    return;
                }

                if (feature == QWebEnginePage::Notifications) {
                    page->setFeaturePermission(
                        origin,
                        feature,
                        isTrustedYandexOrigin(origin)
                            ? QWebEnginePage::PermissionGrantedByUser
                            : QWebEnginePage::PermissionDeniedByUser);
                    return;
                }

                const bool sensitive = feature == QWebEnginePage::MediaAudioCapture
                    || feature == QWebEnginePage::MediaVideoCapture
                    || feature == QWebEnginePage::MediaAudioVideoCapture
                    || feature == QWebEnginePage::DesktopVideoCapture
                    || feature == QWebEnginePage::DesktopAudioVideoCapture;

                if (!sensitive) {
                    page->setFeaturePermission(
                        origin,
                        feature,
                        QWebEnginePage::PermissionDeniedByUser);
                    return;
                }

                const QString question = QStringLiteral(
                    "Сайт %1 запрашивает %2.\n\nРазрешить?")
                    .arg(origin.host(), permissionDescription(feature));

                const auto result = QMessageBox::question(
                    this,
                    QStringLiteral("Разрешение сайта"),
                    question,
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);

                page->setFeaturePermission(
                    origin,
                    feature,
                    result == QMessageBox::Yes
                        ? QWebEnginePage::PermissionGrantedByUser
                        : QWebEnginePage::PermissionDeniedByUser);
            });
#endif
}

void MessengerWindow::handleNewWindowRequest(QWebEnginePage *sourcePage, QWebEnginePage::WebWindowType type,
                                            const QUrl &requestedUrl, bool userInitiated)
{
    Q_UNUSED(sourcePage);
    Q_UNUSED(type);
    Q_UNUSED(requestedUrl);
    Q_UNUSED(userInitiated);
}

void MessengerWindow::openPopupForRequest(const QUrl &requestedUrl, bool userInitiated)
{
    Q_UNUSED(userInitiated);

    if (!m_popupWindow) {
        m_popupWindow = new WebPopupWindow(m_profile, this);
        configureWebPage(m_popupWindow->page());
        connect(m_popupWindow, &QDialog::finished, this, [this](int) {
            if (m_view) {
                m_view->reload();
            }
            if (m_view) {
                showAndActivate();
            }
        });
    }

    if (m_popupWindow && m_popupWindow->page()) {
        m_popupWindow->page()->setUrl(requestedUrl.isEmpty()
                                         ? QUrl(QString::fromLatin1("https://passport.yandex.ru"))
                                         : requestedUrl);
        m_popupWindow->activatePopup();
    }
}

void MessengerWindow::closePopupIfNeeded(const QUrl &url)
{
    if (!m_popupWindow) {
        return;
    }

    if (isMessengerPage(url)) {
        m_popupWindow->close();
        m_view->reload();
        showAndActivate();
    }
}

void MessengerWindow::setVisualPatchesEnabled(bool enabled)
{
    if (m_visualPatchesEnabled == enabled) {
        return;
    }

    m_visualPatchesEnabled = enabled;
    m_settings.setValue(QStringLiteral("visualPatches"), enabled);
    updatePatchActionStates();

    if (enabled) {
        applyVisualPatches();
    } else {
        restoreVisualPatches();
        reloadPage();
    }
}

void MessengerWindow::setSidebarEnabled(bool enabled)
{
    if (m_sidebarHidden == enabled) {
        return;
    }

    m_sidebarHidden = enabled;
    m_settings.setValue(QStringLiteral("hideServiceSidebar"), enabled);

    if (m_visualPatchesEnabled) {
        m_page->runJavaScript(sidebarPatchScript(enabled));
    }
}

void MessengerWindow::setCompactLayoutEnabled(bool enabled)
{
    if (m_compactLayout == enabled) {
        return;
    }

    m_compactLayout = enabled;
    m_settings.setValue(QStringLiteral("compactLayout"), enabled);

    if (m_visualPatchesEnabled) {
        m_page->runJavaScript(compactLayoutPatchScript(enabled));
    }
}

void MessengerWindow::setAutostartEnabled(bool enabled)
{
    QString errorMessage;
    if (!AutostartManager::setEnabled(enabled, &errorMessage)) {
        QSignalBlocker blocker(m_autostartAction);
        m_autostartAction->setChecked(!enabled);
        QMessageBox::critical(this, QString::fromLatin1(kAppName), errorMessage);
        return;
    }

    m_settings.setValue(QStringLiteral("autostartEnabled"), enabled);
}

void MessengerWindow::clearCookies()
{
    const auto result = QMessageBox::question(
        this,
        QStringLiteral("Очистить cookies"),
        QStringLiteral("Удалить cookies и HTTP-кэш приложения? После этого может потребоваться повторный вход."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    m_profile->cookieStore()->deleteAllCookies();
    m_profile->clearHttpCache();
    reloadPage();

    showMessage(
        QString::fromLatin1(kAppName),
        QStringLiteral("Cookies и кэш очищены. Может потребоваться повторный вход."));
}

void MessengerWindow::resetProfile()
{
    const auto result = QMessageBox::question(
        this,
        QStringLiteral("Сбросить профиль приложения"),
        QStringLiteral("Приложение будет закрыто, а профиль и кэш текущего приложения будут удалены. Продолжить?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    m_pendingProfileReset = true;
    showMessage(
        QString::fromLatin1(kAppName),
        QStringLiteral("Приложение будет закрыто для сброса профиля."));
    qApp->quit();
}

void MessengerWindow::copyDiagnosticsToClipboard()
{
    const QString text = Diagnostics::text()
        + QStringLiteral("\nVisual patches: %1")
            .arg(m_visualPatchesEnabled ? QStringLiteral("enabled") : QStringLiteral("disabled"))
        + QStringLiteral("\nHide sidebar: %1")
            .arg(m_sidebarHidden ? QStringLiteral("enabled") : QStringLiteral("disabled"))
        + QStringLiteral("\nCompact layout: %1")
            .arg(m_compactLayout ? QStringLiteral("enabled") : QStringLiteral("disabled"))
        + QStringLiteral("\nAutostart: %1")
            .arg(m_autostartAction->isChecked() ? QStringLiteral("enabled") : QStringLiteral("disabled"))
        + QStringLiteral("\nStart hidden: %1")
            .arg(m_startHidden ? QStringLiteral("enabled") : QStringLiteral("disabled"));

    QGuiApplication::clipboard()->setText(text);
    showMessage(
        QString::fromLatin1(kAppName),
        QStringLiteral("Диагностическая информация скопирована в буфер обмена."));
}

void MessengerWindow::showAboutDialog()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("О программе"));
    box.setIcon(QMessageBox::Information);
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral(
        "<b>Yandex Messenger Qt</b><br>"
        "Версия приложения: %1<br>"
        "Версия Qt: %2<br>"
        "Неофициальная оболочка для веб-версии Яндекс Мессенджера<br>"
        "Лицензия: MIT")
        .arg(QString::fromLatin1(APP_VERSION), QString::fromLatin1(qVersion())));

    QPushButton *repoButton = box.addButton(QStringLiteral("Открыть репозиторий"), QMessageBox::ActionRole);
    QPushButton *issuesButton = box.addButton(QStringLiteral("Сообщить об ошибке"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Close);
    box.exec();

    if (box.clickedButton() == repoButton) {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kRepoUrl)));
    } else if (box.clickedButton() == issuesButton) {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kIssuesUrl)));
    }
}

void MessengerWindow::presentNotification(std::unique_ptr<QWebEngineNotification> notification)
{
    if (!notification) {
        return;
    }

    auto *raw = notification.get();
    connect(raw, &QWebEngineNotification::closed,
            this, [this, raw] {
                removeNotification(raw);
            });

    const QString title = notification->title().isEmpty()
        ? QString::fromLatin1(kAppName)
        : notification->title();
    const QString message = notification->message();

    notification->show();
    m_notifications.push_back(std::move(notification));

    markUnread();
    m_tray->showMessage(title, message, QSystemTrayIcon::Information, 10000);
}

void MessengerWindow::removeNotification(QWebEngineNotification *notification)
{
    const auto it = std::find_if(
        m_notifications.begin(),
        m_notifications.end(),
        [notification](const auto &item) { return item.get() == notification; });

    if (it != m_notifications.end()) {
        m_notifications.erase(it);
    }
}

void MessengerWindow::openLatestNotification()
{
    if (m_notifications.empty()) {
        return;
    }

    std::unique_ptr<QWebEngineNotification> notification = std::move(m_notifications.back());
    m_notifications.pop_back();

    disconnect(notification.get(), nullptr, this, nullptr);
    notification->click();
    notification->close();
}

void MessengerWindow::markUnread()
{
    if (isActiveWindow()) {
        return;
    }

    m_hasUnread = true;
    m_tray->setIcon(m_unreadTrayIcon);
    m_tray->setToolTip(QStringLiteral("Yandex Messenger — есть новые сообщения"));
}

void MessengerWindow::clearUnread()
{
    if (!m_hasUnread) {
        return;
    }

    m_hasUnread = false;
    m_tray->setIcon(m_normalTrayIcon);
    m_tray->setToolTip(QString::fromLatin1(kAppName));
}

#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
void MessengerWindow::handleLegacyPermissionRequest(const QUrl &origin, QWebEnginePage::Feature feature)
{
    if (feature == QWebEnginePage::Notifications) {
        m_page->setFeaturePermission(
            origin,
            feature,
            isTrustedYandexOrigin(origin)
                ? QWebEnginePage::PermissionGrantedByUser
                : QWebEnginePage::PermissionDeniedByUser);
        return;
    }

    const bool sensitive = feature == QWebEnginePage::MediaAudioCapture
        || feature == QWebEnginePage::MediaVideoCapture
        || feature == QWebEnginePage::MediaAudioVideoCapture
        || feature == QWebEnginePage::DesktopVideoCapture
        || feature == QWebEnginePage::DesktopAudioVideoCapture;

    if (!sensitive) {
        m_page->setFeaturePermission(
            origin,
            feature,
            QWebEnginePage::PermissionDeniedByUser);
        return;
    }

    const QString question = QStringLiteral(
        "Сайт %1 запрашивает %2.\n\nРазрешить?")
        .arg(origin.host(), permissionDescription(feature));

    const auto result = QMessageBox::question(
        this,
        QStringLiteral("Разрешение сайта"),
        question,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    m_page->setFeaturePermission(
        origin,
        feature,
        result == QMessageBox::Yes
            ? QWebEnginePage::PermissionGrantedByUser
            : QWebEnginePage::PermissionDeniedByUser);
}
#endif

void MessengerWindow::handlePageLoaded(bool ok)
{
    if (!ok) {
        return;
    }

    if (m_visualPatchesEnabled) {
        applyVisualPatches();
    }
}

void MessengerWindow::performProfileCleanupIfNeeded()
{
    if (!m_pendingProfileReset) {
        return;
    }

    const QString profile = profilePath();
    const QString cache = cachePath();
    if (profile.isEmpty() && cache.isEmpty()) {
        return;
    }

    const QString command = QStringLiteral("rm -rf -- %1 %2")
        .arg(shellEscape(profile), shellEscape(cache));

    QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), command});
}

void MessengerWindow::showMessage(const QString &title, const QString &message, QSystemTrayIcon::MessageIcon icon)
{
    if (m_tray && m_tray->isVisible()) {
        m_tray->showMessage(title, message, icon, 5000);
    } else {
        QMessageBox::information(this, title, message);
    }
}

bool MessengerWindow::isTrustedYandexOrigin(const QUrl &origin) const
{
    return isTrustedYandexHost(origin.host()) || normalizedHost(origin) == QStringLiteral("passport.yandex.ru");
}

bool MessengerWindow::isTrustedYandexHost(const QString &host) const
{
    const QString normalized = host.toLower();
    return normalized == QStringLiteral("yandex.ru") || normalized.endsWith(QStringLiteral(".yandex.ru"));
}

bool MessengerWindow::isMessengerPage(const QUrl &url) const
{
    const QString host = normalizedHost(url);
    return isTrustedYandexOrigin(url) && url.path().startsWith(QStringLiteral("/chat"));
}

bool MessengerWindow::shouldHandlePopupUrl(const QUrl &url) const
{
    if (!url.isValid() || url.isEmpty()) {
        return false;
    }
    return isTrustedYandexOrigin(url) || isMessengerPage(url);
}

QString MessengerWindow::profilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/webengine");
}

QString MessengerWindow::cachePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/webengine");
}

QString MessengerWindow::shellQuote(const QString &value)
{
    QString quoted = value;
    quoted.replace('\'', QStringLiteral("'\\''"));
    return QStringLiteral("'") + quoted + QStringLiteral("'");
}
