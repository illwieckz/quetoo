/*
 * Copyright(c) 1997-2001 Id Software, Inc.
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

#include "cg_local.h"

/**
 * Cg_UpdateFov
 *
 * Update the field of view, which affects the view port as well as the culling
 * frustum.
 */
static void Cg_UpdateFov(void) {

	if (!cg_fov->modified && !cgi.view->update)
		return;

	if (cg_fov->value < 10.0 || cg_fov->value > 179.0)
		cg_fov->value = 100.0;

	const float x = cgi.context->width / tan(cg_fov->value / 360.0 * M_PI);

	const float a = atan(cgi.context->height / x);

	cgi.view->fov[0] = cg_fov->value;

	cgi.view->fov[1] = a * 360.0 / M_PI;

	cg_fov->modified = false;
}


/**
 * Cg_UpdateThirdperson
 *
 * Update the third person offset, if any. This is used as a client-side
 * option, or as the default chase camera view.
 */
static void Cg_UpdateThirdperson(const player_state_t *ps) {
	vec3_t angles, forward, dest;
	float dist;
	c_trace_t tr;

	if (!ps->stats[STAT_CHASE]) { // chasing uses client side 3rd person

		// if we're spectating, don't translate the origin because we have
		// no visible player model to begin with
		if (ps->pmove.pm_type == PM_SPECTATOR && !ps->stats[STAT_HEALTH])
			return;

		if (!cg_third_person->value)
			return;
	}

	cgi.client->third_person = true;

	// we're either chasing, or opting to use 3rd person
	VectorCopy(cgi.view->angles, angles);

	if (cg_third_person->value < 0.0) // in front of the player
		angles[1] += 180.0;

	AngleVectors(angles, forward, NULL, NULL);

	dist = cg_third_person->value;

	if (!dist)
		dist = 1.0;

	dist = fabs(150.0 * dist);

	// project the view origin back and up for 3rd person
	VectorMA(cgi.view->origin, -dist, forward, dest);
	dest[2] += 20.0;

	// clip it to the world
	tr = cgi.Trace(cgi.view->origin, dest, 5.0, MASK_SHOT);
	VectorCopy(tr.end, cgi.view->origin);

	// adjust view angles to compensate for height offset
	VectorMA(cgi.view->origin, 2048.0, forward, dest);
	VectorSubtract(dest, cgi.view->origin, dest);

	// copy angles back to view
	VectorAngles(dest, cgi.view->angles);
	AngleVectors(cgi.view->angles, cgi.view->forward, cgi.view->right, cgi.view->up);
}

#define KICK_TIME 300

/**
 * Cg_UpdateKick
 *
 * Calculate the view kick.
 */
static void Cg_UpdateKick(const player_state_t *ps) {
	static unsigned int kick_time;
	static float kick;

	if (ps->pmove.pm_type == PM_FREEZE) {
		return;
	}

	// clamp the kick time to our working bounds

	if (kick_time > cgi.client->time + KICK_TIME) {
		kick_time = cgi.client->time;
	} else if (kick_time < cgi.client->time) {
		kick_time = cgi.client->time;
	}

	// add falling kick

	const cl_entity_t *ent = &cgi.client->entities[cgi.client->player_num + 1];

	switch (ent->current.event) {

	case EV_CLIENT_LAND:
		kick = 2.5;
		kick_time += KICK_TIME;
		break;
	case EV_CLIENT_FALL:
		kick = 5.0;
		kick_time += KICK_TIME;
		break;
	case EV_CLIENT_FALL_FAR:
		kick = 10.0;
		kick_time += KICK_TIME;
		break;
	default:
		break;
	}

	cgi.view->angles[PITCH] += kick * ((kick_time - cgi.client->time) / (float) KICK_TIME);
}

/**
 * Cg_UpdateBob
 *
 * Calculate the view bob.  This is done using a running time counter and a
 * simple sin function.  The player's speed, as well as whether or not they
 * are on the ground, determine the bob frequency and amplitude.
 */
static void Cg_UpdateBob(const player_state_t *ps) {
	static float time, vtime;
	float ftime, speed;
	vec3_t velocity;

	if (!cg_bob->value)
		return;

	if (cg_third_person->value)
		return;

	if (cgi.client->frame.ps.pmove.pm_type != PM_NORMAL)
		return;

	const bool ducked = cgi.client->frame.ps.pmove.pm_flags & PMF_DUCKED;

	VectorScale(ps->pmove.velocity, 0.125, velocity);
	velocity[2] = 0.0;

	speed = VectorLength(velocity) / (ducked ? 150 : 450.0);

	if (speed > 1.0)
		speed = 1.0;

	ftime = cgi.view->time - vtime;

	if (ftime < 0.0) // clamp for level changes
		ftime = 0.0;

	ftime *= (1.0 + speed * 1.0 + speed);

	if (!(ps->pmove.pm_flags & PMF_ON_GROUND))
		ftime *= 0.25;

	time += ftime;
	vtime = cgi.view->time;

	cgi.view->bob = sin(3.5 * time) * (0.5 + speed) * (0.5 + speed);

	cgi.view->bob *= cg_bob->value; // scale via cvar too

	VectorMA(cgi.view->origin, -cgi.view->bob, cgi.view->forward, cgi.view->origin);
	VectorMA(cgi.view->origin, cgi.view->bob, cgi.view->right, cgi.view->origin);
	VectorMA(cgi.view->origin, cgi.view->bob, cgi.view->up, cgi.view->origin);
}

/**
 * Cg_UpdateView
 *
 * Updates the view for the renderer. The camera origin, bob effect, and field
 * of view are each augmented here. Other modifications can be made at your own
 * risk. This is called once per frame by the engine to finalize the view so
 * that rendering may begin.
 */
void Cg_UpdateView(const cl_frame_t *frame) {

	Cg_UpdateFov();

	Cg_UpdateThirdperson(&frame->ps);

	Cg_UpdateKick(&frame->ps);

	Cg_UpdateBob(&frame->ps);
}

/**
 * Cg_PopulateView
 *
 * Processes all entities, particles, emits, etc.. adding them to the view.
 * This is called once per frame by the engine, after Cg_UpdateView, and is run
 * in a separate thread while the renderer begins drawing the world.
 */
void Cg_PopulateView(const cl_frame_t *frame) {

	// add entities
	Cg_AddEntities();

	// and particles
	Cg_AddParticles();

	// and client sided emits
	Cg_AddEmits();
}
