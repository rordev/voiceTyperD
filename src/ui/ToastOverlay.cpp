#include "ui/ToastOverlay.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

#ifdef Q_OS_MAC
#include "ui/MacWindowUtils.h"
#endif

namespace vt {

namespace {
constexpr int kWidth        = 270;  // minimum: the overlay's width
constexpr int kHeight       = 40;
constexpr int kMaxWidth     = 480;  // beyond this, text wraps
constexpr int kPadX         = 16;
constexpr int kPadY         = 10;
constexpr int kMargin       = 24;   // matches OverlayWindow
constexpr int kOverlayH     = 64;   // recording overlay height
constexpr int kGap          = 8;    // gap between toast and overlay slot
constexpr int kDurationMs   = 2000;
} // namespace

ToastOverlay::ToastOverlay(QWidget* parent) : QWidget(parent) {
    // See OverlayWindow.cpp for why Qt::BypassWindowManagerHint is dropped
    // on macOS only, keeping Windows/X11 behavior unchanged.
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool |
                            Qt::WindowDoesNotAcceptFocus;
#ifndef Q_OS_MAC
    flags |= Qt::BypassWindowManagerHint;
#endif
    setWindowFlags(flags);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    resize(kWidth, kHeight);

#ifdef Q_OS_MAC
    mac::preventPanelHideOnDeactivate(this);
#endif

    hideTimer_.setSingleShot(true);
    hideTimer_.setInterval(kDurationMs);
    connect(&hideTimer_, &QTimer::timeout, this, &QWidget::hide);
}

void ToastOverlay::showToast(const QString& text) {
    text_ = text;
    fitToText();
    positionAboveOverlay();
    update();
    show();
    raise();
    hideTimer_.start(); // restarts automatically if already running
}

QFont ToastOverlay::textFont() const {
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() + 0.5);
    return f;
}

void ToastOverlay::fitToText() {
    int maxWidth = kMaxWidth;
    if (const QScreen* screen = QGuiApplication::primaryScreen())
        maxWidth = qMin(maxWidth, screen->availableGeometry().width() - 2 * kMargin);
    const QRect text = QFontMetrics(textFont()).boundingRect(
        QRect(0, 0, maxWidth - 2 * kPadX, 0),
        Qt::AlignCenter | Qt::TextWordWrap, text_);
    resize(qMax(kWidth, text.width() + 2 * kPadX),
           qMax(kHeight, text.height() + 2 * kPadY));
}

void ToastOverlay::positionAboveOverlay() {
    const QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect avail = screen->availableGeometry();
    // Sit just above where the recording overlay lives so they never overlap.
    const int bottomOffset = kMargin + kOverlayH + kGap;
    move(avail.right() - width() - kMargin,
         avail.bottom() - height() - bottomOffset);
}

void ToastOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Same dark-frosted look as the recording overlay.
    QPainterPath bg;
    bg.addRoundedRect(rect().adjusted(0, 0, -1, -1), 12, 12);
    p.fillPath(bg, QColor(28, 28, 30, 225));

    p.setPen(QColor(240, 240, 240));
    p.setFont(textFont());
    p.drawText(rect().adjusted(kPadX, 0, -kPadX, 0),
               Qt::AlignCenter | Qt::TextWordWrap, text_);
}

} // namespace vt
