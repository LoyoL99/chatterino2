// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/GeneralPage.hpp"

#include "Application.hpp"
#include "common/Literals.hpp"  // IWYU pragma: keep
#include "common/Modes.hpp"
#include "common/Version.hpp"
#include "controllers/hotkeys/HotkeyCategory.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "providers/recentmessages/Api.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/CrashHandler.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/NativeMessaging.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/FuzzyConvert.hpp"
#include "util/Helpers.hpp"
#include "util/IncognitoBrowser.hpp"
#include "widgets/BaseWindow.hpp"
#include "widgets/helper/FontSettingWidget.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QCompleter>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDialog>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QProcess>
#include <QSignalBlocker>

namespace {

using namespace chatterino;
using namespace chatterino::literals;

const QString CHROME_EXTENSION_LINK =
    u"https://chrome.google.com/webstore/detail/chatterino-native-host/glknmaideaikkmemifbfkhnomoknepka"_s;
const QString FIREFOX_EXTENSION_LINK =
    u"https://addons.mozilla.org/en-US/firefox/addon/chatterino-native-host/"_s;

#ifdef Q_OS_WIN
const QString META_KEY = u"Windows"_s;
#else
const QString META_KEY = u"Meta"_s;
#endif

const QStringList ZOOM_LEVELS = {
    "0.5x", "0.6x", "0.7x", "0.8x",  "0.9x",  "Default", "1.2x", "1.4x",
    "1.6x", "1.8x", "2x",   "2.33x", "2.66x", "3x",      "3.5x", "4x",
};

void addKeyboardModifierSetting(GeneralPageView &layout, const QString &title,
                                EnumSetting<Qt::KeyboardModifier> &setting)
{
    layout.addDropdown<std::underlying_type_t<Qt::KeyboardModifier>>(
        title, {"None", "Shift", "Control", "Alt", META_KEY}, setting,
        [](int index) {
            switch (index)
            {
                case Qt::ShiftModifier:
                    return 1;
                case Qt::ControlModifier:
                    return 2;
                case Qt::AltModifier:
                    return 3;
                case Qt::MetaModifier:
                    return 4;
                default:
                    return 0;
            }
        },
        [](DropdownArgs args) {
            switch (args.index)
            {
                case 1:
                    return Qt::ShiftModifier;
                case 2:
                    return Qt::ControlModifier;
                case 3:
                    return Qt::AltModifier;
                case 4:
                    return Qt::MetaModifier;
                default:
                    return Qt::NoModifier;
            }
        },
        false);
}
}  // namespace

namespace chatterino {

GeneralPage::GeneralPage()
{
    auto *y = new QVBoxLayout;
    auto *x = new QHBoxLayout;
    auto *view = GeneralPageView::withNavigation(this);
    this->view_ = view;
    x->addWidget(view);
    auto *z = new QFrame;
    z->setLayout(x);
    y->addWidget(z);
    this->setLayout(y);

    this->initLayout(*view);

    this->initExtra();
}

bool GeneralPage::filterElements(const QString &query)
{
    if (this->view_)
    {
        return this->view_->filterElements(query) || query.isEmpty();
    }
    else
    {
        return false;
    }
}

void GeneralPage::initLayout(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle(this->tr("Interface"));

    {
        auto *themes = getApp()->getThemes();
        auto available = themes->availableThemes();
        available.emplace_back("System", "System");

        SettingWidget::dropdown(this->tr("Theme"), themes->themeName, available)
            ->addTo(layout);

        SettingWidget::dropdown(this->tr("Dark system theme"),
                                themes->darkSystemThemeName,
                                themes->availableThemes())
            ->setTooltip(this->tr("This theme is selected if your system is in a dark ")
                         "theme and you enabled the adaptive 'System' theme.")
            ->conditionallyEnabledBy(themes->themeName, "System")
            ->addTo(layout);

        SettingWidget::dropdown(this->tr("Light system theme"),
                                themes->lightSystemThemeName,
                                themes->availableThemes())
            ->setTooltip(this->tr("This theme is selected if your system is in a light ")
                         "theme and you enabled the adaptive 'System' theme.")
            ->conditionallyEnabledBy(themes->themeName, "System")
            ->addTo(layout);
    }

    auto *langWidget = SettingWidget::dropdown(
        this->tr("Language (requires restart)"),
        s.uiLanguage,
        {
            {QString::fromUtf8("Русский"), "ru"},
            {QString::fromUtf8("English"), "en"},
        });
    langWidget->addTo(layout);

    layout.addButton(
        this->tr("Restart Chatterino to apply language"),
        [] {
            getSettings()->requestSave();
            QProcess::startDetached(qApp->applicationFilePath(), {});
            QApplication::exit(0);
        });

    layout.addDropdown<float>(
        this->tr("Zoom"), ZOOM_LEVELS, s.uiScale,
        [](auto val) {
            if (val == 1)
            {
                return QString("Default");
            }
            else
            {
                return QString::number(val) + "x";
            }
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 1.f);
        });
    ComboBox *tabDirectionDropdown =
        layout.addDropdown<std::underlying_type_t<NotebookTabLocation>>(
            this->tr("Tab layout"), {"Top", "Left", "Right", "Bottom"}, s.tabDirection,
            [](auto val) {
                switch (val)
                {
                    case NotebookTabLocation::Top:
                        return "Top";
                    case NotebookTabLocation::Left:
                        return "Left";
                    case NotebookTabLocation::Right:
                        return "Right";
                    case NotebookTabLocation::Bottom:
                        return "Bottom";
                }

                return "";
            },
            [](auto args) {
                if (args.value == "Bottom")
                {
                    return NotebookTabLocation::Bottom;
                }
                else if (args.value == "Left")
                {
                    return NotebookTabLocation::Left;
                }
                else if (args.value == "Right")
                {
                    return NotebookTabLocation::Right;
                }
                else
                {
                    // default to top
                    return NotebookTabLocation::Top;
                }
            },
            false);
    tabDirectionDropdown->setMinimumWidth(
        tabDirectionDropdown->minimumSizeHint().width());

    layout.addDropdown<std::underlying_type_t<NotebookTabVisibility>>(
        this->tr("Tab visibility"), {"All tabs", "Only live tabs"}, s.tabVisibility,
        [](auto val) {
            switch (val)
            {
                case NotebookTabVisibility::LiveOnly:
                    return "Only live tabs";
                case NotebookTabVisibility::AllTabs:
                default:
                    return "All tabs";
            }
        },
        [](auto args) {
            if (args.value == "Only live tabs")
            {
                return NotebookTabVisibility::LiveOnly;
            }
            else
            {
                return NotebookTabVisibility::AllTabs;
            }
        },
        false, "Choose which tabs are visible in the notebook");

    SettingWidget::dropdown(this->tr("Tab style"), s.tabStyle)->addTo(layout);
    SettingWidget::checkbox(this->tr("Extend wrapped tabs"), s.growWrappedNotebookLines)
        ->setTooltip(this->tr("When horizontal tabs are wrapped, extend the line for ")
                     "the whole width of the window.")
        ->addTo(layout);

    layout.addWidget(new FontSettingWidget(s.chatFontFamily, s.chatFontSize,
                                           s.chatFontWeight),
                     {"font", "weight", "size"});

