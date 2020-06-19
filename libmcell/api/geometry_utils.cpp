/******************************************************************************
 *
 * Copyright (C) 2020 by
 * The Salk Institute for Biological Studies
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
******************************************************************************/

#include "generated/gen_geometry_utils.h"

#include "api/geometry_object.h"

using namespace std;

namespace MCell {
namespace API {

namespace geometry_utils {

std::shared_ptr<GeometryObject> create_box(const std::string& name, const float_t edge_length) {
  float_t hl = edge_length / 2;

  vector<vector<float_t>> vertex_list {
      {-hl, -hl, -hl},
      {-hl, -hl,  hl},
      {-hl,  hl, -hl},
      {-hl,  hl,  hl},
      { hl, -hl, -hl},
      { hl, -hl,  hl},
      { hl,  hl, -hl},
      { hl,  hl,  hl}
  };

  vector<vector<int>> element_connections {
      {1, 2, 0},
      {3, 6, 2},
      {7, 4, 6},
      {5, 0, 4},
      {6, 0, 2},
      {3, 5, 7},
      {1, 3, 2},
      {3, 7, 6},
      {7, 5, 4},
      {5, 1, 0},
      {6, 4, 0},
      {3, 1, 5}
  };

  auto res = make_shared<GeometryObject>(
      name,
      vertex_list,
      element_connections
  );

  return res;
}

} // namespace geometry_utils

} // namespace API
} // namespace MCell
