/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

#pragma once

#include "r_types.h"

r_material_t *R_FindMaterial(const char *name, cm_asset_context_t context);
r_material_t *R_LoadMaterial(const char *name, cm_asset_context_t context);
ssize_t R_LoadMaterials(const char *path, cm_asset_context_t context, GList **materials);

#ifdef __R_LOCAL_H__
#define R_OFFSET_UNITS -1.0
#define R_OFFSET_FACTOR -1.0

void R_DrawMaterialBspSurfaces(const r_bsp_surfaces_t *surfs);
void R_DrawMeshMaterial(r_material_t *m, const GLuint offset, const GLuint count);
void R_InitMaterials(void);
void R_LoadModelMaterials(r_model_t *mod);
void R_ShutdownMaterials(void);
#endif /* __R_LOCAL_H__ */
