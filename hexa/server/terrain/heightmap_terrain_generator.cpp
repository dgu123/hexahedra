//---------------------------------------------------------------------------
// server/terrain/heightmap_terrain_generator.cpp
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
// Copyright 2012-2014, nocte@hippie.nu
//---------------------------------------------------------------------------

#include "heightmap_terrain_generator.hpp"

#include <stdexcept>
#include <mutex>
#include <unordered_map>
#include <boost/range/algorithm.hpp>

#include <hexa/block_types.hpp>
#include <hexa/lru_cache.hpp>
#include <hexa/trace.hpp>

#include "../hndl.hpp"
#include "../world.hpp"

using namespace boost::property_tree;
using namespace boost::range;

namespace hexa
{

struct heightmap_terrain_generator::impl
{
    std::unique_ptr<noise::generator_i> func_;
    block fill_material_;

    impl(world& w, const ptree& conf)
        : func_{compile_hndl(conf.get<std::string>("hndl"))}
        , fill_material_{
              find_material(conf.get<std::string>("material", "stone"), 1)}
    {
    }

    ~impl() {}

    void generate(world_terraingen_access& data, const chunk_coordinates& pos,
                  chunk& cnk)
    {
        trace("heightmap terrain generation for %1%",
              world_vector{pos - world_chunk_center});

        auto hm = hndl_area_int16(*func_, pos);
        uint32_t bottom = pos.z * chunk_size;

        for (uint16_t y = 0; y < chunk_size; ++y) {
            for (uint16_t x = 0; x < chunk_size; ++x) {
                // Absolute height
                uint32_t h = world_center.z + hm(x, y);
                if (h > bottom) {
                    auto range = std::min<uint16_t>(h - bottom, chunk_size);
                    for (uint16_t z = 0; z < range; ++z) {
                        auto& blk = cnk(x, y, z);
                        if (blk == type::air)
                            blk = fill_material_;
                    }
                }
            }
        }
    }

    void generate(world_terraingen_access& data, map_coordinates xy,
                  area_data& sm)
    {
        auto hm = hndl_area_int16(*func_, xy);
        auto j = hm.begin();
        for (auto i = sm.begin(); i != sm.end(); ++i, ++j)
            *i = std::max(*i, *j);
    }

    chunk_height estimate_height(world_terraingen_access& data,
                                 map_coordinates xy, chunk_height prev) const
    {
        auto hm = hndl_area_int16(*func_, xy);
        uint32_t highest = world_center.z
                           + *std::max_element(hm.begin(), hm.end());

        return (highest >> cnkshift) + 1;
    }
};

heightmap_terrain_generator::heightmap_terrain_generator(world& w,
                                                         const ptree& conf)
    : terrain_generator_i{w}
    , pimpl_{std::make_unique<impl>(w, conf)}
{
}

heightmap_terrain_generator::~heightmap_terrain_generator()
{
}

void heightmap_terrain_generator::generate(world_terraingen_access& data,
                                           const chunk_coordinates& pos,
                                           chunk& cnk)
{
    pimpl_->generate(data, pos, cnk);
}

chunk_height heightmap_terrain_generator::estimate_height(
    world_terraingen_access& data, map_coordinates xy, chunk_height prev) const
{
    return pimpl_->estimate_height(data, xy, prev);
}

bool heightmap_terrain_generator::generate(world_terraingen_access& proxy,
                                           const std::string& type,
                                           map_coordinates pos,
                                           area_data& data) const
{
    if (type != "surface")
        return false;

    pimpl_->generate(proxy, pos, data);

    return true;
}

} // namespace hexa
