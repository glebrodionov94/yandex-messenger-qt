#include "web_popup_window.h"

#include <QCloseEvent>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineView>

WebPopupWindow::WebPopupWindow(QWebEngineProfile *profile, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Yandex Messenger Qt"));
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(700, 900);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new QWebEngineView(this);
    layout->addWidget(m_view);

    if (profile) {
        m_page = new QWebEnginePage(profile, m_view);
        m_view->setPage(m_page);
    }
}

void WebPopupWindow::setPage(QWebEnginePage *page)
{
    if (!page) {
        return;
    }

    m_page = page;
    m_view->setPage(m_page);
}

QWebEnginePage *WebPopupWindow::page() const
{
    return m_page;
}

QWebEngineView *WebPopupWindow::view() const
{
    return m_view;
}

void WebPopupWindow::load(const QUrl &url)
{
    if (m_view) {
        m_view->setUrl(url);
    }
}

void WebPopupWindow::activatePopup()
{
    show();
    raise();
    activateWindow();
}

void WebPopupWindow::setCloseOnMessengerReturn(bool enabled)
{
    m_closeOnMessengerReturn = enabled;
}

void WebPopupWindow::closeEvent(QCloseEvent *event)
{
    if (m_closeOnMessengerReturn) {
        emit rejected();
    }
    QDialog::closeEvent(event);
}
