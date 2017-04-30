/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// tr_models.c -- model loading and caching

#include "tr_local.h"

#define LL( x ) x = LittleLong( x )

// Ridah
static qboolean R_LoadMDC( model_t *mod, int lod, void *buffer, const char *modName );
// done.
static qboolean R_LoadMD3( model_t *mod, int lod, void *buffer, const char *modName );
static qboolean R_LoadMDS( model_t *mod, void *buffer, const char *mod_name );
static qboolean R_LoadMDR( model_t *mod, void *buffer, int filesize, const char *mod_name );

extern cvar_t *r_compressModels;
extern cvar_t *r_exportCompressedModels;
extern cvar_t *r_buildScript;

/*
====================
R_RegisterMD3
====================
*/
qhandle_t R_RegisterMD3(const char *name, model_t *mod)
{
	union {
		unsigned *u;
		void *v;
	} buf;
	int			lod;
	int			ident = 0;
	qboolean	loaded = qfalse;
	int			numLoaded;
	char filename[MAX_QPATH], namebuf[MAX_QPATH+20];
	char *fext, defex[] = "md3";

	numLoaded = 0;

	strcpy(filename, name);

	fext = strchr(filename, '.');
	if(!fext)
		fext = defex;
	else
	{
		*fext = '\0';
		fext++;
	}

	for (lod = MD3_MAX_LODS - 1 ; lod >= 0 ; lod--)
	{
		if(lod)
			Com_sprintf(namebuf, sizeof(namebuf), "%s_%d.%s", filename, lod, fext);
		else
			Com_sprintf(namebuf, sizeof(namebuf), "%s.%s", filename, fext);


		if ( r_compressModels->integer ) {
			namebuf[strlen( namebuf ) - 1] = '3';  // try MD3 first
		} else {
			namebuf[strlen( namebuf ) - 1] = 'c';  // try MDC first
		}

		ri.FS_ReadFile( namebuf, &buf.v );
		if ( !buf.u ) {
			if ( r_compressModels->integer ) {
				namebuf[strlen( namebuf ) - 1] = 'c';  // try MDC second
			} else {
				namebuf[strlen( namebuf ) - 1] = '3';  // try MD3 second
			}
			ri.FS_ReadFile( namebuf, (void **)&buf );
			if ( !buf.u ) {
				continue;
			}
		}
		
		ident = LittleLong(* (unsigned *) buf.u);
		// Ridah, mesh compression
		if ( ident != MD3_IDENT && ident != MDC_IDENT ) {
			ri.Printf( PRINT_WARNING,"RE_RegisterModel: unknown fileid for %s\n", name );
			goto fail;
		}

		if (ident == MD3_IDENT) {
			loaded = R_LoadMD3(mod, lod, buf.u, name);
		} else if (ident == MDC_IDENT) {
			loaded = R_LoadMDC( mod, lod, buf.u, name );
		}
		// done.
	
		ri.FS_FreeFile(buf.v);

		if(loaded)
		{
			mod->numLods++;
			numLoaded++;
		}
		else
			break;
	}

	if(numLoaded)
	{
		// duplicate into higher lod spots that weren't
		// loaded, in case the user changes r_lodbias on the fly
		for(lod--; lod >= 0; lod--)
		{
			mod->numLods++;
			mod->mdv[lod] = mod->mdv[lod + 1];
		}

		return mod->index;
	}

#ifdef _DEBUG
	ri.Printf(PRINT_WARNING,"R_RegisterMD3: couldn't load %s\n", name);
#endif

fail:
	// we still keep the model_t around, so if the model name is asked for
	// again, we won't bother scanning the filesystem
	mod->type = MOD_BAD;
	return 0;
}

