#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QFont>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QWebEngineNotification>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QWebEnginePermission>
#endif

#include <algorithm>
#include <memory>
#include <vector>

namespace {
constexpr auto kMessengerUrl = "https://yandex.ru/chat";

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
    // The tray icon is intentionally monochrome and compact so that it visually
    // matches KDE Plasma panel icons. The application/window icon remains branded.
    constexpr int size = 64;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor foreground = QApplication::palette().color(QPalette::WindowText);
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
} // namespace

class MessengerWindow final : public QMainWindow
{
public:
    MessengerWindow()
    {
        setWindowTitle(QStringLiteral("Yandex Messenger"));
        resize(1100, 760);

        m_windowIcon = createWindowIcon();
        m_normalIcon = createTrayIcon(false);
        m_unreadIcon = createTrayIcon(true);
        setWindowIcon(m_windowIcon);

        m_profile = new QWebEngineProfile(QStringLiteral("YandexMessenger"), this);
        m_profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        m_profile->setPersistentPermissionsPolicy(
            QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
#endif

        m_view = new QWebEngineView(this);
        m_page = new QWebEnginePage(m_profile, m_view);
        m_view->setPage(m_page);
        setCentralWidget(m_view);

        createTray();

        m_hideServiceSidebar = m_settings.value(
            QStringLiteral("hideServiceSidebar"), true).toBool();
        m_hideSidebarAction->setChecked(m_hideServiceSidebar);

        m_compactLayout = m_settings.value(
            QStringLiteral("compactLayout"), true).toBool();
        m_compactLayoutAction->setChecked(m_compactLayout);

        connect(m_view, &QWebEngineView::loadFinished,
                this, [this](bool ok) {
                    if (ok) {
                        applyCompactLayoutMode();
                        applyServiceSidebarMode();
                    }
                });

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        connect(m_page, &QWebEnginePage::permissionRequested,
                this, [this](QWebEnginePermission permission) {
                    handlePermission(permission);
                });
#else
        connect(m_page, &QWebEnginePage::featurePermissionRequested,
                this, [this](const QUrl &origin, QWebEnginePage::Feature feature) {
                    handleLegacyPermission(origin, feature);
                });
#endif

        m_profile->setNotificationPresenter(
            [this](std::unique_ptr<QWebEngineNotification> notification) {
                presentNotification(std::move(notification));
            });

        m_view->setUrl(QUrl(QString::fromLatin1(kMessengerUrl)));
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        hide();
        event->ignore();

        if (!m_closeHintShown) {
            m_closeHintShown = true;
            m_tray->showMessage(
                QStringLiteral("Yandex Messenger"),
                QStringLiteral("Приложение продолжает работать в системном трее."),
                QSystemTrayIcon::Information,
                5000);
        }
    }

    void changeEvent(QEvent *event) override
    {
        QMainWindow::changeEvent(event);
        if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
            clearUnread();
        }
    }

private:
    void createTray()
    {
        m_tray = new QSystemTrayIcon(m_normalIcon, this);
        m_tray->setToolTip(QStringLiteral("Yandex Messenger"));

        auto *menu = new QMenu(this);
        auto *openAction = menu->addAction(QStringLiteral("Открыть"));
        auto *reloadAction = menu->addAction(QStringLiteral("Перезагрузить страницу"));
        m_hideSidebarAction = menu->addAction(QStringLiteral("Скрыть левую панель сервисов"));
        m_hideSidebarAction->setCheckable(true);
        m_compactLayoutAction = menu->addAction(QStringLiteral("Убирать внешние отступы страницы"));
        m_compactLayoutAction->setCheckable(true);
        menu->addSeparator();
        auto *quitAction = menu->addAction(QStringLiteral("Выход"));

        connect(openAction, &QAction::triggered, this, [this] { showAndActivate(); });
        connect(reloadAction, &QAction::triggered, m_view, &QWebEngineView::reload);
        connect(m_hideSidebarAction, &QAction::toggled,
                this, [this](bool enabled) {
                    m_hideServiceSidebar = enabled;
                    m_settings.setValue(QStringLiteral("hideServiceSidebar"), enabled);
                    applyServiceSidebarMode();
                });
        connect(m_compactLayoutAction, &QAction::toggled,
                this, [this](bool enabled) {
                    m_compactLayout = enabled;
                    m_settings.setValue(QStringLiteral("compactLayout"), enabled);
                    applyCompactLayoutMode();
                });
        connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

        connect(m_tray, &QSystemTrayIcon::activated,
                this, [this](QSystemTrayIcon::ActivationReason reason) {
                    if (reason != QSystemTrayIcon::Trigger &&
                        reason != QSystemTrayIcon::DoubleClick) {
                        return;
                    }

                    if (isVisible() && isActiveWindow()) {
                        hide();
                    } else {
                        showAndActivate();
                    }
                });

        connect(m_tray, &QSystemTrayIcon::messageClicked,
                this, [this] {
                    showAndActivate();
                    openLatestNotification();
                });

        m_tray->setContextMenu(menu);
        m_tray->show();
    }

