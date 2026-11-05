// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayhost.h"
#include <QDesktopServices>
#include <QMessageBox>
#include <QKeyEvent>
#include <QTextDocument>
#include <QToolTip>
#include <QUrl>

bool QtDisplayHost::applyNativeEffect(const QJsonObject &command) {
    if (command.contains("Celebrate")) {
        celebrate(command.value("Celebrate").toObject());
        return true;
    }
    if (command.contains("ShowShareSheet")) {
        if (command.value("ShowShareSheet").toObject().value("url").isString())
            emitEvent({{"HardwareUnavailable", QJsonObject{{"transport", "share_sheet"}}}});
        return true;
    }
    if (command.contains("PresentAlert")) {
        const auto alert = command.value("PresentAlert").toObject().value("alert").toObject();
        const auto title = alert.value("title");
        const auto message = alert.value("message");
        if (title.isString() && message.isString()) {
            auto *dialog = new QMessageBox(QMessageBox::Information, title.toString(),
                message.toString(), QMessageBox::Ok, this);
            dialog->setTextFormat(Qt::PlainText);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->open();
        }
        return true;
    }
    if (command.contains("ShowToast")) {
        const auto message = command.value("ShowToast").toObject()
            .value("toast").toObject().value("message");
        if (message.isString())
            QToolTip::showText(mapToGlobal(rect().center()),
                Qt::convertFromPlainText(message.toString()), this);
        return true;
    }
    if (command.contains("OpenExternalUrl")) {
        const auto value = command.value("OpenExternalUrl").toObject().value("url");
        if (value.isString()) {
            const QUrl url(value.toString(), QUrl::StrictMode);
            if (url.isValid() && !url.isRelative() && !url.scheme().isEmpty())
                QDesktopServices::openUrl(url);
        }
        return true;
    }
    return false;
}

void QtDisplayHost::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && !(event->modifiers() & Qt::AltModifier)) {
        const auto surfaceId = m_state.activeSurfaceId();
        if (!surfaceId.isEmpty()) {
            emitEvent({{"SurfaceActivated", QJsonObject{{"surface_id", surfaceId}}}});
            emitEvent({{"BackRequested", QJsonObject{{"surface_id", surfaceId}}}});
        }
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}