/*
====================
R_RegisterMDS
====================
*/
qhandle_t R_RegisterMDS(const char *name, model_t *mod)
{
	union {
		unsigned *u;
		void *v;
	} buf;
	int	ident;
	qboolean loaded = qfalse;

	ri.FS_ReadFile(name, (void **) &buf.v);
	if(!buf.u)
	{
		mod->type = MOD_BAD;
		return 0;
	}
	
	ident = LittleLong(*(unsigned *)buf.u);
	if(ident == MDS_IDENT)
		loaded = R_LoadMDS(mod, buf.u, name);

	ri.FS_FreeFile (buf.v);
	
	if(!loaded)
	{
		ri.Printf(PRINT_WARNING,"R_RegisterMDS: couldn't load mds file %s\n", name);
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}

/*
====================
R_RegisterMDR
====================
*/
qhandle_t R_RegisterMDR(const char *name, model_t *mod)
{
	union {
		unsigned *u;
		void *v;
	} buf;
	int	ident;
	qboolean loaded = qfalse;
	int filesize;

	filesize = ri.FS_ReadFile(name, (void **) &buf.v);
	if(!buf.u)
	{
		mod->type = MOD_BAD;
		return 0;
	}
	
	ident = LittleLong(*(unsigned *)buf.u);
	if(ident == MDR_IDENT)
		loaded = R_LoadMDR(mod, buf.u, filesize, name);

	ri.FS_FreeFile (buf.v);
	
	if(!loaded)
	{
		ri.Printf(PRINT_WARNING,"R_RegisterMDR: couldn't load mdr file %s\n", name);
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}

/*
====================
R_RegisterIQM
====================
*/
qhandle_t R_RegisterIQM(const char *name, model_t *mod)
{
	union {
		unsigned *u;
		void *v;
	} buf;
	qboolean loaded = qfalse;
	int filesize;

	filesize = ri.FS_ReadFile(name, (void **) &buf.v);
	if(!buf.u)
	{
		mod->type = MOD_BAD;
		return 0;
	}
	
	loaded = R_LoadIQM(mod, buf.u, filesize, name);

	ri.FS_FreeFile (buf.v);
	
	if(!loaded)
	{
		ri.Printf(PRINT_WARNING,"R_RegisterIQM: couldn't load iqm file %s\n", name);
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}


typedef struct
{
	char *ext;
	qhandle_t (*ModelLoader)( const char *, model_t * );
} modelExtToLoaderMap_t;

// Note that the ordering indicates the order of preference used
// when there are multiple models of different formats available
static modelExtToLoaderMap_t modelLoaders[ ] =
{
	{ "iqm", R_RegisterIQM },
	{ "mdr", R_RegisterMDR },
	{ "mds", R_RegisterMDS },
	{ "md3", R_RegisterMD3 },
	{ "mdc", R_RegisterMD3 }
};

static int numModelLoaders = ARRAY_LEN(modelLoaders);

/*
** R_GetModelByHandle
*/
model_t *R_GetModelByHandle( qhandle_t index ) {
	model_t     *mod;

	// out of range gets the defualt model
	if ( index < 1 || index >= tr.numModels ) {
		return tr.models[0];
	}

	mod = tr.models[index];

	return mod;
}

//===============================================================================

/*
** R_AllocModel
*/
model_t *R_AllocModel( void ) {
	model_t     *mod;

	if ( tr.numModels == MAX_MOD_KNOWN ) {
		return NULL;
	}

	mod = ri.Hunk_Alloc( sizeof( *tr.models[tr.numModels] ), h_low );
	mod->index = tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}

/*
====================
RE_RegisterModel

Loads in a model for the given name

Zero will be returned if the model fails to load.
An entry will be retained for failed models as an
optimization to prevent disk rescanning if they are
asked for again.
====================
*/
qhandle_t RE_RegisterModel( const char *name ) {
	model_t		*mod;
	qhandle_t	hModel;
	qboolean	orgNameFailed = qfalse;
	int			orgLoader = -1;
	int			i;
	char		localName[ MAX_QPATH ];
	const char	*ext;
	char		altName[ MAX_QPATH ];

	if ( !name || !name[0] ) {
		// Ridah, disabled this, we can see models that can't be found because they won't be there
		//ri.Printf( PRINT_ALL, "RE_RegisterModel: NULL name\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Model name exceeds MAX_QPATH\n" );
		return 0;
	}

	//
	// search the currently loaded models
	//
	for ( hModel = 1 ; hModel < tr.numModels; hModel++ ) {
		mod = tr.models[hModel];
		if ( !strcmp( mod->name, name ) ) {
			if( mod->type == MOD_BAD ) {
				return 0;
			}
			return hModel;
		}
	}

	// allocate a new model_t

	if ( ( mod = R_AllocModel() ) == NULL ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterModel: R_AllocModel() failed for '%s'\n", name );
		return 0;
	}

	// only set the name after the model has been successfully loaded
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );

// GR - by default models are not tessellated
	mod->ATI_tess = qfalse;
// GR - check if can be tessellated...
//		make sure to tessellate model heads
	if ( strstr( name, "head" ) ) {
		mod->ATI_tess = qtrue;
	}

	R_IssuePendingRenderCommands();
 
	mod->type = MOD_BAD;
	mod->numLods = 0;

	//
	// load the files
	//
	Q_strncpyz( localName, name, MAX_QPATH );

	ext = COM_GetExtension( localName );
	if( *ext )
	{
		// Look for the correct loader and use it
		for( i = 0; i < numModelLoaders; i++ )
		{
			if( !Q_stricmp( ext, modelLoaders[ i ].ext ) )
			{
				// Load
				hModel = modelLoaders[ i ].ModelLoader( localName, mod );
				break;
			}
		}
		// A loader was found
		if( i < numModelLoaders )
		{
			if( !hModel )
			{
				// Loader failed, most likely because the file isn't there;
				// try again without the extension
				orgNameFailed = qtrue;
				orgLoader = i;
				COM_StripExtension( name, localName, MAX_QPATH );
			}
			else
			{
				// Something loaded
				return mod->index;
			}
		}
	}

	// Try and find a suitable match using all
	// the model formats supported
	for( i = 0; i < numModelLoaders; i++ )
	{
		if (i == orgLoader)
			continue;

		Com_sprintf( altName, sizeof (altName), "%s.%s", localName, modelLoaders[ i ].ext );

		// Load
		hModel = modelLoaders[ i ].ModelLoader( altName, mod );

		if( hModel )
		{
			if( orgNameFailed )
			{
				ri.Printf( PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n",
						name, altName );
			}

			break;
		}
	}
	
	return hModel;
}

//-------------------------------------------------------------------------------
// Ridah, mesh compression
float r_anormals[NUMMDCVERTEXNORMALS][3] = {
#include "anorms256.h"
};

/*
=============
R_MDC_GetVec
=============
*/
void R_MDC_GetVec( unsigned char anorm, vec3_t dir ) {
	VectorCopy( r_anormals[anorm], dir );
}

/*
=============
R_MDC_GetAnorm
=============
*/
unsigned char R_MDC_GetAnorm( const vec3_t dir ) {
	int i, best_start_i[3] = { 0 }, next_start, next_end, best = 0;     // TTimo: init
	float best_diff, group_val, this_val, diff;
	float   *this_norm;

	// find best Z match

	if ( dir[2] > 0.097545f ) {
		next_start = 144;
		next_end = NUMMDCVERTEXNORMALS;
	} else
	{
		next_start = 0;
		next_end = 144;
	}

	best_diff = 999;
	this_val = -999;

	for ( i = next_start ; i < next_end ; i++ )
	{
		if ( r_anormals[i][2] == this_val ) {
			continue;
		} else {
			this_val = r_anormals[i][2];
		}

		if ( ( diff = fabs( dir[2] - r_anormals[i][2] ) ) < best_diff ) {
			best_diff = diff;
			best_start_i[2] = i;

		}

		if ( next_start ) {
			if ( r_anormals[i][2] > dir[2] ) {
				break;  // we've gone passed the dir[2], so we can't possibly find a better match now
			}
		} else {
			if ( r_anormals[i][2] < dir[2] ) {
				break;  // we've gone passed the dir[2], so we can't possibly find a better match now
			}
		}
	}

	best_diff = -999;

	// find best match within the Z group

	for ( i = best_start_i[2], group_val = r_anormals[i][2]; i < NUMMDCVERTEXNORMALS; i++ )
	{
		this_norm = r_anormals[i];

		if ( this_norm[2] != group_val ) {
			break; // done checking the group
		}
		diff = DotProduct( dir, this_norm );

		if ( diff > best_diff ) {
			best_diff = diff;
			best = i;
		}
	}

	return (unsigned char)best;
}

/*
=================
R_MDC_EncodeOfsVec
=================
*/
qboolean R_MDC_EncodeXyzCompressed( const vec3_t vec, const vec3_t normal, mdcXyzCompressed_t *out ) {
	mdcXyzCompressed_t retval;
	int i;
	unsigned char anorm;

	retval.ofsVec = 0;
	for ( i = 0; i < 3; i++ ) {
		if ( fabs( vec[i] ) >= MDC_MAX_DIST ) {
			return qfalse;
		}
		retval.ofsVec += ( ( (int)fabs( ( vec[i] + MDC_DIST_SCALE * 0.5 ) * ( 1.0 / MDC_DIST_SCALE ) + MDC_MAX_OFS ) ) << ( i * MDC_BITS_PER_AXIS ) );
	}
	anorm = R_MDC_GetAnorm( normal );
	retval.ofsVec |= ( (int)anorm ) << 24;

	*out = retval;
	return qtrue;
}

/*
=================
R_LoadMDC
=================
*/
static qboolean R_LoadMDC( model_t *mod, int lod, void *buffer, const char *modName ) 
{
	int             f, i, j, k;

	mdcHeader_t         *mdcModel;
	md3Frame_t          *md3Frame;
	mdcSurface_t        *mdcSurf;
	md3Shader_t         *md3Shader;
	md3Triangle_t       *md3Tri;
	md3St_t             *md3st;
	md3XyzNormal_t      *md3xyz;
	mdcXyzCompressed_t  *mdcxyzComp;
	short               *mdcBaseFrame, *mdcCompFrame;
	mdcTag_t            *mdcTag;
	mdcTagName_t        *mdcTagName;

	mdvModel_t     *mdvModel;
	mdvFrame_t     *frame;
	mdvSurface_t   *surf;//, *surface;
	int            *shaderIndex;
	glIndex_t	   *tri;
	mdvVertex_t    *v;
	mdvSt_t        *st;
	mdvTag_t       *tag;
	mdvTagName_t   *tagName;

	int             version;
	int             size;

	mdcModel = (mdcHeader_t *) buffer;

	version = LittleLong(mdcModel->version);
	if(version != MDC_VERSION)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDC: %s has wrong version (%i should be %i)\n", modName, version, MDC_VERSION);
		return qfalse;
	}

	mod->type = MOD_MESH;
	size = LittleLong(mdcModel->ofsEnd);
	mod->dataSize += size;
	mdvModel = mod->mdv[lod] = ri.Hunk_Alloc(sizeof(mdvModel_t), h_low);

	LL( mdcModel->ident );
	LL( mdcModel->version );
	LL( mdcModel->numFrames );
	LL( mdcModel->numTags );
	LL( mdcModel->numSurfaces );
	LL( mdcModel->ofsFrames );
	LL( mdcModel->ofsTagNames );
	LL( mdcModel->ofsTags );
	LL( mdcModel->ofsSurfaces );
	LL( mdcModel->ofsEnd );
	LL( mdcModel->flags );
	LL( mdcModel->numSkins );

	if( mdcModel->numFrames < 1 )
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDC: %s has no frames\n", modName);
		return qfalse;
	}

	// swap all the frames
	mdvModel->numFrames = mdcModel->numFrames;
	mdvModel->frames = frame = ri.Hunk_Alloc(sizeof(*frame) * mdcModel->numFrames, h_low);

	md3Frame = (md3Frame_t *) ((byte *) mdcModel + mdcModel->ofsFrames);
	for(i = 0; i < mdcModel->numFrames; i++, frame++, md3Frame++)
	{
		frame->radius = LittleFloat(md3Frame->radius);
		if ( strstr( mod->name, "sherman" ) || strstr( mod->name, "mg42" ) ) 
		{
			frame->radius = 256;
			for ( j = 0 ; j < 3 ; j++ ) 
			{
				frame->bounds[0][j] = 128;
				frame->bounds[1][j] = -128;
				frame->localOrigin[j] = LittleFloat( md3Frame->localOrigin[j] );
			}
		}
		else
		{		
			for(j = 0; j < 3; j++)
			{
				frame->bounds[0][j] = LittleFloat(md3Frame->bounds[0][j]);
				frame->bounds[1][j] = LittleFloat(md3Frame->bounds[1][j]);
				frame->localOrigin[j] = LittleFloat(md3Frame->localOrigin[j]);
			}
		}
	}

	// swap all the tags
	mdvModel->numTags = mdcModel->numTags;
	mdvModel->tags = tag = ri.Hunk_Alloc(sizeof(*tag) * (mdcModel->numTags * mdcModel->numFrames), h_low);

	mdcTag = (mdcTag_t *) ((byte *) mdcModel + mdcModel->ofsTags);
	for(i = 0; i < mdcModel->numTags * mdcModel->numFrames; i++, tag++, mdcTag++)
	{
		vec3_t angles;
		for(j = 0; j < 3; j++)
		{
			tag->origin[j] = LittleShort(mdcTag->xyz[j]) * MD3_XYZ_SCALE;
			angles[j] = LittleShort(mdcTag->angles[j]) * MDC_TAG_ANGLE_SCALE;
		}
		AnglesToAxis(angles, tag->axis);
	}


	mdvModel->tagNames = tagName = ri.Hunk_Alloc(sizeof(*tagName) * (mdcModel->numTags), h_low);

	mdcTagName = (mdcTagName_t *) ((byte *) mdcModel + mdcModel->ofsTagNames);
	for(i = 0; i < mdcModel->numTags; i++, tagName++, mdcTagName++)
	{
		Q_strncpyz(tagName->name, mdcTagName->name, sizeof(tagName->name));
	}

	// swap all the surfaces
	mdvModel->numSurfaces = mdcModel->numSurfaces;
	mdvModel->surfaces = surf = ri.Hunk_Alloc(sizeof(*surf) * mdcModel->numSurfaces, h_low);

	mdcSurf = (mdcSurface_t *) ((byte *) mdcModel + mdcModel->ofsSurfaces);
	for(i = 0; i < mdcModel->numSurfaces; i++)
	{
		LL( mdcSurf->ident );
		LL( mdcSurf->flags );
		LL( mdcSurf->numBaseFrames );
		LL( mdcSurf->numCompFrames );
		LL( mdcSurf->numShaders );
		LL( mdcSurf->numTriangles );
		LL( mdcSurf->ofsTriangles );
		LL( mdcSurf->numVerts );
		LL( mdcSurf->ofsShaders );
		LL( mdcSurf->ofsSt );
		LL( mdcSurf->ofsXyzNormals );
		LL( mdcSurf->ofsXyzCompressed );
		LL( mdcSurf->ofsFrameBaseFrames );
		LL( mdcSurf->ofsFrameCompFrames );
		LL( mdcSurf->ofsEnd );

		if(mdcSurf->numVerts >= SHADER_MAX_VERTEXES)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMDC: %s has more than %i verts on %s (%i).\n",
				modName, SHADER_MAX_VERTEXES - 1, mdcSurf->name[0] ? mdcSurf->name : "a surface",
				mdcSurf->numVerts );
			return qfalse;
		}
		if(mdcSurf->numTriangles * 3 >= SHADER_MAX_INDEXES)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMDC: %s has more than %i triangles on %s (%i).\n",
				modName, ( SHADER_MAX_INDEXES / 3 ) - 1, mdcSurf->name[0] ? mdcSurf->name : "a surface",
				mdcSurf->numTriangles );
			return qfalse;
		}

		// change to surface identifier
		surf->surfaceType = SF_MDV;

		// give pointer to model for Tess_SurfaceMDX
		surf->model = mdvModel;

		// copy surface name
		Q_strncpyz(surf->name, mdcSurf->name, sizeof(surf->name));

		// lowercase the surface name so skin compares are faster
		Q_strlwr(surf->name);

		// strip off a trailing _1 or _2
		// this is a crutch for q3data being a mess
		j = strlen(surf->name);
		if(j > 2 && surf->name[j - 2] == '_')
		{
			surf->name[j - 2] = 0;
		}

		// register the shaders
		surf->numShaderIndexes = mdcSurf->numShaders;
		surf->shaderIndexes = shaderIndex = ri.Hunk_Alloc(sizeof(*shaderIndex) * mdcSurf->numShaders, h_low);

		md3Shader = (md3Shader_t *) ((byte *) mdcSurf + mdcSurf->ofsShaders);
		for(j = 0; j < mdcSurf->numShaders; j++, shaderIndex++, md3Shader++)
		{
			shader_t       *sh;

			sh = R_FindShader(md3Shader->name, LIGHTMAP_NONE, qtrue);
			if(sh->defaultShader)
			{
				*shaderIndex = 0;
			}
			else
			{
				*shaderIndex = sh->index;
			}
		}

		// swap all the triangles
		surf->numIndexes = mdcSurf->numTriangles * 3;
		surf->indexes = tri = ri.Hunk_Alloc(sizeof(*tri) * 3 * mdcSurf->numTriangles, h_low);

		md3Tri = (md3Triangle_t *) ((byte *) mdcSurf + mdcSurf->ofsTriangles);
		for(j = 0; j < mdcSurf->numTriangles; j++, tri += 3, md3Tri++)
		{
			tri[0] = LittleLong(md3Tri->indexes[0]);
			tri[1] = LittleLong(md3Tri->indexes[1]);
			tri[2] = LittleLong(md3Tri->indexes[2]);
		}

		// swap all the ST
		surf->st = st = ri.Hunk_Alloc(sizeof(*st) * mdcSurf->numVerts, h_low);

		md3st = (md3St_t *) ((byte *) mdcSurf + mdcSurf->ofsSt);
		for(j = 0; j < mdcSurf->numVerts; j++, md3st++, st++)
		{
			st->st[0] = LittleFloat(md3st->st[0]);
			st->st[1] = LittleFloat(md3st->st[1]);
		}

		// swap all the XyzNormals
		md3xyz = ( md3XyzNormal_t * )( (byte *)mdcSurf + mdcSurf->ofsXyzNormals );
		for ( j = 0 ; j < mdcSurf->numVerts * mdcSurf->numBaseFrames ; j++, md3xyz++ )
		{
			md3xyz->xyz[0] = LittleShort( md3xyz->xyz[0] );
			md3xyz->xyz[1] = LittleShort( md3xyz->xyz[1] );
			md3xyz->xyz[2] = LittleShort( md3xyz->xyz[2] );

			md3xyz->normal = LittleShort( md3xyz->normal );
		}

		// swap all the XyzCompressed
		mdcxyzComp = ( mdcXyzCompressed_t * )( (byte *)mdcSurf + mdcSurf->ofsXyzCompressed );
		for ( j = 0 ; j < mdcSurf->numVerts * mdcSurf->numCompFrames ; j++, mdcxyzComp++ )
		{
			LL( mdcxyzComp->ofsVec );
		}

		// swap the frameBaseFrames
		mdcBaseFrame = ( short * )( (byte *)mdcSurf + mdcSurf->ofsFrameBaseFrames );
		for ( j = 0; j < mdcModel->numFrames; j++, mdcBaseFrame++ )
		{
			*mdcBaseFrame = LittleShort( *mdcBaseFrame );
		}

		// swap the frameCompFrames
		mdcCompFrame = ( short * )( (byte *)mdcSurf + mdcSurf->ofsFrameCompFrames );
		for ( j = 0; j < mdcModel->numFrames; j++, mdcCompFrame++ )
		{
			*mdcCompFrame = LittleShort( *mdcCompFrame );
		}

		// expand the base frames
		surf->numVerts = mdcSurf->numVerts;
		surf->verts = v = ri.Hunk_Alloc(sizeof(*v) * (mdcSurf->numVerts * mdcModel->numFrames), h_low);
		
		mdcBaseFrame = ( short * )( (byte *)mdcSurf + mdcSurf->ofsFrameBaseFrames );
		for(j = 0; j < mdcModel->numFrames; j++, mdcBaseFrame++)
		{
			md3xyz = ( md3XyzNormal_t * )( (byte *)mdcSurf + mdcSurf->ofsXyzNormals ) + ( *mdcBaseFrame * surf->numVerts );
			for(k = 0; k < mdcSurf->numVerts; k++, md3xyz++, v++)
			{
				unsigned lat, lng;
				unsigned short normal;
				vec3_t fNormal;

				v->xyz[0] = md3xyz->xyz[0] * MD3_XYZ_SCALE;
				v->xyz[1] = md3xyz->xyz[1] * MD3_XYZ_SCALE;
				v->xyz[2] = md3xyz->xyz[2] * MD3_XYZ_SCALE;

				normal = md3xyz->normal;

				lat = ( normal >> 8 ) & 0xff;
				lng = ( normal & 0xff );
				lat *= (FUNCTABLE_SIZE/256);
				lng *= (FUNCTABLE_SIZE/256);

				// decode X as cos( lat ) * sin( long )
				// decode Y as sin( lat ) * sin( long )
				// decode Z as cos( long )

				fNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
				fNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
				fNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

				R_VaoPackNormal(v->normal, fNormal);
			}
		}

		// expand the compressed frames
		if (mdcSurf->numCompFrames > 0)
		{
			mdcCompFrame = ( short * )( (byte *)mdcSurf + mdcSurf->ofsFrameCompFrames );
			for(j = 0; j < mdcModel->numFrames; j++, mdcCompFrame++)
			{
				if (*mdcCompFrame < 0)
					continue;
					
				v = surf->verts + j * surf->numVerts;
				mdcxyzComp = ( mdcXyzCompressed_t * )( (byte *)mdcSurf + mdcSurf->ofsXyzCompressed ) + ( *mdcCompFrame * surf->numVerts );

				for(k = 0; k < mdcSurf->numVerts; k++, mdcxyzComp++, v++)
				{
					vec3_t ofsVec;
					vec3_t fNormal;

					R_MDC_DecodeXyzCompressed(mdcxyzComp->ofsVec, ofsVec, fNormal);
					VectorAdd( v->xyz, ofsVec, v->xyz );

					R_VaoPackNormal(v->normal, fNormal);
				}
			}
		}

		// calc tangent spaces
		{
			vec3_t *sdirs = ri.Z_Malloc(sizeof(*sdirs) * surf->numVerts * mdvModel->numFrames);
			vec3_t *tdirs = ri.Z_Malloc(sizeof(*tdirs) * surf->numVerts * mdvModel->numFrames);

			for(j = 0, v = surf->verts; j < (surf->numVerts * mdvModel->numFrames); j++, v++)
			{
				VectorClear(sdirs[j]);
				VectorClear(tdirs[j]);
			}

			for(f = 0; f < mdvModel->numFrames; f++)
			{
				for(j = 0, tri = surf->indexes; j < surf->numIndexes; j += 3, tri += 3)
				{
					vec3_t sdir, tdir;
					const float *v0, *v1, *v2, *t0, *t1, *t2;
					glIndex_t index0, index1, index2;

					index0 = surf->numVerts * f + tri[0];
					index1 = surf->numVerts * f + tri[1];
					index2 = surf->numVerts * f + tri[2];

					v0 = surf->verts[index0].xyz;
					v1 = surf->verts[index1].xyz;
					v2 = surf->verts[index2].xyz;

					t0 = surf->st[tri[0]].st;
					t1 = surf->st[tri[1]].st;
					t2 = surf->st[tri[2]].st;

					R_CalcTexDirs(sdir, tdir, v0, v1, v2, t0, t1, t2);
				
					VectorAdd(sdir, sdirs[index0], sdirs[index0]);
					VectorAdd(sdir, sdirs[index1], sdirs[index1]);
					VectorAdd(sdir, sdirs[index2], sdirs[index2]);
					VectorAdd(tdir, tdirs[index0], tdirs[index0]);
					VectorAdd(tdir, tdirs[index1], tdirs[index1]);
					VectorAdd(tdir, tdirs[index2], tdirs[index2]);
				}
			}

			for(j = 0, v = surf->verts; j < (surf->numVerts * mdvModel->numFrames); j++, v++)
			{
				vec3_t normal;
				vec4_t tangent;

				VectorNormalize(sdirs[j]);
				VectorNormalize(tdirs[j]);

				R_VaoUnpackNormal(normal, v->normal);

				tangent[3] = R_CalcTangentSpace(tangent, NULL, normal, sdirs[j], tdirs[j]);

				R_VaoPackTangent(v->tangent, tangent);
			}

			ri.Free(sdirs);
			ri.Free(tdirs);
		}

		// find the next surface
		mdcSurf = (mdcSurface_t *) ((byte *) mdcSurf + mdcSurf->ofsEnd);
		surf++;
	}

	{
		srfVaoMdvMesh_t *vaoSurf;

		mdvModel->numVaoSurfaces = mdvModel->numSurfaces;
		mdvModel->vaoSurfaces = ri.Hunk_Alloc(sizeof(*mdvModel->vaoSurfaces) * mdvModel->numSurfaces, h_low);

		vaoSurf = mdvModel->vaoSurfaces;
		surf = mdvModel->surfaces;
		for (i = 0; i < mdvModel->numSurfaces; i++, vaoSurf++, surf++)
		{
			uint32_t offset_xyz, offset_st, offset_normal, offset_tangent;
			uint32_t stride_xyz, stride_st, stride_normal, stride_tangent;
			uint32_t dataSize, dataOfs;
			uint8_t *data;

			if (mdvModel->numFrames > 1)
			{
				// vertex animation, store texcoords first, then position/normal/tangents
				offset_st      = 0;
				offset_xyz     = surf->numVerts * sizeof(vec2_t);
				offset_normal  = offset_xyz + sizeof(vec3_t);
				offset_tangent = offset_normal + sizeof(int16_t) * 4;
				stride_st  = sizeof(vec2_t);
				stride_xyz = sizeof(vec3_t) + sizeof(int16_t) * 4;
				stride_xyz += sizeof(int16_t) * 4;
				stride_normal = stride_tangent = stride_xyz;

				dataSize = offset_xyz + surf->numVerts * mdvModel->numFrames * stride_xyz;
			}
			else
			{
				// no animation, interleave everything
				offset_xyz     = 0;
				offset_st      = offset_xyz + sizeof(vec3_t);
				offset_normal  = offset_st + sizeof(vec2_t);
				offset_tangent = offset_normal + sizeof(int16_t) * 4;
				stride_xyz = offset_tangent + sizeof(int16_t) * 4;
				stride_st = stride_normal = stride_tangent = stride_xyz;

				dataSize = surf->numVerts * stride_xyz;
			}


			data = ri.Z_Malloc(dataSize);
			dataOfs = 0;

			if (mdvModel->numFrames > 1)
			{
				st = surf->st;
				for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
					memcpy(data + dataOfs, &st->st, sizeof(vec2_t));
					dataOfs += sizeof(st->st);
				}

				v = surf->verts;
				for ( j = 0; j < surf->numVerts * mdvModel->numFrames ; j++, v++ )
				{
					// xyz
					memcpy(data + dataOfs, &v->xyz, sizeof(vec3_t));
					dataOfs += sizeof(vec3_t);

					// normal
					memcpy(data + dataOfs, &v->normal, sizeof(int16_t) * 4);
					dataOfs += sizeof(int16_t) * 4;

					// tangent
					memcpy(data + dataOfs, &v->tangent, sizeof(int16_t) * 4);
					dataOfs += sizeof(int16_t) * 4;
				}
			}
			else
			{
				v = surf->verts;
				st = surf->st;
				for ( j = 0; j < surf->numVerts; j++, v++, st++ )
				{
					// xyz
					memcpy(data + dataOfs, &v->xyz, sizeof(vec3_t));
					dataOfs += sizeof(v->xyz);

					// st
					memcpy(data + dataOfs, &st->st, sizeof(vec2_t));
					dataOfs += sizeof(st->st);

					// normal
					memcpy(data + dataOfs, &v->normal, sizeof(int16_t) * 4);
					dataOfs += sizeof(int16_t) * 4;

					// tangent
					memcpy(data + dataOfs, &v->tangent, sizeof(int16_t) * 4);
					dataOfs += sizeof(int16_t) * 4;
				}
			}

			vaoSurf->surfaceType = SF_VAO_MDVMESH;
			vaoSurf->mdvModel = mdvModel;
			vaoSurf->mdvSurface = surf;
			vaoSurf->numIndexes = surf->numIndexes;
			vaoSurf->numVerts = surf->numVerts;
			
			vaoSurf->vao = R_CreateVao(va("staticMD3Mesh_VAO '%s'", surf->name), data, dataSize, (byte *)surf->indexes, surf->numIndexes * sizeof(*surf->indexes), VAO_USAGE_STATIC);

			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].enabled = 1;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].enabled = 1;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].enabled = 1;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].enabled = 1;
			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].count = 3;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].count = 2;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].count = 4;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].count = 4;

			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].type = GL_FLOAT;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].type = GL_FLOAT;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].type = GL_SHORT;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].type = GL_SHORT;

			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].normalized = GL_FALSE;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].normalized = GL_FALSE;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].normalized = GL_TRUE;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].normalized = GL_TRUE;

			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].offset = offset_xyz;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].offset = offset_st;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].offset = offset_normal;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].offset = offset_tangent;

			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].stride = stride_xyz;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].stride = stride_st;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].stride = stride_normal;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].stride = stride_tangent;

			if (mdvModel->numFrames > 1)
			{
				vaoSurf->vao->attribs[ATTR_INDEX_POSITION2] = vaoSurf->vao->attribs[ATTR_INDEX_POSITION];
				vaoSurf->vao->attribs[ATTR_INDEX_NORMAL2  ] = vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ];
				vaoSurf->vao->attribs[ATTR_INDEX_TANGENT2 ] = vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ];

				vaoSurf->vao->frameSize = stride_xyz    * surf->numVerts;
			}

			Vao_SetVertexPointers(vaoSurf->vao);

			ri.Free(data);
		}
	}

	return qtrue;
}
// done.
//-------------------------------------------------------------------------------

