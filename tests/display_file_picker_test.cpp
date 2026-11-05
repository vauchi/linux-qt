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
QString selectedPath;
QEventLoop *pendingRead = nullptr;

QJsonObject pick(const QString &id = QStringLiteral("opaque-file"), qint64 limit = 5) {
    return {{"PickFile", QJsonObject{{"request_id", id}, {"title", "Prepared import title"},
        {"accepted_extensions", QJsonArray{"data", "bin"}},
        {"accepted_mime_types", QJsonArray{"application/octet-stream"}}, {"max_bytes", limit}}}};
}

QJsonObject cancel(const QString &id) {
    return {{"CancelFilePick", QJsonObject{{"request_id", id}}}};
}

QJsonObject completed(const QString &id, const QJsonValue &outcome) {
    return event("FilePickCompleted", {{"request_id", id}, {"outcome", outcome}});
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

void receivePicker(const char *json, void *context) {
    receive(json, context);
    if (pendingRead && QJsonDocument::fromJson(json).object().contains("FilePickCompleted"))
        pendingRead->quit();
}

void chooseFile() {
    auto *picker = dialog();
    QEventLoop completion;
    pendingRead = &completion;
    const auto before = events.size();
    picker->selectFile(selectedPath);
    assert(QMetaObject::invokeMethod(picker, "accept", Qt::DirectConnection));
    if (events.size() == before) completion.exec();
    pendingRead = nullptr;
}

QList<Step> availability() {
    return {
        {{}, [] {
            assert((filePickerCapabilities == QJsonArray{event("FilePickerCapabilitiesChanged",
                {{"correlated_results", true}})}));
        }},
        {{pick(), cancel("opaque-file")}, [] {
            assert(filePickerCapabilities.size() == 1);
            assert(events.isEmpty());
        }},
    };
}

QList<Step> preparedDialog() {
    auto mime = pick("mime-only");
    auto payload = mime.value("PickFile").toObject();
    payload.insert("accepted_extensions", QJsonArray{});
    payload.insert("accepted_mime_types", QJsonArray{"application/json"});
    mime.insert("PickFile", payload);
    return {
        {{pick(), pick()}, [] {
            auto *picker = dialog();
            assert(picker->windowTitle() == QStringLiteral("Prepared import title"));
            assert(picker->fileMode() == QFileDialog::ExistingFile);
            assert(picker->acceptMode() == QFileDialog::AcceptOpen);
            assert(picker->nameFilters() == QStringList{"*.data *.bin"});
            picker->reject();
        }},
        {{}, [] {
            assert(dialogs().isEmpty());
            assert((events == QJsonArray{completed("opaque-file", "cancelled")}));
            events = {};
        }},
        {{mime}, [] {
            auto *picker = dialog();
            assert(picker->mimeTypeFilters() == QStringList{"application/json"});
            picker->reject();
        }},
        {{}, [] {
            assert((events == QJsonArray{completed("mime-only", "cancelled")}));
        }},
    };
}

QList<Step> cancellation() {
    return {
        {{pick(), cancel("different-id")}, [] {
            assert(dialogs().size() == 1);
            assert(events.isEmpty());
        }},
        {{cancel("opaque-file"), cancel("opaque-file")}, [] {
            assert(dialogs().isEmpty());
            assert(events.isEmpty());
        }},
        {{pick("opaque-next")}, [] { dialog()->reject(); }},
        {{}, [] {
            assert((events == QJsonArray{completed("opaque-next", "cancelled")}));
        }},
    };
}

QList<Step> selection() {
    return {
        {{pick()}, [] {
            chooseFile();
            assert((events == QJsonArray{completed("opaque-file", QJsonObject{{"selected",
                QJsonObject{{"bytes", QJsonArray{0, 255, 128, 65, 10}},
                            {"filename", "chosen.data"}}}})}));
        }},
        {{}, [] { assert(events.size() == 1); }},
    };
}

QList<Step> limits() {
    return {
        {{pick("too-large", 4)}, [] {
            chooseFile();
            assert((events == QJsonArray{completed("too-large",
                QJsonObject{{"failed", QJsonObject{{"code", "too_large"}}}})}));
            events = {};
        }},
        {{pick("invalid-limit", -1)}, [] {
            assert(dialogs().isEmpty());
            assert((events == QJsonArray{completed("invalid-limit",
                QJsonObject{{"failed", QJsonObject{{"code", "invalid_request"}}}})}));
        }},
    };
}

QList<Step> replacement() {
    return {
        {{pick("retired"), pick("current"), cancel("retired")}, [] {
            assert(dialogs().size() == 1);
            assert(events.isEmpty());
            dialog()->reject();
        }},
        {{}, [] {
            assert((events == QJsonArray{completed("current", "cancelled")}));
        }},
    };
}

} // namespace

static void scheduleVerification() { display_test::schedule(); }
Q_COREAPP_STARTUP_FUNCTION(scheduleVerification)

int main(int argc, char **argv) {
    assert(argc == 2);
    directory = std::make_unique<QTemporaryDir>();
    assert(directory->isValid());
    selectedPath = directory->filePath("chosen.data");
    QFile file(selectedPath);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write(QByteArray::fromHex("00ff80410a")) == 5);
    file.close();
    const QString test = QString::fromUtf8(argv[1]);
    if (test == "availability") steps = availability();
    else if (test == "dialog") steps = preparedDialog();
    else if (test == "cancel") steps = cancellation();
    else if (test == "selection") steps = selection();
    else if (test == "limits") steps = limits();
    else if (test == "replacement") steps = replacement();
    else return 2;
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    return qt_display_start(argc, argv, receivePicker, &events);
}
