// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <atomic>
#include <memory>

class QFileDialog;
class QWidget;
class QThreadPool;

class FilePicker : public QObject {
    Q_OBJECT
public:
    /// A supplied worker pool must outlive queued reads.
    explicit FilePicker(QWidget *parent, QThreadPool *pool = nullptr);
    ~FilePicker() override;
    void pick(const QJsonObject &command);
    void cancel(const QString &requestId);
    static QJsonObject readPickedFile(const QString &path, qint64 maxBytes);
signals:
    void eventReady(const QByteArray &eventJson);
private:
    QWidget *m_parent;
    QThreadPool *m_pool;
    QPointer<QFileDialog> m_dialog;
    QString m_requestId;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    void startRead(const QString &path, qint64 maxBytes);
    void complete(const QString &requestId, const QJsonValue &outcome);
    static QJsonObject readPickedFile(const QString &path, qint64 maxBytes,
                                     const std::shared_ptr<std::atomic_bool> &cancelled);
};