/*
=================
R_LoadMD3
=================
*/
static qboolean R_LoadMD3(model_t * mod, int lod, void *buffer, const char *modName)
{
	int             f, i, j;
 
	md3Header_t    *md3Model;
	md3Frame_t     *md3Frame;
	md3Surface_t   *md3Surf;
	md3Shader_t    *md3Shader;
	md3Triangle_t  *md3Tri;
	md3St_t        *md3st;
	md3XyzNormal_t *md3xyz;
	md3Tag_t       *md3Tag;

	mdvModel_t     *mdvModel;
	mdvFrame_t     *frame;
	mdvSurface_t   *surf;//, *surface;
	int            *shaderIndex;
	glIndex_t	   *tri;
	mdvVertex_t    *v;
	mdvSt_t        *st;
	mdvTag_t       *tag;
	mdvTagName_t   *tagName;

	int             version;
	int             size;

	qboolean fixRadius = qfalse;

	md3Model = (md3Header_t *) buffer;

	version = LittleLong(md3Model->version);
	if(version != MD3_VERSION)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has wrong version (%i should be %i)\n", modName, version, MD3_VERSION);
		return qfalse;
	}

	mod->type = MOD_MESH;
	size = LittleLong(md3Model->ofsEnd);
	mod->dataSize += size;
	// Ridah, convert to compressed format
	if ( !r_compressModels->integer ) {
		mdvModel = mod->mdv[lod] = ri.Hunk_Alloc(sizeof(mdvModel_t), h_low);
	} else {
		mdvModel = mod->mdv[lod] = ri.Hunk_AllocateTempMemory( size );
	}
	// done.

	//memcpy( mod->md3[lod], buffer, LittleLong( pinmodel->ofsEnd ) );

	LL(md3Model->ident);
	LL(md3Model->version);
	LL(md3Model->numFrames);
	LL(md3Model->numTags);
	LL(md3Model->numSurfaces);
	LL(md3Model->ofsFrames);
	LL(md3Model->ofsTags);
	LL(md3Model->ofsSurfaces);
	LL(md3Model->ofsEnd);

	if(md3Model->numFrames < 1)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has no frames\n", modName);
		return qfalse;
	}

	if ( strstr( mod->name,"sherman" ) || strstr( mod->name, "mg42" ) ) {
		fixRadius = qtrue;
	}

	// swap all the frames
	mdvModel->numFrames = md3Model->numFrames;
	mdvModel->frames = frame = ri.Hunk_Alloc(sizeof(*frame) * md3Model->numFrames, h_low);

	md3Frame = (md3Frame_t *) ((byte *) md3Model + md3Model->ofsFrames);
	for(i = 0; i < md3Model->numFrames; i++, frame++, md3Frame++)
	{
		frame->radius = LittleFloat( md3Frame->radius );
		if ( fixRadius ) {
			frame->radius = 256;
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = 128;
				frame->bounds[1][j] = -128;
				frame->localOrigin[j] = LittleFloat( md3Frame->localOrigin[j] );
			}
		}
		// Hack for Bug using plugin generated model
		else if ( frame->radius == 1 ) {
			frame->radius = 256;
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = 128;
				frame->bounds[1][j] = -128;
				frame->localOrigin[j] = LittleFloat( md3Frame->localOrigin[j] );
			}
		} else
		{
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = LittleFloat( md3Frame->bounds[0][j] );
				frame->bounds[1][j] = LittleFloat( md3Frame->bounds[1][j] );
				frame->localOrigin[j] = LittleFloat( md3Frame->localOrigin[j] );
			}
		}
	}

	// swap all the tags
	mdvModel->numTags = md3Model->numTags;
	mdvModel->tags = tag = ri.Hunk_Alloc(sizeof(*tag) * (md3Model->numTags * md3Model->numFrames), h_low);

	md3Tag = (md3Tag_t *) ((byte *) md3Model + md3Model->ofsTags);
	for(i = 0; i < md3Model->numTags * md3Model->numFrames; i++, tag++, md3Tag++)
	{
		for(j = 0; j < 3; j++)
		{
			tag->origin[j] = LittleFloat(md3Tag->origin[j]);
			tag->axis[0][j] = LittleFloat(md3Tag->axis[0][j]);
			tag->axis[1][j] = LittleFloat(md3Tag->axis[1][j]);
			tag->axis[2][j] = LittleFloat(md3Tag->axis[2][j]);
		}
	}


	mdvModel->tagNames = tagName = ri.Hunk_Alloc(sizeof(*tagName) * (md3Model->numTags), h_low);

	md3Tag = (md3Tag_t *) ((byte *) md3Model + md3Model->ofsTags);
	for(i = 0; i < md3Model->numTags; i++, tagName++, md3Tag++)
	{
		Q_strncpyz(tagName->name, md3Tag->name, sizeof(tagName->name));
	}

	// swap all the surfaces
	mdvModel->numSurfaces = md3Model->numSurfaces;
	mdvModel->surfaces = surf = ri.Hunk_Alloc(sizeof(*surf) * md3Model->numSurfaces, h_low);

	md3Surf = (md3Surface_t *) ((byte *) md3Model + md3Model->ofsSurfaces);
	for(i = 0; i < md3Model->numSurfaces; i++)
	{
		LL(md3Surf->ident);
		LL(md3Surf->flags);
		LL(md3Surf->numFrames);
		LL(md3Surf->numShaders);
		LL(md3Surf->numTriangles);
		LL(md3Surf->ofsTriangles);
		LL(md3Surf->numVerts);
		LL(md3Surf->ofsShaders);
		LL(md3Surf->ofsSt);
		LL(md3Surf->ofsXyzNormals);
		LL(md3Surf->ofsEnd);

		if(md3Surf->numVerts >= SHADER_MAX_VERTEXES)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has more than %i verts on %s (%i).\n",
				modName, SHADER_MAX_VERTEXES - 1, md3Surf->name[0] ? md3Surf->name : "a surface",
				md3Surf->numVerts );
			return qfalse;
		}
		if(md3Surf->numTriangles * 3 >= SHADER_MAX_INDEXES)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has more than %i triangles on %s (%i).\n",
				modName, ( SHADER_MAX_INDEXES / 3 ) - 1, md3Surf->name[0] ? md3Surf->name : "a surface",
				md3Surf->numTriangles );
			return qfalse;
		}

		// change to surface identifier
		surf->surfaceType = SF_MDV;

		// give pointer to model for Tess_SurfaceMDX
		surf->model = mdvModel;

		// copy surface name
		Q_strncpyz(surf->name, md3Surf->name, sizeof(surf->name));

		// lowercase the surface name so skin compares are faster
		Q_strlwr(surf->name);

		// strip off a trailing _1 or _2
		// this is a crutch for q3data being a mess
		j = strlen(surf->name);
		if(j > 2 && surf->name[j - 2] == '_')
		{
			surf->name[j - 2] = 0;
		}

		// register the shaders
		surf->numShaderIndexes = md3Surf->numShaders;
		surf->shaderIndexes = shaderIndex = ri.Hunk_Alloc(sizeof(*shaderIndex) * md3Surf->numShaders, h_low);
		md3Shader = (md3Shader_t *) ((byte *) md3Surf + md3Surf->ofsShaders);
		for(j = 0; j < md3Surf->numShaders; j++, shaderIndex++, md3Shader++)
		{
			shader_t       *sh;

			sh = R_FindShader(md3Shader->name, LIGHTMAP_NONE, qtrue);
			if(sh->defaultShader)
			{
				*shaderIndex = 0;
			}

			else
			{
				*shaderIndex = sh->index;
			}
		}

		// swap all the triangles
		surf->numIndexes = md3Surf->numTriangles * 3;
		surf->indexes = tri = ri.Hunk_Alloc(sizeof(*tri) * 3 * md3Surf->numTriangles, h_low);

		md3Tri = (md3Triangle_t *) ((byte *) md3Surf + md3Surf->ofsTriangles);
		for(j = 0; j < md3Surf->numTriangles; j++, tri += 3, md3Tri++)
		{
			tri[0] = LittleLong(md3Tri->indexes[0]);
			tri[1] = LittleLong(md3Tri->indexes[1]);
			tri[2] = LittleLong(md3Tri->indexes[2]);
		}

		// swap all the XyzNormals
		surf->numVerts = md3Surf->numVerts;
		surf->verts = v = ri.Hunk_Alloc(sizeof(*v) * (md3Surf->numVerts * md3Surf->numFrames), h_low);

		md3xyz = (md3XyzNormal_t *) ((byte *) md3Surf + md3Surf->ofsXyzNormals);
		for(j = 0; j < md3Surf->numVerts * md3Surf->numFrames; j++, md3xyz++, v++)
		{
			unsigned lat, lng;
			unsigned short normal;
			vec3_t fNormal;

			v->xyz[0] = LittleShort(md3xyz->xyz[0]) * MD3_XYZ_SCALE;
			v->xyz[1] = LittleShort(md3xyz->xyz[1]) * MD3_XYZ_SCALE;
			v->xyz[2] = LittleShort(md3xyz->xyz[2]) * MD3_XYZ_SCALE;

			normal = LittleShort(md3xyz->normal);

			lat = ( normal >> 8 ) & 0xff;
			lng = ( normal & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )
 
			fNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			fNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			fNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			R_VaoPackNormal(v->normal, fNormal);
		}

		// swap all the ST
		surf->st = st = ri.Hunk_Alloc(sizeof(*st) * md3Surf->numVerts, h_low);

		md3st = (md3St_t *) ((byte *) md3Surf + md3Surf->ofsSt);
		for(j = 0; j < md3Surf->numVerts; j++, md3st++, st++)
		{
			st->st[0] = LittleFloat(md3st->st[0]);
			st->st[1] = LittleFloat(md3st->st[1]);
		}

		// calc tangent spaces
		{
			vec3_t *sdirs = ri.Z_Malloc(sizeof(*sdirs) * surf->numVerts * mdvModel->numFrames);
			vec3_t *tdirs = ri.Z_Malloc(sizeof(*tdirs) * surf->numVerts * mdvModel->numFrames);

			for(j = 0, v = surf->verts; j < (surf->numVerts * mdvModel->numFrames); j++, v++)
			{
				VectorClear(sdirs[j]);
				VectorClear(tdirs[j]);
			}

			for(f = 0; f < mdvModel->numFrames; f++)
			{
				for(j = 0, tri = surf->indexes; j < surf->numIndexes; j += 3, tri += 3)
				{
					vec3_t sdir, tdir;
					const float *v0, *v1, *v2, *t0, *t1, *t2;
					glIndex_t index0, index1, index2;

					index0 = surf->numVerts * f + tri[0];
					index1 = surf->numVerts * f + tri[1];
					index2 = surf->numVerts * f + tri[2];

					v0 = surf->verts[index0].xyz;
					v1 = surf->verts[index1].xyz;
					v2 = surf->verts[index2].xyz;

					t0 = surf->st[tri[0]].st;
					t1 = surf->st[tri[1]].st;
					t2 = surf->st[tri[2]].st;

					R_CalcTexDirs(sdir, tdir, v0, v1, v2, t0, t1, t2);
				
					VectorAdd(sdir, sdirs[index0], sdirs[index0]);
					VectorAdd(sdir, sdirs[index1], sdirs[index1]);
					VectorAdd(sdir, sdirs[index2], sdirs[index2]);
					VectorAdd(tdir, tdirs[index0], tdirs[index0]);
					VectorAdd(tdir, tdirs[index1], tdirs[index1]);
					VectorAdd(tdir, tdirs[index2], tdirs[index2]);
				}
			}

			for(j = 0, v = surf->verts; j < (surf->numVerts * mdvModel->numFrames); j++, v++)
			{
				vec3_t normal;
				vec4_t tangent;

				VectorNormalize(sdirs[j]);
				VectorNormalize(tdirs[j]);

				R_VaoUnpackNormal(normal, v->normal);

				tangent[3] = R_CalcTangentSpace(tangent, NULL, normal, sdirs[j], tdirs[j]);

				R_VaoPackTangent(v->tangent, tangent);
			}

			ri.Free(sdirs);
			ri.Free(tdirs);
		}

		// find the next surface
		md3Surf = (md3Surface_t *) ((byte *) md3Surf + md3Surf->ofsEnd);
		surf++;
	}

	{
		srfVaoMdvMesh_t *vaoSurf;

		mdvModel->numVaoSurfaces = mdvModel->numSurfaces;
		mdvModel->vaoSurfaces = ri.Hunk_Alloc(sizeof(*mdvModel->vaoSurfaces) * mdvModel->numSurfaces, h_low);

		vaoSurf = mdvModel->vaoSurfaces;
		surf = mdvModel->surfaces;
		for (i = 0; i < mdvModel->numSurfaces; i++, vaoSurf++, surf++)
		{
			uint32_t offset_xyz, offset_st, offset_normal, offset_tangent;
			uint32_t stride_xyz, stride_st, stride_normal, stride_tangent;
			uint32_t dataSize, dataOfs;
			uint8_t *data;

			if (mdvModel->numFrames > 1)
			{
				// vertex animation, store texcoords first, then position/normal/tangents
				offset_st      = 0;
				offset_xyz     = surf->numVerts * sizeof(vec2_t);
				offset_normal  = offset_xyz + sizeof(vec3_t);
				offset_tangent = offset_normal + sizeof(int16_t) * 4;
				stride_st  = sizeof(vec2_t);
				stride_xyz = sizeof(vec3_t) + sizeof(int16_t) * 4;
				stride_xyz += sizeof(int16_t) * 4;
				stride_normal = stride_tangent = stride_xyz;

				dataSize = offset_xyz + surf->numVerts * mdvModel->numFrames * stride_xyz;
			}
			else
			{
				// no animation, interleave everything
				offset_xyz     = 0;
				offset_st      = offset_xyz + sizeof(vec3_t);
				offset_normal  = offset_st + sizeof(vec2_t);
				offset_tangent = offset_normal + sizeof(int16_t) * 4;
				stride_xyz = offset_tangent + sizeof(int16_t) * 4;
				stride_st = stride_normal = stride_tangent = stride_xyz;

				dataSize = surf->numVerts * stride_xyz;
			}


			data = ri.Z_Malloc(dataSize);
			dataOfs = 0;

			if (mdvModel->numFrames > 1)
			{
				st = surf->st;
				for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
					memcpy(data + dataOfs, &st->st, sizeof(vec2_t));
					dataOfs += sizeof(st->st);
				}

				v = surf->verts;
				for ( j = 0; j < surf->numVerts * mdvModel->numFrames ; j++, v++ )
				{
					// xyz
					memcpy(data + dataOfs, &v->xyz, sizeof(vec3_t));
					dataOfs += sizeof(vec3_t);

					// normal
					memcpy(data + dataOfs, &v->normal, sizeof(int16_t) * 4);
					dataOfs += sizeof(int16_t) * 4;

					// tangent
					memcpy(data + dataOfs, &v->tangent, sizeof(int16_t) * 4);
					dataOfs += sizeof(int16_t) * 4;
				}
			}
			else
			{
				v = surf->verts;
				st = surf->st;
				for ( j = 0; j < surf->numVerts; j++, v++, st++ )
				{
					// xyz
					memcpy(data + dataOfs, &v->xyz, sizeof(vec3_t));
					dataOfs += sizeof(v->xyz);

					// st
					memcpy(data + dataOfs, &st->st, sizeof(vec2_t));
					dataOfs += sizeof(st->st);

					// normal
					memcpy(data + dataOfs, &v->normal, sizeof(int16_t) * 4);
					dataOfs += sizeof(int16_t) * 4;

					// tangent
					memcpy(data + dataOfs, &v->tangent, sizeof(int16_t) * 4);
					dataOfs += sizeof(int16_t) * 4;
				}
			}

			vaoSurf->surfaceType = SF_VAO_MDVMESH;
			vaoSurf->mdvModel = mdvModel;
			vaoSurf->mdvSurface = surf;
			vaoSurf->numIndexes = surf->numIndexes;
			vaoSurf->numVerts = surf->numVerts;
			
			vaoSurf->vao = R_CreateVao(va("staticMD3Mesh_VAO '%s'", surf->name), data, dataSize, (byte *)surf->indexes, surf->numIndexes * sizeof(*surf->indexes), VAO_USAGE_STATIC);

			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].enabled = 1;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].enabled = 1;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].enabled = 1;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].enabled = 1;
			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].count = 3;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].count = 2;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].count = 4;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].count = 4;

			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].type = GL_FLOAT;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].type = GL_FLOAT;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].type = GL_SHORT;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].type = GL_SHORT;

			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].normalized = GL_FALSE;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].normalized = GL_FALSE;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].normalized = GL_TRUE;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].normalized = GL_TRUE;

			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].offset = offset_xyz;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].offset = offset_st;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].offset = offset_normal;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].offset = offset_tangent;

			vaoSurf->vao->attribs[ATTR_INDEX_POSITION].stride = stride_xyz;
			vaoSurf->vao->attribs[ATTR_INDEX_TEXCOORD].stride = stride_st;
			vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ].stride = stride_normal;
			vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ].stride = stride_tangent;

			if (mdvModel->numFrames > 1)
			{
				vaoSurf->vao->attribs[ATTR_INDEX_POSITION2] = vaoSurf->vao->attribs[ATTR_INDEX_POSITION];
				vaoSurf->vao->attribs[ATTR_INDEX_NORMAL2  ] = vaoSurf->vao->attribs[ATTR_INDEX_NORMAL  ];
				vaoSurf->vao->attribs[ATTR_INDEX_TANGENT2 ] = vaoSurf->vao->attribs[ATTR_INDEX_TANGENT ];

				vaoSurf->vao->frameSize = stride_xyz    * surf->numVerts;
			}

			Vao_SetVertexPointers(vaoSurf->vao);

			ri.Free(data);
		}
	}

	return qtrue;
}

