/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

#include "q2wmap.h"
#include "qbsp.h"

vec_t microvolume = 1.0;
qboolean noprune;
qboolean nodetail;
qboolean fulldetail;
qboolean onlyents;
qboolean nomerge;
qboolean nowater;
qboolean nofill;
qboolean nocsg;
qboolean noweld;
qboolean noshare;
qboolean nosubdivide;
qboolean notjunc;
qboolean noopt;
qboolean leaktest;
qboolean verboseentities;

int block_xl = -8, block_xh = 7, block_yl = -8, block_yh = 7;

int entity_num;

static node_t *block_nodes[10][10];


/*
BlockTree
*/
static node_t *BlockTree(int xl, int yl, int xh, int yh){
	node_t *node;
	vec3_t normal;
	float dist;
	int mid;

	if(xl == xh && yl == yh){
		node = block_nodes[xl + 5][yl + 5];
		if(!node){					 // return an empty leaf
			node = AllocNode();
			node->planenum = PLANENUM_LEAF;
			node->contents = 0;	  //CONTENTS_SOLID;
			return node;
		}
		return node;
	}
	// create a seperator along the largest axis
	node = AllocNode();

	if(xh - xl > yh - yl){		 // split x axis
		mid = xl + (xh - xl) / 2 + 1;
		normal[0] = 1;
		normal[1] = 0;
		normal[2] = 0;
		dist = mid * 1024;
		node->planenum = FindFloatPlane(normal, dist);
		node->children[0] = BlockTree(mid, yl, xh, yh);
		node->children[1] = BlockTree(xl, yl, mid - 1, yh);
	} else {
		mid = yl + (yh - yl) / 2 + 1;
		normal[0] = 0;
		normal[1] = 1;
		normal[2] = 0;
		dist = mid * 1024;
		node->planenum = FindFloatPlane(normal, dist);
		node->children[0] = BlockTree(xl, mid, xh, yh);
		node->children[1] = BlockTree(xl, yl, xh, mid - 1);
	}

	return node;
}


/*
ProcessBlock_Thread
*/
static int brush_start, brush_end;
static void ProcessBlock_Thread(int blocknum){
	int xblock, yblock;
	vec3_t mins, maxs;
	bspbrush_t *brushes;
	tree_t *tree;
	node_t *node;

	yblock = block_yl + blocknum / (block_xh - block_xl + 1);
	xblock = block_xl + blocknum % (block_xh - block_xl + 1);

	Verbose("############### block %2i,%2i ###############\n",
	            xblock, yblock);

	mins[0] = xblock * 1024;
	mins[1] = yblock * 1024;
	mins[2] = -MAX_WORLD_WIDTH;
	maxs[0] = (xblock + 1) * 1024;
	maxs[1] = (yblock + 1) * 1024;
	maxs[2] = MAX_WORLD_WIDTH;

	ThreadLock();

	// the makelist and chopbrushes could be cached between the passes...
	brushes = MakeBspBrushList(brush_start, brush_end, mins, maxs);
	if(!brushes){
		node = AllocNode();
		node->planenum = PLANENUM_LEAF;
		node->contents = CONTENTS_SOLID;
		block_nodes[xblock + 5][yblock + 5] = node;
		ThreadUnlock();
		return;
	}


	if(!nocsg)
		brushes = ChopBrushes(brushes);

	tree = BrushBSP(brushes, mins, maxs);

	ThreadUnlock();
	block_nodes[xblock + 5][yblock + 5] = tree->headnode;
}


