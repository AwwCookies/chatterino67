// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Channel.hpp"
#include "messages/Emote.hpp"

#include <pajlada/signals/scoped-connection.hpp>
#include <QWidget>

#include <deque>

class QTimer;

namespace chatterino {

/**
 * @brief Shows the emote that the most users used in the last couple of seconds
 *
 * Every user counts once per emote, no matter how many times they sent it.
 * The emote is taken from the messages of the channel this widget is attached
 * to. Clicking the emote sends it to that channel.
 *
 * The widget is meant to float over the chat (the split positions it), it never
 * takes part in a layout.
 */
class RecentEmoteButton final : public QWidget
{
    Q_OBJECT

public:
    static constexpr int WINDOW_MS = 2000;
    /// How many different users need to use an emote before it's shown
    static constexpr int MIN_USERS = 2;

    explicit RecentEmoteButton(QWidget *parent = nullptr);

    /// Attach the widget to `channel` (and watch its messages)
    void setChannel(const ChannelPtr &channel);

Q_SIGNALS:
    void emoteClicked(const QString &emoteName);
    /// Emitted when the size or the visibility of the widget changed
    void layoutChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    QSize sizeHint() const override;

private:
    struct Entry {
        qint64 time;
        QString emoteName;
        QString user;
        EmotePtr emote;
    };

    void onMessage(const MessagePtr &message);
    void addEntry(const QString &emoteName, const QString &user,
                  const EmotePtr &emote);
    void refresh();
    void clearCurrent();
    int emoteHeight() const;

    std::deque<Entry> entries_;
    pajlada::Signals::ScopedConnection messageConnection_;

    EmotePtr currentEmote_;
    QString currentName_;
    int currentUsers_ = 0;

    QTimer *expireTimer_;
};

}  // namespace chatterino