    SettingWidget::inverseCheckbox(this->tr("Show message reply context"),
                                   s.hideReplyContext)
        ->setTooltip(
            this->tr("This setting will only affect how messages are shown. You can ")
            "reply to a message regardless of this setting.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show message reply button"), s.showReplyButton)
        ->setTooltip(this->tr("Show a reply button next to every chat message"))
        ->addTo(layout);

    auto removeTabSeq = getApp()->getHotkeys()->getDisplaySequence(
        HotkeyCategory::Window, "removeTab");
    QString removeTabShortcut = "an assigned hotkey (Window -> remove tab)";
    if (!removeTabSeq.isEmpty())
    {
        removeTabShortcut =
            removeTabSeq.toString(QKeySequence::SequenceFormat::NativeText);
    }

    SettingWidget::checkbox(this->tr("Show tab close button"), s.showTabCloseButton)
        ->setTooltip(
            this->tr("When disabled, the x to close a tab will be hidden.\nTabs can ")
            "still be closed by right-clicking or pressing " +
            removeTabShortcut + ".")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Always on top"), s.windowTopMost)
        ->setTooltip(this->tr("Always keep Chatterino as the top window."))
        ->addTo(layout);

#ifdef USEWINSDK
    SettingWidget::checkbox(this->tr("Start with Windows"), s.autorun)
        ->setTooltip(this->tr("Start Chatterino when your computer starts."))
        ->addTo(layout);