    void applyCompactLayoutMode()
    {
        const QString enabled = m_compactLayout
            ? QStringLiteral("true")
            : QStringLiteral("false");

        const QString script = QStringLiteral(R"JS(
            (() => {
                window.__ymqtCompactLayoutEnabled = %1;
                const marker = "data-ymqt-original-inline-style";
                const styleId = "ymqt-compact-layout-style";

                const restore = () => {
                    document.querySelectorAll("[" + marker + "]").forEach((element) => {
                        const oldStyle = element.getAttribute(marker);
                        if (oldStyle === "__empty__") {
                            element.removeAttribute("style");
                        } else {
                            element.setAttribute("style", oldStyle);
                        }
                        element.removeAttribute(marker);
                    });
                };

                const saveInlineStyle = (element) => {
                    if (!element.hasAttribute(marker)) {
                        element.setAttribute(
                            marker,
                            element.getAttribute("style") || "__empty__");
                    }
                };

                const apply = () => {
                    restore();

                    if (!window.__ymqtCompactLayoutEnabled) {
                        document.getElementById(styleId)?.remove();
                        return;
                    }

                    let style = document.getElementById(styleId);
                    if (!style) {
                        style = document.createElement("style");
                        style.id = styleId;
                        style.textContent = `
                            html, body {
                                margin: 0 !important;
                                padding: 0 !important;
                                width: 100% !important;
                                height: 100% !important;
                                overflow: hidden !important;
                            }
                        `;
                        document.head.appendChild(style);
                    }

                    const viewportWidth = window.innerWidth;
                    const viewportHeight = window.innerHeight;

                    const wrappers = [document.body, ...document.body.querySelectorAll("*")]
                        .map((element) => ({
                            element,
                            rect: element.getBoundingClientRect()
                        }))
                        .filter(({ element, rect }) => {
                            if (!element.isConnected) return false;
                            if (rect.width < viewportWidth * 0.88) return false;
                            if (rect.height < viewportHeight * 0.88) return false;

                            const rightGap = Math.abs(viewportWidth - rect.right);
                            const bottomGap = Math.abs(viewportHeight - rect.bottom);

                            return rect.left >= -2 && rect.left <= 24 &&
                                   rect.top >= -2 && rect.top <= 24 &&
                                   rightGap <= 24 &&
                                   bottomGap <= 24;
                        })
                        .slice(0, 5);

                    wrappers.forEach(({ element }, index) => {
                        saveInlineStyle(element);
                        element.style.setProperty("margin", "0", "important");
                        element.style.setProperty("padding", "0", "important");
                        element.style.setProperty("max-width", "none", "important");
                        element.style.setProperty("max-height", "none", "important");
                        element.style.setProperty("border-radius", "0", "important");

                        if (index <= 2) {
                            element.style.setProperty("width", "100vw", "important");
                            element.style.setProperty("height", "100vh", "important");
                        }
                    });
                };

                apply();

                if (!window.__ymqtCompactLayoutObserver) {
                    let timer = null;
                    window.__ymqtCompactLayoutObserver = new MutationObserver(() => {
                        clearTimeout(timer);
                        timer = setTimeout(apply, 150);
                    });

                    window.__ymqtCompactLayoutObserver.observe(document.documentElement, {
                        childList: true,
                        subtree: true
                    });
                }
            })();
        )JS").arg(enabled);

        m_page->runJavaScript(script);
    }

