#include "widgets/dialogs/ModerationLogPopup.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/QLogging.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "messages/Message.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/Scrollbar.hpp"

#include <QVBoxLayout>

namespace chatterino {

ModerationLogPopup::ModerationLogPopup(QWidget *parent, ChannelPtr channel)
    : BasePopup({BaseWindow::EnableCustomFrame, BaseWindow::DisableLayoutSave},
                parent)
    , sourceChannel_(std::move(channel))
{
    this->setWindowTitle("Moderation Log - #" + this->sourceChannel_->getName());

    auto bounds = getApp()->getWindows()->emotePopupBounds();
    if (bounds.size().isEmpty())
    {
        bounds.setSize(QSize{400, 500} * this->scale());
    }
    this->setInitialBounds(bounds, widgets::BoundsChecking::DesiredPosition);

    auto *layout = new QVBoxLayout();
    this->getLayoutContainer()->setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    this->channelView_ =
        new ChannelView(this, nullptr, ChannelView::Context::None);
    this->channelView_->setMinimumSize(300, 200);
    this->channelView_->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Expanding);

    layout->addWidget(this->channelView_);

    this->setChannel(this->sourceChannel_);
    this->addShortcuts();

    this->signalHolder_.managedConnect(getApp()->getHotkeys()->onItemsUpdated,
                                       [this]() {
                                           this->clearShortcuts();
                                           this->addShortcuts();
                                       });
}

void ModerationLogPopup::setChannel(ChannelPtr channel)
{
    this->sourceChannel_ = std::move(channel);
    this->setWindowTitle("Moderation Log - #" + this->sourceChannel_->getName());

    if (this->sourceChannel_->isTwitchChannel())
    {
        this->virtualChannel_ =
            std::make_shared<TwitchChannel>(this->sourceChannel_->getName());
    }
    else
    {
        this->virtualChannel_ = std::make_shared<Channel>(
            this->sourceChannel_->getName(), Channel::Type::None);
    }

    auto snapshot = this->sourceChannel_->getMessageSnapshot();
    for (size_t i = 0; i < snapshot.size(); ++i)
    {
        const auto &msg = snapshot[i];
        if (isModerationMessage(msg))
        {
            auto overrideFlags = std::optional<MessageFlags>(msg->flags);
            overrideFlags->set(MessageFlag::DoNotLog);
            this->virtualChannel_->addMessage(msg, MessageContext::Repost,
                                              overrideFlags);
        }
    }

    this->channelView_->setChannel(this->virtualChannel_);
    this->channelView_->setSourceChannel(this->sourceChannel_);

    this->messageConnection_ =
        std::make_unique<pajlada::Signals::ScopedConnection>(
            this->sourceChannel_->messageAppended.connect(
                [this](MessagePtr &message, auto) {
                    if (isModerationMessage(message))
                    {
                        auto overrideFlags =
                            std::optional<MessageFlags>(message->flags);
                        overrideFlags->set(MessageFlag::DoNotLog);
                        this->virtualChannel_->addMessage(
                            message, MessageContext::Repost, overrideFlags);
                    }
                }));
}

bool ModerationLogPopup::isModerationMessage(const MessagePtr &message)
{
    return message->flags.has(MessageFlag::ModerationAction) ||
           message->flags.has(MessageFlag::Timeout) ||
           message->flags.has(MessageFlag::ClearChat) ||
           message->flags.has(MessageFlag::AutoMod) ||
           message->flags.has(MessageFlag::AutoModOffendingMessageHeader) ||
           message->flags.has(MessageFlag::AutoModOffendingMessage) ||
           message->flags.has(MessageFlag::AutoModBlockedTerm) ||
           message->flags.has(MessageFlag::LowTrustUsers);
}

void ModerationLogPopup::addShortcuts()
{
    HotkeyController::HotkeyMap actions{
        {"delete",
         [this](std::vector<QString>) -> QString {
             this->close();
             return "";
         }},
        {"scrollPage",
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.empty())
             {
                 qCWarning(chatterinoHotkeys)
                     << "scrollPage hotkey called without arguments!";
                 return "scrollPage hotkey called without arguments!";
             }
             auto direction = arguments.at(0);

             auto &scrollbar = this->channelView_->getScrollBar();
             if (direction == "up")
             {
                 scrollbar.offset(-scrollbar.getPageSize());
             }
             else if (direction == "down")
             {
                 scrollbar.offset(scrollbar.getPageSize());
             }
             else
             {
                 qCWarning(chatterinoHotkeys) << "Unknown scroll direction";
             }
             return "";
         }},

        {"reject", nullptr},
        {"accept", nullptr},
        {"openTab", nullptr},
        {"search", nullptr},
    };

    this->shortcuts_ = getApp()->getHotkeys()->shortcutsForCategory(
        HotkeyCategory::PopupWindow, actions, this);
}

void ModerationLogPopup::saveBounds() const
{
    if (isAppAboutToQuit())
    {
        return;
    }

    auto bounds = this->getBounds();
    if (!bounds.isNull())
    {
        getApp()->getWindows()->setEmotePopupBounds(bounds);
    }
}

void ModerationLogPopup::closeEvent(QCloseEvent *event)
{
    this->saveBounds();
    BasePopup::closeEvent(event);
}

void ModerationLogPopup::resizeEvent(QResizeEvent *event)
{
    this->saveBounds();
    BasePopup::resizeEvent(event);
}

void ModerationLogPopup::moveEvent(QMoveEvent *event)
{
    this->saveBounds();
    BasePopup::moveEvent(event);
}

}  // namespace chatterino
