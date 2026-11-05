// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayhost.h"
#include "presentationeffectpayload.h"
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
