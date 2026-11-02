// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/MirrorChannel.hpp"

#include "Application.hpp"
#include "messages/Message.hpp"
#include "messages/MessageElement.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"

#include <algorithm>
#include <iterator>

namespace chatterino {
namespace {

const QString &deletedMessageIdPrefix()
{
    static const QString prefix = QStringLiteral("msg:");
    return prefix;
}

/**
 * @brief Inserts `elements` right after the message's timestamp
 */
void insertAfterTimestamp(
    Message &message,
    std::vector<std::unique_ptr<MessageElement>> elements)
{
    auto pos = message.elements.begin();
    if (pos != message.elements.end() &&
        dynamic_cast<TimestampElement *>(pos->get()) != nullptr)
    {
        ++pos;
    }

    message.elements.insert(pos, std::make_move_iterator(elements.begin()),
                            std::make_move_iterator(elements.end()));
}

/**
 * @brief Prepares a copy of a message so it can be added to a mirror
 *
 * - the message is tagged with the channel it happened in
 * - the channel name is prepended (if the message doesn't have it already)
 * - an optional marker is prepended after the channel name
 * - logging and notifications are disabled so nothing is duplicated
 */
void prepareCopy(Message &copy, const Channel &source, const QString &marker)
{
    copy.channelName = source.getName();
    copy.flags.set(MessageFlag::DoNotLog);
    copy.flags.set(MessageFlag::DoNotTriggerNotification);

    std::vector<std::unique_ptr<MessageElement>> prefix;

    const auto hasChannelName =
        std::ranges::any_of(copy.elements, [](const auto &element) {
            return element->getFlags().has(MessageElementFlag::ChannelName);
        });

    if (!hasChannelName)
    {
        auto element = std::make_unique<TextElement>(
            "#" + source.getName() + " ", MessageElementFlag::ChannelName,
            MessageColor::System);
        element->setLink(Link{Link::JumpToChannel, source.getName()});
        prefix.emplace_back(std::move(element));
    }

    if (!marker.isEmpty())
    {
        prefix.emplace_back(std::make_unique<TextElement>(
            marker, MessageElementFlag::Text, MessageColor::System));
    }

    if (!prefix.empty())
    {
        insertAfterTimestamp(copy, std::move(prefix));
    }
}


/**
 * @brief Returns the login of the user that was punished by `action`
 */
QString punishedUser(const MessagePtr &action)
{
    if (!action->timeoutUser.isEmpty() &&
        !action->timeoutUser.startsWith(deletedMessageIdPrefix()))
    {
        return action->timeoutUser;
    }

    // AutoMod messages and held messages carry the offending user here
    if (!action->loginName.isEmpty())
    {
        return action->loginName;
    }

    return {};
}

/**
 * @brief Returns the ID of the deleted message if `action` is a deletion
 */
QString deletedMessageID(const MessagePtr &action)
{
    if (!action->timeoutUser.startsWith(deletedMessageIdPrefix()))
    {
        return {};
    }

    return action->timeoutUser.mid(deletedMessageIdPrefix().size());
}

MessagePtr findMessageWithID(const Channel &source, const QString &id)
{
    const auto snapshot = source.getMessageSnapshot();
    const auto size = static_cast<int>(snapshot.size());

    for (auto i = size - 1; i >= 0; --i)
    {
        const auto &message = snapshot[i];
        if (message && message->id == id)
        {
            return message;
        }
    }

    return nullptr;
}

/**
 * @brief Finds the last message `userName` sent in `source`
 */
MessagePtr findLastMessageFrom(const Channel &source, const QString &userName,
                               const MessagePtr &action)
{
    const auto snapshot = source.getMessageSnapshot();
    const auto size = static_cast<int>(snapshot.size());

    for (auto i = size - 1; i >= 0; --i)
    {
        const auto &message = snapshot[i];
        if (!message || message.get() == action.get())
        {
            continue;
        }

        // System & moderation messages are not written by the user
        if (message->flags.hasAny({MessageFlag::System, MessageFlag::Timeout,
                                   MessageFlag::ClearChat,
                                   MessageFlag::ModerationAction}))
        {
            continue;
        }

        if (message->loginName.compare(userName, Qt::CaseInsensitive) == 0)
        {
            return message;
        }
    }

    return nullptr;
}

}  // namespace

MirrorChannel::MirrorChannel(MirrorKind kind)
    : MirrorChannel(std::move(kind), QString{})
{
}

MirrorChannel::MirrorChannel(MirrorKind kind, QString sourceChannel)
    : Channel(channelName(kind, sourceChannel),
              kind == MirrorKind::ModerationLog
                  ? Channel::Type::TwitchModerationLog
                  : Channel::Type::TwitchFirstMessages)
    , kind_(kind)
    , sourceChannel_(std::move(sourceChannel))
    , displayName_(displayName(kind, this->sourceChannel_))
{
}

const QString &MirrorChannel::getDisplayName() const
{
    return this->displayName_;
}

const QString &MirrorChannel::getLocalizedName() const
{
    return this->displayName_;
}

MirrorKind MirrorChannel::kind() const
{
    return this->kind_;
}

const QString &MirrorChannel::sourceChannel() const
{
    return this->sourceChannel_;
}

QString MirrorChannel::channelName(MirrorKind kind,
                                   const QString &sourceChannel)
{
    const auto base = kind == MirrorKind::ModerationLog
                          ? QStringLiteral("/modlog")
                          : QStringLiteral("/firstmsg");

    if (sourceChannel.isEmpty())
    {
        return base;
    }

    return base + u':' + sourceChannel;
}

QString MirrorChannel::displayName(MirrorKind kind,
                                   const QString &sourceChannel)
{
    const auto base = kind == MirrorKind::ModerationLog
                          ? QStringLiteral("Moderation Log")
                          : QStringLiteral("First Time Chatters");

    if (sourceChannel.isEmpty())
    {
        return base;
    }

    return base + QStringLiteral(" (#") + sourceChannel + u')';
}

bool MirrorChannel::matches(MirrorKind kind, const MessagePtr &message)
{
    if (message == nullptr)
    {
        return false;
    }

    switch (kind)
    {
        case MirrorKind::ModerationLog:
            return message->flags.hasAny(
                {MessageFlag::ModerationAction, MessageFlag::Timeout,
                 MessageFlag::ClearChat, MessageFlag::AutoMod,
                 MessageFlag::AutoModOffendingMessageHeader,
                 MessageFlag::AutoModOffendingMessage,
                 MessageFlag::AutoModBlockedTerm, MessageFlag::LowTrustUsers});

        case MirrorKind::FirstMessages:
            return message->flags.has(MessageFlag::FirstMessage);
    }

    return false;
}


void MirrorChannel::mirror(const Channel &source, const MessagePtr &message)
{
    if (!matches(this->kind_, message))
    {
        return;
    }

    // Per-channel mirrors only care about their own channel
    if (!this->sourceChannel_.isEmpty() &&
        this->sourceChannel_.compare(source.getName(), Qt::CaseInsensitive) !=
            0)
    {
        return;
    }

    auto copy = message->clone();
    prepareCopy(*copy, source, {});
    this->addMessage(copy, MessageContext::Repost);

    if (this->kind_ == MirrorKind::ModerationLog)
    {
        this->addContextForAction(source, message);
    }
}

void MirrorChannel::addContextForAction(const Channel &source,
                                        const MessagePtr &action)
{
    MessagePtr lastMessage;
    if (const auto deletedID = deletedMessageID(action); !deletedID.isEmpty())
    {
        lastMessage = findMessageWithID(source, deletedID);
    }
    else if (!action->flags.has(MessageFlag::AutoModOffendingMessage))
    {
        // AutoMod offending messages already contain the message they caught
        const auto user = punishedUser(action);
        if (!user.isEmpty())
        {
            lastMessage = findLastMessageFrom(source, user, action);
        }
    }

    if (lastMessage == nullptr)
    {
        return;
    }

    auto copy = lastMessage->clone();
    prepareCopy(*copy, source, QString(QChar(0x2514)) + u' ');
    this->addMessage(copy, MessageContext::Repost);
}

void forwardMirroredMessage(const Channel &source, const MessagePtr &message)
{
    if (message == nullptr)
    {
        return;
    }

    const auto isModerationAction =
        MirrorChannel::matches(MirrorKind::ModerationLog, message);
    const auto isFirstMessage =
        MirrorChannel::matches(MirrorKind::FirstMessages, message);
    if (!isModerationAction && !isFirstMessage)
    {
        return;
    }

    auto *app = tryGetApp();
    if (app == nullptr)
    {
        return;
    }

    auto *twitch = app->getTwitch();
    if (twitch == nullptr)
    {
        return;
    }

    const auto sourceName = source.getName();

    const auto mirrorInto = [&](const ChannelPtr &channel) {
        if (auto mirror = std::dynamic_pointer_cast<MirrorChannel>(channel))
        {
            mirror->mirror(source, message);
        }
    };

    if (isModerationAction)
    {
        // Global log first, then the pane for this channel (if it exists)
        mirrorInto(twitch->getModerationLogChannel());
        mirrorInto(twitch->findModerationLogChannelFor(sourceName));
    }

    if (isFirstMessage)
    {
        mirrorInto(twitch->getFirstMessageChannel());
        mirrorInto(twitch->findFirstMessageChannelFor(sourceName));
    }
}

}  // namespace chatterino
