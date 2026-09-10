// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

// Core sends an avatar as `Image { data, fallback_text, shape }`. This
// shell read neither `shape` nor gave the initials a ground: a round
// avatar and a natural-cornered diagram were drawn identically, and a
// contact with no picture got a bare letter on the window background —
// the same defect `ios!651` fixed on Apple.
//
// Assertions read alpha values off the rendered QPixmap: vauchi::avatarPixmap()
// paints the fallback and masks image data directly, so the shape is a
// property of the pixmap's pixels, not a stylesheet.

#include "coreui/presentationsurface.h"
#include "coreui/presentationsurface_avatar.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <cassert>

namespace {

int alphaAt(const QPixmap &pixmap, int x, int y) {
    return qAlpha(pixmap.toImage().pixel(x, y));
}

QPixmap solidPixmap(int side, const QColor &color) {
    QPixmap pixmap(side, side);
    pixmap.fill(color);
    return pixmap;
}

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

QJsonObject surfaceWith(const QJsonObject &node,
                        const QJsonObject &tokens = QJsonObject{}) {
    return {
        {"surface_id", "profile"},
        {"revision", 1},
        {"title", "Profile"},
        {"layout", "single"},
        {"tokens", tokens},
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

    // vauchi::avatarPixmap() is the mask/fallback helper F1 and F3 need:
    // it must paint a circular fallback (never a bare, unstyled label) and
    // mask real image data to the shape Core asked for.
    {
        // A circular fallback: the corners of the canvas fall outside the
        // ellipse (transparent); the centre is inside the fill (opaque).
        const QPixmap avatar = vauchi::avatarPixmap(
            QPixmap(), QStringLiteral("BS"), /*circular=*/true, 64, 8,
            QColor(Qt::blue), QColor(Qt::white));
        assert(!avatar.isNull());
        assert(avatar.size() == QSize(64, 64));
        assert(alphaAt(avatar, 0, 0) == 0
               && "a circular fallback must not fill the canvas corner");
        assert(alphaAt(avatar, 32, 32) == 255
               && "a circular fallback must fill its centre");
    }
    {
        // A rectangular fallback (cornerRadius 0) fills the corner too —
        // this is what distinguishes "natural" from "circle".
        const QPixmap avatar = vauchi::avatarPixmap(
            QPixmap(), QStringLiteral("BS"), /*circular=*/false, 64, 0,
            QColor(Qt::blue), QColor(Qt::white));
        assert(alphaAt(avatar, 0, 0) == 255
               && "an unrounded rect fallback must fill its own corner");
    }
    {
        // Real image data must be masked the same way — this is the F3 gap:
        // `setPixmap` previously ignored `shape: Circle` entirely.
        const QPixmap avatar = vauchi::avatarPixmap(
            solidPixmap(32, QColor(Qt::red)), QString(), /*circular=*/true,
            64, 8, QColor(Qt::blue), QColor(Qt::white));
        assert(alphaAt(avatar, 0, 0) == 0
               && "circular image data must be masked at the corner");
        assert(alphaAt(avatar, 32, 32) == 255
               && "circular image data must still fill its centre");
    }
    {
        // A non-positive diameter is a no-op, not a crash.
        assert(vauchi::avatarPixmap(QPixmap(), QStringLiteral("BS"), true, 0,
                                    8, QColor(Qt::blue), QColor(Qt::white))
                   .isNull());
    }

    {
        PresentationSurface renderer(surfaceWith(
            imageNode(QStringLiteral("BS"), QStringLiteral("circle"),
                      QStringLiteral("Avatar for Bob")),
            QJsonObject{{"minimum_target_size", 64}}));
        auto *avatar = avatarNamed(renderer, QStringLiteral("Avatar for Bob"));
        assert(avatar != nullptr && "no widget carries the avatar's name");

        const QPixmap pixmap = avatar->pixmap(Qt::ReturnByValue);
        assert(!pixmap.isNull()
               && "the initials have no fill, so they read as a stray letter");
        assert(pixmap.size() == QSize(64, 64)
               && "the avatar must be sized from tokens.minimum_target_size");
        assert(alphaAt(pixmap, 0, 0) == 0
               && "a circular avatar must not fill the pixmap corner");
        assert(alphaAt(pixmap, 32, 32) == 255
               && "the initials have no ground to sit on");
    }

    {
        PresentationSurface renderer(surfaceWith(
            imageNode(QStringLiteral("BS"), QStringLiteral("natural"),
                      QStringLiteral("Diagram")),
            QJsonObject{{"minimum_target_size", 64}, {"corner_radius", 0}}));
        auto *avatar = avatarNamed(renderer, QStringLiteral("Diagram"));
        assert(avatar != nullptr);

        const QPixmap pixmap = avatar->pixmap(Qt::ReturnByValue);
        assert(!pixmap.isNull() && "a natural fallback still needs a ground");
        // Core asks a natural image to keep its corners. Rendering it with
        // the circle's mask would round away what it distinguishes.
        assert(alphaAt(pixmap, 0, 0) == 255
               && "a natural-shaped image was rounded like an avatar");
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
