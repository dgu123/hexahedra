//---------------------------------------------------------------------------
/// \file   hexa/client/hud.hpp
/// \brief  Displays information to the player
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

#pragma once

#include <list>
#include <string>
#include <vector>
#include <hexa/protocol.hpp>
#include <hexa/hotbar_slot.hpp>

namespace hexa
{

class hud
{
public:
    typedef hotbar_slot slot;
    hotbar bar;
    bool hotbar_needs_update;
    unsigned int active_slot;
    bool show_debug_info;
    chunk_height local_height;

    struct timeout_msg
    {
        std::string msg;
        float time;
    };

public:
    hud();

    void time_tick(float seconds);

    void console_message_timeout(float seconds);
    void console_message(const std::string& msg);

    std::vector<std::string> console_messages() const;
    std::vector<std::string> old_console_messages() const;

    void show_input(bool on_off) { show_input_ = on_off; }
    bool show_input() const { return show_input_; }

    void set_input(const std::u32string& msg);
    void set_cursor(unsigned int pos);

    const std::u32string& get_input() const { return input_; }

    unsigned int get_cursor() const { return input_cursor_; }

private:
    std::list<timeout_msg> console_;
    float console_timeout_;
    unsigned int max_msgs_;

    bool show_input_;
    std::u32string input_;
    unsigned int input_cursor_;
};

} // namespace hexa