    void applyServiceSidebarMode()
    {
        const QString enabled = m_hideServiceSidebar
            ? QStringLiteral("true")
            : QStringLiteral("false");

        const QString script = QStringLiteral(R"JS(
            (() => {
                window.__ymqtSidebarEnabled = %1;
                const marker = "data-ymqt-hidden-service-sidebar";

                const restore = () => {
                    document.querySelectorAll("[" + marker + "]").forEach((element) => {
                        const oldDisplay = element.getAttribute(marker);
                        if (oldDisplay === "__empty__") {
                            element.style.removeProperty("display");
                        } else {
                            element.style.setProperty("display", oldDisplay);
                        }
                        element.removeAttribute(marker);
                    });
                };

                const hide = () => {
                    restore();
                    if (!window.__ymqtSidebarEnabled) {
                        return;
                    }

                    const candidates = [...document.querySelectorAll("body *")]
                        .map((element) => ({ element, rect: element.getBoundingClientRect() }))
                        .filter(({ element, rect }) => {
                            if (!element.isConnected) return false;
                            if (rect.left < -2 || rect.left > 2) return false;
                            if (rect.width < 48 || rect.width > 84) return false;
                            if (rect.height < window.innerHeight * 0.75) return false;
                            const style = getComputedStyle(element);
                            if (style.display === "none" || style.visibility === "hidden") return false;
                            return true;
                        })
                        .sort((a, b) => a.rect.width - b.rect.width);

                    const candidate = candidates[0]?.element;
                    if (!candidate) {
                        return;
                    }

                    const oldDisplay = candidate.style.getPropertyValue("display");
                    candidate.setAttribute(marker, oldDisplay || "__empty__");
                    candidate.style.setProperty("display", "none", "important");
                };

                hide();

                if (!window.__ymqtSidebarObserver) {
                    let timer = null;
                    window.__ymqtSidebarObserver = new MutationObserver(() => {
                        clearTimeout(timer);
                        timer = setTimeout(hide, 120);
                    });
                    window.__ymqtSidebarObserver.observe(document.documentElement, {
                        childList: true,
                        subtree: true
                    });
                }
            })();
        )JS").arg(enabled);

        m_page->runJavaScript(script);
    }

    void showAndActivate()
    {
        showNormal();
        raise();
        activateWindow();
        clearUnread();
    }

    void markUnread()
    {
        if (isActiveWindow()) {
            return;
        }

        m_hasUnread = true;
        m_tray->setIcon(m_unreadIcon);
        m_tray->setToolTip(QStringLiteral("Yandex Messenger — есть новые сообщения"));
    }

    void clearUnread()
    {
        if (!m_hasUnread) {
            return;
        }

        m_hasUnread = false;
        m_tray->setIcon(m_normalIcon);
        m_tray->setToolTip(QStringLiteral("Yandex Messenger"));
    }

    void presentNotification(std::unique_ptr<QWebEngineNotification> notification)
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
            ? QStringLiteral("Yandex Messenger")
            : notification->title();
        const QString message = notification->message();

        notification->show();
        m_notifications.push_back(std::move(notification));

        markUnread();
        m_tray->showMessage(title, message, QSystemTrayIcon::Information, 10000);
    }

    void removeNotification(QWebEngineNotification *notification)
    {
        const auto it = std::find_if(
            m_notifications.begin(), m_notifications.end(),
            [notification](const auto &item) { return item.get() == notification; });

        if (it != m_notifications.end()) {
            m_notifications.erase(it);
        }
    }

    void openLatestNotification()
    {
        if (m_notifications.empty()) {
            return;
        }

        std::unique_ptr<QWebEngineNotification> notification =
            std::move(m_notifications.back());
        m_notifications.pop_back();

        disconnect(notification.get(), nullptr, this, nullptr);
        notification->click();
        notification->close();
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    void handlePermission(QWebEnginePermission permission)
    {
        using Type = QWebEnginePermission::PermissionType;

        if (permission.permissionType() == Type::Notifications) {
            permission.grant();
            return;
        }

        const QString question = QStringLiteral(
            "Сайт %1 запрашивает %2.\n\nРазрешить?")
            .arg(permission.origin().host(),
                 permissionDescription(static_cast<int>(permission.permissionType())));

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
    }
#else
    void handleLegacyPermission(const QUrl &origin, QWebEnginePage::Feature feature)
    {
        if (feature == QWebEnginePage::Notifications) {
            m_page->setFeaturePermission(
                origin,
                feature,
                QWebEnginePage::PermissionGrantedByUser);
            return;
        }

        const QString question = QStringLiteral(
            "Сайт %1 запрашивает дополнительное разрешение.\n\nРазрешить?")
            .arg(origin.host());

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

    QWebEngineProfile *m_profile = nullptr;
    QWebEnginePage *m_page = nullptr;
    QWebEngineView *m_view = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QAction *m_hideSidebarAction = nullptr;
    QAction *m_compactLayoutAction = nullptr;

    QSettings m_settings;
    QIcon m_windowIcon;
    QIcon m_normalIcon;
    QIcon m_unreadIcon;
    bool m_hasUnread = false;
    bool m_closeHintShown = false;
    bool m_hideServiceSidebar = true;
    bool m_compactLayout = true;

    std::vector<std::unique_ptr<QWebEngineNotification>> m_notifications;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Yandex Messenger Qt"));
    QApplication::setOrganizationName(QStringLiteral("Local"));
    QApplication::setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::warning(
            nullptr,
            QStringLiteral("Yandex Messenger"),
            QStringLiteral(
                "Системный трей не найден. Приложение запустится, "
                "но сворачивание в трей может не работать."));
    }

    MessengerWindow window;
    window.show();

    return app.exec();
}
