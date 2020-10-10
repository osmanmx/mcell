/*
 * complex_species.cpp
 *
 *  Created on: Jan 9, 2020
 *      Author: ahusar
 */
#include <iostream>
#include <sstream>
#include <map>

#include "bng/ast.h"
#include "bng/bng_engine.h"
#include "bng/mol_type.h"
#include "bng/mol_instance.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/vf2_sub_graph_iso.hpp>
#include <boost/graph/connected_components.hpp>

#include "nauty/traces.h"
#include "nauty/nausparse.h"

#include "debug_config.h"

#include "bng/cplx.h"

//#define DEBUG_CANONICALIZATION

using namespace boost;
using namespace std;

namespace BNG {

// ------------------------------------ CplxInstance -------------------------

bool Cplx::is_fully_qualified() const {
  for (const MolInstance& mi: mol_instances) {
    if (!mi.is_fully_qualified(*bng_data)) {
      return false;
    }
  }
  return true;
}


bool Cplx::is_connected() const {
  assert(is_finalized());

  // simply count connected components
  size_t num_vertices = boost::num_vertices(graph);
  vector<int> component_per_vertex(num_vertices);
  int num_components = boost::connected_components(
      graph,
      boost::make_iterator_property_map(
          component_per_vertex.begin(), boost::get(boost::vertex_index, graph), component_per_vertex[0]
      )
  );
  assert(num_components > 0);
  return num_components == 1;
}


void Cplx::finalize() {
  if (mol_instances.empty()) {
    return; // empty complex, ignoring finalization
  }

  // volume or surface type
  bool surf_type = false;
  bool reactive_surf_type = false;
  for (MolInstance& mp: mol_instances) {
    // need to finalize flags - copy them from molecule type
    mp.finalize_flags_and_sort_components(*bng_data);
    // if at least one is a surface molecule then the whole cplx is surface molecule
    if (mp.is_surf()) {
      surf_type = true;
    }

    // if at least one is a reactive surface then the whole cplx is reactive surface
    if (mp.is_reactive_surface()) {
      reactive_surf_type = true;
    }
  }
  if (surf_type) {
    release_assert(!reactive_surf_type && "Species cannot be both reactive surface and surface molecule.");
    set_flag(SPECIES_CPLX_MOL_FLAG_SURF);
  }
  else if (reactive_surf_type) {
    set_flag(SPECIES_CPLX_MOL_FLAG_REACTIVE_SURFACE);
  }

  // CPLX_FLAG_SINGLE_MOL_NO_COMPONENTS
  bool is_simple = true;
  if (mol_instances.size() > 1) {
    is_simple = false;
  }
  if (is_simple) {
    for (MolInstance& mp: mol_instances) {
      if (!mp.component_instances.empty()) {
        is_simple = false;
        break;
      }
    }
  }
  set_flag(SPECIES_CPLX_FLAG_ONE_MOL_NO_COMPONENTS, is_simple);

  // we need graphs even for simple complexes because they can be used in reaction patterns
  graph.clear();
  create_graph();

  set_finalized();
}


void Cplx::create_graph() {
  graph.clear();

  // convert molecule instances and their bonds into the boost graph representation

  map<bond_value_t, vector<Graph::vertex_descriptor>> bonds_to_vertices_map;

  // add all molecules with their components and remember how they should be bound
  for (MolInstance& mi: mol_instances) {
    Graph::vertex_descriptor mol_desc = boost::add_vertex(MtVertexProperty(Node(&mi)), graph);

    for (ComponentInstance& ci: mi.component_instances) {
      // for patterns, only components that were explicitly listed are in component instances

      Graph::vertex_descriptor comp_desc = boost::add_vertex(MtVertexProperty(Node(&ci)), graph);

      // connect the component to its molecule
      boost::add_edge(mol_desc, comp_desc, graph);

      // and remember its bond
      if (ci.bond_has_numeric_value()) {
        bonds_to_vertices_map[ci.bond_value].push_back(comp_desc);
      }
    }
  }

  // connect components
  for (auto it: bonds_to_vertices_map) {
    release_assert(it.second.size() == 2 && "There must be exactly pairs of components connected with numbered bonds.");
    boost::add_edge(it.second[0], it.second[1], graph);
  }
}


bool Cplx::matches_complex_pattern_ignore_orientation(const Cplx& pattern) const {

#ifdef DEBUG_CPLX_MATCHING
  cout << "** matches_complex_pattern_ignore_orientation:\n";
  cout << "pattern:\n";
  pattern.dump(false); cout << "\n";
  pattern.dump(true); cout << "\n";
  dump_graph(pattern.graph, bng_data);
  cout << "this instance:\n";
  dump(false); cout << "\n";
  dump(true); cout << "\n";
  dump_graph(graph, bng_data);
#endif
  // this result cannot be cached because it might not be and applicable for other equivalent complexes,
  // we might need to impose some ordering on elementary molecules and then we can reuse the result when
  // creating products
  VertexMappingVector mappings;
  get_subgraph_isomorphism_mappings(pattern.graph, graph, true, mappings);
  assert((mappings.size() == 0 || mappings.size() == 1) && "We are searching only for the first match");

#ifdef DEBUG_CPLX_MATCHING
  cout << "** result: " << !mappings.empty() << "\n";
#endif
  // we need at least one match
  return !mappings.empty();
}


uint Cplx::get_pattern_num_matches(const Cplx& pattern) const {
  assert(is_finalized() && pattern.is_finalized());
  VertexMappingVector mappings;
  get_subgraph_isomorphism_mappings(pattern.graph, graph, false, mappings);
  return mappings.size();
}


bool Cplx::matches_complex_fully_ignore_orientation(const Cplx& pattern) const {
  if (graph.m_vertices.size() != pattern.graph.m_vertices.size()) {
    // we need full match
    return false;
  }

  VertexMappingVector mappings;
  get_subgraph_isomorphism_mappings(pattern.graph, graph, true, mappings);
  assert((mappings.size() == 0 || mappings.size()) == 1 && "We are searching only for the first match");

  return mappings.size() == 1 && mappings[0].size() == graph.m_vertices.size();
}


void Cplx::renumber_bonds() {
  map<bond_value_t, bond_value_t> new_bond_values;

  for (MolInstance& mi: mol_instances) {
    for (ComponentInstance& ci: mi.component_instances) {
      if (ci.bond_has_numeric_value()) {
        auto it = new_bond_values.find(ci.bond_value);
        if (it == new_bond_values.end()) {
          // new bond value, numbering from 1
          bond_value_t new_value = new_bond_values.size() + 1;
          new_bond_values[ci.bond_value] = new_value;
          ci.bond_value = new_value;
        }
        else {
          // we have already seen this bond value, use the new value
          ci.bond_value = it->second;
        }
      }
    }
  }
}


// does not seem to work ...
// even the usage in nfsim seems to be wrong?
// TODO: split into multiple functions
// https://computationalcombinatorics.wordpress.com/2012/09/20/canonical-labelings-with-nauty/
void Cplx::canonicalize() {
  if (mol_instances.size() == 1) {
    // sort components in molecules back to their prescribed form
    // and in a way that the state name is ascending (wee have no bonds here)
    for (MolInstance& mi: mol_instances) {
      mi.canonicalize(*bng_data);
    }

    set_flag(SPECIES_CPLX_FLAG_IS_CANONICAL);
    return;
  }

  // we use nauty/traces to construct a canonical version of the graph
  // we are using only the base BNG API, not the boost graphs to stay independent

  // 1) construct mapping vertex - index -> molecule or component
  //    and also store information on bonds and other details
  vector<Node> nodes;
  // map bond index -> vertex indices
  map<bond_value_t, vector<int>> bond_index_to_components;
  // mapping to know to which molecule a component belongs
  map<int, int> component_to_mol_index;
  // indexed by index, pair is (molecule index, component index) in this cplx
  // second value is -1 if the index represents a molecule
  vector<pair<int, int>> index_to_mol_and_component_index;
  int index = 0;
  for (int m_index = 0; m_index < (int)mol_instances.size(); m_index++) {
    MolInstance& mi = mol_instances[m_index];
    nodes.push_back(Node(&mi));
    int mol_index = index;
    index_to_mol_and_component_index.push_back(make_pair(m_index, -1));
    index++;

    for (int c_index = 0; c_index < (int)mi.component_instances.size(); c_index++) {
      ComponentInstance& ci = mi.component_instances[c_index];
      nodes.push_back(Node(&ci));
      component_to_mol_index[index] = mol_index;
      index_to_mol_and_component_index.push_back(make_pair(m_index, c_index));
      if (ci.bond_has_numeric_value()) {
        bond_index_to_components[ci.bond_value].push_back(index);
      }
      index++;
    }
  }

  // 2) create nauty graph representation
  vector<size_t> v_edge_indices;
  vector<int> d_out_degrees;
  vector<int> e_neighbors;
  map<string, vector<int>> color_classes;

  for (int index = 0; index < (int)nodes.size(); index++) {
    const Node& n = nodes[index];

    // the index for this node for neighbors simply starts where the last ended
    v_edge_indices.push_back(e_neighbors.size());

    // neighbors
    int num_neighbors = 0;
    if (n.is_mol) {
      // for a molecule - neighbors are all edges - the indices directly follow ours
      for (int i = 0; i < (int)n.mol->component_instances.size(); i++) {
        e_neighbors.push_back(index + i + 1); // need +1 because the first component is the next one
        num_neighbors++;
      }

      // also remember color
      // use name instead of id so that we are not dependent on the order of declatation in the BNG file
      color_classes["M:" + bng_data->get_molecule_type(n.mol->mol_type_id).name].push_back(index);
    }
    else {
      // index of the component's molecule
      assert(component_to_mol_index.count(index) != 0);
      e_neighbors.push_back(component_to_mol_index[index]);
      num_neighbors++;

      // index of the second component, if connected
      if (n.component->bond_has_numeric_value()) {
        assert(bond_index_to_components.count(n.component->bond_value) != 0);
        vector<int>& bonds = bond_index_to_components[n.component->bond_value];
        assert(bonds.size() == 2);
        int second_index = -1;
        if (bonds[0] == index) {
          second_index = bonds[1];
        }
        else if (bonds[1] == index) {
          second_index = bonds[0];
        }
        else {
          assert(false);
        }
        e_neighbors.push_back(second_index);
        num_neighbors++;
      }

      // also remember 'color', we need to distinguish states as well
      string component_label = "C:" + bng_data->get_component_type(n.component->component_type_id).name;
      if (n.component->state_is_set()) {
        component_label += "~" + bng_data->get_state_name(n.component->state_id);
      }
      color_classes[component_label].push_back(index);
    }

    d_out_degrees.push_back(num_neighbors);
  }

  // define coloring, nauty has a weird way of assigning colors to nodes:
  // libs/nauty/nug27.pdf, p. 18:
  // if ptn[i] = 0, then a cell (colour class) ends at position i.
  // so let's say I have these data:
  //   lab: 2 3 5 6 1 0 4 7 8 all vertices in some order
  //   ptn: 0 0 1 1 1 0 1 1 0 cells end where the zeros are (non-zero value specifies continuation)
  // it defines these classes
  //   [{2}, {3}, {0, 1, 5, 6}, {4, 7, 8}].

  vector<int> labels; // vertex indices
  vector<int> permutations; // ptn in nauty

#ifdef DEBUG_CANONICALIZATION
  cout << "Before " << to_str(*bng_data) << "\n";
#endif

  // labels.push_back(index); // index of vertices in the colors array
  for (auto it_color: color_classes) {
    vector<int>& indices = it_color.second;
    for (size_t i = 0; i < indices.size(); i++) {
      labels.push_back(indices[i]);
      // 1 - there are more, 0 - last of this class
      if (i != indices.size() - 1) {
        permutations.push_back(1);
      }
      else {
        permutations.push_back(0);
      }
    }
  }

  // setup sparse graph representation
  SG_DECL(sg1);
  int num_verts = v_edge_indices.size();
  sg1.nde = e_neighbors.size();
  sg1.nv = num_verts;
  sg1.d = d_out_degrees.data();
  sg1.dlen = d_out_degrees.size();
  sg1.v = v_edge_indices.data();
  sg1.vlen = num_verts;
  sg1.e = e_neighbors.data();
  sg1.elen = e_neighbors.size();

  // 3) get canonical mapping
  SG_DECL(cg1);
  //DEFAULTOPTIONS_SPARSEGRAPH(options);
  //statsblk stats;
  DEFAULTOPTIONS_TRACES(options);
  options.getcanon = TRUE;
  options.defaultptn = FALSE;
  options.digraph = FALSE;
  TracesStats stats;

  int* orbits = new int[num_verts]; // unused but must be allocated

  // - do the actual canonicalization, labels define how to reorder molecules and components
  //   using function Traces instead of sparsenauty or nauty because it does not leave so much
  //   unfreed memory
  // - WARNING: Threads function may not be thread safe, nauty uses many globals
  // - overwrites contents of labels and permutations
#ifdef DEBUG_CANONICALIZATION
  dump_container(labels, "labels before");
  dump_container(permutations, "permutations before");
#endif

  Traces(&sg1, labels.data(), permutations.data(), orbits, &options, &stats, &cg1);

#ifdef DEBUG_CANONICALIZATION
  dump_container(labels, "labels after");
  dump_container(permutations, "permutations after");
#endif

  SG_FREE( cg1 );
  delete [] orbits;
  nausparse_freedyn(); // frees allocated thread local storage memory

  // 4) create molecules and components in this complex from scratch
  // the numeric bonds are still ok

  MolInstanceVector new_mol_instances(mol_instances.size());
  map<int, int> old_to_new_mol_index;
  // now go by the ordered reverse mapping and create mols
  int new_mol_index = 0;
  for (int index: labels) {
    // is this node a molecule?
    pair<int, int> orig_mci = index_to_mol_and_component_index[index];
    if (orig_mci.second == -1) {
      // copy everything and clear components, they will be added later
      new_mol_instances[new_mol_index] = mol_instances[orig_mci.first];
      new_mol_instances[new_mol_index].component_instances.clear();
      old_to_new_mol_index[orig_mci.first] = new_mol_index;
      new_mol_index++;
    }
  }

  // once we have mols, add also the components
  for (int index: labels) {
    // is this node a component?
    pair<int, int> orig_mci = index_to_mol_and_component_index[index];
    if (orig_mci.second != -1) {
      int new_mol_index = old_to_new_mol_index[orig_mci.first];
      assert(new_mol_index < (int)new_mol_instances.size());
      // copy components
      new_mol_instances[new_mol_index].component_instances.push_back(
          mol_instances[orig_mci.first].component_instances[orig_mci.second]
      );
    }
  }

  // and overwrite
  mol_instances = new_mol_instances;

  // 5) renumber bonds so that they follow the new molecule ordering
  renumber_bonds();

  // 6) sort components in molecules back to their prescribed form
  // and in a way that the bond index is increasing
  for (MolInstance& mi: mol_instances) {
    mi.canonicalize(*bng_data);
  }

  // 7) and renumber bonds again
  renumber_bonds();

  // 8) update the boost graph representation
  finalize();

  set_flag(SPECIES_CPLX_FLAG_IS_CANONICAL);

#ifdef DEBUG_CANONICALIZATION
  cout << "After " << to_str(*bng_data) << "\n";
#endif

}


std::string Cplx::to_str(bool in_surf_reaction) const {
  stringstream ss;
  for (size_t i = 0; i < mol_instances.size(); i++) {
    ss << mol_instances[i].to_str(*bng_data);

    if (i != mol_instances.size() - 1) {
      ss << ".";
    }
  }
  if (orientation == ORIENTATION_UP) {
    ss << "'";
  }
  else if (orientation == ORIENTATION_DOWN) {
    ss << ",";
  }
  else if (in_surf_reaction && orientation == ORIENTATION_NONE) {
    ss << ";";
  }
  return ss.str();
}


void Cplx::dump(const bool for_diff, const std::string ind) const {
  if (!for_diff) {
    cout << ind << to_str();
  }
  else {
    cout << ind << "orientation: " << orientation << "\n";
    cout << ind << "mol_instances:\n";
    for (size_t i = 0; i < mol_instances.size(); i++) {
      cout << ind << i << ":\n";
      mol_instances[i].dump(*bng_data, true, ind + "  ");
    }
  }
}

} /* namespace BNG */
