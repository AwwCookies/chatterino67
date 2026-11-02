// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Atomic.hpp"
#include "common/Channel.hpp"
#include "common/Common.hpp"
#include "providers/irc/IrcConnection2.hpp"
#include "providers/twitch/TwitchReadConnectionPool.hpp"
#include "util/CancellationToken.hpp"
#include "util/RatelimitBucket.hpp"

#include <IrcMessage>
#include <pajlada/signals/signal.hpp>
#include <pajlada/signals/signalholder.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

namespace chatterino {

class Settings;
class Paths;
class TwitchChannel;
class ModerationLogChannel;
class MirrorChannel;
enum class MirrorKind : std::uint8_t;
class BttvEmotes;
class FfzEmotes;
class SeventvEmotes;
class RatelimitBucket;
class BttvLiveUpdates;
class SeventvEventAPI;

class ITwitchIrcServer
{
public:
    ITwitchIrcServer() = default;
    virtual ~ITwitchIrcServer() = default;
    ITwitchIrcServer(const ITwitchIrcServer &) = delete;
    ITwitchIrcServer(ITwitchIrcServer &&) = delete;
    ITwitchIrcServer &operator=(const ITwitchIrcServer &) = delete;
    ITwitchIrcServer &operator=(ITwitchIrcServer &&) = delete;

    virtual void connect() = 0;

    virtual void sendRawMessage(const QString &rawMessage) = 0;

    virtual ChannelPtr getOrAddChannel(const QString &dirtyChannelName) = 0;
    virtual ChannelPtr getChannelOrEmpty(const QString &dirtyChannelName) = 0;

    virtual void addFakeMessage(const QString &data) = 0;

    virtual void addGlobalSystemMessage(const QString &messageText) = 0;

    virtual void forEachChannel(std::function<void(ChannelPtr)> func) = 0;

    virtual void forEachChannelAndSpecialChannels(
        std::function<void(ChannelPtr)> func) = 0;

    virtual std::shared_ptr<Channel> getChannelOrEmptyByID(
        const QString &channelID) = 0;

    virtual void dropSeventvChannel(const QString &userID,
                                    const QString &emoteSetID) = 0;

    virtual const IndirectChannel &getWatchingChannel() const = 0;
    virtual void setWatchingChannel(ChannelPtr newWatchingChannel) = 0;
    virtual ChannelPtr getWhispersChannel() const = 0;
    virtual ChannelPtr getMentionsChannel() const = 0;
    virtual ChannelPtr getLiveChannel() const = 0;
    virtual ChannelPtr getAutomodChannel() const = 0;
    virtual ChannelPtr getModerationLogChannel() const = 0;
    /// Log that only collects actions from `sourceChannel` (created on demand)
    virtual ChannelPtr getModerationLogChannelFor(
        const QString &sourceChannel) = 0;
    /// Same as above, but returns nullptr if that log doesn't exist yet
    virtual ChannelPtr findModerationLogChannelFor(
        const QString &sourceChannel) const = 0;

    /// Panel with the first messages of every channel
    virtual ChannelPtr getFirstMessageChannel() const = 0;
    /// Same as above, but only for `sourceChannel` (created on demand)
    virtual ChannelPtr getFirstMessageChannelFor(
        const QString &sourceChannel) = 0;
    /// Same as above, but returns nullptr if it doesn't exist yet
    virtual ChannelPtr findFirstMessageChannelFor(
        const QString &sourceChannel) const = 0;

    virtual QString getLastUserThatWhisperedMe() const = 0;
    virtual void setLastUserThatWhisperedMe(const QString &user) = 0;

    virtual void initEventAPIs(BttvLiveUpdates *bttvLiveUpdates,
                               SeventvEventAPI *seventvEventAPI) = 0;

    virtual bool isModeratorIn(const QString &broadcasterLogin) const = 0;

    // Update this interface with TwitchIrcServer methods as needed
};

class TwitchIrcServer final : public ITwitchIrcServer, public QObject
{
public:
    enum class ConnectionType {
        Read,
        Write,
    };

    TwitchIrcServer();
    ~TwitchIrcServer() override = default;

    TwitchIrcServer(const TwitchIrcServer &) = delete;
    TwitchIrcServer(TwitchIrcServer &&) = delete;
    TwitchIrcServer &operator=(const TwitchIrcServer &) = delete;
    TwitchIrcServer &operator=(TwitchIrcServer &&) = delete;

    void initialize();

    void aboutToQuit();

    void forEachChannelAndSpecialChannels(
        std::function<void(ChannelPtr)> func) override;

    std::shared_ptr<Channel> getChannelOrEmptyByID(
        const QString &channelID) override;

    void reloadAllBTTVChannelEmotes();
    void reloadAllFFZChannelEmotes();
    void reloadAllSevenTVChannelEmotes();

