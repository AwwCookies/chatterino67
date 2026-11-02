// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Channel.hpp"

namespace chatterino {

/**
 * @brief Channel that aggregates moderation actions from every Twitch channel
 *
 * This backs the "Moderation Log" panel. Actions (timeouts, bans, deletions,
 * chat clears and AutoMod events) from every channel the client is in are
 * copied into this channel, tagged with the channel they happened in.
 *
 * For each action, the last message the punished user sent before the action
 * is copied in right after it, so the reason for the action stays visible.
 */
class ModerationLogChannel final : public Channel
{
public:
    ModerationLogChannel();

    const QString &getDisplayName() const override;
    const QString &getLocalizedName() const override;

    /**
     * @brief Checks if a message is a moderation action
     *
     * @param message The message to check
     * @return true if the message belongs in the moderation log
     */
    static bool isModerationAction(const MessagePtr &message);

    /**
     * @brief Copy a moderation action from `source` into this channel
     *
     * @param source The channel the action happened in
     * @param action The moderation action message
     */
    void addModerationAction(const Channel &source, const MessagePtr &action);

private:
    QString displayName_;
};

/**
 * @brief Forward a moderation action into the global moderation log
 *
 * Does nothing if `message` is not a moderation action or if the moderation
 * log channel is unavailable.
 *
 * @param source The channel the action happened in
 * @param message The moderation action message
 */
void forwardModerationAction(const Channel &source, const MessagePtr &message);

}  // namespace chatterino
