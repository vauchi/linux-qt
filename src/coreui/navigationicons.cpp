// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "navigationicons.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QImageReader>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <utility>

namespace vauchi {
namespace {

/// Shown when Core names a token this build has not learned, so a new
/// destination arrives with a marker rather than a hole in the row. An
/// apps-grid icon reads as "some section of this app" and stays truthful;
/// reusing a concrete icon such as the house would put a confident lie next
/// to a label that says something else.
const QStringList &fallbackNames() {
    static const QStringList names{
        QStringLiteral("applications-other"),
        QStringLiteral("applications-utilities"),
        QStringLiteral("application-x-executable"),
    };
    return names;
}

/// Core names its tokens after the SF Symbols core set, which shares no
/// namespace with the freedesktop icon spec, so the translation has to be an
/// explicit table. Each entry lists Breeze's name first and more widely
/// installed spec names after it, so a non-KDE theme still resolves.
///
/// Deliberately *not* the `-symbolic` variants: those are thin monochrome
/// outlines that lose definition at the size a navigation row uses and are
/// the first thing to disappear for low-vision users.
const QHash<QString, QStringList> &namesByToken() {
    static const QHash<QString, QStringList> table{
        {QStringLiteral("person.crop.rectangle"),
         {QStringLiteral("user-identity"), QStringLiteral("avatar-default"),
          QStringLiteral("user-info")}},
        {QStringLiteral("person.2"),
         {QStringLiteral("system-users"), QStringLiteral("user-group"),
          QStringLiteral("group")}},
        {QStringLiteral("qrcode"),
         {QStringLiteral("view-barcode-qr"), QStringLiteral("view-barcode")}},
        {QStringLiteral("folder"),
         {QStringLiteral("folder"), QStringLiteral("folder-documents")}},
        {QStringLiteral("tag"),
         {QStringLiteral("tag"), QStringLiteral("bookmarks")}},
        {QStringLiteral("mappin.and.ellipse"),
         {QStringLiteral("mark-location"), QStringLiteral("gps"),
          QStringLiteral("applications-geography")}},
        {QStringLiteral("person.badge.plus"),
         {QStringLiteral("list-add-user"), QStringLiteral("contact-new"),
          QStringLiteral("user-others")}},
        {QStringLiteral("gearshape"),
         {QStringLiteral("configure"), QStringLiteral("settings-configure"),
          QStringLiteral("preferences-system")}},
        {QStringLiteral("questionmark.circle"),
         {QStringLiteral("help-contents"), QStringLiteral("help-browser"),
          QStringLiteral("help-about")}},
        {QStringLiteral("key.horizontal"),
         {QStringLiteral("dialog-password"),
          QStringLiteral("application-pgp-keys"),
          QStringLiteral("password-show")}},
        {QStringLiteral("laptopcomputer"),
         {QStringLiteral("computer-laptop"), QStringLiteral("computer"),
          QStringLiteral("preferences-devices")}},
        // Vauchi's backup is guardian-held and on-device. A cloud icon would
        // misrepresent the threat model to the reader least able to check it,
        // so this names local storage all the way down the chain.
        {QStringLiteral("externaldrive"),
         {QStringLiteral("drive-harddisk"),
          QStringLiteral("drive-removable-media"),
          QStringLiteral("media-floppy")}},
        {QStringLiteral("hand.raised"),
         {QStringLiteral("security-high"),
          QStringLiteral("preferences-desktop-security"),
          QStringLiteral("system-lock-screen")}},
        {QStringLiteral("bubble.left.and.bubble.right"),
         {QStringLiteral("dialog-messages"),
          QStringLiteral("internet-group-chat"),
          QStringLiteral("mail-message")}},
        {QStringLiteral("list.bullet.rectangle"),
         {QStringLiteral("view-list-details"),
          QStringLiteral("format-list-unordered"),
          QStringLiteral("view-list-text")}},
        {QStringLiteral("house"),
         {QStringLiteral("go-home"), QStringLiteral("user-home")}},
    };
    return table;
}

/// `pictogram.<group>.<name>` names one of Vauchi's own pictograms, bundled
/// in resources.qrc. The mapping is a pure rename so a new pictogram needs
/// only its SVG and a qrc entry, never a table entry here.
QString pictogramResourcePath(const QString &token) {
    static const QString prefix = QStringLiteral("pictogram.");
    if (!token.startsWith(prefix)) {
        return {};
    }
    const QStringList segments =
        token.mid(prefix.size()).split(QLatin1Char('.'));
    const auto wellFormed = [](const QString &segment) {
        if (segment.isEmpty()) {
            return false;
        }
        for (const QChar c : segment) {
            const char16_t u = c.unicode();
            const bool allowed = (u >= 'a' && u <= 'z')
                                 || (u >= '0' && u <= '9') || u == '_';
            if (!allowed) {
                return false;
            }
        }
        return true;
    };
    if (segments.size() != 2 || !wellFormed(segments[0])
        || !wellFormed(segments[1])) {
        return {};
    }
    return QStringLiteral(":/pictograms/%1/%2.svg")
        .arg(segments[0], segments[1]);
}

/// Draws a bundled pictogram in the palette's text colour at paint time, so
/// it follows the theme (and selected / disabled states) like the label next
/// to it. The SVG is rasterised by Qt's SVG image plugin and only its alpha
/// is kept: whatever colour it was authored in is replaced.
class PictogramIconEngine : public QIconEngine {
public:
    explicit PictogramIconEngine(QString path) : path_(std::move(path)) {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
               QIcon::State state) override {
        const qreal ratio = painter->device()->devicePixelRatioF();
        QPixmap tinted = pixmap(rect.size() * ratio, mode, state);
        tinted.setDevicePixelRatio(ratio);
        const QSize drawn = tinted.deviceIndependentSize().toSize();
        painter->drawPixmap(
            rect.x() + (rect.width() - drawn.width()) / 2,
            rect.y() + (rect.height() - drawn.height()) / 2, tinted);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode,
                   QIcon::State /*state*/) override {
        QImageReader reader(path_);
        reader.setScaledSize(reader.size().scaled(size, Qt::KeepAspectRatio));
        QImage image =
            reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
        if (image.isNull()) {
            return {};
        }
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), textColor(mode));
        painter.end();
        return QPixmap::fromImage(image);
    }