#endif
    if (!BaseWindow::supportsCustomWindowFrame())
    {
        auto settingsSeq = getApp()->getHotkeys()->getDisplaySequence(
            HotkeyCategory::Window, "openSettings");
        QString shortcut = " (no key bound to open them otherwise)";
        // TODO: maybe prevent the user from locking themselves out of the settings?
        if (!settingsSeq.isEmpty())
        {
            shortcut = QStringLiteral(" (%1 to show)")
                           .arg(settingsSeq.toString(
                               QKeySequence::SequenceFormat::NativeText));
        }

        SettingWidget::inverseCheckbox("Show preferences button" + shortcut,
                                       s.hidePreferencesButton)
            ->addTo(layout);

        SettingWidget::inverseCheckbox("Show user button", s.hideUserButton)
            ->addTo(layout);
    }

    SettingWidget::checkbox(this->tr("Mark tabs with live channels"), s.showTabLive)
        ->setTooltip(this->tr("Shows a red dot in the top right corner of a tab to ")
                     "indicate one of the channels in the tab is live.")
        ->addTo(layout);

    layout.addTitle(this->tr("Chat"));

    layout.addDropdown<float>(
        this->tr("Pause on mouse hover"),
        {"Disabled", "0.5s", "1s", "2s", "5s", "Indefinite"},
        s.pauseOnHoverDuration,
        [](auto val) {
            if (val < -0.5f)
            {
                return QString("Indefinite");
            }
            else if (val < 0.001f)
            {
                return QString("Disabled");
            }
            else
            {
                return QString::number(val) + "s";
            }
        },
        [](auto args) {
            if (args.index == 0)
            {
                return 0.0f;
            }
            else if (args.value == "Indefinite")
            {
                return -1.0f;
            }
            else
            {
                return fuzzyToFloat(args.value,
                                    std::numeric_limits<float>::infinity());
            }
        });
    addKeyboardModifierSetting(layout, "Pause while holding a key",
                               s.pauseChatModifier);
    layout.addDropdown<float>(
        this->tr("Mousewheel scroll speed"), {"0.5x", "0.75x", "Default", "1.5x", "2x"},
        s.mouseScrollMultiplier,
        [](auto val) {
            if (val == 1)
            {
                return QString("Default");
            }
            else
            {
                return QString::number(val) + "x";
            }
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 1.f);
        });

    SettingWidget::checkbox(this->tr("Smooth scrolling"), s.enableSmoothScrolling)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Smooth scrolling on new messages"),
                            s.enableSmoothScrollingNewMessages)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show input when it's empty"), s.showEmptyInput)
        ->setTooltip(this->tr("Show the chat box even when there is nothing typed."))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show message length while typing"),
                            s.showMessageLength)
        ->setTooltip(
            this->tr("Show how many characters are currently in your input box.\n")
            "Useful for making sure you don't go past the 500 character Twitch "
            "limit, or a lower limit enforced by a moderation bot")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show countdown on slow mode or when timed out"),
                            s.showSendWaitTimer)
        ->setTooltip(this->tr("Show how long you may need to wait before being able to ")
                     "send in a Twitch channel again if the channel is in slow "
                     "mode or if you have been timed out")
        ->addKeywords({"slowmode", "timeout"})
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Allow sending duplicate messages"),
                            s.allowDuplicateMessages)
        ->setTooltip(
            this->tr("Allow a single message to be repeatedly sent without any changes."))
        ->addTo(layout);

    layout.addDropdown<std::underlying_type_t<MessageOverflow>>(
        this->tr("Message overflow"),
        {this->tr("Highlight"), this->tr("Prevent"), this->tr("Allow")},
        s.messageOverflow,
        [](auto index) {
            return static_cast<int>(index);
        },
        [](auto args) {
            return static_cast<MessageOverflow>(args.index);
        },
        false,
        this->tr("Specify how Chatterino will handle messages that exceed Twitch "
                 "message limits"));
    layout.addDropdown<std::underlying_type_t<UsernameRightClickBehavior>>(
        this->tr("Username right-click behavior"),
        {
            this->tr("Reply"),
            this->tr("Mention"),
            this->tr("Ignore"),
        },
        s.usernameRightClickBehavior,
        [](auto index) {
            return static_cast<int>(index);
        },
        [](auto args) {
            return static_cast<UsernameRightClickBehavior>(args.index);
        },
        false,
        this->tr("Specify how Chatterino will handle right-clicking a username in "
                 "chat when not holding the modifier."));
    layout.addDropdown<std::underlying_type_t<UsernameRightClickBehavior>>(
        this->tr("Username right-click with modifier behavior"),
        {
            this->tr("Reply"),
            this->tr("Mention"),
            this->tr("Ignore"),
        },
        s.usernameRightClickModifierBehavior,
        [](auto index) {
            return static_cast<int>(index);
        },
        [](auto args) {
            return static_cast<UsernameRightClickBehavior>(args.index);
        },
        false,
        this->tr("Specify how Chatterino will handle right-clicking a username in "
                 "chat when holding down the modifier."));
    layout.addDropdown<std::underlying_type_t<Qt::KeyboardModifier>>(
        this->tr("Modifier for alternate right-click action"),
        {this->tr("Shift"), this->tr("Control"), this->tr("Alt"), META_KEY},
        s.usernameRightClickModifier,
        [](int index) {
            switch (index)
            {
                case Qt::ShiftModifier:
                    return 0;
                case Qt::ControlModifier:
                    return 1;
                case Qt::AltModifier:
                    return 2;
                case Qt::MetaModifier:
                    return 3;
                default:
                    return 0;
            }
        },
        [](DropdownArgs args) {
            switch (args.index)
            {
                case 0:
                    return Qt::ShiftModifier;
                case 1:
                    return Qt::ControlModifier;
                case 2:
                    return Qt::AltModifier;
                case 3:
                    return Qt::MetaModifier;
                default:
                    return Qt::NoModifier;
            }
        },
        false);

    SettingWidget::checkbox(this->tr("Hide scrollbar thumb"), s.hideScrollbarThumb)
        ->setTooltip(this->tr("Hiding the scrollbar thumb (the handle you can drag) ")
                     "will disable all mouse interaction in the scrollbar.")
        ->addKeywords({"scroll bar"})
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Hide scrollbar highlights"),
                            s.hideScrollbarHighlights)
        ->addKeywords({"scroll bar"})
        ->addTo(layout);

    SettingWidget::checkbox(
        this->tr("Pulse text input when one of your messages is successfully sent"),
        s.pulseTextInputOnSelfMessage)
        ->setTooltip(
            this->tr("Pulses the text input in a green color whenever a message of ")
            "yours is successfully sent in the matching channel.")
        ->addTo(layout);

    layout.addTitle(this->tr("Messages"));

    SettingWidget::checkbox(this->tr("Separate with lines"), s.separateMessages)
        ->setTooltip(
            this->tr("Adds a line between each message to help better tell them apart."))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Alternate background color"), s.alternateMessages)
        ->setTooltip(this->tr("Slightly change the background behind every other ")
                     "message to help better tell them apart.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Reduce opacity of message history"),
                            s.fadeMessageHistory)
        ->setTooltip(
            this->tr("Reduce opacity of messages that were posted before Chatterino ")
            "was started or while re-connecting.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Hide deleted messages"), s.hideModerated)
        ->setTooltip(
            this->tr("When enabled, messages deleted by moderators will be hidden."))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Hide message timestamps when channel is live"),
                            s.hideMessageTimestampsWhenLive)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Correct ASCII art wrapping"), s.wrapAsciiArt)
        ->setTooltip(this->tr("Limit the width of messages containing ASCII art to ")
                     "match the width of Twitch web chat.")
        ->addTo(layout);

    auto *timestampFormatDropdown = layout.addDropdown<QString>(
        this->tr("Message timestamp format"),
        {"Disable", "h:mm", "hh:mm", "h:mm a", "hh:mm a", "h:mm:ss", "hh:mm:ss",
         "h:mm:ss a", "hh:mm:ss a", "h:mm:ss.zzz", "h:mm:ss.zzz a",
         "hh:mm:ss.zzz", "hh:mm:ss.zzz a"},
        s.timestampFormat,
        [](auto val) {
            return getSettings()->showTimestamps.getValue()
                       ? val
                       : QString("Disable");
        },
        [](auto args) {
            getSettings()->showTimestamps.setValue(args.index != 0);

            return args.index == 0 ? getSettings()->timestampFormat.getValue()
                                   : args.value;
        },
        true, "a = am/pm, zzz = milliseconds");
    timestampFormatDropdown->completer()->setCaseSensitivity(Qt::CaseSensitive);

    SettingWidget::checkbox(this->tr("Show header timestamps"), s.showHeaderTimestamps)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show announcement header"),
                            s.showAnnouncementHeader)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show subscription header"),
                            s.showSubscriptionHeader)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show watch streak header"), s.showWatchStreakHeader)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show Twitch GIFs"), s.showTwitchGifs)
        ->setTooltip(this->tr("Twitch GIFs will be shown inline. When disabled, they're ")
                     "shown as links.")
        ->addTo(layout);

    auto *twitchGifSize = layout.addDropdown<float>(
        this->tr("Twitch GIF size"),
        {"0.5x", "0.75x", "Default", "1.25x", "1.4x (Twitch)", "1.5x", "2x"},
        s.twitchGifScale,
        [](auto value) {
            if (value == 1)
            {
                return QString("Default");
            }
            if (qFuzzyCompare(value, 1.4F))
            {
                return QString("1.4x (Twitch)");
            }
            return QString::number(value) + "x";
        },
        [](const auto &args) {
            return fuzzyToFloat(args.value, 1.F);
        });
    twitchGifSize->setEnabled(s.showTwitchGifs);
    s.showTwitchGifs.connect(
        [twitchGifSize](bool enabled) {
            twitchGifSize->setEnabled(enabled);
        },
        this->managedConnections_);

    layout.addDropdown<int>(
        this->tr("Limit message height"),
        {"Never", "2 lines", "3 lines", "4 lines", "5 lines"},
        s.collpseMessagesMinLines,
        [](auto val) {
            return val ? QString::number(val) + " lines" : QString("Never");
        },
        [](auto args) {
            return fuzzyToInt(args.value, 0);
        });
    layout.addDropdown<int>(
        this->tr("Limit length of deleted messages"),
        {"No limit", "50 characters", "100 characters", "200 characters",
         "300 characters", "400 characters"},
        s.deletedMessageLengthLimit,
        [](auto val) {
            return val ? QString::number(val) + " characters"
                       : QString("No limit");
        },
        [](const auto &args) {
            return fuzzyToInt(args.value, 0);
        },
        true,
        {"Limits the amount of characters displayed in deleted messages "
         "when announced via system message."});

    layout.addSeparator();

    SettingWidget::checkbox(this->tr("Draw a line below the most recent message before ")
                            "switching applications.",
                            s.showLastMessageIndicator)
        ->setTooltip(this->tr("Adds an underline below the most recent message ")
                     "sent before you tabbed out of Chatterino.")
        ->addTo(layout);

    SettingWidget::dropdown("Line style", s.lastMessagePattern)->addTo(layout);

    SettingWidget::colorButton("Line color", s.lastMessageColor)->addTo(layout);

    layout.addTitle(this->tr("Emotes"));

    SettingWidget::checkbox(this->tr("Enable"), s.enableEmoteImages)->addTo(layout);

    SettingWidget::checkbox(this->tr("Animate"), s.animateEmotes)->addTo(layout);

    SettingWidget::checkbox(this->tr("Animate only when Chatterino is focused"),
                            s.animationsWhenFocused)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Enable zero-width emotes"), s.enableZeroWidthEmotes)
        ->setTooltip(
            this->tr("When disabled, emotes that overlap other emotes, such as BTTV's ")
            "cvMask and 7TV's RainTime, will appear as normal emotes.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Enable emote completion by typing :"),
                            s.emoteCompletionWithColon)
        ->setTooltip(
            this->tr("With this setting enabled, typing the colon character opens the ")
            "colon-completion popup which gives you an updating list of emotes "
            "matching the text after the colon.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Use experimental smarter emote completion."),
                            s.useSmartEmoteCompletion)
        ->addTo(layout);

    layout.addDropdown<float>(
        "Size", {"0.5x", "0.75x", "Default", "1.25x", "1.5x", "2x"},
        s.emoteScale,
        [](auto val) {
            if (val == 1)
            {
                return QString("Default");
            }
            else
            {
                return QString::number(val) + "x";
            }
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 1.f);
        });

    SettingWidget::checkbox(this->tr("Remove spaces between emotes"),
                            s.removeSpacesBetweenEmotes)
        ->setTooltip(this->tr("When enabled, adjacent emotes will no longer have an ")
                     "added space separating them.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show unlisted 7TV emotes"),
                            s.showUnlistedSevenTVEmotes)
        ->addKeywords({"seventv"})
        ->addTo(layout);
    // TODO: Add a tooltip explaining what an unlisted 7TV emote is
    // but wait until https://github.com/Chatterino/wiki/pull/255 is resolved,
    // as an official description from 7TV devs is best
    s.showUnlistedSevenTVEmotes.connect(
        []() {
            getApp()->getTwitch()->forEachChannelAndSpecialChannels(
                [](const auto &c) {
                    if (c->isTwitchChannel())
                    {
                        auto *channel = dynamic_cast<TwitchChannel *>(c.get());
                        if (channel != nullptr)
                        {
                            channel->refreshSevenTVChannelEmotes(false);
                        }
                    }
                });
        },
        false);

    SettingWidget::dropdown("Show emote & badge thumbnail on hover",
                            s.emotesTooltipPreview)
        ->addTo(layout);

    SettingWidget::dropdown("Emote & badge thumbnail size on hover",
                            s.emoteTooltipScale)
        ->addTo(layout);

    SettingWidget::dropdown(this->tr("Emoji style"), s.emojiSet)->addTo(layout);

    SettingWidget::checkbox(this->tr("Show BetterTTV global emotes"),
                            s.enableBTTVGlobalEmotes)
        ->addKeywords({"bttv"})
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Show BetterTTV channel emotes"),
                            s.enableBTTVChannelEmotes)
        ->addKeywords({"bttv"})
        ->addTo(layout);
    SettingWidget::checkbox(
        this->tr("Enable BetterTTV live emote updates (requires restart)"),
        s.enableBTTVLiveUpdates)
        ->addKeywords({"bttv"})
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Send activity to BetterTTV"), s.sendBTTVActivity)
        ->setTooltip(
            this->tr("When enabled, Chatterino will signal an activity to BetterTTV ")
            "when you send a chat message. This is used for badges, "
            " and personal emotes. When disabled, no activity "
            "is sent and others won't see your cosmetics.")
        ->addKeywords({"bttv"})
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show FrankerFaceZ global emotes"),
                            s.enableFFZGlobalEmotes)
        ->addKeywords({"ffz"})
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Show FrankerFaceZ channel emotes"),
                            s.enableFFZChannelEmotes)
        ->addKeywords({"ffz"})
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show 7TV global emotes"),
                            s.enableSevenTVGlobalEmotes)
        ->addKeywords({"seventv"})
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Show 7TV channel emotes"),
                            s.enableSevenTVChannelEmotes)
        ->addKeywords({"seventv"})
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Enable 7TV live emote updates (requires restart)"),
                            s.enableSevenTVEventAPI)
        ->addKeywords({"seventv"})
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Send activity to 7TV"), s.sendSevenTVActivity)
        ->setTooltip(this->tr("When enabled, Chatterino will signal an activity to 7TV ")
                     "when you send a chat message. This is used for badges, "
                     "paints, and personal emotes. When disabled, no activity "
                     "is sent and others won't see your cosmetics.")
        ->addKeywords({"seventv"})
        ->addTo(layout);

    layout.addTitle(this->tr("Streamer Mode"));
    layout.addDescription(
        "Chatterino can automatically change behavior if it detects that any "
        "streaming software is running.\nSelect which things you want to "
        "change while streaming");

    SettingWidget::dropdown("Enable Streamer Mode", s.enableStreamerMode)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Hide usercard avatars"),
                            s.streamerModeHideUsercardAvatars)
        ->setTooltip(this->tr("Prevent potentially explicit avatars from showing."))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Hide link thumbnails"),
                            s.streamerModeHideLinkThumbnails)
        ->setTooltip(this->tr("Prevent potentially explicit thumbnails from showing ")
                     "when hovering links.")
        ->addTo(layout);

    SettingWidget::checkbox(
        this->tr("Hide viewer count and stream length while hovering over split header"),
        s.streamerModeHideViewerCountAndDuration)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Hide moderation actions"),
                            s.streamerModeHideModActions)
        ->setTooltip(
            this->tr("Hide bans, timeouts, and automod messages from appearing in chat."))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Hide messages from restricted users"),
                            s.streamerModeHideRestrictedUsers)
        ->setTooltip(this->tr("Restricted users can be marked by you, your moderators, ")
                     "or Twitch's AutoMod")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Hide blocked terms"),
                            s.streamerModeHideBlockedTermText)
        ->setTooltip(
            this->tr("Hide blocked terms from showing up in places like AutoMod ")
            "messages. This can be useful in case you have some blocked terms "
            "that you don't want to show on stream.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Hide user notes"), s.streamerModeHideUserNotes)
        ->setTooltip(this->tr("Hide user notes from showing in usercards."))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Mute mention sounds"), s.streamerModeMuteMentions)
        ->setTooltip(this->tr("Mute your ping sound from playing."))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Suppress Live Notifications"),
                            s.streamerModeSuppressLiveNotifications)
        ->setTooltip(
            this->tr("Hide Live notification popups from appearing. (Windows Only)"))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Suppress Inline Whispers"),
                            s.streamerModeSuppressInlineWhispers)
        ->setTooltip(this->tr("Hide whispers sent to you from appearing in chat."))
        ->addTo(layout);

    layout.addTitle(this->tr("Link Previews"));
    layout.addDescription(
        "Extra information like \"youtube video stats\" or title of webpages "
        "can be loaded for all links if enabled. Optionally you can also show "
        "thumbnails for emotes, videos and more. The information is pulled "
        "from our servers. The Link Previews are loaded through <a "
        "href=\"https://github.com/Chatterino/api\">an API</a> hosted by the "
        "Chatterino developers. These are the API <a "
        "href=\"https://braize.pajlada.com/chatterino/legal/"
        "terms-of-service\">Terms of Services</a> and <a "
        "href=\"https://braize.pajlada.com/chatterino/legal/"
        "privacy-policy\">Privacy Policy</a>.");

    SettingWidget::checkbox(this->tr("Enable"), s.linkInfoTooltip)->addTo(layout);

    layout.addDropdown<int>(
        this->tr("Also show thumbnails if available"),
        {"Off", "Small", "Medium", "Large"}, s.thumbnailSize,
        [](auto val) {
            if (val == 0)
            {
                return QString("Off");
            }
            else if (val == 100)
            {
                return QString("Small");
            }
            else if (val == 200)
            {
                return QString("Medium");
            }
            else if (val == 300)
            {
                return QString("Large");
            }
            else
            {
                return QString::number(val);
            }
        },
        [](auto args) {
            if (args.value == "Small")
            {
                return 100;
            }
            else if (args.value == "Medium")
            {
                return 200;
            }
            else if (args.value == "Large")
            {
                return 300;
            }

            return fuzzyToInt(args.value, 0);
        });
    layout.addDropdown<int>(
        this->tr("Show thumbnails of streams"), {"Off", "Small", "Medium", "Large"},
        s.thumbnailSizeStream,
        [](auto val) {
            if (val == 0)
            {
                return QString("Off");
            }
            else if (val == 1)
            {
                return QString("Small");
            }
            else if (val == 2)
            {
                return QString("Medium");
            }
            else if (val == 3)
            {
                return QString("Large");
            }
            else
            {
                return QString::number(val);
            }
        },
        [](auto args) {
            if (args.value == "Small")
            {
                return 1;
            }
            else if (args.value == "Medium")
            {
                return 2;
            }
            else if (args.value == "Large")
            {
                return 3;
            }

            return fuzzyToInt(args.value, 0);
        });

    layout.addNavigationSpacing();
    layout.addTitle(this->tr("Beta"));
    if (Version::instance().isSupportedOS())
    {
        layout.addDescription(
            "You can receive updates earlier by ticking the box below. Report "
            "issues <a href='https://chatterino.com/link/issues'>here</a>.");

        SettingWidget::checkbox(this->tr("Receive beta updates"), s.betaUpdates)
            ->addTo(layout);
    }
    else
    {
        layout.addDescription(
            "Your operating system is not officially supplied with builds. For "
            "updates, please rebuild Chatterino from sources. Report "
            "issues <a href='https://chatterino.com/link/issues'>here</a>.");
    }

    layout.addTitle(this->tr("Browser Integration"));