    /** Calls `func` with all twitch channels that have `emoteSetId` added. */
    void forEachSeventvEmoteSet(const QString &emoteSetId,
                                std::function<void(TwitchChannel &)> func);
    /** Calls `func` with all twitch channels where the seventv-user-id is `userId`. */
    void forEachSeventvUser(const QString &userId,
                            std::function<void(TwitchChannel &)> func);
    /**
     * Checks if any channel still needs this `userID` or `emoteSetID`.
     * If not, it unsubscribes from the respective messages.
     *
     * It's currently not possible to share emote sets among users,
     * but it's a commonly requested feature.
     */
    void dropSeventvChannel(const QString &userID,
                            const QString &emoteSetID) override;

    void addFakeMessage(const QString &data) override;

    void addGlobalSystemMessage(const QString &messageText) override;

    // iteration
    void forEachChannel(std::function<void(ChannelPtr)> func) override;

    void connect() override;
    void disconnect();

    void sendMessage(const QString &channelName, const QString &message);
    void sendRawMessage(const QString &rawMessage) override;

    ChannelPtr getOrAddChannel(const QString &dirtyChannelName) override;

    ChannelPtr getChannelOrEmpty(const QString &dirtyChannelName) override;

    bool isModeratorIn(const QString &broadcasterLogin) const override;

private:
    Atomic<QString> lastUserThatWhisperedMe;

    const ChannelPtr whispersChannel;
    const ChannelPtr mentionsChannel;
    const ChannelPtr liveChannel;
    const ChannelPtr automodChannel;
    const ChannelPtr moderationLogChannel;
    /// Panel with the first message of every channel
    const ChannelPtr firstMessageChannel;
    /// Per-channel mirrors (moderation log / first messages), keyed by name
    std::unordered_map<QString, std::shared_ptr<MirrorChannel>> mirrorChannels_;
    IndirectChannel watchingChannel;

public:
    const IndirectChannel &getWatchingChannel() const override;
    void setWatchingChannel(ChannelPtr newWatchingChannel) override;
    ChannelPtr getWhispersChannel() const override;
    ChannelPtr getMentionsChannel() const override;
    ChannelPtr getLiveChannel() const override;
    ChannelPtr getAutomodChannel() const override;
    ChannelPtr getModerationLogChannel() const override;
    ChannelPtr getModerationLogChannelFor(const QString &sourceChannel) override;
    ChannelPtr findModerationLogChannelFor(
        const QString &sourceChannel) const override;
    ChannelPtr getFirstMessageChannel() const override;
    ChannelPtr getFirstMessageChannelFor(const QString &sourceChannel) override;
    ChannelPtr findFirstMessageChannelFor(
        const QString &sourceChannel) const override;

    QString getLastUserThatWhisperedMe() const override;
    void setLastUserThatWhisperedMe(const QString &user) override;

    void initEventAPIs(BttvLiveUpdates *bttvLiveUpdates,
                       SeventvEventAPI *seventvEventAPI) override;

    static void initializeConnection(IrcConnection *connection,
                                     ConnectionType type);

protected:
    std::shared_ptr<Channel> createChannel(const QString &channelName);

    void privateMessageReceived(Communi::IrcPrivateMessage *message);
    void readConnectionMessageReceived(Communi::IrcMessage *message);
    void writeConnectionMessageReceived(Communi::IrcMessage *message);

    std::shared_ptr<Channel> getCustomChannel(const QString &channelname);

    /// Returns (creating if needed) the mirror of `kind` for `sourceChannel`
    ChannelPtr getMirrorChannel(MirrorKind kind, const QString &sourceChannel);
    /// Returns the mirror of `kind` for `sourceChannel`, or nullptr
    ChannelPtr findMirrorChannel(MirrorKind kind,
                                 const QString &sourceChannel) const;

private:
    void onMessageSendRequested(const std::shared_ptr<TwitchChannel> &channel,
                                const QString &message, bool &sent);
    void onReplySendRequested(const std::shared_ptr<TwitchChannel> &channel,
                              const QString &message, const QString &replyId,
                              bool &sent);

    bool prepareToSend(const std::shared_ptr<TwitchChannel> &channel);

    void refreshModeratedChannels();
    void applyModeratedChannelInfo();

    QMap<QString, std::weak_ptr<Channel>> channels;
    std::mutex channelMutex;

    QObjectPtr<IrcConnection> writeConnection_ = nullptr;
    QObjectPtr<TwitchReadConnectionPool> readConnection_ = nullptr;

    std::mutex connectionMutex_;

    pajlada::Signals::SignalHolder signalHolder;

    std::mutex lastMessageMutex_;
    std::queue<std::chrono::steady_clock::time_point> lastMessagePleb_;
    std::queue<std::chrono::steady_clock::time_point> lastMessageMod_;
    std::chrono::steady_clock::time_point lastErrorTimeSpeed_;
    std::chrono::steady_clock::time_point lastErrorTimeAmount_;

    QSet</* login */ QString> moderatedChannels;
    ScopedCancellationToken moderatedChannelFetchToken;
};

}  // namespace chatterino
