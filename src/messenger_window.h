#pragma once

#include <QMainWindow>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QPointer>
#include <QWebEngineNotification>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QUrl>

#include <memory>
#include <vector>

class QAction;
class QCloseEvent;
class QEvent;
class QMenu;
class WebPopupWindow;

class MessengerWindow final : public QMainWindow
{
public:
    explicit MessengerWindow();

    bool startHidden() const;
    void showAndActivate();
    void activateFromSecondInstance();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void createTray();
    void createActions();
    void connectActions();
    void loadSettings();
    void restoreWindowState();
    void saveWindowState();
    void updatePatchActionStates();
    void applyVisualPatches();
    void restoreVisualPatches();
    void reloadPage();
    void configureWebPage(QWebEnginePage *page);
    void handleNewWindowRequest(QWebEnginePage *sourcePage, QWebEnginePage::WebWindowType type,
                                const QUrl &requestedUrl, bool userInitiated);
    void openPopupForRequest(const QUrl &requestedUrl, bool userInitiated);
    void closePopupIfNeeded(const QUrl &url);
    void setVisualPatchesEnabled(bool enabled);
    void setSidebarEnabled(bool enabled);
    void setCompactLayoutEnabled(bool enabled);
    void setAutostartEnabled(bool enabled);
    void clearCookies();
    void resetProfile();
    void copyDiagnosticsToClipboard();
    void showAboutDialog();
    void presentNotification(std::unique_ptr<QWebEngineNotification> notification);
    void removeNotification(QWebEngineNotification *notification);
    void openLatestNotification();
    void markUnread();
    void clearUnread();
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    void handleLegacyPermissionRequest(const QUrl &origin, QWebEnginePage::Feature feature);
#endif
    void handlePageLoaded(bool ok);
    void performProfileCleanupIfNeeded();
    void showMessage(const QString &title, const QString &message,
                     QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);
    bool isTrustedYandexOrigin(const QUrl &origin) const;
    bool isTrustedYandexHost(const QString &host) const;
    bool isMessengerPage(const QUrl &url) const;
    bool shouldHandlePopupUrl(const QUrl &url) const;
    QString profilePath() const;
    QString cachePath() const;
    static QString shellQuote(const QString &value);

    QWebEngineProfile *m_profile = nullptr;
    QWebEnginePage *m_page = nullptr;
    QWebEngineView *m_view = nullptr;
    QPointer<WebPopupWindow> m_popupWindow;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;

    QAction *m_openAction = nullptr;
    QAction *m_reloadAction = nullptr;
    QAction *m_autostartAction = nullptr;
    QAction *m_startHiddenAction = nullptr;
    QAction *m_visualPatchesAction = nullptr;
    QAction *m_hideSidebarAction = nullptr;
    QAction *m_compactLayoutAction = nullptr;
    QAction *m_clearCookiesAction = nullptr;
    QAction *m_resetProfileAction = nullptr;
    QAction *m_copyDiagnosticsAction = nullptr;
    QAction *m_aboutAction = nullptr;
    QAction *m_quitAction = nullptr;

    QSettings m_settings;

    QIcon m_windowIcon;
    QIcon m_normalTrayIcon;
    QIcon m_unreadTrayIcon;

    bool m_hasUnread = false;
    bool m_closeHintShown = false;
    bool m_startHidden = false;
    bool m_visualPatchesEnabled = true;
    bool m_sidebarHidden = true;
    bool m_compactLayout = true;
    bool m_pendingProfileReset = false;
    bool m_savedMaximized = false;

    QSize m_savedSize;
    QPoint m_savedPosition;

    std::vector<std::unique_ptr<QWebEngineNotification>> m_notifications;
};
