/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "sv_local.h"


/*
 * @brief The world is divided into evenly sized sectors to aid in entity
 * management. This works like a meta-BSP tree, providing fast searches via
 * recursion to find entities within an arbitrary box.
 */
typedef struct sv_sector_s {
	int32_t axis; // -1 = leaf
	vec_t dist;
	struct sv_sector_s *children[2];
	GList *edicts;
} sv_sector_t;

#define SECTOR_DEPTH	4
#define SECTOR_NODES	32

/*
 * @brief The world structure contains all sectors and also the current query
 * context issued to Sv_AreaEdicts.
 */
typedef struct {
	sv_sector_t sectors[SECTOR_NODES];
	uint16_t num_sectors;

	const vec_t *area_mins, *area_maxs;

	g_edict_t **area_edicts;

	size_t num_area_edicts, max_area_edicts;
	uint32_t area_type; // AREA_SOLID, AREA_TRIGGER, ..
} sv_world_t;

static sv_world_t sv_world;

/*
 * @brief Builds a uniformly subdivided tree for the given world size.
 */
static sv_sector_t *Sv_CreateSector(int32_t depth, vec3_t mins, vec3_t maxs) {
	vec3_t size, mins1, maxs1, mins2, maxs2;

	sv_sector_t *sector = &sv_world.sectors[sv_world.num_sectors];
	sv_world.num_sectors++;

	if (depth == SECTOR_DEPTH) {
		sector->axis = -1;
		sector->children[0] = sector->children[1] = NULL;
		return sector;
	}

	VectorSubtract(maxs, mins, size);
	if (size[0] > size[1])
		sector->axis = 0;
	else
		sector->axis = 1;

	sector->dist = 0.5 * (maxs[sector->axis] + mins[sector->axis]);
	VectorCopy(mins, mins1);
	VectorCopy(mins, mins2);
	VectorCopy(maxs, maxs1);
	VectorCopy(maxs, maxs2);

	maxs1[sector->axis] = mins2[sector->axis] = sector->dist;

	sector->children[0] = Sv_CreateSector(depth + 1, mins2, maxs2);
	sector->children[1] = Sv_CreateSector(depth + 1, mins1, maxs1);

	return sector;
}

/*
 * @brief Resolve our area nodes for a newly loaded level. This is called prior to
 * linking any entities.
 */
void Sv_InitWorld(void) {

	for (uint16_t i = 0; i < sv_world.num_sectors; i++) {
		g_list_free(sv_world.sectors[i].edicts);
	}

	memset(&sv_world, 0, sizeof(sv_world));

	Sv_CreateSector(0, sv.cm_models[0]->mins, sv.cm_models[0]->maxs);
}

/*
 * @brief Called before moving or freeing an entity to remove it from the clipping
 * hull.
 */
void Sv_UnlinkEdict(g_edict_t *ent) {

	if (ent->link.sector) {
		sv_sector_t *sector = (sv_sector_t *) ent->link.sector;
		sector->edicts = g_list_remove(sector->edicts, ent);

		ent->link.sector = NULL;
	}
}

#define MAX_TOTAL_ENT_LEAFS 128

/*
 * @brief Called whenever an entity changes origin, mins, maxs, or solid to add it to
 * the clipping hull.
 */