#ifdef Q_OS_WIN
    layout.addDescription(
        "The browser extension replaces the default "
        "Twitch.tv chat with Chatterino, and updates the /watching split on "
        "Chatterino when Twitch.tv is open.");
#else
    layout.addDescription("The browser extension updates the /watching "
                          "split on Chatterino when Twitch.tv is open.");
#endif

    {
        if (auto err = nmIpcError().get())
        {
            layout.addDescription(
                "An error happened during initialization of the "
                "browser extension: " +
                *err);
        }
    }

    layout.addDescription(formatRichNamedLink(
        CHROME_EXTENSION_LINK,
        "Download for Google Chrome and similar browsers."));
    layout.addDescription(
        formatRichNamedLink(FIREFOX_EXTENSION_LINK, "Download for Firefox"));

#ifdef Q_OS_WIN
    if (getApp()->getModes().isPortable)
    {
        layout.addDescription(
            "Portable Chatterino does not register browser integration "
            "automatically. Registration writes to your Windows user registry "
            "to point your browser extension to this copy of Chatterino. "
            "You may manually register it below.");
        layout.addButton("Register browser integration", [this] {
            const auto result = registerNmHost(getApp()->getPaths());
            if (result)
            {
                QMessageBox::information(this, "Registration Successful",
                                         "Browser integration registered.");
            }
            else
            {
                QMessageBox::warning(
                    this, "Registration Failed",
                    "Failed to register browser integration:\n" +
                        result.error());
            }
        });
    }

    layout.addDescription("Chatterino only attaches to known browsers to avoid "
                          "attaching to other windows by accident.");
    SettingWidget::checkbox(this->tr("Attach to any browser (may cause issues)"),
                            s.attachExtensionToAnyProcess)
        ->setTooltip(
            this->tr("Attempt to force the Chatterino Browser Extension to work in ")
            "certain browsers that do not work automatically.\ne.g. Librewolf")
        ->addTo(layout);
