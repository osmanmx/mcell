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

#include <iostream>
#include <fstream>
#include <sstream>

#include "species_cleanup_event.h"

#include "world.h"

using namespace std;

namespace MCell {


void SpeciesCleanupEvent::dump(const string ind) const {
  cout << ind << "Rxn class cleanup event:\n";
  string ind2 = ind + "  ";
  BaseEvent::dump(ind2);
}


void SpeciesCleanupEvent::step() {

  // remove all rxn classes and all caches
  world->get_all_rxns().reset_caches();

  for (BNG::Species* sp: world->get_all_species().get_species_vector()) {
    release_assert(sp != nullptr);

    // num_instantiations tells us that there are no molecules of these species
    // is_removable - species were created on the fly
    if (sp->get_num_instantiations() == 0 && sp->is_removable()) {

      // tell partitions that this species is not known anymore
      if (sp->is_vol()) {
        for (Partition& p: world->get_partitions()) {
          p.remove_from_known_vol_species(sp->id);
        }
      }

      if (world->config.rxn_and_species_report) {
        stringstream ss;
        ss << sp->id << ": " << sp->to_str() << " - removed\n";
        BNG::append_to_report(world->config.get_species_report_file_name(), ss.str());
      }

      // delete this species
      world->get_all_species().remove(sp->id);

      // and also from caches used by RxnRules
      world->get_all_rxns().remove_species_id_references(sp->id);
    }
  }

  // cleanup the species array, we must remove nullptrs from the species array
  world->get_all_species().defragment();

  for (BNG::Species* sp: world->get_all_species().get_species_vector()) {
    release_assert(sp != nullptr);
  }
}


} /* namespace MCell */
