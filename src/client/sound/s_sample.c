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

#include "s_local.h"
#include "client.h"

extern cl_client_t cl;
extern cl_static_t cls;

/*
 * @brief
 */
static s_sample_t *S_AllocSample(void) {
	int32_t i;

	// find a free sample
	for (i = 0; i < s_env.num_samples; i++) {
		if (!s_env.samples[i].name[0])
			break;
	}

	if (i == s_env.num_samples) {
		if (s_env.num_samples == MAX_SAMPLES) {
			Com_Warn("S_AllocSample: MAX_SAMPLES reached.\n");
			return NULL;
		}
		s_env.num_samples++;
	}

	memset(&s_env.samples[i], 0, sizeof(s_env.samples[0]));
	return &s_env.samples[i];
}

/*
 * @brief
 */
static s_sample_t *S_FindName(const char *name) {
	char basename[MAX_QPATH];
	int32_t i;

	memset(basename, 0, sizeof(basename));
	StripExtension(name, basename);

	// see if it's already loaded
	for (i = 0; i < s_env.num_samples; i++) {
		if (!strcmp(s_env.samples[i].name, basename))
			return &s_env.samples[i];
	}

	return NULL;
}

/*
 * @brief
 */
static s_sample_t *S_AliasSample(s_sample_t *sample, const char *alias) {
	s_sample_t *s;

	if (!(s = S_AllocSample()))
		return NULL;

	g_strlcpy(s->name, alias, sizeof(s->name));
	s->chunk = sample->chunk;
	s->alias = true;

	return s;
}

static const char *SAMPLE_TYPES[] = { ".ogg", ".wav", NULL };

/*
 * @brief
 */
static void S_LoadSampleChunk(s_sample_t *sample) {
	char path[MAX_QPATH];
	void *buf;
	int32_t i, len;
	SDL_RWops *rw;

	if (sample->name[0] == '*') // place holder
		return;

	if (sample->name[0] == '#') { // global path
		g_strlcpy(path, (sample->name + 1), sizeof(path));
	} else {
		// or relative
		g_snprintf(path, sizeof(path), "sounds/%s", sample->name);
	}

	buf = NULL;
	rw = NULL;

	i = 0;
	while (SAMPLE_TYPES[i]) {

		StripExtension(path, path);
		strcat(path, SAMPLE_TYPES[i++]);

		if ((len = Fs_LoadFile(path, &buf)) == -1)
			continue;

		if (!(rw = SDL_RWFromMem(buf, len))) {
			Fs_FreeFile(buf);
			continue;
		}

		if (!(sample->chunk = Mix_LoadWAV_RW(rw, false)))
			Com_Warn("S_LoadSoundChunk: %s.\n", Mix_GetError());

		Fs_FreeFile(buf);

		SDL_FreeRW(rw);

		if (sample->chunk) // success
			break;
	}

	if (sample->chunk)
		Mix_VolumeChunk(sample->chunk, s_volume->value * MIX_MAX_VOLUME);
	else
		Com_Warn("S_LoadSoundChunk: Failed to load %s.\n", sample->name);
}

/*
 * @brief
 */
s_sample_t *S_LoadSample(const char *name) {
	char key[MAX_QPATH];
	s_sample_t *sample;

	if (!s_env.initialized)
		return NULL;

	if (!name || !name[0]) {
		Com_Warn("S_LoadSample: NULL or empty name.\n");
		return NULL;
	}

	StripExtension(name, key);

	if ((sample = S_FindName(key))) // found it
		return sample;

	if (!(sample = S_AllocSample()))
		return NULL;

	g_strlcpy(sample->name, key, sizeof(sample->name));

	S_LoadSampleChunk(sample);

	return sample;
}

/*
 * @brief
 */
s_sample_t *S_LoadModelSample(entity_state_t *ent, const char *name) {
	int32_t n;
	char *p;
	s_sample_t *sample;
	char model[MAX_QPATH];
	char alias[MAX_QPATH];
	char path[MAX_QPATH];
	FILE *f;

	if (!s_env.initialized)
		return NULL;

	// determine what model the client is using
	memset(model, 0, sizeof(model));

	if (ent->number - 1 >= MAX_CLIENTS) {
		Com_Warn("S_LoadModelSample: Invalid client entity: %d\n", ent->number - 1);
		return NULL;
	}

	n = CS_CLIENTS + ent->number - 1;
	if (cl.config_strings[n][0]) {
		p = strchr(cl.config_strings[n], '\\');
		if (p) {
			p += 1;
			strcpy(model, p);
			p = strchr(model, '/');
			if (p)
				*p = 0;
		}
	}

	// if we can't figure it out, use common
	if (*model == '\0')
		strcpy(model, "common");

	// see if we already know of the model specific sound
	g_snprintf(alias, sizeof(alias), "#players/%s/%s", model, name + 1);
	sample = S_FindName(alias);

	if (sample) // we do, use it
		return sample;

	// we don't, try it
	if (Fs_OpenFile(alias + 1, &f, FILE_READ) > 0) {
		Fs_CloseFile(f);
		return S_LoadSample(alias);
	}

	// that didn't work, so load the common one and alias it
	g_snprintf(path, sizeof(path), "#players/common/%s", name + 1);
	sample = S_LoadSample(path);

	if (sample)
		return S_AliasSample(sample, alias);

	Com_Warn("S_LoadModelSample: Failed to load %s.\n", alias);
	return NULL;
}

/*
 * @brief
 */
void S_FreeSamples(void) {
	int32_t i;

	for (i = 0; i < MAX_SAMPLES; i++) {
		if (s_env.samples[i].chunk && !s_env.samples[i].alias)
			Mix_FreeChunk(s_env.samples[i].chunk);
	}

	memset(s_env.samples, 0, sizeof(s_env.samples));
	s_env.num_samples = 0;
}

/*
 * @brief
 */
void S_LoadSamples(void) {
	int32_t i;

	S_FreeSamples();

	for (i = 1; i < MAX_SOUNDS; i++) {

		if (!cl.config_strings[CS_SOUNDS + i][0])
			break;

		cl.sound_precache[i] = S_LoadSample(cl.config_strings[CS_SOUNDS + i]);
	}
}
