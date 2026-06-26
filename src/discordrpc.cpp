#include "discordrpc.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QCoreApplication>

DiscordRPC::DiscordRPC(const QString &clientId, QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_retryTimer(new QTimer(this))
    , m_clientId(clientId)
{
    connect(m_socket, &QLocalSocket::connected,    this, &DiscordRPC::onConnected);
    connect(m_socket, &QLocalSocket::disconnected, this, &DiscordRPC::onDisconnected);
    connect(m_socket, &QLocalSocket::readyRead,    this, &DiscordRPC::onReadyRead);
    connect(m_retryTimer, &QTimer::timeout,        this, &DiscordRPC::tryConnect);

    m_retryTimer->setInterval(15000);
    m_retryTimer->start();
    tryConnect();
}

void DiscordRPC::tryConnect() {
    if (m_socket->state() != QLocalSocket::UnconnectedState) return;
    m_ready = false;
#ifdef Q_OS_WIN
    m_socket->connectToServer(R"(\\.\pipe\discord-ipc-0)");
#else
    const QString tmp = qEnvironmentVariable("XDG_RUNTIME_DIR", "/tmp");
    m_socket->connectToServer(tmp + "/discord-ipc-0");
#endif
}

void DiscordRPC::onConnected() {
    QJsonObject hs;
    hs["v"] = 1;
    hs["client_id"] = m_clientId;
    sendFrame(0, QJsonDocument(hs).toJson(QJsonDocument::Compact));
}

void DiscordRPC::onDisconnected() {
    m_ready = false;
}

void DiscordRPC::onReadyRead() {
    while (m_socket->bytesAvailable() >= 8) {
        const QByteArray hdr = m_socket->peek(8);
        const quint32 op  = *reinterpret_cast<const quint32*>(hdr.constData());
        const quint32 len = *reinterpret_cast<const quint32*>(hdr.constData() + 4);
        if ((quint64)m_socket->bytesAvailable() < 8u + len) break;
        m_socket->read(8);
        const QByteArray payload = m_socket->read(len);
        if (op == 1) {
            const QJsonObject obj = QJsonDocument::fromJson(payload).object();
            if (obj["evt"].toString() == "READY") {
                m_ready = true;
                if (m_hasPending) { sendActivity(m_pending); m_hasPending = false; }
            }
        }
    }
}

void DiscordRPC::sendFrame(int opcode, const QByteArray &json) {
    if (m_socket->state() != QLocalSocket::ConnectedState) return;
    QByteArray pkt;
    quint32 op  = opcode;
    quint32 len = json.size();
    pkt.append(reinterpret_cast<const char*>(&op),  4);
    pkt.append(reinterpret_cast<const char*>(&len), 4);
    pkt.append(json);
    m_socket->write(pkt);
    m_socket->flush();
}

void DiscordRPC::sendActivity(const QJsonObject &activity) {
    QJsonObject args;
    args["pid"]      = (int)QCoreApplication::applicationPid();
    args["activity"] = activity;
    QJsonObject cmd;
    cmd["cmd"]   = "SET_ACTIVITY";
    cmd["args"]  = args;
    cmd["nonce"] = QString::number(QDateTime::currentMSecsSinceEpoch());
    sendFrame(1, QJsonDocument(cmd).toJson(QJsonDocument::Compact));
}

void DiscordRPC::updateActivity(const QString &title, const QString &artist, bool playing) {
    QJsonObject activity;
    activity["details"] = title.isEmpty() ? "EchoBox II" : title;
    activity["state"]   = playing
        ? (artist.isEmpty() ? "EchoBox II" : artist)
        : "⏸ На паузе";
    activity["type"]    = 2; // Listening

    if (playing) {
        QJsonObject ts;
        ts["start"] = QDateTime::currentSecsSinceEpoch();
        activity["timestamps"] = ts;
    }

    if (m_ready) { sendActivity(activity); }
    else         { m_pending = activity; m_hasPending = true; }
}

void DiscordRPC::clearActivity() {
    m_hasPending = false;
    if (!m_ready) return;
    QJsonObject args;
    args["pid"] = (int)QCoreApplication::applicationPid();
    QJsonObject cmd;
    cmd["cmd"]   = "SET_ACTIVITY";
    cmd["args"]  = args;
    cmd["nonce"] = QString::number(QDateTime::currentMSecsSinceEpoch());
    sendFrame(1, QJsonDocument(cmd).toJson(QJsonDocument::Compact));
}
