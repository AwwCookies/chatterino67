#pragma once

#include "ForwardDecl.hpp"
#include "widgets/BasePopup.hpp"

#include <pajlada/signals/scoped-connection.hpp>

namespace chatterino {

class ChannelView;
class Split;

class ModerationLogPopup : public BasePopup
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a moderation log popup
     * @param parent The parent widget
     * @param channel The channel to display moderation logs for
     */
    explicit ModerationLogPopup(QWidget *parent, ChannelPtr channel);

    void setChannel(ChannelPtr channel);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private:
    void addShortcuts();
    void saveBounds() const;

    /**
     * @brief Checks if a message should be shown in the moderation log
     * @param message The message to check
     * @return true if the message is a moderation action
     */
    static bool isModerationMessage(const MessagePtr &message);

    ChannelView *channelView_{};
    ChannelPtr sourceChannel_;
    ChannelPtr virtualChannel_;

    std::unique_ptr<pajlada::Signals::ScopedConnection> messageConnection_;
    pajlada::Signals::SignalHolder signalHolder_;
};

}  // namespace chatterino
