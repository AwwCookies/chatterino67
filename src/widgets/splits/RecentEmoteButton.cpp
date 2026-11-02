// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/RecentEmoteButton.hpp"

#include "messages/Emote.hpp"
#include "messages/Message.hpp"
#include "messages/MessageElement.hpp"

#include <QDateTime>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <unordered_map>

namespace chatterino {

namespace {

/// Image scale requested from the emote set
constexpr float EMOTE_IMAGE_SCALE = 1.0F;
/// Padding around the emote
constexpr int PADDING = 2;
/// Space between the emote and the count
constexpr int COUNT_SPACING = 4;

}  // namespace

RecentEmoteButton::RecentEmoteButton(QWidget *parent)
    : QWidget(parent)
    , expireTimer_(new QTimer(this))
{
    this->setFocusPolicy(Qt::NoFocus);
    this->setCursor(Qt::PointingHandCursor);
    this->setVisible(false);

    this->expireTimer_->setInterval(200);
    QObject::connect(this->expireTimer_, &QTimer::timeout, this, [this] {
        this->refresh();
    });
}

void RecentEmoteButton::setChannel(const ChannelPtr &channel)
{
    this->messageConnection_ = pajlada::Signals::ScopedConnection{};
    this->entries_.clear();
    this->clearCurrent();

    if (channel == nullptr)
    {
        return;
    }

    this->messageConnection_ = channel->messageAppended.connect(
        [this](const MessagePtr &message, auto /*flags*/) {
            this->onMessage(message);
        });
}

void RecentEmoteButton::onMessage(const MessagePtr &message)
{
    if (message == nullptr)
    {
        return;
    }

    for (const auto &element : message->elements)
    {
        auto *emoteElement = dynamic_cast<EmoteElement *>(element.get());
        if (emoteElement == nullptr)
        {
            continue;
        }

        auto emote = emoteElement->getEmote();
        if (emote == nullptr || emote->zeroWidth)
        {
            continue;
        }

        this->addEntry(emote->name.string,
                       emote->images.getImage(EMOTE_IMAGE_SCALE));
    }
}

void RecentEmoteButton::addEntry(const QString &name, const ImagePtr &image)
{
    this->entries_.push_back(
        {QDateTime::currentMSecsSinceEpoch(), name, image});
    this->refresh();
}

void RecentEmoteButton::refresh()
{
    const auto cutoff = QDateTime::currentMSecsSinceEpoch() - WINDOW_MS;
    while (!this->entries_.empty() && this->entries_.front().time < cutoff)
    {
        this->entries_.pop_front();
    }

    if (this->entries_.empty())
    {
        this->clearCurrent();
        return;
    }

    std::unordered_map<QString, int> counts;
    for (const auto &entry : this->entries_)
    {
        counts[entry.name]++;
    }

    // Most used emote; ties go to the one that was used most recently
    const Entry *best = nullptr;
    for (const auto &entry : this->entries_)
    {
        const auto count = counts[entry.name];
        if (best == nullptr || count >= counts[best->name])
        {
            best = &entry;
        }
    }

    if (best == nullptr || counts[best->name] < MIN_COUNT)
    {
        this->clearCurrent();
        return;
    }

    const auto count = counts[best->name];
    const auto changed = best->name != this->currentName_ ||
                         count != this->currentCount_;

    this->currentName_ = best->name;
    this->currentImage_ = best->image;
    this->currentCount_ = count;

    this->setToolTip(QStringLiteral("Click to send %1 (%2x in the last %3s)")
                         .arg(this->currentName_)
                         .arg(this->currentCount_)
                         .arg(WINDOW_MS / 1000));

    if (!this->isVisible())
    {
        this->setVisible(true);
    }
    if (!this->expireTimer_->isActive())
    {
        this->expireTimer_->start();
    }

    if (changed)
    {
        this->updateGeometry();
    }
    this->update();
}

void RecentEmoteButton::clearCurrent()
{
    this->expireTimer_->stop();

    if (this->currentName_.isEmpty() && !this->isVisible())
    {
        return;
    }

    this->currentName_.clear();
    this->currentImage_.reset();
    this->currentCount_ = 0;
    this->setVisible(false);
    this->updateGeometry();
}

int RecentEmoteButton::emoteHeight() const
{
    return int(QFontMetrics(this->font()).height() * 1.5);
}

QSize RecentEmoteButton::sizeHint() const
{
    const auto height = this->emoteHeight();

    int width = height;
    if (this->currentCount_ > 0)
    {
        width += COUNT_SPACING +
                 QFontMetrics(this->font())
                     .horizontalAdvance(u'x' +
                                        QString::number(this->currentCount_));
    }

    return {2 * PADDING + width, 2 * PADDING + height};
}

void RecentEmoteButton::paintEvent(QPaintEvent * /*event*/)
{
    if (this->currentName_.isEmpty())
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const auto height = this->emoteHeight();
    const QRect emoteRect(PADDING, PADDING, height, height);

    if (this->currentImage_ != nullptr)
    {
        if (auto pixmap = this->currentImage_->pixmapOrLoad(); pixmap)
        {
            painter.drawPixmap(
                emoteRect, pixmap->scaled(emoteRect.size(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
        }
        else
        {
            // still loading - show the name instead
            painter.drawText(emoteRect, Qt::AlignCenter, this->currentName_);
        }
    }

    painter.setPen(this->palette().color(QPalette::WindowText));
    const QRect countRect(emoteRect.right() + COUNT_SPACING, PADDING,
                          this->width() - emoteRect.right() - COUNT_SPACING,
                          height);
    painter.drawText(countRect, Qt::AlignLeft | Qt::AlignVCenter,
                     u'x' + QString::number(this->currentCount_));
}

void RecentEmoteButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || this->currentName_.isEmpty())
    {
        event->ignore();
        return;
    }

    event->accept();
    this->emoteClicked(this->currentName_);
}

}  // namespace chatterino
