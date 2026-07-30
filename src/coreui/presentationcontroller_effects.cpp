// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationcontroller.h"
#include "presentationeffectpayload.h"

#include "../platform/hardwarebackend.h"

#include <QDesktopServices>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSaveFile>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <memory>

void PresentationController::dispatchRawEvent(const QJsonValue &event) {
    if (!m_app) {
        return;
    }
    QByteArray eventJson;
    if (event.isObject()) {
        eventJson =
            QJsonDocument(event.toObject()).toJson(QJsonDocument::Compact);
    } else if (event.isArray()) {
        eventJson =
            QJsonDocument(event.toArray()).toJson(QJsonDocument::Compact);
    } else {
        QJsonArray wrapper{event};
        eventJson = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
        eventJson = eventJson.mid(1, eventJson.size() - 2);
    }
    char *json = vauchi_app_dispatch(m_app, eventJson.constData());
    if (!json) {
        return;
    }
    const QByteArray envelope(json);
    vauchi_string_free(json);
    applyEnvelope(envelope);
}

void PresentationController::executeEffect(const QJsonValue &command) {
    if (command.isString()) {
        if (command.toString() == QStringLiteral("PerformNativeBack")) {
            emit nativeBackRequested();
            return;
        }
        m_hardware->dispatchCommands(QJsonArray{command});
        return;
    }
    const QJsonObject object = command.toObject();
    if (object.contains(QStringLiteral("PresentOverlay"))) {
        const QJsonObject payload =
            object.value(QStringLiteral("PresentOverlay")).toObject();
        presentOverlay(
            payload.value(QStringLiteral("surface_id")).toString(),
            payload.value(QStringLiteral("overlay")).toObject());
        return;
    }
    if (object.contains(QStringLiteral("ShowToast"))) {
        const QJsonObject toast =
            object.value(QStringLiteral("ShowToast")).toObject()
                .value(QStringLiteral("toast")).toObject();
        QToolTip::showText(mapToGlobal(rect().center()),
                           toast.value(QStringLiteral("message")).toString(),
                           this);
        return;
    }
    if (object.contains(QStringLiteral("PresentAlert"))) {
        const QJsonObject alert =
            object.value(QStringLiteral("PresentAlert")).toObject()
                .value(QStringLiteral("alert")).toObject();
        QMessageBox::information(
            this, alert.value(QStringLiteral("title")).toString(),
            alert.value(QStringLiteral("message")).toString());
        return;
    }
    if (object.contains(QStringLiteral("OpenExternalUrl"))) {
        const QString url =
            object.value(QStringLiteral("OpenExternalUrl")).toObject()
                .value(QStringLiteral("url")).toString();
        QDesktopServices::openUrl(QUrl(url));
        return;
    }
    if (object.contains(QStringLiteral("ScheduleWakeup"))) {
        const auto seconds = static_cast<uint32_t>(
            object.value(QStringLiteral("ScheduleWakeup")).toObject()
                .value(QStringLiteral("deadline_secs")).toInt(30));
        emit wakeupScheduled(seconds);
        return;
    }
    if (object.contains(QStringLiteral("FilePickFromUser"))) {
        pickFile(object.value(QStringLiteral("FilePickFromUser")).toObject());
        return;
    }
    if (object.contains(QStringLiteral("ImagePickFromFile"))
        || object.contains(QStringLiteral("ImagePickFromLibrary"))) {
        pickImage();
        return;
    }
    if (object.contains(QStringLiteral("ExportFile"))) {
        const auto file = PresentationEffectPayload::exportFile(object);
        if (!file.has_value()) {
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Export file"),
            file->suggestedName);
        if (path.isEmpty()) {
            return;
        }
        QSaveFile output(path);
        if (output.open(QIODevice::WriteOnly)) {
            output.write(file->data);
            output.commit();
        }
        return;
    }
    m_hardware->dispatchCommands(QJsonArray{command});
}

