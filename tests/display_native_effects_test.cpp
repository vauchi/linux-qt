// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QAbstractButton>
#include <QAccessible>
#include <QDesktopServices>
#include <QLabel>
#include <QMessageBox>
#include <QToolTip>
#include <QUrl>
#include <algorithm>

using namespace display_test;

class UrlCapture : public QObject {
    Q_OBJECT
public:
    QList<QUrl> urls;
public slots:
    void capture(const QUrl &url) { urls.append(url); }
};

namespace {
UrlCapture *capture = nullptr;
const QString message = QString::fromUtf8("Core <b>literal</b> & préparé\nNext line");

QJsonObject toast(const QString &text) {
    return {{"ShowToast", QJsonObject{{"toast", QJsonObject{{"message", text}}}}}};
}

QList<QMessageBox *> alerts() {
    QList<QMessageBox *> result;
    for (auto *alert : window()->findChildren<QMessageBox *>())
        if (alert->isVisible()) result.append(alert);
    return result;
}

QList<QLabel *> tooltips() {
    QList<QLabel *> result;
    for (auto *widget : QApplication::topLevelWidgets())
        if (auto *label = qobject_cast<QLabel *>(widget);
            label && label->isVisible() && label->windowType() == Qt::ToolTip)
            result.append(label);
    return result;
}

// Feature: identity_management.feature — present Core's prepared feedback verbatim.
QList<Step> alert() {
    return {{{QJsonObject{{"PresentAlert", QJsonObject{{"alert", QJsonObject{
        {"title", "Prepared alert & title"}, {"message", message}}}}}},
        surface("first", 1, "Following surface"), profile("first")}, [] {
        const auto shown = alerts();
        assert(shown.size() == 1);
        auto *box = shown.first();
        assert(box->windowTitle() == "Prepared alert & title");
        assert(box->text() == message);
        assert(box->textFormat() == Qt::PlainText);
        assert(box->standardButtons() == QMessageBox::Ok);
        const auto titles = window()->findChildren<QLabel *>();
        assert(std::any_of(titles.begin(), titles.end(), [](auto *label) {
            return label->text() == "Following surface";
        }));
        box->button(QMessageBox::Ok)->click();
    }}, {{}, [] {
        assert(alerts().isEmpty());
        assert(events.isEmpty());
    }}};
}

void checkTooltip(const QString &expected) {
    const auto shown = tooltips();
    assert(shown.size() == 1);
    auto *accessible = QAccessible::queryAccessibleInterface(shown.first());
    assert(accessible);
    assert(accessible->text(QAccessible::Name) == expected);
    assert(events.isEmpty());
}

QList<Step> transient() {
    return {{{toast(message)}, [] { checkTooltip(message); }},
        {{toast("Replacement feedback")}, [] {
            checkTooltip("Replacement feedback");
            QToolTip::hideText();
        }}};
}

QList<Step> urls() {
    return {{{QJsonObject{{"OpenExternalUrl", QJsonObject{
        {"url", "https://example.invalid/a%20b?q=one%26two#fragment"}}}},
        QJsonObject{{"OpenExternalUrl", QJsonObject{
        {"url", "mailto:person@example.invalid?subject=Prepared%20subject"}}}}}, [] {
        assert((capture->urls == QList<QUrl>{
            QUrl("https://example.invalid/a%20b?q=one%26two#fragment"),
            QUrl("mailto:person@example.invalid?subject=Prepared%20subject")}));
        assert(events.isEmpty());
    }}};
}

QList<Step> malformed() {
    return {{{QJsonObject{{"PresentAlert", QJsonObject{}}},
        QJsonObject{{"ShowToast", QJsonObject{{"toast", QJsonObject{{"message", 4}}}}}},
        QJsonObject{{"OpenExternalUrl", QJsonObject{{"url", "https://example.invalid/%GG"}}}},
        QJsonObject{{"OpenExternalUrl", QJsonObject{{"url", "/relative/path"}}}},
        QJsonObject{{"OpenExternalUrl", QJsonObject{{"url", ""}}}}}, [] {
        assert(alerts().isEmpty());
        assert(tooltips().isEmpty());
        assert(capture->urls.isEmpty());
        assert(events.isEmpty());
    }}};
}

void setup() { schedule(); }
}
Q_COREAPP_STARTUP_FUNCTION(setup)

int main(int argc, char **argv) {
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    UrlCapture receiver;
    capture = &receiver;
    QDesktopServices::setUrlHandler("https", capture, "capture");
    QDesktopServices::setUrlHandler("mailto", capture, "capture");
    const QString name = argc > 1 ? QString::fromUtf8(argv[1]) : "alert";
    if (name == "alert") steps = alert();
    else if (name == "toast") steps = transient();
    else if (name == "url") steps = urls();
    else if (name == "malformed") steps = malformed();
    else return 2;
    const auto result = qt_display_start(argc, argv, receive, &events);
    QDesktopServices::unsetUrlHandler("https");
    QDesktopServices::unsetUrlHandler("mailto");
    assert(result == 0);
    return result;
}

#include "display_native_effects_test.moc"
