// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "filepicker.h"
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <QWidget>
#include <limits>

namespace {
QJsonObject failure(const char *code) {
    return {{"failed", QJsonObject{{"code", code}}}};
}
QByteArray encodedResult(const QString &requestId, const QJsonValue &outcome) {
    return QJsonDocument(QJsonObject{{"FilePickCompleted", QJsonObject{
        {"request_id", requestId}, {"outcome", outcome}}}}).toJson(QJsonDocument::Compact);
}
bool validLimit(qint64 limit) {
    return limit >= 0 && limit < std::numeric_limits<qint64>::max();
}
QThreadPool *filePool() {
    static QThreadPool pool;
    static const bool initialized = [] { pool.setMaxThreadCount(2); return true; }();
    Q_UNUSED(initialized);
    return &pool;
}
}

FilePicker::FilePicker(QWidget *parent, QThreadPool *pool)
    : QObject(parent), m_parent(parent), m_pool(pool ? pool : filePool()) {}
FilePicker::~FilePicker() { cancel(m_requestId); }

void FilePicker::cancel(const QString &requestId) {
    if (requestId != m_requestId) return;
    if (m_cancelled) m_cancelled->store(true);
    m_cancelled.reset();
    m_requestId.clear();
    if (m_dialog) {
        disconnect(m_dialog, nullptr, this, nullptr);
        m_dialog->reject();
        m_dialog.clear();
    }
}

void FilePicker::complete(const QString &requestId, const QJsonValue &outcome) {
    if (m_requestId != requestId) return;
    m_requestId.clear();
    m_cancelled.reset();
    emit eventReady(encodedResult(requestId, outcome));
}

void FilePicker::pick(const QJsonObject &command) {
    const QString requestId = command.value("request_id").toString();
    if (requestId.isEmpty() || requestId.size() > 128) return;
    if (requestId == m_requestId) {
        if (m_dialog) m_dialog->raise();
        return;
    }
    cancel(m_requestId);
    m_requestId = requestId;
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    const qint64 limit = command.value("max_bytes").toInteger(-1);
    if (!validLimit(limit) || !command.value("title").isString()) {
        complete(requestId, failure("invalid_request"));
        return;
    }
    auto *dialog = new QFileDialog(m_parent, command.value("title").toString());
    m_dialog = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    QStringList filters;
    for (const auto &extension : command.value("accepted_extensions").toArray()) {
        filters.append(QStringLiteral("*.") + extension.toString());
    }
    if (!filters.isEmpty()) {
        dialog->setNameFilter(filters.join(' '));
    } else {
        QStringList mimeTypes;
        for (const auto &mime : command.value("accepted_mime_types").toArray()) {
            mimeTypes.append(mime.toString());
        }
        if (!mimeTypes.isEmpty()) {
            dialog->setMimeTypeFilters(mimeTypes);
        }
    }
    connect(dialog, &QDialog::accepted, this, [this, dialog, limit, requestId] {
        m_dialog.clear();
        const auto paths = dialog->selectedFiles();
        dialog->deleteLater();
        if (paths.isEmpty()) complete(requestId, failure("invalid_request"));
        else startRead(paths.first(), limit);
    });
    connect(dialog, &QDialog::rejected, this, [this, requestId] {
        m_dialog.clear();
        complete(requestId, QStringLiteral("cancelled"));
    });
    dialog->open();
}

QJsonObject FilePicker::readPickedFile(const QString &path, qint64 maxBytes) {
    return readPickedFile(path, maxBytes, std::make_shared<std::atomic_bool>(false));
}

void FilePicker::startRead(const QString &path, qint64 maxBytes) {
    const QString requestId = m_requestId;
    const auto cancelled = m_cancelled;
    auto *watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this,
            [this, watcher, requestId, cancelled] {
        if (!cancelled->load() && m_requestId == requestId) {
            m_requestId.clear();
            m_cancelled.reset();
            emit eventReady(watcher->result());
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(m_pool, [path, maxBytes, cancelled, requestId] {
        const auto outcome = readPickedFile(path, maxBytes, cancelled);
        if (cancelled->load()) return QByteArray();
        return encodedResult(requestId, outcome);
    }));
}

QJsonObject FilePicker::readPickedFile(const QString &path, qint64 maxBytes,
                                     const std::shared_ptr<std::atomic_bool> &cancelled) {
    if (cancelled->load()) return {};
    if (!validLimit(maxBytes)) return failure("invalid_request");
    if (!QFileInfo(path).isFile()) return failure("unsupported_file");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return failure("read_failed");
    if (file.size() > maxBytes) return failure("too_large");
    QJsonArray values;
    qint64 total = 0;
    while (!file.atEnd()) {
        if (cancelled->load()) return {};
        // One extra byte detects growth; chunks bound cancellation latency (#764).
        const auto bytes = file.read(qMin(qint64(65536), maxBytes - total + 1));
        if (file.error() != QFileDevice::NoError) return failure("read_failed");
        total += bytes.size();
        if (total > maxBytes) return failure("too_large");
        for (unsigned char byte : bytes) values.append(static_cast<int>(byte));
    }
    return {{"selected", QJsonObject{
        {"bytes", values}, {"filename", QFileInfo(path).fileName()}}}};
}