/*
=================
R_LoadMDR
=================
*/
static qboolean R_LoadMDR( model_t *mod, void *buffer, int filesize, const char *mod_name ) 
{
	int					i, j, k, l;
	mdrHeader_t			*pinmodel, *mdr;
	mdrFrame_t			*frame;
	mdrLOD_t			*lod, *curlod;
	mdrSurface_t			*surf, *cursurf;
	mdrTriangle_t			*tri, *curtri;
	mdrVertex_t			*v, *curv;
	mdrWeight_t			*weight, *curweight;
	mdrTag_t			*tag, *curtag;
	int					size;
	shader_t			*sh;

	pinmodel = (mdrHeader_t *)buffer;

	pinmodel->version = LittleLong(pinmodel->version);
	if (pinmodel->version != MDR_VERSION) 
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has wrong version (%i should be %i)\n", mod_name, pinmodel->version, MDR_VERSION);
		return qfalse;
	}

	size = LittleLong(pinmodel->ofsEnd);
	
	if(size > filesize)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: Header of %s is broken. Wrong filesize declared!\n", mod_name);
		return qfalse;
	}
	
	mod->type = MOD_MDR;

	LL(pinmodel->numFrames);
	LL(pinmodel->numBones);
	LL(pinmodel->ofsFrames);

	// This is a model that uses some type of compressed Bones. We don't want to uncompress every bone for each rendered frame
	// over and over again, we'll uncompress it in this function already, so we must adjust the size of the target mdr.
	if(pinmodel->ofsFrames < 0)
	{
		// mdrFrame_t is larger than mdrCompFrame_t:
		size += pinmodel->numFrames * sizeof(frame->name);
		// now add enough space for the uncompressed bones.
		size += pinmodel->numFrames * pinmodel->numBones * ((sizeof(mdrBone_t) - sizeof(mdrCompBone_t)));
	}
	
	// simple bounds check
	if(pinmodel->numBones < 0 ||
		sizeof(*mdr) + pinmodel->numFrames * (sizeof(*frame) + (pinmodel->numBones - 1) * sizeof(*frame->bones)) > size)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
		return qfalse;
	}

	mod->dataSize += size;
	mod->modelData = mdr = ri.Hunk_Alloc( size, h_low );

	// Copy all the values over from the file and fix endian issues in the process, if necessary.
	
	mdr->ident = LittleLong(pinmodel->ident);
	mdr->version = pinmodel->version;	// Don't need to swap byte order on this one, we already did above.
	Q_strncpyz(mdr->name, pinmodel->name, sizeof(mdr->name));
	mdr->numFrames = pinmodel->numFrames;
	mdr->numBones = pinmodel->numBones;
	mdr->numLODs = LittleLong(pinmodel->numLODs);
	mdr->numTags = LittleLong(pinmodel->numTags);
	// We don't care about the other offset values, we'll generate them ourselves while loading.

	mod->numLods = mdr->numLODs;

	if ( mdr->numFrames < 1 ) 
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has no frames\n", mod_name);
		return qfalse;
	}

	/* The first frame will be put into the first free space after the header */
	frame = (mdrFrame_t *)(mdr + 1);
	mdr->ofsFrames = (int)((byte *) frame - (byte *) mdr);
		
	if (pinmodel->ofsFrames < 0)
	{
		mdrCompFrame_t *cframe;
				
		// compressed model...				
		cframe = (mdrCompFrame_t *)((byte *) pinmodel - pinmodel->ofsFrames);
		
		for(i = 0; i < mdr->numFrames; i++)
		{
			for(j = 0; j < 3; j++)
			{
				frame->bounds[0][j] = LittleFloat(cframe->bounds[0][j]);
				frame->bounds[1][j] = LittleFloat(cframe->bounds[1][j]);
				frame->localOrigin[j] = LittleFloat(cframe->localOrigin[j]);
			}

			frame->radius = LittleFloat(cframe->radius);
			frame->name[0] = '\0';	// No name supplied in the compressed version.
			
			for(j = 0; j < mdr->numBones; j++)
			{
				for(k = 0; k < (sizeof(cframe->bones[j].Comp) / 2); k++)
				{
					// Do swapping for the uncompressing functions. They seem to use shorts
					// values only, so I assume this will work. Never tested it on other
					// platforms, though.
					
					((unsigned short *)(cframe->bones[j].Comp))[k] =
						LittleShort( ((unsigned short *)(cframe->bones[j].Comp))[k] );
				}
				
				/* Now do the actual uncompressing */
				MC_UnCompress(frame->bones[j].matrix, cframe->bones[j].Comp);
			}
			
			// Next Frame...
			cframe = (mdrCompFrame_t *) &cframe->bones[j];
			frame = (mdrFrame_t *) &frame->bones[j];
		}
	}
	else
	{
		mdrFrame_t *curframe;
		
		// uncompressed model...
		//
    
		curframe = (mdrFrame_t *)((byte *) pinmodel + pinmodel->ofsFrames);
		
		// swap all the frames
		for ( i = 0 ; i < mdr->numFrames ; i++) 
		{
			for(j = 0; j < 3; j++)
			{
				frame->bounds[0][j] = LittleFloat(curframe->bounds[0][j]);
				frame->bounds[1][j] = LittleFloat(curframe->bounds[1][j]);
				frame->localOrigin[j] = LittleFloat(curframe->localOrigin[j]);
			}
			
			frame->radius = LittleFloat(curframe->radius);
			Q_strncpyz(frame->name, curframe->name, sizeof(frame->name));
			
			for (j = 0; j < (int) (mdr->numBones * sizeof(mdrBone_t) / 4); j++) 
			{
				((float *)frame->bones)[j] = LittleFloat( ((float *)curframe->bones)[j] );
			}
			
			curframe = (mdrFrame_t *) &curframe->bones[mdr->numBones];
			frame = (mdrFrame_t *) &frame->bones[mdr->numBones];
		}
	}
	
	// frame should now point to the first free address after all frames.
	lod = (mdrLOD_t *) frame;
	mdr->ofsLODs = (int) ((byte *) lod - (byte *)mdr);
	
	curlod = (mdrLOD_t *)((byte *) pinmodel + LittleLong(pinmodel->ofsLODs));
		
	// swap all the LOD's
	for ( l = 0 ; l < mdr->numLODs ; l++)
	{
		// simple bounds check
		if((byte *) (lod + 1) > (byte *) mdr + size)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
			return qfalse;
		}

		lod->numSurfaces = LittleLong(curlod->numSurfaces);
		
		// swap all the surfaces
		surf = (mdrSurface_t *) (lod + 1);
		lod->ofsSurfaces = (int)((byte *) surf - (byte *) lod);
		cursurf = (mdrSurface_t *) ((byte *)curlod + LittleLong(curlod->ofsSurfaces));
		
		for ( i = 0 ; i < lod->numSurfaces ; i++)
		{
			// simple bounds check
			if((byte *) (surf + 1) > (byte *) mdr + size)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
				return qfalse;
			}

			// first do some copying stuff
			
			surf->ident = SF_MDR;
			Q_strncpyz(surf->name, cursurf->name, sizeof(surf->name));
			Q_strncpyz(surf->shader, cursurf->shader, sizeof(surf->shader));
			
			surf->ofsHeader = (byte *) mdr - (byte *) surf;
			
			surf->numVerts = LittleLong(cursurf->numVerts);
			surf->numTriangles = LittleLong(cursurf->numTriangles);
			// numBoneReferences and BoneReferences generally seem to be unused
			
			// now do the checks that may fail.
			if ( surf->numVerts >= SHADER_MAX_VERTEXES ) 
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has more than %i verts on %s (%i).\n",
					  mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
					  surf->numVerts );
				return qfalse;
			}
			if ( surf->numTriangles*3 >= SHADER_MAX_INDEXES ) 
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has more than %i triangles on %s (%i).\n",
					  mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, surf->name[0] ? surf->name : "a surface",
					  surf->numTriangles );
				return qfalse;
			}
			// lowercase the surface name so skin compares are faster
			Q_strlwr( surf->name );

			// register the shaders
			sh = R_FindShader(surf->shader, LIGHTMAP_NONE, qtrue);
			if ( sh->defaultShader ) {
				surf->shaderIndex = 0;
			} else {
				surf->shaderIndex = sh->index;
			}
			
			// now copy the vertexes.
			v = (mdrVertex_t *) (surf + 1);
			surf->ofsVerts = (int)((byte *) v - (byte *) surf);
			curv = (mdrVertex_t *) ((byte *)cursurf + LittleLong(cursurf->ofsVerts));
			
			for(j = 0; j < surf->numVerts; j++)
			{
				LL(curv->numWeights);
			
				// simple bounds check
				if(curv->numWeights < 0 || (byte *) (v + 1) + (curv->numWeights - 1) * sizeof(*weight) > (byte *) mdr + size)
				{
					ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
					return qfalse;
				}

				v->normal[0] = LittleFloat(curv->normal[0]);
				v->normal[1] = LittleFloat(curv->normal[1]);
				v->normal[2] = LittleFloat(curv->normal[2]);
				
				v->texCoords[0] = LittleFloat(curv->texCoords[0]);
				v->texCoords[1] = LittleFloat(curv->texCoords[1]);
				
				v->numWeights = curv->numWeights;
				weight = &v->weights[0];
				curweight = &curv->weights[0];
				
				// Now copy all the weights
				for(k = 0; k < v->numWeights; k++)
				{
					weight->boneIndex = LittleLong(curweight->boneIndex);
					weight->boneWeight = LittleFloat(curweight->boneWeight);
					
					weight->offset[0] = LittleFloat(curweight->offset[0]);
					weight->offset[1] = LittleFloat(curweight->offset[1]);
					weight->offset[2] = LittleFloat(curweight->offset[2]);
					
					weight++;
					curweight++;
				}
				
				v = (mdrVertex_t *) weight;
				curv = (mdrVertex_t *) curweight;
			}
						
			// we know the offset to the triangles now:
			tri = (mdrTriangle_t *) v;
			surf->ofsTriangles = (int)((byte *) tri - (byte *) surf);
			curtri = (mdrTriangle_t *)((byte *) cursurf + LittleLong(cursurf->ofsTriangles));
			
			// simple bounds check
			if(surf->numTriangles < 0 || (byte *) (tri + surf->numTriangles) > (byte *) mdr + size)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
				return qfalse;
			}

			for(j = 0; j < surf->numTriangles; j++)
			{
				tri->indexes[0] = LittleLong(curtri->indexes[0]);
				tri->indexes[1] = LittleLong(curtri->indexes[1]);
				tri->indexes[2] = LittleLong(curtri->indexes[2]);
				
				tri++;
				curtri++;
			}
			
			// tri now points to the end of the surface.
			surf->ofsEnd = (byte *) tri - (byte *) surf;
			surf = (mdrSurface_t *) tri;

			// find the next surface.
			cursurf = (mdrSurface_t *) ((byte *) cursurf + LittleLong(cursurf->ofsEnd));
		}

		// surf points to the next lod now.
		lod->ofsEnd = (int)((byte *) surf - (byte *) lod);
		lod = (mdrLOD_t *) surf;

		// find the next LOD.
		curlod = (mdrLOD_t *)((byte *) curlod + LittleLong(curlod->ofsEnd));
	}
	
	// lod points to the first tag now, so update the offset too.
	tag = (mdrTag_t *) lod;
	mdr->ofsTags = (int)((byte *) tag - (byte *) mdr);
	curtag = (mdrTag_t *) ((byte *)pinmodel + LittleLong(pinmodel->ofsTags));

	// simple bounds check
	if(mdr->numTags < 0 || (byte *) (tag + mdr->numTags) > (byte *) mdr + size)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
		return qfalse;
	}
	
	for (i = 0 ; i < mdr->numTags ; i++)
	{
		tag->boneIndex = LittleLong(curtag->boneIndex);
		Q_strncpyz(tag->name, curtag->name, sizeof(tag->name));
		
		tag++;
		curtag++;
	}
	
	// And finally we know the real offset to the end.
	mdr->ofsEnd = (int)((byte *) tag - (byte *) mdr);

	// phew! we're done.
	
	return qtrue;
}


