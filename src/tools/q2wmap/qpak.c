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

#include <unistd.h>

#include "qbsp.h"
#include "pak.h"


static char paths[MAX_PAK_ENTRIES][MAX_QPATH];
static int num_paths;

static int num_images;
static int num_models;
static int num_sounds;

/*
 * FindPath
 *
 * Returns the index of the specified path if it exists, or -1 otherwise.
 */
static int FindPath(char *path){
	int i;

	for(i = 0; i < num_paths; i++)
		if(!strncmp(paths[i], path, MAX_QPATH))
			return i;

	return -1;
}


/*
 * AddPath
 *
 * Adds the specified path to the resource list, after ensuring
 * that it is unique.
 */
static void AddPath(char *path){

	if(num_paths == MAX_PAK_ENTRIES){
		Com_Error(ERR_FATAL, "MAX_PAK_ENTRIES exhausted\n");
		return;
	}

	Com_Debug("AddPath: %s\n", path);
	strncpy(paths[num_paths++], path, MAX_QPATH);
}


#define NUM_SAMPLE_FORMATS 2
static const char *sample_formats[NUM_SAMPLE_FORMATS] = {"ogg", "wav"};

/*
 * AddSound
 *
 * Attempts to add the specified sound in any available format.
 */
static void AddSound(const char *sound){
	char snd[MAX_QPATH], path[MAX_QPATH];
	FILE *f;
	int i;

	Com_StripExtension(sound, snd);

	memset(path, 0, sizeof(path));

	for(i = 0; i < NUM_SAMPLE_FORMATS; i++){

		snprintf(path, sizeof(path) - 5, "sounds/%s.%s", snd, sample_formats[i]);

		if(FindPath(path) != -1)
			return;

		if(Fs_OpenFile(path, &f, FILE_READ) > 0){
			AddPath(path);
			Fs_CloseFile(f);
			break;
		}
	}

	if(i == NUM_SAMPLE_FORMATS){
		Com_Warn("Couldn't load sound %s\n", sound);
		return;
	}

	num_sounds++;
}


#define NUM_IMAGE_FORMATS 5
static const char *image_formats[NUM_IMAGE_FORMATS] = {"tga", "png", "jpg", "pcx", "wal"};

/*
 * AddImage
 *
 * Attempts to add the specified image in any available format.  If required,
 * a warning will be issued should we fail to resolve the specified image.
 */
static void AddImage(const char *image, qboolean required){
	char img[MAX_QPATH], path[MAX_QPATH];
	FILE *f;
	int i;

	Com_StripExtension(image, img);

	memset(path, 0, sizeof(path));

	for(i = 0; i < NUM_IMAGE_FORMATS; i++){

		snprintf(path, sizeof(path), "%s.%s", img, image_formats[i]);

		if(FindPath(path) != -1)
			return;

		if(Fs_OpenFile(path, &f, FILE_READ) > 0){
			AddPath(path);
			Fs_CloseFile(f);
			break;
		}
	}

	if(i == NUM_IMAGE_FORMATS){
		if(required)
			Com_Warn("Couldn't load image %s\n", img);
		return;
	}

	num_images++;
}


#define NUM_MODEL_FORMATS 3
static char *model_formats[NUM_MODEL_FORMATS] = {"md3", "md2", "obj"};

/*
 * AddModel
 *
 * Attempts to add the specified mesh model.
 */
static void AddModel(char *model){
	char mod[MAX_QPATH], path[MAX_QPATH];
	FILE *f;
	int i;

	if(model[0] == '*')  // bsp submodel
		return;

	Com_StripExtension(model, mod);

	memset(path, 0, sizeof(path));

	for(i = 0; i < NUM_MODEL_FORMATS; i++){

		snprintf(path, sizeof(path), "%s.%s", mod, model_formats[i]);

		if(FindPath(path) != -1)
			return;

		if(Fs_OpenFile(path, &f, FILE_READ) > 0){
			AddPath(path);
			Fs_CloseFile(f);
			break;
		}
	}

	if(i == NUM_MODEL_FORMATS){
		Com_Warn("Couldn't load model %s\n", mod);
		return;
	}

	Com_Dirname(model, path);
	strcat(path, "skin");

	AddImage(path, true);

	Com_Dirname(model, path);
	strcat(path, "world.cfg");

	AddPath(path);

	num_models++;
}


static char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

/*
 * AddSky
 */
static void AddSky(char *sky){
	int i;

	for(i = 0; i < 6; i++)
		AddImage(va("env/%s%s", sky, suf[i]), true);
}


/*
 * AddAnimation
 */
static void AddAnimation(char *name, int count){
	int i, j, k;

	Com_Debug("Adding %d frames for %s\n", count, name);

	j = strlen(name);

	if((i = atoi(&name[j - 1])) < 0)
		return;

	name[j - 1] = 0;

	for(k = 1, i = i + 1; k < count; k++, i++){
		const char *c = va("%s%d", name, i);
		AddImage(c, true);
	}
}


/*
 * AddMaterials
 *
 * Adds all resources specified by the materials file, and the materials
 * file itself.  See src/r_material.c for materials reference.
 */
