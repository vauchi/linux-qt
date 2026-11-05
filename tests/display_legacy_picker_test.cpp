// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QTemporaryDir>
#include <memory>

using namespace display_test;

namespace {
std::unique_ptr<QTemporaryDir> directory;
QString selected;
QEventLoop *pending = nullptr;

QList<QFileDialog *> dialogs() {
    QList<QFileDialog *> result;
    for (auto *picker : window()->findChildren<QFileDialog *>())
        if (picker->isVisible()) result.append(picker);
    return result;
}

QFileDialog *dialog() {
    const auto shown = dialogs();
    assert(shown.size() == 1);
    return shown.first();
}

void receiveLegacy(const char *json, void *context) {
    const auto value = QJsonDocument::fromJson(QByteArray("[") + json + "]").array().first();
    if (value.isObject()) receive(json, context);
    else events.append(value);
    if (pending) pending->quit();
}

void choose() {
    auto *picker = dialog();
    QEventLoop completion;
    pending = &completion;
    const auto before = events.size();
    picker->selectFile(selected);
    assert(QMetaObject::invokeMethod(picker, "accept", Qt::DirectConnection));
    if (before == events.size()) completion.exec();
    pending = nullptr;
}

QJsonObject filePick(qint64 maxBytes = 5) {
    return {{"FilePickFromUser", QJsonObject{{"purpose", "ImportContacts"},
        {"accepted_extensions", QJsonArray{"bin"}},
        {"accepted_mime_types", QJsonArray{"application/octet-stream"}},
        {"max_bytes", maxBytes}}}};
}

// Feature: identity_management.feature @backup — preserve legacy native observations.
QList<Step> fileSelection() {
    return {{{filePick()}, [] {
        assert(dialog()->fileMode() == QFileDialog::ExistingFile);
        assert(dialog()->nameFilters() == QStringList{"*.bin"});
        choose();
    }}, {{}, [] {
        assert((events == QJsonArray{event("FilePickedFromUser", {
            {"filename", "raw.bin"}, {"bytes", QJsonArray{0, 255, 128, 10, 13}}})}));
        assert(dialogs().isEmpty());
    }}};
}

QList<Step> images() {
    return {{{"ImagePickFromFile"}, [] { choose(); }}, {{}, [] {
        assert((events == QJsonArray{event("ImageReceived", {{"data", QJsonArray{0, 255, 128, 10, 13}}})}));
        events = {};
    }}, {{"ImagePickFromLibrary", "ImagePickFromFile"}, [] {
        assert(dialogs().size() == 1);
        choose();
    }}, {{}, [] {
        assert((events == QJsonArray{event("ImageReceived", {{"data", QJsonArray{0, 255, 128, 10, 13}}})}));
        assert(dialogs().isEmpty());
    }}};
}

QList<Step> cancel() {
    return {{{"ImagePickFromLibrary"}, [] { dialog()->reject(); }}, {{}, [] {
        assert(events == QJsonArray{"ImagePickCancelled"});
        events = {};
    }}, {{filePick()}, [] { dialog()->reject(); }}, {{}, [] {
        assert(events == QJsonArray{"FilePickCancelledByUser"});
        assert(dialogs().isEmpty());
    }}};
}

QList<Step> limits() {
    return {{{filePick(4)}, [] { choose(); }}, {{}, [] {
        assert(events == QJsonArray{"FilePickCancelledByUser"});
        events = {};
    }}, {{filePick(-1)}, [] {
        assert(events == QJsonArray{"FilePickCancelledByUser"});
        assert(dialogs().isEmpty());
    }}};
}

void setup() { schedule(); }
}
Q_COREAPP_STARTUP_FUNCTION(setup)

int main(int argc, char **argv) {
    directory = std::make_unique<QTemporaryDir>();
    assert(directory->isValid());
    selected = directory->filePath("raw.bin");
    QFile file(selected);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write(QByteArray::fromHex("00ff800a0d")) == 5);
    file.close();
    const QString name = argc > 1 ? QString::fromUtf8(argv[1]) : "file";
    if (name == "file") steps = fileSelection();
    else if (name == "image") steps = images();
    else if (name == "cancel") steps = cancel();
    else if (name == "limits") steps = limits();
    else return 2;
    const auto result = qt_display_start(argc, argv, receiveLegacy, &events);
    assert(result == 0);
    return result;
}
