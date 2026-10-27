// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/filepicker.h"
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
#include <QThreadPool>
#include <QSemaphore>
#include <cassert>

// @scenario: generic_presentation_protocol.feature :: User interaction returns as an opaque event
static void selected_file_preserves_bytes_and_only_the_basename() {
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("chosen.data"));
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write(QByteArray::fromHex("00ff80410a")) == 5);
    file.close();
    const QJsonObject expected{{"selected", QJsonObject{
        {"bytes", QJsonArray{0, 255, 128, 65, 10}}, {"filename", "chosen.data"}}}};
    assert(FilePicker::readPickedFile(path, 5) == expected);
    assert(FilePicker::readPickedFile(path, 6) == expected);
    const QJsonObject oversized = FilePicker::readPickedFile(path, 4);
    assert(oversized.value("failed").toObject().value("code") == "too_large");
    assert(!oversized.contains("selected"));
}

// @scenario: generic_presentation_protocol.feature :: Invalid boundary input fails safely
static void file_errors_are_not_cancellations() {
    QTemporaryDir directory;
    for (const auto &path : {directory.path(), directory.filePath("missing")}) {
        const QJsonObject event = FilePicker::readPickedFile(path, 12);
        assert(event.keys() == QStringList{"failed"});
        assert(event.value("failed").toObject().value("code") == "unsupported_file");
        assert(!event.value("failed").toObject().contains("error"));
    }
}

// @scenario: generic_presentation_protocol.feature :: User interaction returns as an opaque event
static void picker_uses_supplied_filters_and_reports_cancel_without_blocking() {
    QWidget owner;
    FilePicker picker(&owner);
    QList<QJsonValue> events;
    QObject::connect(&picker, &FilePicker::eventReady, &owner,
                     [&](const QByteArray &event) { events.append(QJsonDocument::fromJson(event).object()); });
    const QJsonObject command{{"accepted_extensions", QJsonArray{"abc", "xyz"}},
                              {"max_bytes", 42}, {"request_id", "opaque-dialog"}, {"title", "Prepared title"}};
    picker.pick(command);
    const auto dialogs = owner.findChildren<QFileDialog *>();
    assert(dialogs.size() == 1);
    assert(dialogs[0]->fileMode() == QFileDialog::ExistingFile);
    assert(dialogs[0]->windowTitle() == "Prepared title");
    assert(dialogs[0]->nameFilters() == QStringList{"*.abc *.xyz"});
    picker.pick(command);
    assert(owner.findChildren<QFileDialog *>().size() == 1);
    dialogs[0]->reject();
    const QJsonObject cancelled{{"FilePickCompleted", QJsonObject{
        {"request_id", "opaque-dialog"}, {"outcome", "cancelled"}}}};
    assert(events == QList<QJsonValue>{cancelled});
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    assert(owner.findChildren<QFileDialog *>().isEmpty());
}

// @scenario: generic_presentation_protocol.feature :: Invalid boundary input fails safely
static void absent_or_invalid_limits_fail_closed() {
    QWidget owner;
    FilePicker picker(&owner);
    QList<QJsonValue> events;
    QObject::connect(&picker, &FilePicker::eventReady, &owner,
                     [&](const QByteArray &event) { events.append(QJsonDocument::fromJson(event).object()); });
    for (const auto &limit : {QJsonValue(), QJsonValue(-1), QJsonValue(1.5),
                              QJsonValue("42")}) {
        picker.pick(QJsonObject{{"max_bytes", limit}, {"request_id", "invalid-limit"}, {"title", "Prepared title"}});
        assert(owner.findChildren<QFileDialog *>().isEmpty());
        assert(!events.isEmpty());
        const auto result = events.last().toObject().value("FilePickCompleted").toObject();
        assert(result.value("request_id") == "invalid-limit");
        assert(result.value("outcome").toObject().value("failed").toObject().value("code") == "invalid_request");
    }
    assert(events.size() == 4);
}

// @scenario: generic_presentation_protocol.feature :: Invalid boundary input fails safely
static void destroying_an_executor_does_not_wait_for_or_deliver_pending_work() {
    QThreadPool pool;
    pool.setMaxThreadCount(1);
    QSemaphore entered, resume;
    pool.start([&] { entered.release(); resume.acquire(); });
    assert(entered.tryAcquire(1, 2000));
    QTemporaryDir directory;
    const QString path = directory.filePath("pending.data");
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write("bytes") == 5);
    file.close();
    QWidget owner;
    auto *picker = new FilePicker(&owner, &pool);
    int deliveries = 0;
    QObject::connect(picker, &FilePicker::eventReady, &owner, [&](const QByteArray &) { ++deliveries; });
    picker->pick(QJsonObject{{"request_id", "destroyed"}, {"title", "Prepared title"}, {"max_bytes", 42}});
    auto *dialog = owner.findChild<QFileDialog *>();
    assert(dialog);
    dialog->selectFile(path);
    assert(QMetaObject::invokeMethod(dialog, "accepted", Qt::DirectConnection));
    delete picker;
    resume.release();
    assert(pool.waitForDone(2000));
    QApplication::processEvents();
    assert(deliveries == 0);
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    selected_file_preserves_bytes_and_only_the_basename();
    file_errors_are_not_cancellations();
    picker_uses_supplied_filters_and_reports_cancel_without_blocking();
    absent_or_invalid_limits_fail_closed();
    destroying_an_executor_does_not_wait_for_or_deliver_pending_work();
    // @scenario: generic_presentation_protocol.feature :: User interaction returns as an opaque event
    QTemporaryDir directory;
    const QString path = directory.filePath("async.data");
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write("raw") == 3);
    file.close();
    QWidget owner;
    FilePicker picker(&owner);
    QList<QJsonValue> events;
    QEventLoop loop;
    QObject::connect(&picker, &FilePicker::eventReady, &owner, [&](const QByteArray &event) {
        events.append(QJsonDocument::fromJson(event).object());
        loop.quit();
    });
    picker.pick(QJsonObject{{"request_id", "opaque-first"}, {"title", "Prepared title"}, {"max_bytes", 42}});
    auto *dialog = owner.findChild<QFileDialog *>();
    assert(dialog);
    dialog->selectFile(path);
    assert(QMetaObject::invokeMethod(dialog, "accepted", Qt::DirectConnection));
    assert(events.isEmpty());
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
    assert(events.size() == 1);
    assert(events[0].toObject().value("FilePickCompleted").toObject().value("request_id") == "opaque-first");
    assert(events[0].toObject().value("FilePickCompleted").toObject().value("outcome").toObject()
               .value("selected").toObject().value("bytes") == QJsonArray({114, 97, 119}));
    events.clear();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    // @scenario: generic_presentation_protocol.feature :: Invalid boundary input fails safely
    picker.pick(QJsonObject{{"request_id", "cancel-me"}, {"title", "Prepared title"}, {"max_bytes", 42}});
    dialog = owner.findChild<QFileDialog *>();
    assert(dialog);
    dialog->selectFile(path);
    assert(QMetaObject::invokeMethod(dialog, "accepted", Qt::DirectConnection));
    picker.cancel("cancel-me");
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    picker.pick(QJsonObject{{"request_id", "replacement"}, {"title", "Prepared title"}, {"max_bytes", 42}});
    dialog = owner.findChild<QFileDialog *>();
    assert(dialog);
    dialog->selectFile(path);
    assert(QMetaObject::invokeMethod(dialog, "accepted", Qt::DirectConnection));
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
    assert(events.size() == 1);
    assert(events[0].toObject().value("FilePickCompleted").toObject().value("request_id") == "replacement");
}