/*
=================
R_LoadMDS
=================
*/
static qboolean R_LoadMDS( model_t *mod, void *buffer, const char *mod_name ) {
	int i, j, k;
	mdsHeader_t         *pinmodel, *mds;
	mdsFrame_t          *frame;
	mdsSurface_t        *surf;
	mdsTriangle_t       *tri;
	mdsVertex_t         *v;
	mdsBoneInfo_t       *bi;
	mdsTag_t            *tag;
	int version;
	int size;
	shader_t            *sh;
	int frameSize;
	int                 *collapseMap, *boneref;

	pinmodel = (mdsHeader_t *)buffer;

	version = LittleLong( pinmodel->version );
	if ( version != MDS_VERSION ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDS: %s has wrong version (%i should be %i)\n",
				   mod_name, version, MDS_VERSION );
		return qfalse;
	}

	mod->type = MOD_MDS;
	size = LittleLong( pinmodel->ofsEnd );
	mod->dataSize += size;
	mds = mod->mds = ri.Hunk_Alloc( size, h_low );

	memcpy( mds, buffer, LittleLong( pinmodel->ofsEnd ) );

	LL( mds->ident );
	LL( mds->version );
	LL( mds->numFrames );
	LL( mds->numBones );
	LL( mds->numTags );
	LL( mds->numSurfaces );
	LL( mds->ofsFrames );
	LL( mds->ofsBones );
	LL( mds->ofsTags );
	LL( mds->ofsEnd );
	LL( mds->ofsSurfaces );
	mds->lodBias = LittleFloat( mds->lodBias );
	mds->lodScale = LittleFloat( mds->lodScale );
	LL( mds->torsoParent );

	if ( mds->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDS: %s has no frames\n", mod_name );
		return qfalse;
	}

	if ( LittleLong( 1 ) != 1 ) {
		// swap all the frames
		//frameSize = (int)( &((mdsFrame_t *)0)->bones[ mds->numBones ] );
		frameSize = (int) ( sizeof( mdsFrame_t ) - sizeof( mdsBoneFrameCompressed_t ) + mds->numBones * sizeof( mdsBoneFrameCompressed_t ) );
		for ( i = 0 ; i < mds->numFrames ; i++, frame++ ) {
			frame = ( mdsFrame_t * )( (byte *)mds + mds->ofsFrames + i * frameSize );
			frame->radius = LittleFloat( frame->radius );
			for ( j = 0 ; j < 3 ; j++ ) {
				frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
				frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
				frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
				frame->parentOffset[j] = LittleFloat( frame->parentOffset[j] );
			}
			for ( j = 0 ; j < mds->numBones * sizeof( mdsBoneFrameCompressed_t ) / sizeof( short ) ; j++ ) {
				( (short *)frame->bones )[j] = LittleShort( ( (short *)frame->bones )[j] );
			}
		}

		// swap all the tags
		tag = ( mdsTag_t * )( (byte *)mds + mds->ofsTags );
		for ( i = 0 ; i < mds->numTags ; i++, tag++ ) {
			LL( tag->boneIndex );
			tag->torsoWeight = LittleFloat( tag->torsoWeight );
		}

		// swap all the bones
		for ( i = 0 ; i < mds->numBones ; i++, bi++ ) {
			bi = ( mdsBoneInfo_t * )( (byte *)mds + mds->ofsBones + i * sizeof( mdsBoneInfo_t ) );
			LL( bi->parent );
			bi->torsoWeight = LittleFloat( bi->torsoWeight );
			bi->parentDist = LittleFloat( bi->parentDist );
			LL( bi->flags );
		}
	}

	// swap all the surfaces
	surf = ( mdsSurface_t * )( (byte *)mds + mds->ofsSurfaces );
	for ( i = 0 ; i < mds->numSurfaces ; i++ ) {
		if ( LittleLong( 1 ) != 1 ) {
			LL( surf->ident );
			LL( surf->shaderIndex );
			LL( surf->minLod );
			LL( surf->ofsHeader );
			LL( surf->ofsCollapseMap );
			LL( surf->numTriangles );
			LL( surf->ofsTriangles );
			LL( surf->numVerts );
			LL( surf->ofsVerts );
			LL( surf->numBoneReferences );
			LL( surf->ofsBoneReferences );
			LL( surf->ofsEnd );
		}

		// change to surface identifier
		surf->ident = SF_MDS;

		if ( surf->numVerts >= SHADER_MAX_VERTEXES ) {
			ri.Printf(PRINT_WARNING, "R_LoadMDS: %s has more than %i verts on %s (%i).\n",
				mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
				surf->numVerts );
			return qfalse;
		}
		if ( surf->numTriangles*3 >= SHADER_MAX_INDEXES ) {
			ri.Printf(PRINT_WARNING, "R_LoadMDS: %s has more than %i triangles on %s (%i).\n",
				mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, surf->name[0] ? surf->name : "a surface",
				surf->numTriangles );
			return qfalse;
		}

		// register the shaders
		if ( surf->shader[0] ) {
			sh = R_FindShader( surf->shader, LIGHTMAP_NONE, qtrue );
			if ( sh->defaultShader ) {
				surf->shaderIndex = 0;
			} else {
				surf->shaderIndex = sh->index;
			}
		} else {
			surf->shaderIndex = 0;
		}

		if ( LittleLong( 1 ) != 1 ) {
			// swap all the triangles
			tri = ( mdsTriangle_t * )( (byte *)surf + surf->ofsTriangles );
			for ( j = 0 ; j < surf->numTriangles ; j++, tri++ ) {
				LL( tri->indexes[0] );
				LL( tri->indexes[1] );
				LL( tri->indexes[2] );
			}

			// swap all the vertexes
			v = ( mdsVertex_t * )( (byte *)surf + surf->ofsVerts );
			for ( j = 0 ; j < surf->numVerts ; j++ ) {
				v->normal[0] = LittleFloat( v->normal[0] );
				v->normal[1] = LittleFloat( v->normal[1] );
				v->normal[2] = LittleFloat( v->normal[2] );

				v->texCoords[0] = LittleFloat( v->texCoords[0] );
				v->texCoords[1] = LittleFloat( v->texCoords[1] );

				v->numWeights = LittleLong( v->numWeights );

				for ( k = 0 ; k < v->numWeights ; k++ ) {
					v->weights[k].boneIndex = LittleLong( v->weights[k].boneIndex );
					v->weights[k].boneWeight = LittleFloat( v->weights[k].boneWeight );
					v->weights[k].offset[0] = LittleFloat( v->weights[k].offset[0] );
					v->weights[k].offset[1] = LittleFloat( v->weights[k].offset[1] );
					v->weights[k].offset[2] = LittleFloat( v->weights[k].offset[2] );
				}

				v = (mdsVertex_t *)&v->weights[v->numWeights];
			}

			// swap the collapse map
			collapseMap = ( int * )( (byte *)surf + surf->ofsCollapseMap );
			for ( j = 0; j < surf->numVerts; j++, collapseMap++ ) {
				*collapseMap = LittleLong( *collapseMap );
			}

			// swap the bone references
			boneref = ( int * )( ( byte *)surf + surf->ofsBoneReferences );
			for ( j = 0; j < surf->numBoneReferences; j++, boneref++ ) {
				*boneref = LittleLong( *boneref );
			}
		}

		// find the next surface
		surf = ( mdsSurface_t * )( (byte *)surf + surf->ofsEnd );
	}

	return qtrue;
}




