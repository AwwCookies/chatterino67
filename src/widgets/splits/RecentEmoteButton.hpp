// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Channel.hpp"
#include "messages/Image.hpp"

#include <pajlada/signals/scoped-connection.hpp>
#include <QWidget>

#include <deque>

class QTimer;

namespace chatterino {

/**
 * @brief Shows the emote that was used most in the last couple of seconds
 *
 * The emote is taken from the messages of the channel this widget is attached
 * to. Clicking the emote sends it to that channel.
 */
class RecentEmoteButton final : public QWidget
{
    Q_OBJECT

public:
    static constexpr int WINDOW_MS = 2000;
    /// How often an emote needs to appear to be shown
    static constexpr int MIN_COUNT = 2;

    explicit RecentEmoteButton(QWidget *parent = nullptr);

    /// Attach the widget to `channel` (and watch its messages)
    void setChannel(const ChannelPtr &channel);

Q_SIGNALS:
    void emoteClicked(const QString &emoteName);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    QSize sizeHint() const override;

private:
    struct Entry {
        qint64 time;
        QString name;
        ImagePtr image;
    };

    void onMessage(const MessagePtr &message);
    void refresh();
    void addEntry(const QString &name, const ImagePtr &image);
    void clearCurrent();
    int emoteHeight() const;

    std::deque<Entry> entries_;
    pajlada::Signals::ScopedConnection messageConnection_;

    QString currentName_;
    ImagePtr currentImage_;
    int currentCount_ = 0;

    QTimer *expireTimer_;
};

}  // namespace chatterino
