// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QTemporaryDir>
#include <memory>

using namespace display_test;

namespace {
std::unique_ptr<QTemporaryDir> directory;
QString destination;

QJsonObject exportCommand(const QJsonArray &bytes = {0, 255, 13, 10, 128},
                          const QString &name = "../prepared.bin") {
    return {{"ExportFile", QJsonObject{{"file", QJsonObject{
        {"suggested_name", name}, {"mime_type", "application/octet-stream"}, {"data", bytes}}}}}};
}

QList<QFileDialog *> dialogs() {
    QList<QFileDialog *> result;
    for (auto *dialog : window()->findChildren<QFileDialog *>())
        if (dialog->isVisible()) result.append(dialog);
    return result;
}

QFileDialog *dialog() {
    const auto shown = dialogs();
    assert(shown.size() == 1);
    return shown.first();
}

QByteArray contents() {
    QFile file(destination);
    assert(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

void choose() {
    auto *picker = dialog();
    picker->selectFile(destination);
    assert(QMetaObject::invokeMethod(picker, "accept", Qt::DirectConnection));
}

// Feature: identity_management.feature @backup — native export writes Core's bytes unchanged.
QList<Step> selection() {
    return {{{exportCommand()}, [] {
        auto *picker = dialog();
        assert(picker->acceptMode() == QFileDialog::AcceptSave);
        assert(picker->fileMode() == QFileDialog::AnyFile);
        assert(QFileInfo(picker->selectedFiles().first()).fileName() == "prepared.bin");
        assert(picker->mimeTypeFilters() == QStringList{"application/octet-stream"});
        assert(!QFileInfo::exists(destination));
        choose();
    }}, {{}, [] {
        assert(contents() == QByteArray::fromHex("00ff0d0a80"));
        assert(dialogs().isEmpty());
        assert(events.isEmpty());
    }}};
}

QList<Step> cancellation() {
    QFile existing(destination);
    assert(existing.open(QIODevice::WriteOnly));
    assert(existing.write("keep") == 4);
    existing.close();
    return {{{exportCommand()}, [] { dialog()->reject(); }}, {{}, [] {
        assert(contents() == "keep");
        assert(events.isEmpty());
        assert(dialogs().isEmpty());
    }}};
}

QList<Step> invalid() {
    return {{{exportCommand({-1}), exportCommand({256}), exportCommand({1.5}),
               exportCommand({"byte"}), exportCommand({QJsonValue::Null}),
               QJsonObject{{"ExportFile", QJsonObject{}}}}, [] {
        assert(dialogs().isEmpty());
        assert(!QFileInfo::exists(destination));
        assert(events.isEmpty());
    }}};
}

QList<Step> replacement() {
    return {{{exportCommand({1}), exportCommand({2}, "replacement.bin")}, [] {
        assert(QFileInfo(dialog()->selectedFiles().first()).fileName() == "replacement.bin");
        choose();
    }}, {{}, [] {
        assert(contents() == QByteArray::fromHex("02"));
        assert(dialogs().isEmpty());
        assert(events.isEmpty());
    }}};
}

QList<Step> empty() {
    return {{{exportCommand({})}, [] { choose(); }}, {{}, [] {
        assert(contents() == QByteArray{});
        assert(events.isEmpty());
        assert(dialogs().isEmpty());
    }}};
}

void setup() { schedule(); }
}

Q_COREAPP_STARTUP_FUNCTION(setup)

int main(int argc, char **argv) {
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    directory = std::make_unique<QTemporaryDir>();
    assert(directory->isValid());
    destination = directory->filePath("chosen.bin");
    const QString name = argc > 1 ? QString::fromUtf8(argv[1]) : "selection";
    if (name == "selection") steps = selection();
    else if (name == "cancellation") steps = cancellation();
    else if (name == "invalid") steps = invalid();
    else if (name == "replacement") steps = replacement();
    else if (name == "empty") steps = empty();
    else return 2;
    const int result = qt_display_start(argc, argv, receive, &events);
    assert(result == 0);
    return result;
}