//=============================================================================

/*
** RE_BeginRegistration
*/
void RE_BeginRegistration( glconfig_t *glconfigOut ) {
	int	i;

	ri.Hunk_Clear();    // (SA) MEM NOTE: not in missionpack

	R_Init();
	*glconfigOut = glConfig;

	R_IssuePendingRenderCommands();

	tr.visIndex = 0;
	// force markleafs to regenerate
	for(i = 0; i < MAX_VISCOUNTS; i++) {
		tr.visClusters[i] = -2;
	}

	R_ClearFlares();
	RE_ClearScene();

	tr.registered = qtrue;
}

/*
===============
R_ModelInit
===============
*/
void R_ModelInit( void ) {
	model_t     *mod;

	// leave a space for NULL model
	tr.numModels = 0;

	mod = R_AllocModel();
	mod->type = MOD_BAD;
}


/*
================
R_Modellist_f
================
*/

void R_Modellist_f( void ) {
	int i, j;
	model_t *mod;
	int total;
	int lods;

	total = 0;
	for ( i = 1 ; i < tr.numModels; i++ ) {
		mod = tr.models[i];
		lods = 1;
		for ( j = 1 ; j < MD3_MAX_LODS ; j++ ) {
			if ( mod->mdv[j] && mod->mdv[j] != mod->mdv[j - 1] ) {
				lods++;
			}
		}
		ri.Printf( PRINT_ALL, "%8i : (%i) %s\n",mod->dataSize, lods, mod->name );
		total += mod->dataSize;
	}
	ri.Printf( PRINT_ALL, "%8i : Total models\n", total );
}


