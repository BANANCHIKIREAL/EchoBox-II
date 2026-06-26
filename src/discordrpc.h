#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>
#include <QJsonObject>

class DiscordRPC : public QObject {
    Q_OBJECT
public:
    explicit DiscordRPC(const QString &clientId, QObject *parent = nullptr);

    void updateActivity(const QString &title, const QString &artist, bool playing = true);
    void clearActivity();
    bool isConnected() const { return m_ready; }

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void tryConnect();

private:
    void sendFrame(int opcode, const QByteArray &json);
    void sendActivity(const QJsonObject &activity);

    QLocalSocket *m_socket;
    QTimer       *m_retryTimer;
    QString       m_clientId;
    bool          m_ready     = false;
    QJsonObject   m_pending;
    bool          m_hasPending = false;
};
