// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayhost.h"
#include "presentationeffectpayload.h"
#include "../platform/filepicker.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>

void QtDisplayHost::exportFile(const QJsonObject &command) {
    const auto file = PresentationEffectPayload::exportFile(command);
    if (!file) return;
    if (m_exportDialog) m_exportDialog->reject();
    auto *dialog = new QFileDialog(this);
    m_exportDialog = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setMimeTypeFilters({file->mimeType});
    const auto name = QFileInfo(file->suggestedName).fileName();
    if (name != "." && name != "..") dialog->selectFile(name);
    connect(dialog, &QDialog::accepted, this, [dialog, data = file->data] {
        const auto paths = dialog->selectedFiles();
        if (paths.isEmpty()) return;
        QSaveFile output(paths.first());
        if (!output.open(QIODevice::WriteOnly)) return;
        if (output.write(data) != data.size()) {
            output.cancelWriting();
            return;
        }
        output.commit();
    });
    dialog->open();
}

bool QtDisplayHost::pickLegacyFile(const QJsonValue &command) {
    const auto name = command.toString();
    const bool image = name == "ImagePickFromLibrary" || name == "ImagePickFromFile";
    const auto object = command.toObject();
    if (!image && !object.contains("FilePickFromUser")) return false;
    if (!m_legacyPicker) {
        m_legacyPicker = new FilePicker(this);
        connect(m_legacyPicker, &FilePicker::eventReady, this, [this](const QByteArray &json) {
            const auto result = QJsonDocument::fromJson(json).object()
                .value("FilePickCompleted").toObject().value("outcome").toObject();
            if (result.contains("selected")) {
                const auto selected = result.value("selected").toObject();
                if (m_legacyImage)
                    emitEvent({{"ImageReceived", QJsonObject{{"data", selected.value("bytes")}}}});
                else emitEvent({{"FilePickedFromUser", QJsonObject{
                    {"bytes", selected.value("bytes")}, {"filename", selected.value("filename")}}}});
            } else if (m_onEvent) {
                m_onEvent(m_legacyImage ? "\"ImagePickCancelled\"" : "\"FilePickCancelledByUser\"", m_userData);
            }
        });
    }
    m_legacyImage = image;
    auto payload = object.value("FilePickFromUser").toObject();
    payload.insert("request_id", image ? "qt-legacy-image" : "qt-legacy-file");
    payload.insert("title", "");
    // The contract's event_json.rs caps legacy file payloads at 32 MiB.
    constexpr qint64 eventLimit = 32 * 1024 * 1024;
    const auto limit = payload.value("max_bytes");
    payload.insert("max_bytes", limit.isNull() || limit.isUndefined()
        ? eventLimit : qMin(eventLimit, limit.toInteger(-1)));
    m_legacyPicker->pick(payload);
    return true;
}
