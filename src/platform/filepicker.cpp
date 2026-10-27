// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "filepicker.h"
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QWidget>
#include <limits>

namespace {
QJsonObject failure(const QString &error) {
    return {{"HardwareError", QJsonObject{{"transport", "file_picker"}, {"error", error}}}};
}
bool validLimit(qint64 limit) {
    return limit >= 0 && limit < std::numeric_limits<qint64>::max();
}
}

FilePicker::FilePicker(QWidget *parent) : QObject(parent), m_parent(parent) {}

void FilePicker::pick(const QJsonObject &command) {
    if (m_dialog) {
        m_dialog->raise();
        return;
    }
    const qint64 limit = command.value("max_bytes").toInteger(-1);
    if (!validLimit(limit)) {
        emit eventReady(failure(QStringLiteral("Invalid file read limit")));
        return;
    }
    auto *dialog = new QFileDialog(m_parent, tr("Open file"));
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
    connect(dialog, &QDialog::accepted, this, [this, dialog, limit] {
        m_dialog.clear();
        const auto paths = dialog->selectedFiles();
        emit eventReady(paths.isEmpty()
            ? failure(QStringLiteral("No selected file"))
            : readPickedFile(paths.first(), limit));
    });
    connect(dialog, &QDialog::rejected, this, [this] {
        m_dialog.clear();
        emit eventReady(QStringLiteral("FilePickCancelledByUser"));
    });
    dialog->open();
}

QJsonObject FilePicker::readPickedFile(const QString &path, qint64 maxBytes) {
    if (!validLimit(maxBytes)) {
        return failure(QStringLiteral("Invalid file read limit"));
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(file.errorString());
    }
    if (file.size() > maxBytes) {
        return failure(QStringLiteral("File exceeds the supplied read limit"));
    }
    // One extra byte detects a file growing after the size check (#764).
    const QByteArray bytes = file.read(maxBytes + 1);
    if (file.error() != QFileDevice::NoError) {
        return failure(file.errorString());
    }
    if (bytes.size() > maxBytes) {
        return failure(QStringLiteral("File exceeds the supplied read limit"));
    }
    QJsonArray values;
    for (unsigned char byte : bytes) {
        values.append(static_cast<int>(byte));
    }
    return {{"FilePickedFromUser", QJsonObject{
        {"bytes", values}, {"filename", QFileInfo(path).fileName()}}}};
}
