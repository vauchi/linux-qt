// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directsendworker.h"

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// VXCH frame: magic(4) + version(1) + length(4 big-endian) + payload
static constexpr char VXCH_MAGIC[4] = {'V', 'X', 'C', 'H'};
static constexpr quint8 VXCH_VERSION = 1;
static constexpr int VXCH_HEADER_SIZE = 4 + 1 + 4; // 9 bytes

DirectSendWorker::DirectSendWorker(const QByteArray &payload, bool isInitiator, QObject *parent)
    : QThread(parent), m_payload(payload), m_isInitiator(isInitiator) {}

void DirectSendWorker::run() {
    if (m_isInitiator) {
        runInitiator();
    } else {
        runResponder();
    }
}

void DirectSendWorker::runInitiator() {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        emit errorOccurred(QString("socket: %1").arg(strerror(errno)));
        return;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DefaultPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::connect(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        emit errorOccurred(QString("connect: %1").arg(strerror(errno)));
        ::close(sock);
        return;
    }

    if (!sendVxch(sock, m_payload)) {
        ::close(sock);
        return;
    }

    QByteArray response = recvVxch(sock);
    ::close(sock);

    if (!response.isNull()) {
        emit payloadReceived(response);
    }
}

void DirectSendWorker::runResponder() {
    int serverSock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        emit errorOccurred(QString("socket: %1").arg(strerror(errno)));
        return;
    }

    int opt = 1;
    ::setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DefaultPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(serverSock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        emit errorOccurred(QString("bind: %1").arg(strerror(errno)));
        ::close(serverSock);
        return;
    }

    if (::listen(serverSock, 1) < 0) {
        emit errorOccurred(QString("listen: %1").arg(strerror(errno)));
        ::close(serverSock);
        return;
    }

    int clientSock = ::accept(serverSock, nullptr, nullptr);
    ::close(serverSock);

    if (clientSock < 0) {
        emit errorOccurred(QString("accept: %1").arg(strerror(errno)));
        return;
    }

    QByteArray incoming = recvVxch(clientSock);
    if (incoming.isNull()) {
        ::close(clientSock);
        return;
    }

    if (!sendVxch(clientSock, m_payload)) {
        ::close(clientSock);
        return;
    }

    ::close(clientSock);
    emit payloadReceived(incoming);
}

bool DirectSendWorker::sendVxch(int sock, const QByteArray &payload) {
    // Build header
    char header[VXCH_HEADER_SIZE];
    std::memcpy(header, VXCH_MAGIC, 4);
    header[4] = static_cast<char>(VXCH_VERSION);
    quint32 len = htonl(static_cast<quint32>(payload.size()));
    std::memcpy(header + 5, &len, 4);

    if (!sendAll(sock, header, VXCH_HEADER_SIZE)) {
        emit errorOccurred(QString("send header: %1").arg(strerror(errno)));
        return false;
    }
    if (!payload.isEmpty() && !sendAll(sock, payload.constData(), payload.size())) {
        emit errorOccurred(QString("send payload: %1").arg(strerror(errno)));
        return false;
    }
    return true;
}

QByteArray DirectSendWorker::recvVxch(int sock) {
    QByteArray header = recvExact(sock, VXCH_HEADER_SIZE);
    if (header.size() != VXCH_HEADER_SIZE) {
        emit errorOccurred("recv header: connection closed prematurely");
        return QByteArray();
    }

    if (std::memcmp(header.constData(), VXCH_MAGIC, 4) != 0) {
        emit errorOccurred("recv: invalid VXCH magic");
        return QByteArray();
    }

    if (static_cast<quint8>(header[4]) != VXCH_VERSION) {
        emit errorOccurred(QString("recv: unsupported VXCH version %1").arg(static_cast<quint8>(header[4])));
        return QByteArray();
    }

    quint32 len = 0;
    std::memcpy(&len, header.constData() + 5, 4);
    len = ntohl(len);

    if (len == 0) {
        return QByteArray();
    }

    QByteArray payload = recvExact(sock, static_cast<int>(len));
    if (payload.size() != static_cast<int>(len)) {
        emit errorOccurred("recv payload: connection closed prematurely");
        return QByteArray();
    }
    return payload;
}

bool DirectSendWorker::sendAll(int sock, const char *data, int len) {
    int sent = 0;
    while (sent < len) {
        int n = static_cast<int>(::send(sock, data + sent, static_cast<size_t>(len - sent), MSG_NOSIGNAL));
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

QByteArray DirectSendWorker::recvExact(int sock, int count) {
    QByteArray buf(count, '\0');
    int received = 0;
    while (received < count) {
        int n = static_cast<int>(::recv(sock, buf.data() + received, static_cast<size_t>(count - received), 0));
        if (n <= 0) break;
        received += n;
    }
    buf.resize(received);
    return buf;
}
