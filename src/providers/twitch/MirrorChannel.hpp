// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Channel.hpp"

#include <cstdint>

namespace chatterino {

/// What a MirrorChannel collects
enum class MirrorKind : std::uint8_t {
    /// Timeouts, bans, deletions, chat clears and AutoMod events
    ModerationLog,
    /// First messages of users in a channel
    FirstMessages,
};

/**
 * @brief Channel that mirrors interesting messages from other channels
 *
 * Backs the "Moderation Log" and "First Time Chatters" panels: every message
 * that `matches()` accepts is copied into this channel, tagged with the channel
 * it happened in.
 *
 * A mirror can be bound to a single source channel, in which case only that
 * channel's messages are mirrored.
 */
class MirrorChannel final : public Channel
{
public:
    /// Mirror that collects messages from every channel
    explicit MirrorChannel(MirrorKind kind);
    /// Mirror that only collects messages from `sourceChannel`
    MirrorChannel(MirrorKind kind, QString sourceChannel);

    const QString &getDisplayName() const override;
    const QString &getLocalizedName() const override;

    MirrorKind kind() const;
    /// The channel this mirror is bound to. Empty for global mirrors.
    const QString &sourceChannel() const;

    /// Whether `message` belongs in a mirror of `kind`
    static bool matches(MirrorKind kind, const MessagePtr &message);

    /// Copy `message`, which happened in `source`, into this mirror
    void mirror(const Channel &source, const MessagePtr &message);

    /// Channel name for a mirror, e.g. "/modlog" or "/firstmsg:somechannel"
    static QString channelName(MirrorKind kind, const QString &sourceChannel);
    /// Display name, e.g. "Moderation Log (#somechannel)"
    static QString displayName(MirrorKind kind, const QString &sourceChannel);

private:
    /// Adds the last message the punished user sent before `action`
    void addContextForAction(const Channel &source, const MessagePtr &action);

    MirrorKind kind_;
    QString sourceChannel_;
    QString displayName_;
};

/// Mirror `message`, which happened in `source`, into the matching panels
void forwardMirroredMessage(const Channel &source, const MessagePtr &message);

}  // namespace chatterino
