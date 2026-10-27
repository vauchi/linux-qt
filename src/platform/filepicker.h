// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>

class QFileDialog;
class QWidget;

class FilePicker : public QObject {
    Q_OBJECT
public:
    explicit FilePicker(QWidget *parent);
    void pick(const QJsonObject &command);
    static QJsonObject readPickedFile(const QString &path, qint64 maxBytes);
signals:
    void eventReady(const QJsonValue &event);
private:
    QWidget *m_parent;
    QPointer<QFileDialog> m_dialog;
};
