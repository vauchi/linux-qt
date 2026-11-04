// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The three-call C ABI of the Qt display shell (include/qt_display.h).
// A DisplayHost on the GUI thread owns one PresentationSurface per
// ReplaceSurface and forwards the surface's signals as contract events.
#include "qt_display.h"

#include "coreui/presentationsurface.h"
#include "coreui/thememanager.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QMainWindow>
#include <QMetaObject>
#include <QPointer>
#include <QScrollArea>

#include <atomic>

namespace {

class DisplayHost : public QMainWindow {
public:
    DisplayHost(QtDisplayEventCallback onEvent, void *userData)
        : m_onEvent(onEvent), m_userData(userData) {
        m_scroll = new QScrollArea(this);
        m_scroll->setWidgetResizable(true);
        setCentralWidget(m_scroll);
        resize(900, 700);
    }

    void render(const QJsonArray &commands) {
        for (const QJsonValue &value : commands) {
            const QJsonObject command = value.toObject();
            if (command.contains(QStringLiteral("SetPresentationProfile"))) {
                const auto profile = command.value(QStringLiteral("SetPresentationProfile"))
                                         .toObject().value(QStringLiteral("profile")).toObject();
                const auto title = profile.value(QStringLiteral("window_title"));
                if (title.isString()) setWindowTitle(title.toString());
            }
            if (command.contains(QStringLiteral("ReplaceSurface"))) {
                replaceSurface(command.value(QStringLiteral("ReplaceSurface"))
                                   .toObject()
                                   .value(QStringLiteral("surface"))
                                   .toObject());
            }
            // TODO(private#1073): route the remaining display commands.
        }
        snapshotIfAsked();
    }

private:
    void replaceSurface(const QJsonObject &surface) {
        auto *next = new PresentationSurface(surface);
        connect(next, &PresentationSurface::interactionReady, this,
                [this](const QString &surfaceId, const QString &interactionId) {
                    emitEvent(QJsonObject{
                        {QStringLiteral("SurfaceActivated"),
                         QJsonObject{{QStringLiteral("surface_id"), surfaceId}}}});
                    emitEvent(QJsonObject{
                        {QStringLiteral("ActionActivated"),
                         QJsonObject{{QStringLiteral("surface_id"), surfaceId},
                                     {QStringLiteral("interaction_id"), interactionId}}}});
                });
        connect(next, &PresentationSurface::valueReady, this,
                [this](const QString &surfaceId, const QString &bindingId,
                       const QJsonValue &value) {
                    emitEvent(QJsonObject{
                        {QStringLiteral("InputChanged"),
                         QJsonObject{{QStringLiteral("surface_id"), surfaceId},
                                     {QStringLiteral("binding_id"), bindingId},
                                     {QStringLiteral("value"), value}}}});
                });
        connect(next, &PresentationSurface::submitReady, this,
                [this](const QString &surfaceId, const QString &bindingId) {
                    emitEvent(QJsonObject{
                        {QStringLiteral("InputSubmitted"),
                         QJsonObject{{QStringLiteral("surface_id"), surfaceId},
                                     {QStringLiteral("binding_id"), bindingId}}}});
                });
        connect(next, &PresentationSurface::focusEndReady, this,
                [this](const QString &surfaceId, const QString &bindingId) {
                    emitEvent(QJsonObject{
                        {QStringLiteral("InputFocusEnded"),
                         QJsonObject{{QStringLiteral("surface_id"), surfaceId},
                                     {QStringLiteral("binding_id"), bindingId}}}});
                });
        QWidget *previous = m_scroll->takeWidget();
        m_scroll->setWidget(next);
        delete previous;
    }

    // QT_DISPLAY_SNAPSHOT=<png> writes the window after each batch, so a
    // headless run (QT_QPA_PLATFORM=offscreen) leaves evidence of what
    // rendered.
    void snapshotIfAsked() {
        const QByteArray path = qgetenv("QT_DISPLAY_SNAPSHOT");
        if (path.isEmpty()) return;
        QCoreApplication::processEvents();
        grab().save(QString::fromUtf8(path));
    }

    void emitEvent(const QJsonObject &event) {
        if (!m_onEvent) return;
        const QByteArray json = QJsonDocument(event).toJson(QJsonDocument::Compact);
        m_onEvent(json.constData(), m_userData);
    }

    QtDisplayEventCallback m_onEvent;
    void *m_userData;
    QScrollArea *m_scroll;
};

QPointer<DisplayHost> g_host;
std::atomic<bool> g_running{false};

}  // namespace

extern "C" int qt_display_start(int argc, char **argv,
                                QtDisplayEventCallback on_event, void *user_data) {
    QApplication app(argc, argv);
    ThemeManager::registerBrandFonts();
    app.setFont(ThemeManager::uiFont());
    ThemeManager::applyDefaultTheme();
    DisplayHost host(on_event, user_data);
    g_host = &host;
    host.show();
    g_running = true;
    const int rc = app.exec();
    g_running = false;
    g_host = nullptr;
    return rc;
}

extern "C" int qt_display_render(const char *commands_json, size_t len) {
    if (!g_running || !g_host) return -1;
    const QJsonDocument doc =
        QJsonDocument::fromJson(QByteArray(commands_json, static_cast<int>(len)));
    if (!doc.isArray()) return -1;
    const QJsonArray commands = doc.array();
    QMetaObject::invokeMethod(
        g_host, [commands] { if (g_host) g_host->render(commands); },
        Qt::QueuedConnection);
    return 0;
}

extern "C" void qt_display_stop(void) {
    if (!g_running) return;
    QMetaObject::invokeMethod(qApp, [] { qApp->quit(); }, Qt::QueuedConnection);
}
