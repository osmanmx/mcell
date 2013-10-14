/***********************************************************************************
 *                                                                                 *
 * Copyright (C) 2006-2013 by                                                      *
 * The Salk Institute for Biological Studies and                                   *
 * Pittsburgh Supercomputing Center, Carnegie Mellon University                    *
 *                                                                                 *
 * This program is free software; you can redistribute it and/or                   *
 * modify it under the terms of the GNU General Public License                     *
 * as published by the Free Software Foundation; either version 2                  *
 * of the License, or (at your option) any later version.                          *
 *                                                                                 *
 * This program is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                  *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                   *
 * GNU General Public License for more details.                                    *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License               *
 * along with this program; if not, write to the Free Software                     *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. *
 *                                                                                 *
 ***********************************************************************************/

#ifndef INIT_H
#define INIT_H

#include "mcell_structs.h"

int init_notifications(struct volume *world);
int init_sim(struct volume *world);

int init_species(struct volume *world);
int init_geom(struct volume *world);
int init_partitions(struct volume *world);

int instance_obj(struct volume *world, struct object *objp, double (*im)[4]);

int instance_release_site(struct volume *world, struct object *objp,
			  double (*im)[4]);

int instance_polygon_object(struct volume *world, struct object *objp);

int init_regions(struct volume *world);
void init_clamp_lists(struct volume *world);

int instance_obj_regions(struct volume *world, struct object *objp);

int init_wall_regions(struct volume *world, struct object *objp);

int init_effectors(struct volume *world);
int instance_obj_effectors(struct volume *world, struct object *objp);
int init_wall_effectors(struct volume *world, struct object *objp);

int init_effectors_by_density(struct volume *world, struct wall *w, 
    struct eff_dat *eff_dat_head);

int init_effectors_by_number(struct volume *world, struct object *objp, 
    struct region_list *rlp);

void cube_corners(struct vector3 *p1,
                  struct vector3 *p2,
                  struct vector3 *corner);

void cube_face(struct vector3 *corner, struct vector3 **face, int i);

void cube_faces(struct vector3 *corner, struct vector3 *(*face)[4]);

void swap_double(double *x, double *y);

int init_releases(struct volume *world);

void publish_special_reactions_report(struct species *sp, 
    struct name_list *vol_species_name_list, 
    struct name_list *surf_species_name_list, 
    int n_species,
    struct species **species_list);

int accumulate_vertex_counts_per_storage(struct volume *world,
    struct object *objp, int *num_vertices_this_storage, double (*im)[4]);

int accumulate_vertex_counts_per_storage_polygon_object(struct volume *world,
    struct object *objp, int *num_vertices_this_storage, double (*im)[4]);

int which_storage_contains_vertex(struct volume *world, struct vector3 *v); 

int fill_world_vertices_array(struct volume *world, struct object *objp, 
    int *num_vertices_this_storage, double (*im)[4]);

int fill_world_vertices_array_polygon_object(struct volume *world,
    struct object *objp, int *num_vertices_this_storage, double (*im)[4]);
void check_for_conflicting_surface_classes(struct wall *w, int n_species,
    struct species **species_list);
void check_for_conflicts_in_surface_class(struct volume *world,
    struct species *sp);
struct species * get_species_by_name(char *name, int n_species, 
    struct species **species_list); 
void create_name_lists_of_volume_and_surface_mols(struct volume *world,
    struct name_list **vol_species_name_list, 
    struct name_list **surf_species_name_list);        			
void remove_molecules_name_list(struct name_list **nlist);    
int check_for_overlapped_walls(int n_subvols, struct subvolume *subvol);
    			
#endif
