// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

// A replacement at the revision already shown keeps the surface's
// structure (contract `SurfaceSpec::revision`), so only nodes whose content
// changed are rebuilt, in their slot, and a line edit the user is typing in
// keeps its widget when Core echoes the text it already shows: the shell
// owns the focused input (vauchi/private#1062). A rebuild would lose the
// caret, selection, input-method composition and the screen reader's place.

#include "presentationsurface.h"

#include <QApplication>
#include <QBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>

namespace {

bool sameFrame(const QJsonObject &shown, const QJsonObject &next) {
    for (const char *key : {"surface_id", "revision", "layout", "tokens"}) {
        const QString name = QString::fromLatin1(key);
        if (shown.value(name) != next.value(name)) {
            return false;
        }
    }
    const auto hasSubtitle = [](const QJsonObject &surface) {
        return !surface.value(QStringLiteral("subtitle")).toString().isEmpty();
    };
    return hasSubtitle(shown) == hasSubtitle(next)
           && shown.value(QStringLiteral("nodes")).toArray().size()
                  == next.value(QStringLiteral("nodes")).toArray().size();
}

} // namespace

bool PresentationSurface::update(const QJsonObject &surface,
                                 const QJsonObject &contextBar,
                                 bool sidebarShown) {
    if (!sameFrame(m_surface, surface) || contextBar != m_contextBar
        || sidebarShown != m_sidebarShown || m_contentLayout == nullptr) {
        return false;
    }
    m_title->setText(surface.value(QStringLiteral("title")).toString());
    if (m_subtitle != nullptr) {
        m_subtitle->setText(surface.value(QStringLiteral("subtitle")).toString());
    }
    setAccessibleName(
        surface.value(QStringLiteral("accessibility_label")).toString());
    const QJsonArray before = m_surface.value(QStringLiteral("nodes")).toArray();
    const QJsonArray after = surface.value(QStringLiteral("nodes")).toArray();
    for (qsizetype index = 0; index < after.size(); ++index) {
        const QJsonObject previous = before.at(index).toObject();
        const QJsonObject next = after.at(index).toObject();
        QWidget *old = m_nodeWidgets.at(index);
        if (previous == next || keepsFocusedTypedValue(previous, next, old)) {
            continue;
        }
        QWidget *replacement = renderNode(next);
        delete m_contentLayout->replaceWidget(old, replacement);
        old->deleteLater();
        m_nodeWidgets[index] = replacement;
    }
    m_surface = surface;
    return true;
}

bool PresentationSurface::keepsFocusedTypedValue(const QJsonObject &previous,
                                                 const QJsonObject &next,
                                                 const QWidget *widget) const {
    const QString key = QStringLiteral("Input");
    if (!previous.contains(key) || !next.contains(key)) {
        return false;
    }
    QJsonObject before = previous.value(key).toObject();
    const QJsonObject after = next.value(key).toObject();
    const QString value = after.value(QStringLiteral("value")).toString();
    const QWidget *focused = QApplication::focusWidget();
    if (focused == nullptr || !(focused == widget || widget->isAncestorOf(focused))) {
        return false;
    }
    const auto *line = widget->findChild<QLineEdit *>();
    if (line == nullptr || line->text() != value) {
        return false;
    }
    before.insert(QStringLiteral("value"), value);
    return before == after;
}
