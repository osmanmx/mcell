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

#include <sstream>
#include "libs/pybind11/include/pybind11/stl.h"
#include "gen_initial_surface_release.h"
#include "../api/initial_surface_release.h"
#include "../api/species.h"

namespace MCell {
namespace API {

void GenInitialSurfaceRelease::check_semantics() const {
  if (!is_set(species)) {
    throw ValueError("Parameter 'species' must be set.");
  }
  if (!is_set(orientation)) {
    throw ValueError("Parameter 'orientation' must be set.");
  }
}

bool GenInitialSurfaceRelease::__eq__(const GenInitialSurfaceRelease& other) const {
  return
    name == other.name &&
    (
      (species != nullptr) ?
        ( (other.species != nullptr) ?
          (species->__eq__(*other.species)) : 
          false
        ) :
        ( (other.species != nullptr) ?
          false :
          true
        )
     )  &&
    orientation == other.orientation &&
    number_to_release == other.number_to_release &&
    density == other.density;
}

void GenInitialSurfaceRelease::set_initialized() {
  if (is_set(species)) {
    species->set_initialized();
  }
  initialized = true;
}

void GenInitialSurfaceRelease::set_all_attributes_as_default_or_unset() {
  class_name = "InitialSurfaceRelease";
  species = nullptr;
  orientation = Orientation::NOT_SET;
  number_to_release = INT_UNSET;
  density = FLT_UNSET;
}

std::string GenInitialSurfaceRelease::to_str(const std::string ind) const {
  std::stringstream ss;
  ss << get_object_name() << ": " <<
      "\n" << ind + "  " << "species=" << "(" << ((species != nullptr) ? species->to_str(ind + "  ") : "null" ) << ")" << ", " << "\n" << ind + "  " <<
      "orientation=" << orientation << ", " <<
      "number_to_release=" << number_to_release << ", " <<
      "density=" << density;
  return ss.str();
}

py::class_<InitialSurfaceRelease> define_pybinding_InitialSurfaceRelease(py::module& m) {
  return py::class_<InitialSurfaceRelease, std::shared_ptr<InitialSurfaceRelease>>(m, "InitialSurfaceRelease")
      .def(
          py::init<
            std::shared_ptr<Species>,
            const Orientation,
            const int,
            const float_t
          >(),
          py::arg("species"),
          py::arg("orientation"),
          py::arg("number_to_release") = INT_UNSET,
          py::arg("density") = FLT_UNSET
      )
      .def("check_semantics", &InitialSurfaceRelease::check_semantics)
      .def("__str__", &InitialSurfaceRelease::to_str, py::arg("ind") = std::string(""))
      .def("dump", &InitialSurfaceRelease::dump)
      .def_property("species", &InitialSurfaceRelease::get_species, &InitialSurfaceRelease::set_species)
      .def_property("orientation", &InitialSurfaceRelease::get_orientation, &InitialSurfaceRelease::set_orientation)
      .def_property("number_to_release", &InitialSurfaceRelease::get_number_to_release, &InitialSurfaceRelease::set_number_to_release)
      .def_property("density", &InitialSurfaceRelease::get_density, &InitialSurfaceRelease::set_density)
    ;
}

} // namespace API
} // namespace MCell