#endif

    {
        auto *note = layout.addDescription(
            "A semicolon-separated list of Chrome or Firefox extension IDs "
            "allowed to interact with Chatterino's browser integration "
            "(requires restart).\n"
            "Using multiple extension IDs from different browsers may cause "
            "issues.");
        note->setWordWrap(true);
        note->setStyleSheet("color: #bbb");

        layout.addWidget(note);

        auto *form = new QFormLayout();
        layout.addLayout(form);

        SettingWidget::lineEdit("Extra extension IDs", s.additionalExtensionIDs,
                                "Extension;IDs;separated;by;semicolons")
            ->addTo(layout, form);
    }

#ifndef Q_OS_WIN
    {
        auto *note = layout.addDescription(
            "A path to write the native messaging manifest to. The manifest is "
            "already automatically created for Firefox and Google Chrome if "
            "they are installed."
#    ifdef Q_OS_LINUX
            "\nYou may use $XDG_CONFIG_HOME or $XDG_DATA_HOME in the path."
#    endif
        );
        note->setWordWrap(true);
        note->setStyleSheet("color: #bbb");
        layout.addWidget(note);

        auto *form = new QFormLayout();
        layout.addLayout(form);
        SettingWidget::lineEdit("Custom manifest path",
                                s.customNativeMessagingManifestPath,
                                "/full/path/to/native/messaging/manifest.json")
            ->addTo(layout, form);

        SettingWidget::dropdown("Custom manifest format",
                                s.customNativeMessagingManifestFormat)
            ->addTo(layout);
    }
