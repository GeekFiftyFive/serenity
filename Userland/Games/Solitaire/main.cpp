/*
 * Copyright (c) 2020, Till Mayer <till.mayer@web.de>
 * Copyright (c) 2021, the SerenityOS developers.
 * Copyright (c) 2022-2023, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Game.h"
#include <AK/URL.h>
#include <Games/Solitaire/SolitaireGML.h>
#include <LibConfig/Client.h>
#include <LibCore/System.h>
#include <LibCore/Timer.h>
#include <LibDesktop/Launcher.h>
#include <LibGUI/Action.h>
#include <LibGUI/ActionGroup.h>
#include <LibGUI/Application.h>
#include <LibGUI/Icon.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Menubar.h>
#include <LibGUI/MessageBox.h>
#include <LibGUI/Statusbar.h>
#include <LibGUI/Window.h>
#include <LibMain/Main.h>
#include <stdio.h>

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    TRY(Core::System::pledge("stdio recvfd sendfd rpath unix proc exec"));

    auto app = TRY(GUI::Application::try_create(arguments));
    auto app_icon = TRY(GUI::Icon::try_create_default_icon("app-solitaire"sv));

    auto const man_file = "/usr/share/man/man6/Solitaire.md"sv;

    TRY(Desktop::Launcher::add_allowed_handler_with_only_specific_urls("/bin/Help", { URL::create_with_file_scheme(man_file) }));
    TRY(Desktop::Launcher::seal_allowlist());

    Config::pledge_domains({ "Games", "Solitaire" });
    Config::monitor_domain("Games");

    TRY(Core::System::pledge("stdio recvfd sendfd rpath proc exec"));

    TRY(Core::System::unveil("/res", "r"));
    TRY(Core::System::unveil("/bin/GamesSettings", "x"));
    TRY(Core::System::unveil(nullptr, nullptr));

    auto window = TRY(GUI::Window::try_create());
    window->set_title("Solitaire");

    auto mode = static_cast<Solitaire::Mode>(Config::read_u32("Solitaire"sv, "Settings"sv, "Mode"sv, to_underlying(Solitaire::Mode::SingleCardDraw)));

    auto update_mode = [&](Solitaire::Mode new_mode) {
        mode = new_mode;
        Config::write_u32("Solitaire"sv, "Settings"sv, "Mode"sv, to_underlying(mode));
    };

    auto high_score = [&]() {
        switch (mode) {
        case Solitaire::Mode::SingleCardDraw:
            return Config::read_u32("Solitaire"sv, "HighScores"sv, "SingleCardDraw"sv, 0);
        case Solitaire::Mode::ThreeCardDraw:
            return Config::read_u32("Solitaire"sv, "HighScores"sv, "ThreeCardDraw"sv, 0);
        default:
            VERIFY_NOT_REACHED();
        }
    };

    auto update_high_score = [&](u32 new_high_score) {
        switch (mode) {
        case Solitaire::Mode::SingleCardDraw:
            Config::write_u32("Solitaire"sv, "HighScores"sv, "SingleCardDraw"sv, new_high_score);
            break;
        case Solitaire::Mode::ThreeCardDraw:
            Config::write_u32("Solitaire"sv, "HighScores"sv, "ThreeCardDraw"sv, new_high_score);
            break;
        default:
            VERIFY_NOT_REACHED();
        }
    };

    if (mode >= Solitaire::Mode::__Count)
        update_mode(Solitaire::Mode::SingleCardDraw);

    auto widget = TRY(window->set_main_widget<GUI::Widget>());
    TRY(widget->load_from_gml(solitaire_gml));

    auto& game = *widget->find_descendant_of_type_named<Solitaire::Game>("game");
    game.set_focus(true);

    auto& statusbar = *widget->find_descendant_of_type_named<GUI::Statusbar>("statusbar");
    statusbar.set_text(0, "Score: 0");
    statusbar.set_text(1, DeprecatedString::formatted("High Score: {}", high_score()));
    statusbar.set_text(2, "Time: 00:00:00");

    app->on_action_enter = [&](GUI::Action& action) {
        auto text = action.status_tip();
        if (text.is_empty())
            text = Gfx::parse_ampersand_string(action.text());
        statusbar.set_override_text(move(text));
    };

    app->on_action_leave = [&](GUI::Action&) {
        statusbar.set_override_text({});
    };

    game.on_score_update = [&](uint32_t score) {
        statusbar.set_text(0, DeprecatedString::formatted("Score: {}", score));
    };

    uint64_t seconds_elapsed = 0;

    auto timer = TRY(Core::Timer::create_repeating(1000, [&]() {
        ++seconds_elapsed;

        uint64_t hours = seconds_elapsed / 3600;
        uint64_t minutes = (seconds_elapsed / 60) % 60;
        uint64_t seconds = seconds_elapsed % 60;

        statusbar.set_text(2, DeprecatedString::formatted("Time: {:02}:{:02}:{:02}", hours, minutes, seconds));
    }));

    game.on_game_start = [&]() {
        seconds_elapsed = 0;
        timer->start();
        statusbar.set_text(2, "Time: 00:00:00");
    };
    game.on_game_end = [&](Solitaire::GameOverReason reason, uint32_t score) {
        if (timer->is_active())
            timer->stop();

        if (reason == Solitaire::GameOverReason::Victory) {
            if (seconds_elapsed >= 30) {
                uint32_t bonus = (20'000 / seconds_elapsed) * 35;
                statusbar.set_text(0, DeprecatedString::formatted("Score: {} (Bonus: {})", score, bonus));
                score += bonus;
            }

            if (score > high_score()) {
                update_high_score(score);
                statusbar.set_text(1, DeprecatedString::formatted("High Score: {}", score));
            }
        }
        statusbar.set_text(2, "Timer starts after your first move");
    };

    auto confirm_end_current_game = [&]() {
        auto game_in_progress = timer->is_active();
        if (game_in_progress) {
            auto result = GUI::MessageBox::show(window,
                "A game is still in progress, are you sure you would like to end it?"sv,
                "Game in progress"sv,
                GUI::MessageBox::Type::Warning,
                GUI::MessageBox::InputType::YesNo);

            return result == GUI::MessageBox::ExecResult::Yes;
        }

        return true;
    };

    window->on_close_request = [&]() {
        if (confirm_end_current_game())
            return GUI::Window::CloseRequestDecision::Close;
        return GUI::Window::CloseRequestDecision::StayOpen;
    };

    GUI::ActionGroup draw_setting_actions;
    draw_setting_actions.set_exclusive(true);

    auto single_card_draw_action = GUI::Action::create_checkable("&Single Card Draw", [&](auto&) {
        update_mode(Solitaire::Mode::SingleCardDraw);

        if (!confirm_end_current_game())
            return;

        statusbar.set_text(1, DeprecatedString::formatted("High Score: {}", high_score()));
        game.setup(mode);
    });
    single_card_draw_action->set_checked(mode == Solitaire::Mode::SingleCardDraw);
    single_card_draw_action->set_status_tip("Draw one card at a time");
    draw_setting_actions.add_action(single_card_draw_action);

    auto three_card_draw_action = GUI::Action::create_checkable("&Three Card Draw", [&](auto&) {
        update_mode(Solitaire::Mode::ThreeCardDraw);

        if (!confirm_end_current_game())
            return;

        statusbar.set_text(1, DeprecatedString::formatted("High Score: {}", high_score()));
        game.setup(mode);
    });
    three_card_draw_action->set_checked(mode == Solitaire::Mode::ThreeCardDraw);
    three_card_draw_action->set_status_tip("Draw three cards at a time");
    draw_setting_actions.add_action(three_card_draw_action);

    game.set_auto_collect(Config::read_bool("Solitaire"sv, "Settings"sv, "AutoCollect"sv, false));
    auto toggle_auto_collect_action = GUI::Action::create_checkable("Auto-&Collect", [&](auto& action) {
        auto checked = action.is_checked();
        game.set_auto_collect(checked);
        Config::write_bool("Solitaire"sv, "Settings"sv, "AutoCollect"sv, checked);
    });
    toggle_auto_collect_action->set_checked(game.is_auto_collecting());
    toggle_auto_collect_action->set_status_tip("Auto-collect to foundation piles");

    auto game_menu = TRY(window->try_add_menu("&Game"));

    TRY(game_menu->try_add_action(GUI::Action::create("&New Game", { Mod_None, Key_F2 }, TRY(Gfx::Bitmap::try_load_from_file("/res/icons/16x16/reload.png"sv)), [&](auto&) {
        if (!confirm_end_current_game())
            return;

        game.setup(mode);
    })));
    TRY(game_menu->try_add_separator());
    auto undo_action = GUI::CommonActions::make_undo_action([&](auto&) {
        game.perform_undo();
    });
    undo_action->set_enabled(false);
    TRY(game_menu->try_add_action(undo_action));
    TRY(game_menu->try_add_separator());
    TRY(game_menu->try_add_action(TRY(Cards::make_cards_settings_action(window))));
    TRY(game_menu->try_add_action(single_card_draw_action));
    TRY(game_menu->try_add_action(three_card_draw_action));
    TRY(game_menu->try_add_separator());
    TRY(game_menu->try_add_action(toggle_auto_collect_action));
    TRY(game_menu->try_add_separator());
    TRY(game_menu->try_add_action(GUI::CommonActions::make_quit_action([&](auto&) { app->quit(); })));

    auto help_menu = TRY(window->try_add_menu("&Help"));
    TRY(help_menu->try_add_action(GUI::CommonActions::make_command_palette_action(window)));
    TRY(help_menu->try_add_action(GUI::CommonActions::make_about_action("Solitaire", app_icon, window)));

    TRY(help_menu->try_add_action(GUI::CommonActions::make_help_action([&man_file](auto&) {
        Desktop::Launcher::open(URL::create_with_file_scheme(man_file), "/bin/Help");
    })));

    window->set_resizable(false);
    window->resize(Solitaire::Game::width, Solitaire::Game::height + statusbar.max_height().as_int());
    window->set_icon(app_icon.bitmap_for_size(16));
    window->show();

    game.on_undo_availability_change = [&](bool undo_available) {
        undo_action->set_enabled(undo_available);
    };

    game.setup(mode);

    return app->exec();
}