static void AddMaterials(void){
	char path[MAX_QPATH];
	char *buffer;
	const char *buf;
	const char *c;
	char texture[MAX_QPATH];
	int i, num_frames;

	// load the materials file
	snprintf(path, sizeof(path), "materials/%s", Com_Basename(bspname));
	strcpy(path + strlen(path) - 3, "mat");

	if((i = Fs_LoadFile(path, (void **)(char *)&buffer)) < 1){
		Com_Warn("Couldn't load materials %s\n", path);
		return;
	}

	AddPath(path);  // add the materials file itself

	buf = buffer;

	num_frames = 0;
	memset(texture, 0, sizeof(texture));

	while(true){

		c = Com_Parse(&buf);

		if(*c == '\0')
			break;

		// texture references should all be added
		if(!strcmp(c, "texture")){
			snprintf(texture, sizeof(texture), "textures/%s", Com_Parse(&buf));
			AddImage(texture, true);
			continue;
		}

		// as should normalmaps
		if(!strcmp(c, "normalmap")){
			snprintf(texture, sizeof(texture), "textures/%s", Com_Parse(&buf));
			AddImage(texture, true);
			continue;
		}

		// and custom envmaps
		if(!strcmp(c, "envmap")){

			c = Com_Parse(&buf);
			i = atoi(c);

			if(i == 0 && strcmp(c, "0")){  // custom, add it
				snprintf(texture, sizeof(texture), "envmaps/%s", c);
				AddImage(texture, true);
			}
			continue;
		}

		// and custom flares
		if(!strcmp(c, "flare")){

			c = Com_Parse(&buf);
			i = atoi(c);

			if(i == 0 && strcmp(c, "0")){  // custom, add it
				snprintf(texture, sizeof(texture), "flares/%s", c);
				AddImage(texture, true);
			}
			continue;
		}

		if(!strcmp(c, "anim")){
			num_frames = atoi(Com_Parse(&buf));
			Com_Parse(&buf);  // read fps
			continue;
		}

		if(*c == '}'){

			if(num_frames)  // add animation frames
				AddAnimation(texture, num_frames);

			num_frames = 0;
			continue;
		}
	}

	Fs_FreeFile(buffer);
}


/*
 * AddLocation
 */
static void AddLocation(void){
	char base[MAX_QPATH];

	Com_StripExtension(bspname, base);
	AddPath(va("%s.loc", base));
}


/*
 * AddDocumentation
 */
static void AddDocumentation(void){
	char base[MAX_OSPATH];
	char *c;

	Com_StripExtension(bspname, base);

	c = strstr(base, "maps/");
	c = c ? c + 5 : base;

	AddPath(va("docs/map-%s.txt", c));
}


/*
 * GetPakfile
 *
 * Returns a suitable pakfile name for the current bsp name, e.g.
 *
 * maps/my.bsp -> map-my.pak.
 */
static pak_t *GetPakfile(void){
	char base[MAX_OSPATH];
	char pakfile[MAX_OSPATH];
	const char *c;
	pak_t *pak;

	Com_StripExtension(bspname, base);

	c = strstr(base, "maps/");
	c = c ? c + 5 : base;

	snprintf(pakfile, sizeof(pakfile), "%s/map-%s-%d.pak", Fs_Gamedir(), c, getpid());

	if(!(pak = Pak_CreatePakstream(pakfile)))
		Com_Error(ERR_FATAL, "Failed to open %s\n", pakfile);

	return pak;
}


/*
 * PAK_Main
 *
 * Loads the specified BSP file, resolves all resources referenced by it,
 * and generates a new pak archive for the project.  This is a very inefficient
 * but straightforward implementation.
 */
int PAK_Main(void){
	time_t start, end;
	int i, total_pak_time;
	epair_t *e;
	pak_t *pak;
	void *p;

	#ifdef _WIN32
		char title[MAX_OSPATH];
		sprintf(title, "Q2WMap [Compiling PAK]");
		SetConsoleTitle(title);
	#endif

	Com_Print("\n----- PAK -----\n\n");

	start = time(NULL);

	LoadBSPFile(bspname);

	// add the textures and normalmaps
	for(i = 0; i < num_texinfo; i++){
		AddImage(va("textures/%s", texinfo[i].texture), true);
		AddImage(va("textures/%s_nm", texinfo[i].texture), false);
		AddImage(va("textures/%s_norm", texinfo[i].texture), false);
		AddImage(va("textures/%s_local", texinfo[i].texture), false);
	}

	// and the materials
	AddMaterials();

	// and the sounds, models, sky, ..
	ParseEntities();

	for(i = 0; i < num_entities; i++){
		e = entities[i].epairs;
		while(e){

			if(!strncmp(e->key, "noise", 5) || !strncmp(e->key, "sound", 5))
				AddSound(e->value);
			else if(!strncmp(e->key, "model", 5))
				AddModel(e->value);
			else if(!strncmp(e->key, "sky", 3))
				AddSky(e->value);

			e = e->next;
		}
	}

	Com_Print("Resolved %d images, ", num_images);
	Com_Print("%d models, ", num_models);
	Com_Print("%d sounds.\n", num_sounds);

	// add location and docs
	AddLocation();
	AddDocumentation();

	// and of course the bsp and map
	AddPath(bspname);
	AddPath(mapname);

	pak = GetPakfile();
	Com_Print("Paking %d resources to %s...\n", num_paths, pak->file_name);

	for(i = 0; i < num_paths; i++){

		const int j = Fs_LoadFile(paths[i], (void **)(char *)&p);

		if(j == -1){
			Com_Print("Couldn't open %s\n", paths[i]);
			continue;
		}

		Pak_AddEntry(pak, paths[i], j, p);
		Z_Free(p);
	}

	Pak_ClosePakstream(pak);

	end = time(NULL);
	total_pak_time = (int)(end - start);
	Com_Print("\nPAK Time: ");
	if(total_pak_time > 59)
		Com_Print("%d Minutes ", total_pak_time / 60);
	Com_Print("%d Seconds\n", total_pak_time % 60);

	return 0;
}