//=============================================================================


/*
================
R_GetTag
================
*/
static int R_GetTag(mdvModel_t * model, int frame, const char *_tagName, int startTagIndex, mdvTag_t ** outTag)
{
	int             i;
	mdvTag_t       *tag;
	mdvTagName_t   *tagName;

	if ( frame >= model->numFrames ) {
		// it is possible to have a bad frame while changing models, so don't error
		frame = model->numFrames - 1;
	}

	if(startTagIndex > model->numTags)
	{
		*outTag = NULL;
		return -1;
	}

#if 1
	tag = model->tags + frame * model->numTags;
	tagName = model->tagNames;
	for(i = 0; i < model->numTags; i++, tag++, tagName++)
	{
		if((i >= startTagIndex) && !strcmp(tagName->name, _tagName))
		{
			*outTag = tag;
			return i;
		}
	}
#endif

	*outTag = NULL;
	return -1;
}

static int R_GetAnimTag( mdrHeader_t *mod, int framenum, const char *tagName, int startTagIndex, mdvTag_t **outTag)
{
	int				i, j, k;
	int				frameSize;
	mdrFrame_t		*frame;
	mdrTag_t		*tag;
	mdvTag_t		*dest = *outTag;

	if ( framenum >= mod->numFrames ) 
	{
		// it is possible to have a bad frame while changing models, so don't error
		framenum = mod->numFrames - 1;
	}

	if ( startTagIndex > mod->numTags ) 
	{
		*outTag = NULL;
		return -1;
	}

	tag = (mdrTag_t *)((byte *)mod + mod->ofsTags);
	for ( i = 0 ; i < mod->numTags ; i++, tag++ )
	{
		if ( ( i >= startTagIndex ) && !strcmp( tag->name, tagName ) )
		{
			// uncompressed model...
			//
			frameSize = (intptr_t)( &((mdrFrame_t *)0)->bones[ mod->numBones ] );
			frame = (mdrFrame_t *)((byte *)mod + mod->ofsFrames + framenum * frameSize );

			for (j = 0; j < 3; j++)
			{
				for (k = 0; k < 3; k++)
					dest->axis[j][k]=frame->bones[tag->boneIndex].matrix[k][j];
			}

			dest->origin[0]=frame->bones[tag->boneIndex].matrix[0][3];
			dest->origin[1]=frame->bones[tag->boneIndex].matrix[1][3];
			dest->origin[2]=frame->bones[tag->boneIndex].matrix[2][3];				

			return i;
		}
	}

	*outTag = NULL;
	return -1;
}


