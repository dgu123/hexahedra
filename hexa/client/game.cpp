//---------------------------------------------------------------------------
// client/game.cpp
//
// This file is part of Hexahedra.
//
// Hexahedra is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Copyright 2013, nocte@hippie.nu
//---------------------------------------------------------------------------

#include "game.hpp"

#include <boost/chrono.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/range/algorithm.hpp>
#include <GL/glew.h>
#include <GL/gl.h>
#include <SFML/Graphics.hpp>

#include <hexa/config.hpp>
#include <hexa/log.hpp>
#include <hexa/os.hpp>
#include <hexa/trace.hpp>

#include "game_state.hpp"
#include "event.hpp"

using namespace boost::range;
using boost::format;
namespace fs = boost::filesystem;

namespace hexa
{

game::game(const std::string& title, unsigned int width, unsigned int height)
    : window_(sf::VideoMode(width, height, 32), title, sf::Style::Default,
              sf::ContextSettings(16, 8, 4))
    , window_title_(title)
    , width_(width)
    , height_(height)
    , rel_mouse_(false)
    , fullscreen_(false)
    , time_(0.0)
{
    std::string icon_file(PIXMAP_PATH "/hexahedra.png");
    if (fs::exists(icon_file)) {
        sf::Image app_icon;
        app_icon.loadFromFile(PIXMAP_PATH "/hexahedra.png");
        window_.setIcon(app_icon.getSize().x, app_icon.getSize().y,
                        app_icon.getPixelsPtr());
    }
    fill(key_pressed_, false);
    fill(joy_btn_pressed_, false);
    fill(mouse_btn_pressed_, false);
    fill(joy_axis_, 0.0f);
    window_.setVerticalSyncEnabled(true);
}

void game::run(std::unique_ptr<game_state> initial_state)
{
    using namespace boost::chrono;

    auto size(window_.getSize());
    initial_state->resize(size.x, size.y);
    initial_state->expose();

    states_.emplace_back(std::move(initial_state));
    auto last_tick(steady_clock::now());
    auto started(last_tick);

    while (!states_.empty()) {
        poll_events();

        auto i(std::prev(states_.end()));
        while (i != states_.begin() && (**i).is_transparent())
            --i;

        auto current_time(steady_clock::now());
        auto delta(current_time - last_tick);
        last_tick = current_time;
        auto delta_seconds(duration_cast<microseconds>(delta).count()
                           * 1.0e-6);

        auto total_passed(current_time - started);
        time_ = duration_cast<microseconds>(total_passed).count() * 1.0e-6;

        try {
            for (; i != states_.end(); ++i) {
                (**i).update(delta_seconds);
                glClear(GL_DEPTH_BUFFER_BIT);
                (**i).render();
            }
            window_.display();
        } catch (std::exception& e) {
            trace("Uncaught exception in game state: %1%",
                  std::string(e.what()));
            log_msg("Uncaught exception in game state: %1%", e.what());

            states_.pop_back();
            if (!states_.empty())
                states_.back()->expose();
        }

        if (states_.back()->is_done()) {
            try {
                auto t(states_.back()->next_state());
                if (t.state == nullptr) {
                    states_.pop_back();
                } else {
                    if (t.replace_current)
                        states_.pop_back();

                    states_.emplace_back(std::move(t.state));
                }

                if (!states_.empty()) {
                    states_.back()->expose();
                }
            } catch (std::exception& e) {
                trace("Game state transition failed: %1%",
                      std::string(e.what()));
                log_msg("Game state transition failed: %1%", e.what());

                if (!states_.empty())
                    states_.back()->expose();
            }
        }
    }
}

void game::poll_events()
{
    vector2<float> mouse_move(0.0f, 0.0f);
    sf::Event ev;

    while (window_.pollEvent(ev)) {
        switch (ev.type) {
        case sf::Event::Closed:
            process_event(event::window_close);
            break;

        case sf::Event::Resized:
            resize(ev.size.width, ev.size.height);
            break;

        case sf::Event::KeyPressed: {
            uint32_t keycode(ev.key.code);
            if (keycode < key_pressed_.size())
                key_pressed_[keycode] = true;

            process_event({event::key_down, keycode});
            handle_keypress(keycode);
        } break;

        case sf::Event::KeyReleased:
            if ((unsigned)ev.key.code < key_pressed_.size())
                key_pressed_[ev.key.code] = false;

            process_event({event::key_up, (uint32_t)ev.key.code});
            break;

        case sf::Event::MouseMoved:
            if (mouse_is_relative()) {
                vector2<float> rel(ev.mouseMove.x - int(width_ * 0.5f),
                                   ev.mouseMove.y - int(height_ * 0.5f));

                mouse_move += rel;
            } else {
                mouse_pos_.x = ev.mouseMove.x;
                mouse_pos_.y = ev.mouseMove.y;

                vector2<float> pos(ev.mouseMove.x, ev.mouseMove.y);
                process_event({event::mouse_move_abs, pos});
            }
            break;

        case sf::Event::MouseButtonPressed:
            mouse_btn_pressed_[ev.mouseButton.button] = true;
            process_event({event::mouse_button_down, ev.mouseButton.button});
            break;

        case sf::Event::MouseButtonReleased:
            mouse_btn_pressed_[ev.mouseButton.button] = false;
            process_event({event::mouse_button_up, ev.mouseButton.button});
            break;

        case sf::Event::MouseWheelMoved:
            process_event({event::mouse_wheel, ev.mouseWheel.delta});
            break;

        case sf::Event::TextEntered:
            process_event({event::key_text, ev.text.unicode});
            break;

        case sf::Event::JoystickButtonPressed:
            process_event({event::joy_button_down, ev.joystickButton.button});
            break;

        case sf::Event::JoystickButtonReleased:
            process_event({event::joy_button_up, ev.joystickButton.button});
            break;

        case sf::Event::JoystickMoved: {
            struct event::axis_info temp
                = {static_cast<uint8_t>(ev.joystickMove.axis),
                   ev.joystickMove.position * 0.01f};

            joy_axis_[temp.id] = temp.position;
            process_event({event::joy_move, temp});
        } break;

        default:
            ; // do nothing
        }
    }

    if (mouse_is_relative() && mouse_move != vector2<float>(0, 0)) {
        sf::Mouse::setPosition(sf::Vector2i(width_ * 0.5, height_ * 0.5),
                               window());
        process_event({event::mouse_move_rel, mouse_move});
    }
}

void game::process_event(const event& ev)
{
    for (auto i(states_.rbegin()); i != states_.rend(); ++i) {
        if ((*i)->process_event(ev))
            return;
    }
}

bool game::key_pressed(key code) const
{
    return sf::Keyboard::isKeyPressed(static_cast<sf::Keyboard::Key>(code));
}

bool game::mouse_button_pressed(unsigned int button) const
{
    if (button >= mouse_btn_pressed_.size())
        return false;

    return mouse_btn_pressed_[button];
}

float game::joystick_pos(unsigned int axis) const
{
    if (axis >= joy_axis_.size())
        return 0.0f;

    return joy_axis_[axis];
}

vector2<int> game::mouse_pos() const
{
    return mouse_pos_;
}

void game::relative_mouse(bool on)
{
    if (on == rel_mouse_)
        return;

    rel_mouse_ = on;
    if (on) {
        sf::Mouse::setPosition(sf::Vector2i(width_ * 0.5, height_ * 0.5),
                               window());
        window().setMouseCursorVisible(false);
    } else {
        window().setMouseCursorVisible(true);
    }
}

void game::handle_keypress(uint32_t keycode)
{
    switch (key(keycode)) {
    case key::f2: {
        using namespace boost::posix_time;

        std::string png_file(
            (format("%1%/screenshot-%2%.jpg") % app_user_dir().string()
             % to_iso_string(second_clock::local_time())).str());

        window().capture().saveToFile(png_file);
        log_msg("screenshot saved to %1%", png_file);
    } break;

    case key::f3:
        log_msg("-----------------");
        break;

    case key::f11:
        toggle_fullscreen();
        break;

    default:
        ;
    }
}

void game::resize(unsigned int width, unsigned int height)
{
    if (width == width_ && height == height_)
        return;

    width_ = width;
    height_ = height;

    for (auto& state : states_)
        state->resize(width_, height_);
}

void game::toggle_fullscreen()
{
    fullscreen_ = !fullscreen_;
    unsigned int new_width, new_height;
    if (fullscreen_) {
        auto desktop(sf::VideoMode::getDesktopMode());
        window_width_ = width_;
        window_height_ = height_;
        new_width = desktop.width;
        new_height = desktop.height;
    } else {
        new_width = window_width_;
        new_height = window_height_;
    }
    resize(new_width, new_height);
    window_.create(sf::VideoMode(width_, height_, 32), window_title_,
                   fullscreen_ ? sf::Style::Fullscreen : sf::Style::Default,
                   sf::ContextSettings(16, 8, 4));
    window_.setVerticalSyncEnabled(true);
    rel_mouse_ = !rel_mouse_;
    relative_mouse(!rel_mouse_);
}

} // namespace hexa