void Sv_LinkEdict(g_edict_t *ent) {
	int32_t leafs[MAX_TOTAL_ENT_LEAFS];
	int32_t clusters[MAX_TOTAL_ENT_LEAFS];
	int32_t i, j;
	int32_t top_node;

	if (ent == svs.game->edicts) // never bother with the world
		return;

	// remove it from its current sector
	Sv_UnlinkEdict(ent);

	if (!ent->in_use) // and if its free, we're done
		return;

	// set the size
	VectorSubtract(ent->maxs, ent->mins, ent->link.size);

	// encode the size into the entity state for client prediction
	if (ent->solid == SOLID_BOX && (!ent->sv_flags & SVF_DEAD_MONSTER)) {
		PackBounds(ent->mins, ent->maxs, &ent->s.solid);
	} else if (ent->solid == SOLID_BSP) {
		ent->s.solid = SOLID_BSP;
	} else {
		ent->s.solid = SOLID_NOT;
	}

	// set the absolute bounding box
	if (ent->solid == SOLID_BSP && (ent->s.angles[0] || ent->s.angles[1] || ent->s.angles[2])) { // expand for rotation
		vec_t max = 0.0;

		for (i = 0; i < 3; i++) {
			vec_t v = fabsf(ent->mins[i]);
			if (v > max)
				max = v;
			v = fabsf(ent->maxs[i]);
			if (v > max)
				max = v;
		}
		for (i = 0; i < 3; i++) {
			ent->link.abs_mins[i] = ent->s.origin[i] - max;
			ent->link.abs_maxs[i] = ent->s.origin[i] + max;
		}
	} else { // normal
		VectorAdd(ent->s.origin, ent->mins, ent->link.abs_mins);
		VectorAdd(ent->s.origin, ent->maxs, ent->link.abs_maxs);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->link.abs_mins[0] -= 1.0;
	ent->link.abs_mins[1] -= 1.0;
	ent->link.abs_mins[2] -= 1.0;
	ent->link.abs_maxs[0] += 1.0;
	ent->link.abs_maxs[1] += 1.0;
	ent->link.abs_maxs[2] += 1.0;

	// link to PVS leafs
	ent->link.num_clusters = 0;
	ent->link.areas[0] = ent->link.areas[1] = 0;

	// get all leafs, including solids
	const int32_t num_leafs = Cm_BoxLeafnums(ent->link.abs_mins, ent->link.abs_maxs, leafs,
			lengthof(leafs), &top_node, 0);

	// set areas, allowing entities (doors) to occupy up to two
	for (i = 0; i < num_leafs; i++) {
		clusters[i] = Cm_LeafCluster(leafs[i]);
		const int32_t area = Cm_LeafArea(leafs[i]);
		if (area) {
			if (ent->link.areas[0] && ent->link.areas[0] != area) {
				if (ent->link.areas[1] && ent->link.areas[1] != area && sv.state == SV_LOADING) {
					Com_Warn("Object touching 3 areas at %s\n", vtos(ent->link.abs_mins));
				}
				ent->link.areas[1] = area;
			} else
				ent->link.areas[0] = area;
		}
	}

	if (num_leafs >= MAX_TOTAL_ENT_LEAFS) { // assume we missed some leafs, and mark by head_node
		ent->link.num_clusters = -1;
		ent->link.head_node = top_node;
	} else {
		ent->link.num_clusters = 0;
		for (i = 0; i < num_leafs; i++) {

			if (clusters[i] == -1)
				continue; // not a visible leaf

			for (j = 0; j < i; j++)
				if (clusters[j] == clusters[i])
					break;

			if (j == i) {
				if (ent->link.num_clusters == MAX_ENT_CLUSTERS) { // assume we missed some leafs, and mark by head_node
					ent->link.num_clusters = -1;
					ent->link.head_node = top_node;
					break;
				}

				ent->link.clusters[ent->link.num_clusters++] = clusters[i];
			}
		}
	}

	if (ent->solid == SOLID_NOT)
		return;

	// find the first sector that the ent's box crosses
	sv_sector_t *sector = sv_world.sectors;
	while (true) {

		if (sector->axis == -1)
			break;

		if (ent->link.abs_mins[sector->axis] > sector->dist)
			sector = sector->children[0];
		else if (ent->link.abs_maxs[sector->axis] < sector->dist)
			sector = sector->children[1];
		else
			break; // crosses the node
	}

	// link it in
	sector->edicts = g_list_prepend(sector->edicts, ent);
	ent->link.sector = sector;
}

/*
 * @return True if the entity matches the current world filter, false otherwise.
 */
static _Bool Sv_AreaEdicts_Filter(const g_edict_t *ent) {

	switch (ent->solid) {
		case SOLID_BOX:
		case SOLID_BSP:
		case SOLID_MISSILE:
			if (sv_world.area_type == AREA_SOLID)
				return true;

		case SOLID_TRIGGER:
			if (sv_world.area_type == AREA_TRIGGER)
				return true;

		default:
			break;
	}

	return false;
}

/*
 * @brief
 */
static void Sv_AreaEdicts_r(sv_sector_t *sector) {

	GList *e = sector->edicts;
	while (e) {
		g_edict_t *ent = (g_edict_t *) e->data;

		if (Sv_AreaEdicts_Filter(ent)) {
			g_link_t *link = &ent->link;

			if (BoxIntersect(link->abs_mins, link->abs_maxs, sv_world.area_mins, sv_world.area_maxs)) {

				sv_world.area_edicts[sv_world.num_area_edicts] = ent;
				sv_world.num_area_edicts++;

				if (sv_world.num_area_edicts == sv_world.max_area_edicts) {
					Com_Warn("sv_world.max_area_edicts reached\n");
					return;
				}
			}
		}

		e = e->next;
	}

	if (sector->axis == -1)
		return; // terminal node

	// recurse down both sides
	if (sv_world.area_maxs[sector->axis] > sector->dist)
		Sv_AreaEdicts_r(sector->children[0]);

	if (sv_world.area_mins[sector->axis] < sector->dist)
		Sv_AreaEdicts_r(sector->children[1]);
}

/*
 * @brief Fills in a table of edict pointers with those which have bounding boxes
 * that intersect the given area. It is possible for a non-axial bsp model
 * to be returned that doesn't actually intersect the area.
 *
 * Returns the number of entities found.
 */
size_t Sv_AreaEdicts(const vec3_t mins, const vec3_t maxs, g_edict_t **list, const size_t len,
		const uint32_t type) {

	sv_world.area_mins = mins;
	sv_world.area_maxs = maxs;
	sv_world.area_edicts = list;
	sv_world.num_area_edicts = 0;
	sv_world.max_area_edicts = len;
	sv_world.area_type = type;

	Sv_AreaEdicts_r(sv_world.sectors);

	return sv_world.num_area_edicts;
}

/*
 * @brief Prepares the collision model to clip to the specified entity. For
 * mesh models, the box hull must be set to reflect the bounds of the entity.
 */
static int32_t Sv_HullForEntity(const g_edict_t *ent) {

	// decide which clipping hull to use, based on the size
	if (ent->solid == SOLID_BSP) { // explicit hulls in the BSP model
		const cm_bsp_model_t *mod = sv.cm_models[ent->s.model1];

		if (!mod)
			Com_Error(ERR_DROP, "SOLID_BSP with no model\n");

		return mod->head_node;
	}

	// create a temporary hull from bounding box sizes
	return Cm_SetBoxHull(ent->mins, ent->maxs);
}

/*
 * @brief Returns the contents mask for the specified point. This includes world
 * contents as well as contents for any entities this point intersects.
 */
int32_t Sv_PointContents(const vec3_t point) {
	g_edict_t *edicts[MAX_EDICTS];

	// get base contents from world
	int32_t contents = Cm_PointContents(point, 0);

	// as well as contents from all intersected entities
	const int32_t count = Sv_AreaEdicts(point, point, edicts, lengthof(edicts), AREA_SOLID);

	for (int32_t i = 0; i < count; i++) {

		const g_edict_t *touch = edicts[i];
		const vec_t *angles;

		// might intersect, so do an exact clip
		const int32_t head_node = Sv_HullForEntity(touch);

		if (touch->solid == SOLID_BSP) // bsp models can rotate
			angles = touch->s.angles;
		else
			angles = vec3_origin;

		contents |= Cm_TransformedPointContents(point, head_node, touch->s.origin, angles);
	}

	return contents;
}

// an entity's movement, with allowed exceptions and other info
typedef struct {
	vec3_t box_mins, box_maxs; // enclose the test object along entire move
	const vec_t *mins, *maxs; // size of the moving object
	const vec_t *start, *end;
	cm_trace_t trace;
	const g_edict_t *skip;
	int32_t contents;
} sv_trace_t;

/*
 * @brief Clips the specified trace to other entities in its area. This is the basis
 * of ALL collision and interaction for the server. Tread carefully.
 */
static void Sv_ClipTraceToEntities(sv_trace_t *trace) {
	g_edict_t *area_edicts[MAX_EDICTS];
	vec_t *angles;
	cm_trace_t tr;
	int32_t i, num, head_node;

	// first resolve the entities found within our desired trace
	num = Sv_AreaEdicts(trace->box_mins, trace->box_maxs, area_edicts, MAX_EDICTS, AREA_SOLID);

	// then iterate them, determining if they have any bearing on our trace
	for (i = 0; i < num; i++) {

		g_edict_t *ent = area_edicts[i];

		if (ent->solid == SOLID_NOT) // can't actually touch us
			continue;

		if (trace->skip) { // see if we can skip it

			if (ent == trace->skip)
				continue; // explicitly (ourselves)

			if (ent->owner == trace->skip)
				continue; // or via ownership (we own it)

			if (trace->skip->owner) {

				if (ent == trace->skip->owner)
					continue; // which is bi-directional (inverse of previous case)

				if (ent->owner == trace->skip->owner)
					continue; // and communitive (we are both owned by the same)
			}
		}

		if (!(trace->contents & CONTENTS_DEAD_MONSTER) && (ent->sv_flags & SVF_DEAD_MONSTER))
			continue; // don't clip against corpses and gibs

		// we couldn't skip it, so trace to it and see if we hit
		head_node = Sv_HullForEntity(ent);

		if (ent->solid == SOLID_BSP) // bsp entities can rotate
			angles = ent->s.angles;
		else
			angles = vec3_origin;

		// perform the trace against this particular entity
		tr = Cm_TransformedBoxTrace(trace->start, trace->end, trace->mins, trace->maxs, head_node,
				trace->contents, ent->s.origin, angles);

		// check for a full or partial intersection
		if (tr.all_solid || tr.start_solid || tr.fraction < trace->trace.fraction) {

			trace->trace = tr;
			trace->trace.ent = ent;

			if (trace->trace.all_solid) // we were actually blocked
				return;
		}
	}
}

/*
 * @brief
 */
static void Sv_TraceBounds(sv_trace_t *trace) {
	int32_t i;

	for (i = 0; i < 3; i++) {
		if (trace->end[i] > trace->start[i]) {
			trace->box_mins[i] = trace->start[i] + trace->mins[i] - 1.0;
			trace->box_maxs[i] = trace->end[i] + trace->maxs[i] + 1.0;
		} else {
			trace->box_mins[i] = trace->end[i] + trace->mins[i] - 1.0;
			trace->box_maxs[i] = trace->start[i] + trace->maxs[i] + 1.0;
		}
	}
}

/*
 * @brief Moves the given box volume through the world from start to end.
 *
 * The skipped edict, and edicts owned by him, are explicitly not checked.
 * This prevents players from clipping against their own projectiles, etc.
 */
cm_trace_t Sv_Trace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
		const g_edict_t *skip, const int32_t contents) {

	sv_trace_t trace;

	memset(&trace, 0, sizeof(trace));

	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	// clip to world
	trace.trace = Cm_BoxTrace(start, end, mins, maxs, 0, contents);
	if (trace.trace.fraction < 1.0) {
		trace.trace.ent = svs.game->edicts;

		if (trace.trace.start_solid) // blocked entirely
			return trace.trace;
	}

	trace.start = start;
	trace.end = end;
	trace.mins = mins;
	trace.maxs = maxs;
	trace.skip = skip;
	trace.contents = contents;

	// create the bounding box of the entire move
	Sv_TraceBounds(&trace);

	// clip to other solid entities
	Sv_ClipTraceToEntities(&trace);

	return trace.trace;
}
