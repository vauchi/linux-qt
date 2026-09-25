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

// Core omits `size` on the wire when it is `None` (avatars) — only the
// onboarding mark carries it. Mirrors that: every other fixture in this
// file has no `size` key at all, matching `#[serde(skip_serializing_if =
// "Option::is_none")]` on the Rust side.
QJsonObject sizedImageNode(const QJsonValue &fallback, const QString &shape,
                          int size, const QString &name) {
    QJsonObject node = imageNode(fallback, shape, name).value("Image").toObject();
    node.insert(QStringLiteral("size"), size);
    return {{"Image", node}};
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

    // vauchi::fittedImagePixmap() is the `size`-hint counterpart of
    // avatarPixmap(): Core's onboarding mark (core!5711401e) fits into its
    // square with its aspect kept, never cropped like an avatar.
    {
        // A non-square source is fit, not cropped: the constrained
        // dimension scales to fill the square and the other letterboxes
        // with transparency instead of being cut off.
        QPixmap tall(10, 40);
        tall.fill(Qt::red);
        const QPixmap fitted = vauchi::fittedImagePixmap(
            tall, QString(), 64, QColor(Qt::white));
        assert(fitted.size() == QSize(64, 64));
        assert(alphaAt(fitted, 0, 32) == 0
               && "a fit image must letterbox rather than crop to fill");
        assert(alphaAt(fitted, 32, 32) == 255
               && "a fit image must still fill its centre");
    }
    {
        // No image data falls back to centred text on a transparent
        // canvas — the sized counterpart of avatarPixmap's filled ground.
        const QPixmap fitted = vauchi::fittedImagePixmap(
            QPixmap(), QStringLiteral("BS"), 64, QColor(Qt::white));
        assert(fitted.size() == QSize(64, 64));
        assert(alphaAt(fitted, 0, 0) == 0
               && "a sized fallback must not paint a filled ground");
    }
    {
        // A non-positive size is a no-op, matching avatarPixmap.
        assert(vauchi::fittedImagePixmap(QPixmap(), QStringLiteral("BS"), 0,
                                         QColor(Qt::white))
                   .isNull());
    }

    {
        // A `size` hint sizes the square from Core, not
        // tokens.minimum_target_size — and never wider than that square,
        // so no fixed minimum is set the way the avatar case sets one.
        PresentationSurface renderer(surfaceWith(
            sizedImageNode(QStringLiteral("VM"), QStringLiteral("natural"), 88,
                          QStringLiteral("Mark")),
            QJsonObject{{"minimum_target_size", 44}}));
        auto *mark = avatarNamed(renderer, QStringLiteral("Mark"));
        assert(mark != nullptr);
        assert(mark->maximumSize() == QSize(88, 88)
               && "a `size` hint must size the square from Core, not tokens");

        const QPixmap pixmap = mark->pixmap(Qt::ReturnByValue);
        assert(pixmap.size() == QSize(88, 88)
               && "the fallback must be sized to the same square as a picture");
    }

    return 0;
}