/*
ProcessWorldModel
*/
static void ProcessWorldModel(void){
	entity_t *e;
	tree_t *tree;
	qboolean leaked;
	qboolean optimize;

	e = &entities[entity_num];

	brush_start = e->firstbrush;
	brush_end = brush_start + e->numbrushes;
	leaked = false;

	// perform per-block operations
	if(block_xh * 1024 > map_maxs[0])
		block_xh = floor(map_maxs[0] / 1024.0);
	if((block_xl + 1) * 1024 < map_mins[0])
		block_xl = floor(map_mins[0] / 1024.0);
	if(block_yh * 1024 > map_maxs[1])
		block_yh = floor(map_maxs[1] / 1024.0);
	if((block_yl + 1) * 1024 < map_mins[1])
		block_yl = floor(map_mins[1] / 1024.0);

	if(block_xl < -4)
		block_xl = -4;
	if(block_yl < -4)
		block_yl = -4;
	if(block_xh > 3)
		block_xh = 3;
	if(block_yh > 3)
		block_yh = 3;

	for(optimize = false; optimize <= true; optimize++){
		Verbose("--------------------------------------------\n");

		RunThreadsOn((block_xh - block_xl + 1) * (block_yh - block_yl + 1),
				!verbose, ProcessBlock_Thread);

		// build the division tree
		// oversizing the blocks guarantees that all the boundaries
		// will also get nodes.

		Verbose("--------------------------------------------\n");

		tree = AllocTree();
		tree->headnode = BlockTree(block_xl - 1, block_yl - 1, block_xh + 1, block_yh + 1);

		tree->mins[0] = (block_xl) * 1024;
		tree->mins[1] = (block_yl) * 1024;
		tree->mins[2] = map_mins[2] - 8;

		tree->maxs[0] = (block_xh + 1) * 1024;
		tree->maxs[1] = (block_yh + 1) * 1024;
		tree->maxs[2] = map_maxs[2] + 8;

		// perform the global operations
		MakeTreePortals(tree);

		if(FloodEntities(tree))
			FillOutside(tree->headnode);
		else {
			leaked = true;
			LeakFile(tree);

			if(leaktest){
				Verbose("--- MAP LEAKED, ABORTING LEAKTEST ---\n");
				exit(0);
			}
			Verbose("**** leaked ****\n");
		}

		MarkVisibleSides(tree, brush_start, brush_end);
		if(noopt || leaked)
			break;
		if(!optimize){
			FreeTree(tree);
		}
	}

	FloodAreas(tree);
	MakeFaces(tree->headnode);
	FixTjuncs(tree->headnode);

	if(!noprune)
		PruneNodes(tree->headnode);

	WriteBSP(tree->headnode);

	if(!leaked)
		WritePortalFile(tree);

	FreeTree(tree);
}


/*
ProcessSubModel
*/
static void ProcessSubModel(void){
	entity_t *e;
	int start, end;
	tree_t *tree;
	bspbrush_t *list;
	vec3_t mins, maxs;

	e = &entities[entity_num];

	start = e->firstbrush;
	end = start + e->numbrushes;

	mins[0] = mins[1] = mins[2] = -MAX_WORLD_WIDTH;
	maxs[0] = maxs[1] = maxs[2] = MAX_WORLD_WIDTH;
	list = MakeBspBrushList(start, end, mins, maxs);
	if(!nocsg)
		list = ChopBrushes(list);
	tree = BrushBSP(list, mins, maxs);
	MakeTreePortals(tree);
	MarkVisibleSides(tree, start, end);
	MakeFaces(tree->headnode);
	FixTjuncs(tree->headnode);
	WriteBSP(tree->headnode);
	FreeTree(tree);
}


/*
ProcessModels
*/
static void ProcessModels(void){
	BeginBSPFile();

	for(entity_num = 0; entity_num < num_entities; entity_num++){
		if(!entities[entity_num].numbrushes)
			continue;

		Verbose("############### model %i ###############\n",
		            nummodels);
		BeginModel();
		if(entity_num == 0)
			ProcessWorldModel();
		else
			ProcessSubModel();
		EndModel();
	}

	EndBSPFile();
}


/*
BSP_Main
*/
int BSP_Main(void){
	time_t start, end;
	char base[MAX_OSPATH];
	int total_bsp_time;

	#ifdef _WIN32
		char title[MAX_OSPATH];
		sprintf(title, "Q2WMap [Compiling BSP]");
		SetConsoleTitle(title);
	#endif

	Print("\n----- BSP -----\n\n");

	start = time(NULL);

	Com_StripExtension(mapname, base);

	// delete portal and line files
	remove(va("%s.prt", base));
	remove(va("%s.lin", base));

	// if onlyents, just grab the entites and resave
	if(onlyents){

		LoadBSPFile(bspname);
		num_entities = 0;

		LoadMapFile(mapname);
		SetModelNumbers();

		UnparseEntities();

		WriteBSPFile(bspname);
	} else {
		// start from scratch
		LoadMapFile(mapname);
		SetModelNumbers();

		ProcessModels();
	}

	end = time(NULL);
	total_bsp_time = (int)(end - start);
	Print("\nBSP Time: ");
	if(total_bsp_time > 59)
		Print("%d Minutes ", total_bsp_time / 60);
	Print("%d Seconds\n", total_bsp_time % 60);

	return 0;
}