/*
================
R_LerpTag

  returns the index of the tag it found, for cycling through tags with the same name
================
*/
int R_LerpTag( orientation_t *tag, const refEntity_t *refent, const char *tagNameIn, int startIndex ) {
	mdvTag_t    *start, *end;
	mdvTag_t	start_space, end_space;
	int i;
	float frontLerp, backLerp;
	model_t     *model;
	char tagName[MAX_QPATH];       //, *ch;
	int retval = 0;
	qhandle_t handle;
	int startFrame, endFrame;
	float frac;

	handle = refent->hModel;
	startFrame = refent->oldframe;
	endFrame = refent->frame;
	frac = 1.0 - refent->backlerp;

	Q_strncpyz( tagName, tagNameIn, MAX_QPATH );

	model = R_GetModelByHandle( handle );
	if ( !model->mdv[0] /*&& !model->mdc[0]*/ && !model->mds ) {
		if(model->type == MOD_MDR)
		{
			start = &start_space;
			end = &end_space;
			retval = R_GetAnimTag((mdrHeader_t *) model->modelData, startFrame, tagName, startIndex, &start);
			R_GetAnimTag((mdrHeader_t *) model->modelData, endFrame, tagName, startIndex, &end);
		}
		else if ( model->type == MOD_IQM ) {
			return R_IQMLerpTag( tag, model->modelData,
					startFrame, endFrame,
					frac, tagName, startIndex );
		} else {
			start = end = NULL;
		}
	} else if ( model->type == MOD_MESH ) {
		// old MD3 style
		retval = R_GetTag(model->mdv[0], startFrame, tagName, startIndex, &start);
		R_GetTag(model->mdv[0], endFrame, tagName, startIndex, &end);
	} else if ( model->type == MOD_MDS ) {    // use bone lerping

		retval = R_GetBoneTag( tag, model->mds, startIndex, refent, tagNameIn );

		if ( retval >= 0 ) {
			return retval;
		}

		// failed
		return -1;
	} else {
		// failed
		return -1;
	}

	if ( !start || !end ) {
		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return -1;
	}

	frontLerp = frac;
	backLerp = 1.0f - frac;

	for ( i = 0 ; i < 3 ; i++ ) {
		tag->origin[i] = start->origin[i] * backLerp +  end->origin[i] * frontLerp;
		tag->axis[0][i] = start->axis[0][i] * backLerp +  end->axis[0][i] * frontLerp;
		tag->axis[1][i] = start->axis[1][i] * backLerp +  end->axis[1][i] * frontLerp;
		tag->axis[2][i] = start->axis[2][i] * backLerp +  end->axis[2][i] * frontLerp;
	}

	VectorNormalize( tag->axis[0] );
	VectorNormalize( tag->axis[1] );
	VectorNormalize( tag->axis[2] );

	return retval;
}

/*
===============
R_TagInfo_f
===============
*/
void R_TagInfo_f( void ) {
	Com_Printf( "command not functional\n" );
}


/*
====================
R_ModelBounds
====================
*/
void R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs ) {
	model_t     *model;

	model = R_GetModelByHandle( handle );

	if ( model->bmodel ) {
		VectorCopy( model->bmodel->bounds[0], mins );
		VectorCopy( model->bmodel->bounds[1], maxs );
		return;
	}

	// Ridah
	if ( model->mdv[0] ) {
		mdvModel_t     *header;
		mdvFrame_t     *frame;

		header = model->mdv[0];
		frame = header->frames;

		VectorCopy( frame->bounds[0], mins );
		VectorCopy( frame->bounds[1], maxs );
		return;
	} else if (model->type == MOD_MDR) {
		mdrHeader_t	*header;
		mdrFrame_t	*frame;

		header = (mdrHeader_t *)model->modelData;
		frame = (mdrFrame_t *) ((byte *)header + header->ofsFrames);

		VectorCopy( frame->bounds[0], mins );
		VectorCopy( frame->bounds[1], maxs );
		
		return;
	} else if(model->type == MOD_IQM) {
		iqmData_t *iqmData;
		
		iqmData = model->modelData;

		if(iqmData->bounds)
		{
			VectorCopy(iqmData->bounds, mins);
			VectorCopy(iqmData->bounds + 3, maxs);
			return;
		}
	}

	VectorClear( mins );
	VectorClear( maxs );
	// done.
}


void *R_Hunk_Begin( void ) {
	return NULL;
}

void *R_Hunk_Alloc( int size ) {
	return NULL;
}

// this is only called when we shutdown GL
void R_Hunk_End( void ) {
	return;
}

void R_Hunk_Reset( void ) {
	return;
}

