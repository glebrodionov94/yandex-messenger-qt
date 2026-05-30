#pragma once

#include <QObject>
#include <QString>

class SingleInstance final : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);

    bool isPrimaryInstance() const;
    bool initialize(const QString &key);
    bool sendActivateRequest(int timeoutMs = 1000) const;

signals:
    void activateRequested();

private:
    QString m_key;
    bool m_primaryInstance = false;
};
