#include "single_instance.h"

#include <QLocalServer>
#include <QLocalSocket>

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

bool SingleInstance::isPrimaryInstance() const
{
    return m_primaryInstance;
}

bool SingleInstance::initialize(const QString &key)
{
    m_key = key;

    if (m_key.isEmpty()) {
        return false;
    }

    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (socket.waitForConnected(200)) {
        return false;
    }

    QLocalServer::removeServer(m_key);

    auto *server = new QLocalServer(this);
    if (!server->listen(m_key)) {
        QLocalServer::removeServer(m_key);
        if (!server->listen(m_key)) {
            delete server;
            return false;
        }
    }

    connect(server, &QLocalServer::newConnection, this, [server]() {
        while (auto *client = server->nextPendingConnection()) {
            client->readAll();
            client->disconnectFromServer();
            client->deleteLater();
        }
    });

    connect(server, &QLocalServer::newConnection, this, [this]() {
        emit activateRequested();
    });

    m_primaryInstance = true;
    return true;
}

bool SingleInstance::sendActivateRequest(int timeoutMs) const
{
    if (m_key.isEmpty()) {
        return false;
    }

    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (!socket.waitForConnected(timeoutMs)) {
        return false;
    }

    socket.write("activate");
    socket.flush();
    socket.waitForBytesWritten(timeoutMs);
    socket.disconnectFromServer();
    if (socket.state() == QLocalSocket::ConnectedState) {
        socket.waitForDisconnected(timeoutMs);
    }
    return true;
}