void PresentationController::presentOverlay(
    const QString &surfaceId, const QJsonObject &overlay) {
    const QString kind = overlay.value(QStringLiteral("kind")).toString();
    const QJsonArray items = overlay.value(QStringLiteral("items")).toArray();
    auto activated = std::make_shared<bool>(false);
    if (kind == QStringLiteral("navigation")) {
        auto *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(
            overlay.value(QStringLiteral("title")).toString());
        auto *layout = new QVBoxLayout(dialog);
        for (const auto &itemValue : items) {
            const QJsonObject item = itemValue.toObject();
            auto *button =
                new QPushButton(item.value(QStringLiteral("label")).toString());
            button->setAccessibleName(
                item.value(QStringLiteral("accessibility_label")).toString());
            button->setEnabled(
                item.value(QStringLiteral("enabled")).toBool(true));
            const QString interaction =
                item.value(QStringLiteral("interaction_id")).toString();
            connect(button, &QPushButton::clicked, dialog,
                    [this, dialog, activated, surfaceId, interaction]() {
                        *activated = true;
                        dialog->accept();
                        dispatchInteraction(surfaceId, interaction);
                    });
            layout->addWidget(button);
        }
        connect(dialog, &QDialog::finished, this,
                [this, activated, surfaceId, kind](int) {
                    if (!*activated) {
                        dispatchEvent(QJsonObject{
                            {QStringLiteral("OverlayDismissed"),
                             QJsonObject{
                                 {QStringLiteral("surface_id"), surfaceId},
                                 {QStringLiteral("kind"), kind}}}});
                    }
                });
        dialog->show();
        if (!m_reducedMotion) {
            const QPoint end = dialog->pos();
            auto *animation =
                new QPropertyAnimation(dialog, "pos", dialog);
            animation->setDuration(180);
            animation->setStartValue(end - QPoint(dialog->width(), 0));
            animation->setEndValue(end);
            animation->start(QAbstractAnimation::DeleteWhenStopped);
        }
        return;
    }

    auto *menu = new QMenu(this);
    menu->setTitle(overlay.value(QStringLiteral("title")).toString());
    for (const auto &itemValue : items) {
        const QJsonObject item = itemValue.toObject();
        auto *action =
            menu->addAction(item.value(QStringLiteral("label")).toString());
        action->setEnabled(
            item.value(QStringLiteral("enabled")).toBool(true));
        const QString interaction =
            item.value(QStringLiteral("interaction_id")).toString();
        connect(action, &QAction::triggered, this,
                [this, activated, surfaceId, interaction]() {
                    *activated = true;
                    dispatchInteraction(surfaceId, interaction);
                });
    }
    connect(menu, &QMenu::aboutToHide, this,
            [this, menu, activated, surfaceId, kind]() {
                if (!*activated) {
                    dispatchEvent(QJsonObject{
                        {QStringLiteral("OverlayDismissed"),
                         QJsonObject{{QStringLiteral("surface_id"), surfaceId},
                                     {QStringLiteral("kind"), kind}}}});
                }
                menu->deleteLater();
            });
    menu->popup(mapToGlobal(QPoint(width(), height())));
}

void PresentationController::pickFile(const QJsonObject &) {
    const QString path = QFileDialog::getOpenFileName(this, tr("Open file"));
    if (path.isEmpty()) {
        dispatchRawEvent(QStringLiteral("FilePickCancelledByUser"));
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        dispatchRawEvent(QStringLiteral("FilePickCancelledByUser"));
        return;
    }
    dispatchEvent(QJsonObject{
        {QStringLiteral("FilePickedFromUser"),
         QJsonObject{{QStringLiteral("bytes"), bytesToJson(file.readAll())},
                     {QStringLiteral("filename"),
                      QFileInfo(path).fileName()}}}});
}

void PresentationController::pickImage() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose image"), {}, tr("Images (*.png *.jpg *.jpeg *.webp)"));
    if (path.isEmpty()) {
        dispatchRawEvent(QStringLiteral("ImagePickCancelled"));
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        dispatchRawEvent(QStringLiteral("ImagePickCancelled"));
        return;
    }
    dispatchEvent(QJsonObject{
        {QStringLiteral("ImageReceived"),
         QJsonObject{{QStringLiteral("data"), bytesToJson(file.readAll())}}}});
}

QJsonArray PresentationController::bytesToJson(const QByteArray &bytes) {
    QJsonArray result;
    for (const unsigned char byte : bytes) {
        result.append(static_cast<int>(byte));
    }
    return result;
}
