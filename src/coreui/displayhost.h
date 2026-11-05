// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "qt_display.h"
#include "presentationstate.h"
#include <QMainWindow>
#include <QPointer>

class NavigationSidebar;
class PresentationSurface;
class QSplitter;
class ApplicationMenuBar;
class FilePicker;
class QFileDialog;
class QKeyEvent;

class QtDisplayHost : public QMainWindow {
public:
    QtDisplayHost(QtDisplayEventCallback onEvent, void *userData);
    void render(const QJsonArray &commands);
    void reportCapabilities();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void renderPresentation();
    void resetPresentation();
    void resetApplicationMenu();
    void resetFileEffects();
    PresentationSurface *createSurface(const QJsonObject &surface,
                                      const QJsonObject &bar, bool sidebar);
    void activate(const QString &surfaceId, const QString &interactionId);
    void emitEvent(const QJsonObject &event);
    void snapshotIfAsked();
    void applyOverlay(const QJsonObject &command);
    void closeOverlay();
    bool applyNativeEffect(const QJsonObject &command);
    bool pickLegacyFile(const QJsonValue &command);
    void exportFile(const QJsonObject &command);
    void applyApplicationMenu(const QJsonObject &payload);
    void overlayDismissed(QWidget *widget, const QString &surfaceId,
                          const QString &kind);

    QtDisplayEventCallback m_onEvent;
    void *m_userData;
    PresentationState m_state;
    QSplitter *m_row;
    QSplitter *m_panes;
    QPointer<NavigationSidebar> m_sidebar;
    QJsonObject m_navigation;
    QString m_navigationSurface;
    QHash<QString, QPointer<PresentationSurface>> m_surfaces;
    QPointer<QWidget> m_overlay;
    QString m_overlaySurface;
    QString m_overlayKind;
    ApplicationMenuBar *m_applicationMenu;
    QString m_menuSurface;
    qint64 m_menuRevision = -1;
    quint64 m_generation = 0;
    FilePicker *m_filePicker = nullptr;
    FilePicker *m_legacyPicker = nullptr;
    bool m_legacyImage = false;
    QPointer<QFileDialog> m_exportDialog;
};
