// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/filepicker.h"
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QTemporaryDir>
#include <cassert>

// @scenario: core_file_picker :: FilePickedFromUser on Contacts imports vCard
static void selected_file_preserves_bytes_and_only_the_basename() {
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("chosen.data"));
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write(QByteArray::fromHex("00ff80410a")) == 5);
    file.close();
    const QJsonObject expected{{"FilePickedFromUser", QJsonObject{
        {"bytes", QJsonArray{0, 255, 128, 65, 10}}, {"filename", "chosen.data"}}}};
    assert(FilePicker::readPickedFile(path, 5) == expected);
    assert(FilePicker::readPickedFile(path, 6) == expected);
    const QJsonObject oversized = FilePicker::readPickedFile(path, 4);
    assert(oversized.value("HardwareError").toObject().value("transport") == "file_picker");
    assert(!oversized.contains("FilePickedFromUser"));
}

// @scenario: generic_presentation_protocol.feature :: Invalid boundary input fails safely
static void file_errors_are_not_cancellations() {
    QTemporaryDir directory;
    for (const auto &path : {directory.path(), directory.filePath("missing")}) {
        const QJsonObject event = FilePicker::readPickedFile(path, 12);
        assert(event.keys() == QStringList{"HardwareError"});
        assert(event.value("HardwareError").toObject().value("transport") == "file_picker");
        assert(!event.value("HardwareError").toObject().value("error").toString().isEmpty());
    }
}

// @scenario: core_file_picker :: cancellation produces no result
static void picker_uses_supplied_filters_and_reports_cancel_without_blocking() {
    QWidget owner;
    FilePicker picker(&owner);
    QList<QJsonValue> events;
    QObject::connect(&picker, &FilePicker::eventReady, &owner,
                     [&](const QJsonValue &event) { events.append(event); });
    const QJsonObject command{{"accepted_extensions", QJsonArray{"abc", "xyz"}},
                              {"max_bytes", 42}};
    picker.pick(command);
    const auto dialogs = owner.findChildren<QFileDialog *>();
    assert(dialogs.size() == 1);
    assert(dialogs[0]->fileMode() == QFileDialog::ExistingFile);
    assert(dialogs[0]->nameFilters() == QStringList{"*.abc *.xyz"});
    picker.pick(command);
    assert(owner.findChildren<QFileDialog *>().size() == 1);
    dialogs[0]->reject();
    assert(events == QList<QJsonValue>{QJsonValue("FilePickCancelledByUser")});
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    assert(owner.findChildren<QFileDialog *>().isEmpty());
}

// @scenario: generic_presentation_protocol.feature :: Invalid boundary input fails safely
static void absent_or_invalid_limits_fail_closed() {
    QWidget owner;
    FilePicker picker(&owner);
    QList<QJsonValue> events;
    QObject::connect(&picker, &FilePicker::eventReady, &owner,
                     [&](const QJsonValue &event) { events.append(event); });
    for (const auto &limit : {QJsonValue(), QJsonValue(-1), QJsonValue(1.5),
                              QJsonValue("42")}) {
        picker.pick(QJsonObject{{"max_bytes", limit}});
        assert(owner.findChildren<QFileDialog *>().isEmpty());
        assert(!events.isEmpty());
        assert(events.last().toObject().keys() == QStringList{"HardwareError"});
    }
    assert(events.size() == 4);
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    selected_file_preserves_bytes_and_only_the_basename();
    file_errors_are_not_cancellations();
    picker_uses_supplied_filters_and_reports_cancel_without_blocking();
    absent_or_invalid_limits_fail_closed();
}