    QIconEngine *clone() const override {
        return new PictogramIconEngine(path_);
    }

    QString key() const override {
        return QStringLiteral("vauchi-pictogram");
    }

private:
    static QColor textColor(QIcon::Mode mode) {
        const QPalette palette = QApplication::palette();
        switch (mode) {
        case QIcon::Disabled:
            return palette.color(QPalette::Disabled, QPalette::WindowText);
        case QIcon::Selected:
            return palette.color(QPalette::Active, QPalette::HighlightedText);
        default:
            return palette.color(QPalette::Active, QPalette::WindowText);
        }
    }

    QString path_;
};

/// A pictogram this build bundles and can rasterise; otherwise a null icon so
/// the caller keeps walking the chain.
QIcon pictogramIcon(const QString &path) {
    if (!QFile::exists(path) || !QImageReader(path).canRead()) {
        return {};
    }
    return QIcon(new PictogramIconEngine(path));
}

} // namespace

QStringList navigationIconNames(const QString &token) {
    const QString trimmed = token.trimmed();
    const QString pictogram = pictogramResourcePath(trimmed);
    if (!pictogram.isEmpty()) {
        return QStringList{pictogram} + fallbackNames();
    }
    const auto found = namesByToken().constFind(trimmed);
    return found == namesByToken().cend() ? fallbackNames() : found.value();
}

QIcon navigationIcon(const QString &token) {
    QStringList candidates = navigationIconNames(token);
    candidates.append(fallbackNames());
    for (const QString &name : std::as_const(candidates)) {
        QIcon icon = name.startsWith(QStringLiteral(":/"))
                         ? pictogramIcon(name)
                         : QIcon::fromTheme(name);
        if (!icon.isNull()) {
            return icon;
        }
    }
    // No icon theme is installed at all — a minimal container, or a session
    // built without one. The style always has this, so the label keeps a
    // glyph instead of an empty gap.
    return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
}

} // namespace vauchi
