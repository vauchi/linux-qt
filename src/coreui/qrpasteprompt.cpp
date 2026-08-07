// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qrpasteprompt.h"

#include <QInputDialog>

QrPastePrompt::QrPastePrompt(QWidget *parent)
    : QObject(parent), m_parentWidget(parent) {}

void QrPastePrompt::prompt() {
    if (m_dialog) {
        m_dialog->raise();
        m_dialog->activateWindow();
        return;
    }
    auto *dialog = new QInputDialog(m_parentWidget);
    dialog->setWindowTitle(tr("Scan QR"));
    dialog->setLabelText(tr("Paste QR data:"));
    dialog->setOption(QInputDialog::UsePlainTextEditForTextInput);
    m_dialog = dialog;
    connect(dialog, &QDialog::finished, this, [this](int result) {
        // Read the text and release the guard before emitting: the emit
        // re-enters core dispatch synchronously and may trigger a fresh
        // QrRequestScan, which must see no open dialog.
        const QString data = m_dialog ? m_dialog->textValue().trimmed()
                                      : QString();
        m_dialog.clear();
        if (result == QDialog::Accepted && !data.isEmpty()) {
            emit eventReady({{QStringLiteral("QrScanned"),
                              QJsonObject{{QStringLiteral("data"), data}}}});
        } else {
            emit eventReady(
                {{QStringLiteral("HardwareUnavailable"),
                  QJsonObject{{QStringLiteral("transport"),
                               QStringLiteral("qr_scan")}}}});
        }
    });
    connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
    dialog->open();
}
