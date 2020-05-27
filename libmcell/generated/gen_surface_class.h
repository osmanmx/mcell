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

#ifndef API_GEN_SURFACE_CLASS_H
#define API_GEN_SURFACE_CLASS_H

#include "../api/common.h"
#include "../api/base_data_class.h"
#include "../api/surface_property.h"


namespace MCell {
namespace API {

class Species;
class SurfaceProperty;

#define SURFACE_CLASS_CTOR() \
    SurfaceClass( \
        const std::string& name_, \
        const std::vector<std::shared_ptr<SurfaceProperty>> properties_ = std::vector<std::shared_ptr<SurfaceProperty>>(), \
        const SurfacePropertyType type_ = SurfacePropertyType::Unset, \
        std::shared_ptr<Species> affected_species_ = nullptr, \
        const Orientation orientation_ = Orientation::NotSet \
    )  : GenSurfaceClass(type_,affected_species_,orientation_) { \
      class_name = "SurfaceClass"; \
      name = name_; \
      properties = properties_; \
      type = type_; \
      affected_species = affected_species_; \
      orientation = orientation_; \
      postprocess_in_ctor();\
      check_semantics();\
    }

class GenSurfaceClass: public SurfaceProperty {
public:
  GenSurfaceClass( 
      const SurfacePropertyType type_ = SurfacePropertyType::Unset, 
      std::shared_ptr<Species> affected_species_ = nullptr, 
      const Orientation orientation_ = Orientation::NotSet 
  )  : SurfaceProperty(type_,affected_species_,orientation_)  {
  }
  void postprocess_in_ctor() override {}
  void check_semantics() const override;
  void set_initialized() override;

  bool __eq__(const GenSurfaceClass& other) const;
  std::string to_str(const std::string ind="") const override;

  // --- attributes ---
  std::vector<std::shared_ptr<SurfaceProperty>> properties;
  virtual void set_properties(const std::vector<std::shared_ptr<SurfaceProperty>> new_properties_) {
    if (initialized) {
      throw RuntimeError("Value 'properties' of object with name " + name + " (class " + class_name + ")"
                         "cannot be set after model was initialized.");
    }
    properties = new_properties_;
  }
  virtual std::vector<std::shared_ptr<SurfaceProperty>> get_properties() const {
    return properties;
  }

  // --- methods ---
}; // GenSurfaceClass

class SurfaceClass;
py::class_<SurfaceClass> define_pybinding_SurfaceClass(py::module& m);
} // namespace API
} // namespace MCell

#endif // API_GEN_SURFACE_CLASS_H