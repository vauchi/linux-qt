// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayhost.h"
#include <QDesktopServices>
#include <QMessageBox>
#include <QTextDocument>
#include <QToolTip>
#include <QUrl>

bool QtDisplayHost::applyNativeEffect(const QJsonObject &command) {
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
