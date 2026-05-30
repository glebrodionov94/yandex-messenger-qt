#pragma once

#include <QDialog>
#include <QPointer>
#include <QUrl>

class QCloseEvent;
class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineView;

class WebPopupWindow final : public QDialog
{
public:
    explicit WebPopupWindow(QWebEngineProfile *profile, QWidget *parent = nullptr);

    void setPage(QWebEnginePage *page);
    QWebEnginePage *page() const;
    QWebEngineView *view() const;
    void load(const QUrl &url);
    void activatePopup();
    void setCloseOnMessengerReturn(bool enabled);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWebEngineView *m_view = nullptr;
    QWebEnginePage *m_page = nullptr;
    bool m_closeOnMessengerReturn = true;
};
