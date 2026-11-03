// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/RecentEmoteButton.hpp"

#include "Application.hpp"
#include "messages/Message.hpp"
#include "messages/MessageElement.hpp"
#include "singletons/Theme.hpp"

#include <QDateTime>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <unordered_map>
#include <unordered_set>

namespace chatterino {

namespace {

/// Scale requested from the emote set - the highest resolution available
constexpr float EMOTE_IMAGE_SCALE = 3.0F;
constexpr int PADDING = 2;
constexpr int COUNT_SPACING = 4;
constexpr int CORNER_RADIUS = 3;

}  // namespace

RecentEmoteButton::RecentEmoteButton(QWidget *parent)
    : QWidget(parent)
    , expireTimer_(new QTimer(this))
{
    this->setFocusPolicy(Qt::NoFocus);
    this->setCursor(Qt::PointingHandCursor);
    this->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    this->setVisible(false);

    this->expireTimer_->setInterval(150);
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

QString RecentEmoteButton::currentEmoteName() const
{
    return this->currentName_;
}

void RecentEmoteButton::onMessage(const MessagePtr &message)
{
    if (message == nullptr || message->loginName.isEmpty())
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

        this->addEntry(emote->name.string, message->loginName.toLower(),
                       emote);
    }
}

void RecentEmoteButton::addEntry(const QString &emoteName, const QString &user,
                                 const EmotePtr &emote)
{
    // start downloading the high resolution variant right away
    emote->images.getImage(EMOTE_IMAGE_SCALE)->load();

    this->entries_.push_back(
        {QDateTime::currentMSecsSinceEpoch(), emoteName, user, emote});
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

    // count the *users* per emote - spamming an emote doesn't count twice
    std::unordered_map<QString, std::unordered_set<QString>> usersPerEmote;
    for (const auto &entry : this->entries_)
    {
        usersPerEmote[entry.emoteName].insert(entry.user);
    }

    // most used emote; ties go to the one that was used most recently
    const Entry *best = nullptr;
    for (const auto &entry : this->entries_)
    {
        if (best == nullptr ||
            usersPerEmote[entry.emoteName].size() >=
                usersPerEmote[best->emoteName].size())
        {
            best = &entry;
        }
    }

    if (best == nullptr || usersPerEmote[best->emoteName].size() < MIN_USERS)
    {
        this->clearCurrent();
        return;
    }

    const auto userCount =
        static_cast<int>(usersPerEmote[best->emoteName].size());
    auto changed = best->emote != this->currentEmote_ ||
                   userCount != this->currentUsers_;

    this->currentEmote_ = best->emote;
    this->currentName_ = best->emoteName;
    this->currentUsers_ = userCount;

    this->setToolTip(
        QStringLiteral(
            "Click or press Enter to send %1 (%2 users in the last %3s)\n"
            "Press Tab with an empty input to insert it")
            .arg(this->currentName_)
            .arg(this->currentUsers_)
            .arg(WINDOW_MS / 1000));

    if (!this->isVisible())
    {
        this->setVisible(true);
        changed = true;
    }
    if (!this->expireTimer_->isActive())
    {
        this->expireTimer_->start();
    }

    if (changed)
    {
        this->setFixedSize(this->sizeHint());
        this->layoutChanged();
    }
    this->update();
}

void RecentEmoteButton::clearCurrent()
{
    this->expireTimer_->stop();

    if (this->currentEmote_ == nullptr && !this->isVisible())
    {
        return;
    }

    this->currentEmote_.reset();
    this->currentName_.clear();
    this->currentUsers_ = 0;
    this->setVisible(false);
    this->layoutChanged();
}

int RecentEmoteButton::emoteHeight() const
{
    return int(QFontMetrics(this->font()).height() * 1.4);
}

QSize RecentEmoteButton::sizeHint() const
{
    const auto height = this->emoteHeight();

    auto width = height;
    if (this->currentUsers_ > 0)
    {
        width += COUNT_SPACING +
                 QFontMetrics(this->font())
                     .horizontalAdvance(u'x' +
                                        QString::number(this->currentUsers_));
    }

    return {2 * PADDING + width, 2 * PADDING + height};
}

void RecentEmoteButton::paintEvent(QPaintEvent * /*event*/)
{
    if (this->currentEmote_ == nullptr)
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // a slightly transparent background so it stays readable over the chat
    auto background = getTheme()->splits.input.background;
    background.setAlpha(215);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(this->rect(), CORNER_RADIUS, CORNER_RADIUS);

    const auto height = this->emoteHeight();
    const QRect emoteRect(PADDING, PADDING, height, height);

    // Prefer whatever variant is already loaded, but ask for the high
    // resolution one so it upgrades as soon as it's available
    const auto &image =
        this->currentEmote_->images.getImageOrLoaded(EMOTE_IMAGE_SCALE);
    if (image != nullptr)
    {
        if (auto pixmap = image->pixmapOrLoad(); pixmap)
        {
            painter.drawPixmap(
                emoteRect, pixmap->scaled(emoteRect.size(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
        }
        else
        {
            // still loading - show the name instead
            painter.setPen(this->palette().color(QPalette::WindowText));
            painter.drawText(emoteRect, Qt::AlignCenter, this->currentName_);
        }
    }

    painter.setPen(this->palette().color(QPalette::WindowText));
    const QRect countRect(emoteRect.right() + COUNT_SPACING, PADDING,
                          this->width() - emoteRect.right() - COUNT_SPACING,
                          height);
    painter.drawText(countRect, Qt::AlignLeft | Qt::AlignVCenter,
                     u'x' + QString::number(this->currentUsers_));
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
