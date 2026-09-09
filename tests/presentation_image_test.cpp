// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

// Core sends an avatar as `Image { data, fallback_text, shape }`. This
// shell read neither `shape` nor gave the initials a ground: a round
// avatar and a natural-cornered diagram were drawn identically, and a
// contact with no picture got a bare letter on the window background —
// the same defect `ios!651` fixed on Apple.
//
// Assertions are on the label's geometry and its stylesheet because that
// is how Qt expresses a rounded, filled box; there is no class system to
// interrogate the way GTK has one.

#include "coreui/presentationsurface.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <cassert>

namespace {

QJsonObject accessibility(const QString &label) {
    return {{"label", label}, {"description", QJsonValue::Null}};
}

QJsonObject imageNode(const QJsonValue &fallback, const QString &shape,
                      const QString &name) {
    return {
        {"Image",
         QJsonObject{
             {"id", QJsonValue::Null},
             {"data", QJsonValue::Null},
             {"fallback_text", fallback},
             {"shape", shape},
             {"brightness", 1.0},
             {"activation", QJsonValue::Null},
             {"accessibility", accessibility(name)},
         }},
    };
}

QJsonObject surfaceWith(const QJsonObject &node) {
    return {
        {"surface_id", "profile"},
        {"revision", 1},
        {"title", "Profile"},
        {"layout", "single"},
        {"nodes", QJsonArray{node}},
    };
}

QLabel *avatarNamed(PresentationSurface &renderer, const QString &name) {
    for (auto *label : renderer.findChildren<QLabel *>()) {
        if (label->accessibleName() == name) {
            return label;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    {
        PresentationSurface renderer(surfaceWith(
            imageNode(QStringLiteral("BS"), QStringLiteral("circle"),
                      QStringLiteral("Avatar for Bob"))));
        auto *avatar = avatarNamed(renderer, QStringLiteral("Avatar for Bob"));
        assert(avatar != nullptr && "no widget carries the avatar's name");

        // A circle clipped from a box that is not square is a stadium.
        assert(avatar->minimumWidth() == avatar->minimumHeight()
               && "a circular avatar must be square");
        assert(avatar->minimumWidth() >= 96
               && "the avatar is too small to read initials in");
        assert(avatar->styleSheet().contains(QStringLiteral("border-radius"))
               && "the initials have no rounded ground to sit on");
        assert(avatar->styleSheet().contains(QStringLiteral("background"))
               && "the initials have no fill, so they read as a stray letter");
    }

    {
        PresentationSurface renderer(surfaceWith(
            imageNode(QStringLiteral("BS"), QStringLiteral("natural"),
                      QStringLiteral("Diagram"))));
        auto *avatar = avatarNamed(renderer, QStringLiteral("Diagram"));
        assert(avatar != nullptr);

        // Core asks a natural image to keep its corners. Rendering it with
        // the circle's radius would round away what it distinguishes.
        assert(!avatar->styleSheet().contains(QStringLiteral("border-radius: 48px"))
               && "a natural-shaped image was rounded like an avatar");
        assert(avatar->styleSheet().contains(QStringLiteral("background"))
               && "a natural fallback still needs a ground");
    }

    {
        // Guard for the two blocks above: with nothing to show, nothing is
        // drawn. Without this, a renderer that emitted a filled square for
        // every input would satisfy every assertion here.
        PresentationSurface renderer(surfaceWith(imageNode(
            QJsonValue::Null, QStringLiteral("circle"), QStringLiteral("Empty"))));
        auto *avatar = avatarNamed(renderer, QStringLiteral("Empty"));
        assert((avatar == nullptr || avatar->minimumWidth() == 0)
               && "an empty avatar still painted a filled box");
    }

    return 0;
}
