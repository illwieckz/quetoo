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

#include "cg_types.h"

#ifdef __CG_LOCAL_H__
cl_entity_t *Cg_Self(void);
_Bool Cg_IsSelf(const cl_entity_t *ent);
_Bool Cg_IsDucking(const cl_entity_t *ent);
void Cg_TraverseStep(cl_entity_step_t *step, uint32_t time, vec_t height);
void Cg_ApplyMeshModelTag(r_entity_t *child, const r_entity_t *parent, const char *tag_name);
r_entity_t *Cg_AddLinkedEntity(const r_entity_t *parent, const r_model_t *model,
                                    const char *tag_name);
void Cg_InterpolateStep(cl_entity_step_t *step);
void Cg_Interpolate(const cl_frame_t *frame);
void Cg_AddEntities(const cl_frame_t *frame);
#endif /* __CG_ENTITY_H__ */