#endif

    layout.addTitle(this->tr("AppData & Cache"));

    layout.addSubtitle(this->tr("Application Data"));
    layout.addDescription("All local files like settings and cache files are "
                          "stored in this directory.");
    layout.addButton("Open AppData directory", [] {
#ifdef Q_OS_DARWIN
        QDesktopServices::openUrl("file://" +
                                  getApp()->getPaths().rootAppDataDirectory);
#else
        QDesktopServices::openUrl(getApp()->getPaths().rootAppDataDirectory);
#endif
    });

    layout.addSubtitle(this->tr("Temporary files (Cache)"));
    layout.addDescription(
        "Files that are used often (such as emotes) are saved to disk to "
        "reduce bandwidth usage and to speed up loading.");

    auto *cachePathLabel = layout.addDescription("placeholder :D");
    getSettings()->cachePath.connect([cachePathLabel](const auto &,
                                                      auto) mutable {
        QString newPath = getApp()->getPaths().cacheDirectory();

        QString pathShortened = "Cache saved at <a href=\"file:///" + newPath +
                                R"("><span style="color: white;">)" +
                                shortenString(newPath, 50) + "</span></a>";
        cachePathLabel->setText(pathShortened);
        cachePathLabel->setToolTip(newPath);
    });

    // Choose and reset buttons
    {
        auto *box = new QHBoxLayout;

        box->addWidget(layout.makeButton("Choose cache path", [this]() {
            getSettings()->cachePath = QFileDialog::getExistingDirectory(this);
        }));
        box->addWidget(layout.makeButton("Reset", []() {
            getSettings()->cachePath = "";
        }));
        box->addWidget(layout.makeButton("Clear Cache", [&layout]() {
            auto reply = QMessageBox::question(
                layout.window(), "Clear cache",
                "Are you sure that you want to clear your cache? Emotes may "
                "take longer to load next time Chatterino is started.",
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes)
            {
                auto cacheDir = QDir(getApp()->getPaths().cacheDirectory());
                cacheDir.removeRecursively();
                cacheDir.mkdir(getApp()->getPaths().cacheDirectory());
            }
        }));
        box->addStretch(1);

        layout.addLayout(box);
    }

    layout.addTitle(this->tr("Sound"));

    SettingWidget::dropdown(this->tr("Sound backend (requires restart)"), s.soundBackend)
        ->setTooltip(this->tr("Change this only if you're noticing issues with sound ")
                     "playback on your system")
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Keep sound backend alive (requires restart)"),
                            s.soundMiniaudioKeepEngineAlive)
        ->setTooltip(
            this->tr("This setting makes Chatterino output silence to your sound ")
            "device, even if no ping is being played. Try this setting if you "
            "have issues with wireless devices not playing the first sound. "
            "Note that this can prevent your monitor or computer from "
            "sleeping.")
        ->addTo(layout);

    layout.addTitle(this->tr("Advanced"));

    layout.addSubtitle(this->tr("Chat title"));
    layout.addDescription("In live channels show:");
    SettingWidget::checkbox(this->tr("Uptime"), s.headerUptime)
        ->setTooltip(this->tr("Show how long the channel has been live"))
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Viewer count"), s.headerViewerCount)
        ->setTooltip(this->tr("Show how many users are watching"))
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Category"), s.headerGame)
        ->setTooltip(this->tr("Show what Category the stream is listed under"))
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Title"), s.headerStreamTitle)
        ->setTooltip(this->tr("Show the stream title"))
        ->addTo(layout);

    layout.addSubtitle(this->tr("Unique chat (R9K)"));
    auto toggleLocalr9kSeq = getApp()->getHotkeys()->getDisplaySequence(
        HotkeyCategory::Window, "toggleLocalR9K");
    QString toggleLocalr9kShortcut =
        "an assigned hotkey (Window -> Toggle local unique chat (R9K))";
    if (!toggleLocalr9kSeq.isEmpty())
    {
        toggleLocalr9kShortcut = toggleLocalr9kSeq.toString(
            QKeySequence::SequenceFormat::NativeText);
    }
    layout.addDescription("Hide similar messages to those previously seen. "
                          "Toggle hidden messages by pressing " +
                          toggleLocalr9kShortcut + ".");

    SettingWidget::checkbox(this->tr("Enable similarity checks"), s.similarityEnabled)
        ->addTo(layout);

    // SettingWidget::checkbox(this->tr("Gray out matches"), s.colorSimilarDisabled)->addTo(layout);

    SettingWidget::checkbox(this->tr("Only if by the same user"), s.hideSimilarBySameUser)
        ->setTooltip(
            this->tr("When checked, messages that are very similar to each other can ")
            "still be shown as long as they're sent by different users.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Hide my own messages"), s.hideSimilarMyself)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Receive notification sounds from hidden messages"),
                            s.shownSimilarTriggerHighlights)
        ->addTo(layout);

    s.hideSimilar.connect(
        []() {
            getApp()->getWindows()->forceLayoutChannelViews();
        },
        false);
    layout.addDropdown<float>(
        this->tr("Similarity threshold"), {"0.5", "0.75", "0.9"}, s.similarityPercentage,
        [](auto val) {
            return QString::number(val);
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 0.9F);
        },
        true,
        "A value of 0.9 means the messages need to be 90% similar to be marked "
        "as similar.");
    layout.addDropdown<int>(
        this->tr("Maximum delay between messages"),
        {"5s", "10s", "15s", "30s", "60s", "120s"}, s.hideSimilarMaxDelay,
        [](auto val) {
            return QString::number(val) + "s";
        },
        [](auto args) {
            return fuzzyToInt(args.value, 5);
        },
        true,
        "A value of 5s means if there's a 5s break between messages, we will "
        "stop looking further through the messages for similarities.");
    layout.addDropdown<int>(
        this->tr("Amount of previous messages to check"), {"1", "2", "3", "4", "5"},
        s.hideSimilarMaxMessagesToCheck,
        [](auto val) {
            return QString::number(val);
        },
        [](auto args) {
            return fuzzyToInt(args.value, 3);
        },
        true,
        "How many messages in the history should be compared to a new one to "
        "establish its similarity rating. Messages in the history will be "
        "compared to only if they are new enough.");

    layout.addSubtitle(this->tr("Visible badges"));
    SettingWidget::checkbox(this->tr("Authority"), s.showBadgesGlobalAuthority)
        ->setTooltip(this->tr("e.g. staff, admin"))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Predictions"), s.showBadgesPredictions)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Channel"), s.showBadgesChannelAuthority)
        ->setTooltip(this->tr("e.g. broadcaster, moderator"))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Subscriber "), s.showBadgesSubscription)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Vanity"), s.showBadgesVanity)
        ->setTooltip(this->tr("e.g. prime, bits, sub gifter"))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Chatterino"), s.showBadgesChatterino)
        ->setTooltip(this->tr("e.g. Chatterino Supporter/Contributor/Developer"))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("FrankerFaceZ"), s.showBadgesFfz)
        ->addKeywords({"ffz"})
        ->setTooltip(this->tr("e.g. Bot, FrankerFaceZ supporter, FrankerFaceZ developer"))
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("7TV"), s.showBadgesSevenTV)
        ->addKeywords({"seventv"})
        ->setTooltip(this->tr("Badges for 7TV admins, developers, and supporters"))
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("BetterTTV"), s.showBadgesBttv)
        ->addKeywords({"bttv"})
        ->addTo(layout);
    layout.addSeparator();
    SettingWidget::checkbox(this->tr("Use custom FrankerFaceZ moderator badges"),
                            s.useCustomFfzModeratorBadges)
        ->addKeywords({"ffz"})
        ->addTo(layout);
    SettingWidget::checkbox(this->tr("Use custom FrankerFaceZ VIP badges"),
                            s.useCustomFfzVipBadges)
        ->addKeywords({"ffz"})
        ->addTo(layout);

    layout.addSubtitle(this->tr("Overlay"));
    layout.addDropdown<float>(
        this->tr("Zoom factor"), ZOOM_LEVELS, s.overlayScaleFactor,
        [](auto val) {
            if (val == 1)
            {
                return u"Default"_s;
            }
            return QString::number(val) + 'x';
        },
        [](const auto &args) {
            return fuzzyToFloat(args.value, 1.F);
        },
        true,
        "The final scale of the messages in the overlay is computed by "
        "multiplying this zoom factor with the global zoom level.");

    SettingWidget::intInput("Background opacity (0-255)",
                            s.overlayBackgroundOpacity,
                            {
                                .min = 0,
                                .max = 255,
                                .singleStep = 1,
                            })
        ->setTooltip(
            this->tr("Controls the opacity of the (possibly alternating) background ")
            "behind messages. The color is set through the current theme. 255 "
            "corresponds to a fully opaque background.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Enable Shadow"), s.enableOverlayShadow)
        ->setTooltip(this->tr("Enables a drop shadow on the overlay. This will use more ")
                     "processing power.")
        ->addTo(layout);

    SettingWidget::intInput("Shadow opacity (0-255)", s.overlayShadowOpacity,
                            {
                                .min = 0,
                                .max = 255,
                                .singleStep = 1,
                            })
        ->setTooltip(this->tr("Controls the opacity of the added drop shadow. 255 ")
                     "corresponds to a fully opaque shadow.")
        ->addTo(layout);

    SettingWidget::colorButton("Shadow color", s.overlayShadowColor)
        ->addTo(layout);

    SettingWidget::intInput("Shadow radius", s.overlayShadowRadius,
                            {
                                .min = 0,
                                .max = 40,
                                .singleStep = 1,
                                .suffix = "dp",
                            })
        ->setTooltip(this->tr("Controls how far the shadow is spread (the blur ")
                     "radius) in device-independent pixels.")
        ->addTo(layout);

    SettingWidget::intInput("Shadow offset x", s.overlayShadowOffsetX,
                            {
                                .min = -20,
                                .max = 20,
                                .singleStep = 1,
                                .suffix = "dp",
                            })
        ->setTooltip(this->tr("Controls how far the shadow is offset on the x axis in ")
                     "device-independent pixels. A negative value offsets to "
                     "the left and a positive to the right.")
        ->addTo(layout);

    SettingWidget::intInput("Shadow offset y", s.overlayShadowOffsetY,
                            {
                                .min = -20,
                                .max = 20,
                                .singleStep = 1,
                                .suffix = "dp",
                            })
        ->setTooltip(this->tr("Controls how far the shadow is offset on the y axis in ")
                     "device-independent pixels. A negative value offsets to "
                     "the top and a positive to the bottom.")
        ->addTo(layout);

    {
        layout.addSubtitle(this->tr("Search"));
        layout.addDescription(
            "Search engine which appears when you select text and right-click "
            "a message. Select a search engine preset from the dropdown below, "
            "or fill in your custom search engine URL and name.");
        SettingWidget::checkbox(this->tr("Enable search in right-click context menu"),
                                s.searchEnabled)
            ->setTooltip(
                this->tr("Allow searching selected text using a search engine from ")
                "the right-click context menu.")
            ->addTo(layout);

        // Preset dropdown
        QStringList presetList = {"DuckDuckGo", "Bing", "Google"};
        auto *presetCombo =
            layout.addDropdown(this->tr("Search engine preset"), presetList,
                               "Select a search engine preset");
        presetCombo->setPlaceholderText("Select...");
        presetCombo->setCurrentIndex(-1);
        // Make placeholder text more visible
        QPalette palette = presetCombo->palette();
        palette.setColor(QPalette::PlaceholderText,
                         QColor(255, 255, 255));  // white
        presetCombo->setPalette(palette);
        s.searchEnabled.connect([presetCombo](bool value) {
            presetCombo->setEnabled(value);
        });

        // Connect preset dropdown to update URL and name settings
        QObject::connect(
            presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [&s, presetCombo](int index) {
                if (index < 0)
                    return;

                QString preset = presetCombo->itemText(index);
                if (preset == "DuckDuckGo")
                {
                    s.searchEngineUrl = "https://duckduckgo.com/?q=";
                    s.searchEngineName = "DuckDuckGo";
                }
                else if (preset == "Bing")
                {
                    s.searchEngineUrl = "https://www.bing.com/search?q=";
                    s.searchEngineName = "Bing";
                }
                else if (preset == "Google")
                {
                    s.searchEngineUrl = "https://www.google.com/search?q=";
                    s.searchEngineName = "Google";
                }
                // Reset to -1 after selection
                {
                    QSignalBlocker blocker(presetCombo);
                    presetCombo->setCurrentIndex(-1);
                }
            });

        // URL and Name text inputs
        SettingWidget::lineEdit("Search engine URL", s.searchEngineUrl)
            ->conditionallyEnabledBy(s.searchEnabled)
            ->addTo(layout);

        SettingWidget::lineEdit("Search engine name", s.searchEngineName)
            ->conditionallyEnabledBy(s.searchEnabled)
            ->addTo(layout);
    }
    if (supportsIncognitoLinks())
    {
        SettingWidget::checkbox(this->tr("Search in incognito/private mode"),
                                s.searchIncognito)
            ->addTo(layout);
    }

    layout.addSubtitle(this->tr("Miscellaneous"));

    if (supportsIncognitoLinks())
    {
        SettingWidget::checkbox(this->tr("Open links in incognito/private mode"),
                                s.openLinksIncognito)
            ->addTo(layout);
    }

    SettingWidget::customCheckbox(
        "Restart on crash (requires restart)",
        getApp()->getCrashHandler()->shouldRecover(),
        [](bool on) {
            getApp()->getCrashHandler()->saveShouldRecover(on);
        })
        ->setTooltip(this->tr("When possible, restart Chatterino if the program crashes"))
        ->addTo(layout);

    SettingWidget::inverseCheckbox("Show moderation messages",
                                   s.hideModerationActions)
        ->setTooltip(
            this->tr("Show messages for timeouts, bans, and other moderator actions."))
        ->addTo(layout);

    SettingWidget::inverseCheckbox("Show deletions of single messages",
                                   s.hideDeletionActions)
        ->setTooltip(this->tr("Show when a single message is deleted.\ne.g. A message ")
                     "from TreuKS was deleted: abc")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Colorize users without color set (gray names)"),
                            s.colorizeNicknames)
        ->setTooltip(this->tr("Grant a random color to users who never set a color for ")
                     "themselves")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Mention users with a comma"),
                            s.mentionUsersWithComma)
        ->setTooltip(this->tr("When using tab-completion, if the username is at the ")
                     "start of the message, include a comma at the end of the "
                     "name.\ne.g. pajl -> pajlada,")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show joined users (< 1000 chatters)"), s.showJoins)
        ->setTooltip(
            this->tr("Show a Twitch system message stating what users have joined the ")
            "chat, only available when the chat has less than 1000 users")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show parted users (< 1000 chatters)"), s.showParts)
        ->setTooltip(
            this->tr("Show a Twitch system message stating what users have left the ")
            "chat, only available when chat has less than 1000 users")
        ->addTo(layout);

    SettingWidget::checkbox(
        this->tr("Automatically close user popup when it loses focus"),
        s.autoCloseUserPopup)
        ->addTo(layout);

    SettingWidget::checkbox(
        this->tr("Automatically close reply thread popup when it loses focus"),
        s.autoCloseThreadPopup)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Always show pinned channel message"),
                            s.alwaysShowPinnedMessage)
        ->setTooltip(
            this->tr("When enabled, pinned messages will stay visible instead of ")
            "automatically hiding after a few seconds.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Lowercase domains (anti-phishing)"),
                            s.lowercaseDomains)
        ->setTooltip(
            this->tr("Make all clickable links lowercase to deter phishing attempts."))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show user's pronouns in user card"), s.showPronouns)
        ->setDescription(
            R"(Pronouns are retrieved from <a href="https://pr.alejo.io">pr.alejo.io</a> when a user card is opened.)")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show stream title in live message"),
                            s.showTitleInLiveMessage)
        ->setTooltip(this->tr("The title in the message will be the title the streamer ")
                     "set when they went live, and will not update as the "
                     "streamer updates their title.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Bold @usernames"), s.boldUsernames)
        ->setTooltip(this->tr("Bold @mentions to make them more noticeable."))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Color @usernames"), s.colorUsernames)
        ->setTooltip(this->tr("If Chatterino has seen a user, highlight @mention's of ")
                     "them with their Twitch color.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Try to find usernames without @ prefix"),
                            s.findAllUsernames)
        ->setTooltip(this->tr("Find mentions of users in chat without the @ prefix."))
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show username autocompletion popup menu"),
                            s.showUsernameCompletionMenu)
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Always include broadcaster in user completions"),
                            s.alwaysIncludeBroadcasterInUserCompletions)
        ->setTooltip(
            this->tr("This will ensure a broadcaster is always easy to ping, even if ")
            "they don't have chat open or have typed recently.")
        ->addTo(layout);

    const QStringList usernameDisplayModes = {"Username", "Localized name",
                                              "Username and localized name"};

    ComboBox *nameDropdown =
        layout.addDropdown<std::underlying_type_t<UsernameDisplayMode>>(
            this->tr("Username style"), usernameDisplayModes, s.usernameDisplayMode,
            [usernameDisplayModes](auto val) {
                return usernameDisplayModes.at(val - 1);
                // UsernameDisplayMode enum indexes from 1
            },
            [](auto args) {
                return args.index + 1;
            },
            false,
            "Customizes how you see Asian Language names.\nUsing an option "
            "that includes \"localized\" will display the username in its "
            "respective Asian language.\ne.g. "
            "Username and localized: testaccount_420(테스트계정420)\n"
            "Username: testaccount_420\n"
            "Localized name: 테스트계정420");
    nameDropdown->setMinimumWidth(nameDropdown->minimumSizeHint().width());

    layout.addDropdown<float>(
        this->tr("Username font weight"), {"50", "Default", "75", "100"}, s.boldScale,
        [](auto val) {
            if (val == 63)
            {
                return QString("Default");
            }
            else
            {
                return QString::number(val);
            }
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 63.f);
        });

    SettingWidget::checkbox(
        this->tr("Double click to open links and other elements in chat"),
        s.linksDoubleClickOnly)
        ->setTooltip(this->tr("When enabled, opening links/usercards requires ")
                     "double-clicking.\nUseful for making sure you don't "
                     "accidentally click on suspicious links.\nClicking a link "
                     "once will pause the chat briefly to allow for a less "
                     "accident-prone double-clicking.")
        ->addKeywords({"pause"})
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Unshorten links"), s.unshortLinks)
        ->setTooltip(this->tr("When enabled, \"right-click + copy link\" will copy the ")
                     "unshortened version of the link.\ne.g. "
                     "https://bit.ly/mrfors -> https://forsen.tv/")
        ->addTo(layout);

    SettingWidget::checkbox(
        this->tr("Only search for emote autocompletion at the start of emote names"),
        s.prefixOnlyEmoteCompletion)
        ->setTooltip(this->tr("When disabled, emote tab-completion will complete based ")
                     "on any part of the name.\ne.g. sheffy -> DatSheffy")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Only search for username autocompletion with an @"),
                            s.userCompletionOnlyWithAt)
        ->setTooltip(this->tr("When enabled, username tab-completion will only complete ")
                     "when using @\ne.g. pajl -> pajl | @pajl -> @pajlada")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show Twitch whispers inline"), s.inlineWhispers)
        ->setTooltip(this->tr("Show whispers as messages in all splits instead of just ")
                     "/whispers.\nHighlighting of this is now controlled in "
                     "the Highlights page.")
        ->addTo(layout);

    SettingWidget::checkbox(
        this->tr("Automatically subscribe to participated reply threads"),
        s.autoSubToParticipatedThreads)
        ->setTooltip(
            this->tr("When enabled, you will automatically subscribe to reply threads ")
            "you participate in.\nThis means reply threads you participate in "
            "will use your \"Subscribed Reply Threads\" highlight settings.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Load message history on connect"),
                            s.loadTwitchMessageHistoryOnConnect)
        ->addTo(layout);

    SettingWidget::lineEdit("Message history URL", s.messageHistoryUrl,
                            recentmessages::DEFAULT_API_URL.toString())
        ->setTooltip(
            this->tr("Use %1 where the channel name should be inserted, for example: ") +
            recentmessages::DEFAULT_API_URL.toString())
        ->addTo(layout);

    // TODO: Change phrasing to use better english once we can tag settings, right now it's kept as history instead of historical so that the setting shows up when the user searches for history
    SettingWidget::intInput("Max number of history messages to load on connect",
                            s.twitchMessageHistoryLimit,
                            {
                                .min = 10,
                                .max = 800,
                                .singleStep = 10,
                            })
        ->addTo(layout);

    SettingWidget::intInput("Split message scrollback limit (requires restart)",
                            s.scrollbackSplitLimit,
                            {
                                .min = 100,
                                .max = 100000,
                                .singleStep = 100,
                            })
        ->addTo(layout);

    SettingWidget::intInput("Usercard scrollback limit (requires restart)",
                            s.scrollbackUsercardLimit,
                            {
                                .min = 100,
                                .max = 100000,
                                .singleStep = 100,
                            })
        ->addTo(layout);

    SettingWidget::dropdown("Show blocked term automod messages",
                            s.showBlockedTermAutomodMessages)
        ->setTooltip(this->tr("Show messages that are blocked by AutoMod for containing ")
                     "a public blocked term in the current channel.")
        ->addTo(layout);

    layout.addDropdown<int>(
        this->tr("Stack timeouts"), {this->tr("Stack"), this->tr("Stack until timeout"), this->tr("Don't stack")},
        s.timeoutStackStyle,
        [](int index) {
            return index;
        },
        [](auto args) {
            return args.index;
        },
        false, "Combine consecutive timeout messages into a single message.");

    SettingWidget::checkbox(this->tr("Combine multiple bit tips into one"), s.stackBits)
        ->setTooltip(this->tr("Combine consecutive cheermotes (sent in a single ")
                     "message) into one cheermote.")
        ->addTo(layout);

    // update this tooltip if https://github.com/Chatterino/chatterino2/pull/1557 is ever merged
    SettingWidget::checkbox(this->tr("Messages in /mentions highlights tab"),
                            s.highlightMentions)
        ->setTooltip(this->tr("When disabled, the /mentions tab will not highlight in ")
                     "red when you are mentioned.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Strip leading mention in replies"),
                            s.stripReplyMention)
        ->setTooltip(
            this->tr("When disabled, messages sent in reply threads will include the ")
            "@mention for the related thread. If the reply context is hidden, "
            "these mentions will never be stripped.")
        ->addTo(layout);

    SettingWidget::dropdown("Chat send protocol", s.chatSendProtocol)
        ->setTooltip(this->tr("'Helix' will use Twitch's Helix API to send messages. ")
                     "'IRC' will use IRC to send messages.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show send message button"), s.showSendButton)
        ->setTooltip(this->tr("Show a Send button next to each split input that can be ")
                     "clicked to send the message")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Disable renaming of tabs on double-click"),
                            s.disableTabRenamingOnClick)
        ->setTooltip(this->tr("Prevents the rename dialog from opening when a tab is ")
                     "double-clicked")
        ->addTo(layout);

    SettingWidget::intInput(
        "Shared chat session status refresh interval",
        s.sharedChatSessionRefreshInterval,
        {.min = 5, .max = 999, .singleStep = 1, .suffix = "s"})
        ->setTooltip(this->tr("How often Chatterino polls the Twitch API for the ")
                     "shared chat session status.")
        ->addTo(layout);

    SettingWidget::checkbox(this->tr("Show shared chat badge for all messages"),
                            s.sharedChatAlwaysShowBadge)
        ->setTooltip(
            this->tr("If turned off, only messages from other participants have a ")
            "shared chat badge")
        ->addTo(layout);

    layout.addStretch();

    // invisible element for width
    auto *inv = new BaseWidget(this);
    //    inv->setScaleIndependentWidth(600);
    layout.addWidget(inv);
}

void GeneralPage::initExtra()
{
    /// update cache path
    if (this->cachePath_)
    {
        getSettings()->cachePath.connect(
            [cachePath = this->cachePath_](const auto &, auto) mutable {
                QString newPath = getApp()->getPaths().cacheDirectory();

                QString pathShortened = "Current location: <a href=\"file:///" +
                                        newPath + "\">" +
                                        shortenString(newPath, 50) + "</a>";

                cachePath->setText(pathShortened);
                cachePath->setToolTip(newPath);
            });
    }
}

}  // namespace chatterino
