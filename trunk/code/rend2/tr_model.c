/*
===========================================================================

Return to Castle Wolfenstein multiplayer GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein multiplayer GPL Source Code (RTCW MP Source Code).  

RTCW MP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW MP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW MP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW MP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW MP Source Code.  If not, please request a copy in writing from id Software at the address below.

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
#ifdef RAVENMD4
static qboolean R_LoadMDR( model_t *mod, void *buffer, int filesize, const char *mod_name );
#endif

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
	int         size;
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
			filename[strlen( filename ) - 1] = '3';  // try MD3 first
		} else {
			filename[strlen( filename ) - 1] = 'c';  // try MDC first
		}

		size = ri.FS_ReadFile( namebuf, &buf.v );
		if ( !buf.u ) {
			if ( r_compressModels->integer ) {
				filename[strlen( filename ) - 1] = 'c';  // try MDC second
			} else {
				filename[strlen( filename ) - 1] = '3';  // try MD3 second
			}
			ri.FS_ReadFile( filename, (void **)&buf );
			if ( !buf.u ) {
				continue;
			}
		}
		
		ident = LittleLong(* (unsigned *) buf.u);
		if (ident == MDS_IDENT)
			loaded = R_LoadMDS(mod, buf.u, name);
		else
		{
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
		}
		
		ri.FS_FreeFile(buf.v);

		if(loaded)
		{
			mod->numLods++;
			numLoaded++;
			// if we have a valid model and are biased
			// so that we won't see any higher detail ones,
			// stop loading them
			if ( lod <= r_lodbias->integer ) {
				break;
			}
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

#ifdef RAVENMD4
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
#endif

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
#ifdef RAVENMD4
	{ "mdr", R_RegisterMDR },
#endif
	{ "mds", R_RegisterMD3 },
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

	// Ridah, caching
	if ( r_cacheGathering->integer ) {
		ri.Cmd_ExecuteText( EXEC_NOW, va( "cache_usedfile model %s\n", name ) );
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
		ri.Printf( PRINT_WARNING, "RE_RegisterModel: R_AllocModel() failed for '%s'\n", name);
		return 0;
	}

	// only set the name after the model has been successfully loaded
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );


	R_IssuePendingRenderCommands();

	// Ridah, look for it cached
	if ( R_FindCachedModel( name, mod ) ) {
		return mod->index;
	}
	// done.

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
	int i, best_start_i[3] = { 0 }, next_start, next_end;
	int best = 0; // TTimo: init
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

	i = sizeof( mdcXyzCompressed_t );

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
	srfTriangle_t  *tri;
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

		if(mdcSurf->numVerts > SHADER_MAX_VERTEXES)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMDC: %s has more than %i verts on a surface (%i)",
					 modName, SHADER_MAX_VERTEXES, mdcSurf->numVerts);
			return qfalse;
		}
		if(mdcSurf->numTriangles * 3 > SHADER_MAX_INDEXES)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMDC: %s has more than %i triangles on a surface (%i)",
					 modName, SHADER_MAX_INDEXES / 3, mdcSurf->numTriangles);
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
		surf->numTriangles = mdcSurf->numTriangles;
		surf->triangles = tri = ri.Hunk_Alloc(sizeof(*tri) * mdcSurf->numTriangles, h_low);

		md3Tri = (md3Triangle_t *) ((byte *) mdcSurf + mdcSurf->ofsTriangles);
		for(j = 0; j < mdcSurf->numTriangles; j++, tri++, md3Tri++)
		{
			tri->indexes[0] = LittleLong(md3Tri->indexes[0]);
			tri->indexes[1] = LittleLong(md3Tri->indexes[1]);
			tri->indexes[2] = LittleLong(md3Tri->indexes[2]);
		}

		R_CalcSurfaceTriangleNeighbors(surf->numTriangles, surf->triangles);

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

				v->xyz[0] = md3xyz->xyz[0] * MD3_XYZ_SCALE;
				v->xyz[1] = md3xyz->xyz[1] * MD3_XYZ_SCALE;
				v->xyz[2] = md3xyz->xyz[2] * MD3_XYZ_SCALE;

				normal = md3xyz->normal;

				lat = ( normal >> 8 ) & 0xff;
				lng = ( normal & 0xff );
				lat *= (FUNCTABLE_SIZE/256);
				lng *= (FUNCTABLE_SIZE/256);

				v->normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
				v->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
				v->normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
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
					R_MDC_DecodeXyzCompressed( mdcxyzComp->ofsVec, ofsVec, v->normal );
					VectorAdd( v->xyz, ofsVec, v->xyz );

				}
			}
		}

#ifdef USE_VERT_TANGENT_SPACE
		// calc tangent spaces
		{
			// Valgrind complaints: Conditional jump or move depends on uninitialised value(s)
			// So lets Initialize them.
			const float    *v0 = NULL, *v1 = NULL, *v2 = NULL;
			const float    *t0 = NULL, *t1 = NULL, *t2 = NULL;
			vec3_t          tangent = { 0, 0, 0 };
			vec3_t          bitangent = { 0, 0, 0 };
			vec3_t          normal = { 0, 0, 0 };

			for(j = 0, v = surf->verts; j < (surf->numVerts * mdvModel->numFrames); j++, v++)
			{
				VectorClear(v->tangent);
				VectorClear(v->bitangent);
				if (r_recalcMD3Normals->integer)
					VectorClear(v->normal);
			}

			for(f = 0; f < mdvModel->numFrames; f++)
			{
				for(j = 0, tri = surf->triangles; j < surf->numTriangles; j++, tri++)
				{
					v0 = surf->verts[surf->numVerts * f + tri->indexes[0]].xyz;
					v1 = surf->verts[surf->numVerts * f + tri->indexes[1]].xyz;
					v2 = surf->verts[surf->numVerts * f + tri->indexes[2]].xyz;

					t0 = surf->st[tri->indexes[0]].st;
					t1 = surf->st[tri->indexes[1]].st;
					t2 = surf->st[tri->indexes[2]].st;

					if (!r_recalcMD3Normals->integer)
						VectorCopy(v->normal, normal);
					else
						VectorClear(normal);

					#if 1
					R_CalcTangentSpace(tangent, bitangent, normal, v0, v1, v2, t0, t1, t2);
					#else
					R_CalcNormalForTriangle(normal, v0, v1, v2);
					R_CalcTangentsForTriangle(tangent, bitangent, v0, v1, v2, t0, t1, t2);
					#endif

					for(k = 0; k < 3; k++)
					{
						float          *v;

						v = surf->verts[surf->numVerts * f + tri->indexes[k]].tangent;
						VectorAdd(v, tangent, v);

						v = surf->verts[surf->numVerts * f + tri->indexes[k]].bitangent;
						VectorAdd(v, bitangent, v);

						if (r_recalcMD3Normals->integer)
						{
							v = surf->verts[surf->numVerts * f + tri->indexes[k]].normal;
							VectorAdd(v, normal, v);
						}
					}
				}
			}

			for(j = 0, v = surf->verts; j < (surf->numVerts * mdvModel->numFrames); j++, v++)
			{
				VectorNormalize(v->tangent);
				VectorNormalize(v->bitangent);
				VectorNormalize(v->normal);
			}
		}
#endif

		// find the next surface
		mdcSurf = (mdcSurface_t *) ((byte *) mdcSurf + mdcSurf->ofsEnd);
		surf++;
	}

	{
		srfVBOMDVMesh_t *vboSurf;

		mdvModel->numVBOSurfaces = mdvModel->numSurfaces;
		mdvModel->vboSurfaces = ri.Hunk_Alloc(sizeof(*mdvModel->vboSurfaces) * mdvModel->numSurfaces, h_low);

		vboSurf = mdvModel->vboSurfaces;
		surf = mdvModel->surfaces;
		for (i = 0; i < mdvModel->numSurfaces; i++, vboSurf++, surf++)
		{
			vec3_t *verts;
			vec3_t *normals;
			vec2_t *texcoords;
#ifdef USE_VERT_TANGENT_SPACE
			vec3_t *tangents;
			vec3_t *bitangents;
#endif

			byte *data;
			int dataSize;

			int ofs_xyz, ofs_normal, ofs_st;
#ifdef USE_VERT_TANGENT_SPACE
			int ofs_tangent, ofs_bitangent;
#endif

			dataSize = 0;

			ofs_xyz = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*verts);

			ofs_normal = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*normals);

#ifdef USE_VERT_TANGENT_SPACE
			ofs_tangent = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*tangents);

			ofs_bitangent = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*bitangents);
#endif

			ofs_st = dataSize;
			dataSize += surf->numVerts * sizeof(*texcoords);

			data = ri.Z_Malloc(dataSize);

			verts =      (void *)(data + ofs_xyz);
			normals =    (void *)(data + ofs_normal);
#ifdef USE_VERT_TANGENT_SPACE
			tangents =   (void *)(data + ofs_tangent);
			bitangents = (void *)(data + ofs_bitangent);
#endif
			texcoords =  (void *)(data + ofs_st);
		
			v = surf->verts;
			for ( j = 0; j < surf->numVerts * mdvModel->numFrames ; j++, v++ )
			{
				VectorCopy(v->xyz,       verts[j]);
				VectorCopy(v->normal,    normals[j]);
#ifdef USE_VERT_TANGENT_SPACE
				VectorCopy(v->tangent,   tangents[j]);
				VectorCopy(v->bitangent, bitangents[j]);
#endif
			}

			st = surf->st;
			for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
				texcoords[j][0] = st->st[0];
				texcoords[j][1] = st->st[1];
			}

			vboSurf->surfaceType = SF_VBO_MDVMESH;
			vboSurf->mdvModel = mdvModel;
			vboSurf->mdvSurface = surf;
			vboSurf->numIndexes = surf->numTriangles * 3;
			vboSurf->numVerts = surf->numVerts;
			
			vboSurf->minIndex = 0;
			vboSurf->maxIndex = surf->numVerts;

			vboSurf->vbo = R_CreateVBO(va("staticMD3Mesh_VBO '%s'", surf->name), data, dataSize, VBO_USAGE_STATIC);

			vboSurf->vbo->ofs_xyz       = ofs_xyz;
			vboSurf->vbo->ofs_normal    = ofs_normal;
#ifdef USE_VERT_TANGENT_SPACE
			vboSurf->vbo->ofs_tangent   = ofs_tangent;
			vboSurf->vbo->ofs_bitangent = ofs_bitangent;
#endif
			vboSurf->vbo->ofs_st        = ofs_st;

			vboSurf->vbo->stride_xyz       = sizeof(*verts);
			vboSurf->vbo->stride_normal    = sizeof(*normals);
#ifdef USE_VERT_TANGENT_SPACE
			vboSurf->vbo->stride_tangent   = sizeof(*tangents);
			vboSurf->vbo->stride_bitangent = sizeof(*bitangents);
#endif
			vboSurf->vbo->stride_st        = sizeof(*st);

			vboSurf->vbo->size_xyz    = sizeof(*verts) * surf->numVerts;
			vboSurf->vbo->size_normal = sizeof(*normals) * surf->numVerts;

			ri.Free(data);

			vboSurf->ibo = R_CreateIBO2(va("staticMD3Mesh_IBO %s", surf->name), surf->numTriangles, surf->triangles, VBO_USAGE_STATIC);
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
	int             f, i, j, k;
 
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
	srfTriangle_t  *tri;
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

		if(md3Surf->numVerts > SHADER_MAX_VERTEXES)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has more than %i verts on a surface (%i)",
					 modName, SHADER_MAX_VERTEXES, md3Surf->numVerts);
		}
		if(md3Surf->numTriangles * 3 > SHADER_MAX_INDEXES)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has more than %i triangles on a surface (%i)",
					 modName, SHADER_MAX_INDEXES / 3, md3Surf->numTriangles);
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
		surf->numTriangles = md3Surf->numTriangles;
		surf->triangles = tri = ri.Hunk_Alloc(sizeof(*tri) * md3Surf->numTriangles, h_low);

		md3Tri = (md3Triangle_t *) ((byte *) md3Surf + md3Surf->ofsTriangles);
		for(j = 0; j < md3Surf->numTriangles; j++, tri++, md3Tri++)
		{
			tri->indexes[0] = LittleLong(md3Tri->indexes[0]);
			tri->indexes[1] = LittleLong(md3Tri->indexes[1]);
			tri->indexes[2] = LittleLong(md3Tri->indexes[2]);
		}

		R_CalcSurfaceTriangleNeighbors(surf->numTriangles, surf->triangles);

		// swap all the XyzNormals
		surf->numVerts = md3Surf->numVerts;
		surf->verts = v = ri.Hunk_Alloc(sizeof(*v) * (md3Surf->numVerts * md3Surf->numFrames), h_low);

		md3xyz = (md3XyzNormal_t *) ((byte *) md3Surf + md3Surf->ofsXyzNormals);
		for(j = 0; j < md3Surf->numVerts * md3Surf->numFrames; j++, md3xyz++, v++)
		{
			unsigned lat, lng;
			unsigned short normal;

			v->xyz[0] = LittleShort(md3xyz->xyz[0]) * MD3_XYZ_SCALE;
			v->xyz[1] = LittleShort(md3xyz->xyz[1]) * MD3_XYZ_SCALE;
			v->xyz[2] = LittleShort(md3xyz->xyz[2]) * MD3_XYZ_SCALE;

			normal = LittleShort(md3xyz->normal);

			lat = ( normal >> 8 ) & 0xff;
			lng = ( normal & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);



			v->normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			v->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			v->normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
		}

		// swap all the ST
		surf->st = st = ri.Hunk_Alloc(sizeof(*st) * md3Surf->numVerts, h_low);

		md3st = (md3St_t *) ((byte *) md3Surf + md3Surf->ofsSt);
		for(j = 0; j < md3Surf->numVerts; j++, md3st++, st++)
		{
			st->st[0] = LittleFloat(md3st->st[0]);
			st->st[1] = LittleFloat(md3st->st[1]);
		}

#ifdef USE_VERT_TANGENT_SPACE
		// calc tangent spaces
		{
			// Valgrind complaints: Conditional jump or move depends on uninitialised value(s)
			// So lets Initialize them.
			const float    *v0 = NULL, *v1 = NULL, *v2 = NULL;
			const float    *t0 = NULL, *t1 = NULL, *t2 = NULL;
			vec3_t          tangent = { 0, 0, 0 };
			vec3_t          bitangent = { 0, 0, 0 };
			vec3_t          normal = { 0, 0, 0 };

			for(j = 0, v = surf->verts; j < (surf->numVerts * mdvModel->numFrames); j++, v++)
			{
				VectorClear(v->tangent);
				VectorClear(v->bitangent);
				if (r_recalcMD3Normals->integer)
				VectorClear(v->normal);
			}

			for(f = 0; f < mdvModel->numFrames; f++)
			{
				for(j = 0, tri = surf->triangles; j < surf->numTriangles; j++, tri++)
				{
					v0 = surf->verts[surf->numVerts * f + tri->indexes[0]].xyz;
					v1 = surf->verts[surf->numVerts * f + tri->indexes[1]].xyz;
					v2 = surf->verts[surf->numVerts * f + tri->indexes[2]].xyz;

					t0 = surf->st[tri->indexes[0]].st;
					t1 = surf->st[tri->indexes[1]].st;
					t2 = surf->st[tri->indexes[2]].st;

					if (!r_recalcMD3Normals->integer)
						VectorCopy(v->normal, normal);
					else
						VectorClear(normal);

					#if 1
					R_CalcTangentSpace(tangent, bitangent, normal, v0, v1, v2, t0, t1, t2);
					#else
					R_CalcNormalForTriangle(normal, v0, v1, v2);
					R_CalcTangentsForTriangle(tangent, bitangent, v0, v1, v2, t0, t1, t2);
					#endif

					for(k = 0; k < 3; k++)
					{
						float          *v;

						v = surf->verts[surf->numVerts * f + tri->indexes[k]].tangent;
						VectorAdd(v, tangent, v);

						v = surf->verts[surf->numVerts * f + tri->indexes[k]].bitangent;
						VectorAdd(v, bitangent, v);

						if (r_recalcMD3Normals->integer)
						{
							v = surf->verts[surf->numVerts * f + tri->indexes[k]].normal;
							VectorAdd(v, normal, v);
						}
					}
				}
			}

			for(j = 0, v = surf->verts; j < (surf->numVerts * mdvModel->numFrames); j++, v++)
			{
				VectorNormalize(v->tangent);
				VectorNormalize(v->bitangent);
				VectorNormalize(v->normal);
			}
		}
#endif

		// find the next surface
		md3Surf = (md3Surface_t *) ((byte *) md3Surf + md3Surf->ofsEnd);
		surf++;
	}


	{
		srfVBOMDVMesh_t *vboSurf;

		mdvModel->numVBOSurfaces = mdvModel->numSurfaces;
		mdvModel->vboSurfaces = ri.Hunk_Alloc(sizeof(*mdvModel->vboSurfaces) * mdvModel->numSurfaces, h_low);

		vboSurf = mdvModel->vboSurfaces;
		surf = mdvModel->surfaces;
		for (i = 0; i < mdvModel->numSurfaces; i++, vboSurf++, surf++)
		{
			vec3_t *verts;
			vec3_t *normals;
			vec2_t *texcoords;
#ifdef USE_VERT_TANGENT_SPACE
			vec3_t *tangents;
			vec3_t *bitangents;
#endif

			byte *data;
			int dataSize;

			int ofs_xyz, ofs_normal, ofs_st;
#ifdef USE_VERT_TANGENT_SPACE
			int ofs_tangent, ofs_bitangent;
#endif

			dataSize = 0;

			ofs_xyz = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*verts);

			ofs_normal = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*normals);

#ifdef USE_VERT_TANGENT_SPACE
			ofs_tangent = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*tangents);

			ofs_bitangent = dataSize;
			dataSize += surf->numVerts * mdvModel->numFrames * sizeof(*bitangents);
#endif

			ofs_st = dataSize;
			dataSize += surf->numVerts * sizeof(*texcoords);

			data = ri.Z_Malloc(dataSize);

			verts =      (void *)(data + ofs_xyz);
			normals =    (void *)(data + ofs_normal);
#ifdef USE_VERT_TANGENT_SPACE
			tangents =   (void *)(data + ofs_tangent);
			bitangents = (void *)(data + ofs_bitangent);
#endif
			texcoords =  (void *)(data + ofs_st);
		
			v = surf->verts;
			for ( j = 0; j < surf->numVerts * mdvModel->numFrames ; j++, v++ )
			{
				VectorCopy(v->xyz,       verts[j]);
				VectorCopy(v->normal,    normals[j]);
#ifdef USE_VERT_TANGENT_SPACE
				VectorCopy(v->tangent,   tangents[j]);
				VectorCopy(v->bitangent, bitangents[j]);
#endif
			}

			st = surf->st;
			for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
				texcoords[j][0] = st->st[0];
				texcoords[j][1] = st->st[1];
			}

			vboSurf->surfaceType = SF_VBO_MDVMESH;
			vboSurf->mdvModel = mdvModel;
			vboSurf->mdvSurface = surf;
			vboSurf->numIndexes = surf->numTriangles * 3;
			vboSurf->numVerts = surf->numVerts;
			
			vboSurf->minIndex = 0;
			vboSurf->maxIndex = surf->numVerts;

			vboSurf->vbo = R_CreateVBO(va("staticMD3Mesh_VBO '%s'", surf->name), data, dataSize, VBO_USAGE_STATIC);

			vboSurf->vbo->ofs_xyz       = ofs_xyz;
			vboSurf->vbo->ofs_normal    = ofs_normal;
#ifdef USE_VERT_TANGENT_SPACE
			vboSurf->vbo->ofs_tangent   = ofs_tangent;
			vboSurf->vbo->ofs_bitangent = ofs_bitangent;
#endif
			vboSurf->vbo->ofs_st        = ofs_st;

			vboSurf->vbo->stride_xyz       = sizeof(*verts);
			vboSurf->vbo->stride_normal    = sizeof(*normals);
#ifdef USE_VERT_TANGENT_SPACE
			vboSurf->vbo->stride_tangent   = sizeof(*tangents);
			vboSurf->vbo->stride_bitangent = sizeof(*bitangents);
#endif
			vboSurf->vbo->stride_st        = sizeof(*st);

			vboSurf->vbo->size_xyz    = sizeof(*verts) * surf->numVerts;
			vboSurf->vbo->size_normal = sizeof(*normals) * surf->numVerts;

			ri.Free(data);

			vboSurf->ibo = R_CreateIBO2(va("staticMD3Mesh_IBO %s", surf->name), surf->numTriangles, surf->triangles, VBO_USAGE_STATIC);
		}
	}

	return qtrue;
}

#ifdef RAVENMD4

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
	// over and over again, we'll uncompress it in this function already, so we must adjust the size of the target md4.
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
			if ( surf->numVerts > SHADER_MAX_VERTEXES ) 
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has more than %i verts on a surface (%i).\n",
					  mod_name, SHADER_MAX_VERTEXES, surf->numVerts );
				return qfalse;
			}
			if ( surf->numTriangles*3 > SHADER_MAX_INDEXES ) 
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has more than %i triangles on a surface (%i).\n",
					  mod_name, SHADER_MAX_INDEXES / 3, surf->numTriangles );
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
#endif

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

	Com_Memcpy( mds, buffer, LittleLong(pinmodel->ofsEnd) );

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

	// swap all the surfaces
	surf = ( mdsSurface_t * )( (byte *)mds + mds->ofsSurfaces );
	for ( i = 0 ; i < mds->numSurfaces ; i++ ) {
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

		if ( surf->numVerts > SHADER_MAX_VERTEXES ) {
			ri.Printf(PRINT_WARNING, "R_LoadMDS: %s has more than %i verts on a surface (%i).\n",
					  mod_name, SHADER_MAX_VERTEXES, surf->numVerts );
			return qfalse;
		}
		if ( surf->numTriangles * 3 > SHADER_MAX_INDEXES ) {
			ri.Printf(PRINT_WARNING, "R_LoadMDS: %s has more than %i triangles on a surface (%i).\n",
					  mod_name, SHADER_MAX_INDEXES / 3, surf->numTriangles );
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

			// find the fixedParent for this vert (if exists)
			v->fixedParent = -1;
			if ( v->numWeights == 2 ) {
				// find the closest parent
				if ( VectorLength( v->weights[0].offset ) < VectorLength( v->weights[1].offset ) ) {
					v->fixedParent = 0;
				} else {
					v->fixedParent = 1;
				}
				v->fixedDist = VectorLength( v->weights[v->fixedParent].offset );
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
	ri.Hunk_Clear();    // (SA) MEM NOTE: not in missionpack

	R_Init();
	*glconfigOut = glConfig;

	R_IssuePendingRenderCommands();

	tr.visIndex = 0;
	memset(tr.visClusters, -2, sizeof(tr.visClusters));	// force markleafs to regenerate

	R_ClearFlares();
	RE_ClearScene();

	tr.registered = qtrue;

	// NOTE: this sucks, for some reason the first stretch pic is never drawn
	// without this we'd see a white flash on a level load because the very
	// first time the level shot would not be drawn
//	RE_StretchPic( 0, 0, 0, 0, 0, 0, 1, 1, 0 );
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

	// Ridah, load in the cacheModels
	R_LoadCacheModels();
	// done.
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

#ifdef RAVENMD4
void R_GetAnimTag( mdrHeader_t *mod, int framenum, const char *tagName, mdvTag_t * dest) 
{
	int				i, j, k;
	int				frameSize;
	mdrFrame_t		*frame;
	mdrTag_t		*tag;

	if ( framenum >= mod->numFrames ) 
	{
		// it is possible to have a bad frame while changing models, so don't error
		framenum = mod->numFrames - 1;
	}

	tag = (mdrTag_t *)((byte *)mod + mod->ofsTags);
	for ( i = 0 ; i < mod->numTags ; i++, tag++ )
	{
		if ( !strcmp( tag->name, tagName ) )
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

			return;
		}
	}

	AxisClear( dest->axis );
	VectorClear( dest->origin );
}
#endif

/*
================
R_LerpTag

  returns the index of the tag it found, for cycling through tags with the same name
================
*/
int R_LerpTag( orientation_t *tag, const refEntity_t *refent, const char *tagNameIn, int startIndex ) {
	mdvTag_t    *start, *end;
#ifdef RAVENMD4
	mdvTag_t	start_space, end_space;
#endif
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
#ifdef RAVENMD4
		if(model->type == MOD_MDR)
		{
			start = &start_space;
			end = &end_space;
			R_GetAnimTag((mdrHeader_t *) model->modelData, startFrame, tagName, start);
			R_GetAnimTag((mdrHeader_t *) model->modelData, endFrame, tagName, end);
		}
		else
#endif
		if( model->type == MOD_IQM ) {
			return R_IQMLerpTag( tag, model->modelData,
					startFrame, endFrame,
					frac, tagName );
		} else {
			AxisClear( tag->axis );
			VectorClear( tag->origin );
			return -1;
		}
	}

	frontLerp = frac;
	backLerp = 1.0f - frac;

	if(model->type == MOD_MESH)
	{
		// old MD3 style
		retval = R_GetTag(model->mdv[0], startFrame, tagName, startIndex, &start);
		retval = R_GetTag(model->mdv[0], endFrame, tagName, startIndex, &end);
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
#ifdef RAVENMD4
	} else if (model->type == MOD_MDR) {
		mdrHeader_t	*header;
		mdrFrame_t	*frame;

		header = (mdrHeader_t *)model->modelData;
		frame = (mdrFrame_t *) ((byte *)header + header->ofsFrames);

		VectorCopy( frame->bounds[0], mins );
		VectorCopy( frame->bounds[1], maxs );
		
		return;
#endif
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

//---------------------------------------------------------------------------
// Virtual Memory, used for model caching, since we can't allocate them
// in the main Hunk (since it gets cleared on level changes), and they're
// too large to go into the Zone, we have a special memory chunk just for
// caching models in between levels.
//
// Optimized for Win32 systems, so that they'll grow the swapfile at startup
// if needed, but won't actually commit it until it's needed.
//
// GOAL: reserve a big chunk of virtual memory for the media cache, and only
// use it when we actually need it. This will make sure the swap file grows
// at startup if needed, rather than each allocation we make.
byte    *membase = NULL;
int hunkmaxsize;
int cursize;

#define R_HUNK_MEGS     24
#define R_HUNK_SIZE     ( R_HUNK_MEGS*1024*1024 )

void *R_Hunk_Begin( void ) {
	int maxsize = R_HUNK_SIZE;

	//Com_Printf("R_Hunk_Begin\n");

	// reserve a huge chunk of memory, but don't commit any yet
	cursize = 0;
	hunkmaxsize = maxsize;

#ifdef _WIN32

	// this will "reserve" a chunk of memory for use by this application
	// it will not be "committed" just yet, but the swap file will grow
	// now if needed
	membase = VirtualAlloc( NULL, maxsize, MEM_RESERVE, PAGE_NOACCESS );

#else

	// show_bug.cgi?id=440
	// if not win32, then just allocate it now
	// it is possible that we have been allocated already, in case we don't do anything
	if ( !membase ) {
		membase = malloc( maxsize );
		// TTimo NOTE: initially, I was doing the memset even if we had an existing membase
		// but this breaks some shaders (i.e. /map mp_beach, then go back to the main menu .. some shaders are missing)
		// I assume the shader missing is because we don't clear memory either on win32
		// meaning even on win32 we are using memory that is still reserved but was uncommited .. it works out of pure luck
		memset( membase, 0, maxsize );
	}

#endif

	if ( !membase ) {
		ri.Error( ERR_DROP, "R_Hunk_Begin: reserve failed" );
	}

	return (void *)membase;
}

void *R_Hunk_Alloc( int size ) {
#ifdef _WIN32
	void    *buf;
#endif

	//Com_Printf("R_Hunk_Alloc(%d)\n", size);

	// round to cacheline
	size = ( size + 31 ) & ~31;

#ifdef _WIN32

	// commit pages as needed
	buf = VirtualAlloc( membase, cursize + size, MEM_COMMIT, PAGE_READWRITE );

	if ( !buf ) {
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPTSTR) &buf, 0, NULL );
		ri.Error( ERR_DROP, "VirtualAlloc commit failed.\n" );
	}

#endif

	cursize += size;
	if ( cursize > hunkmaxsize ) {
		ri.Error( ERR_DROP, "R_Hunk_Alloc overflow" );
	}

	return ( void * )( membase + cursize - size );
}

// this is only called when we shutdown GL
void R_Hunk_End( void ) {
	//Com_Printf("R_Hunk_End\n");

	if ( membase ) {
#ifdef _WIN32
		VirtualFree( membase, 0, MEM_RELEASE );
#else
		free( membase );
#endif
	}

	membase = NULL;
}

void R_Hunk_Reset( void ) {
	//Com_Printf("R_Hunk_Reset\n");

	if ( !membase ) {
		ri.Error( ERR_DROP, "R_Hunk_Reset called without a membase!" );
	}

#ifdef _WIN32
	// mark the existing committed pages as reserved, but not committed
	VirtualFree( membase, cursize, MEM_DECOMMIT );
#endif
	// on non win32 OS, we keep the allocated chunk as is, just start again to curzise = 0

	// start again at the top
	cursize = 0;
}

//=============================================================================
// Ridah, model caching

// TODO: convert the Hunk_Alloc's in the model loading to malloc's, so we don't have
// to move so much memory around during transitions

static int numBackupModels = 0;

/*
===============
R_CacheModelAlloc
===============
*/
void *R_CacheModelAlloc( int size ) {
	if ( r_cache->integer && r_cacheModels->integer ) {
		return R_Hunk_Alloc( size );
	} else {
		return ri.Hunk_Alloc( size, h_low );
	}
}

/*
===============
R_CacheModelFree
===============
*/
void R_CacheModelFree( void *ptr ) {
	if ( r_cache->integer && r_cacheModels->integer ) {
		// TTimo: it's in the hunk, leave it there, next R_Hunk_Begin will clear it all
	} else
	{
		// if r_cache 0, this is never supposed to get called anyway
		Com_Printf( "FIXME: unexpected R_CacheModelFree call (r_cache 0)\n" );
	}
}

/*
===============
R_PurgeModels
===============
*/
void R_PurgeModels( int count ) {
	static int lastPurged = 0;

	if ( !numBackupModels ) {
		return;
	}

	lastPurged = 0;
	numBackupModels = 0;

	// note: we can only do this since we only use the virtual memory for the model caching!
	R_Hunk_Reset();
}

/*
===============
R_BackupModels
===============
*/
void R_BackupModels( void ) {
	return;
}

/*
===============
R_FindCachedModel

  look for the given model in the list of backupModels
===============
*/
qboolean R_FindCachedModel( const char *name, model_t *newmod ) {
	return qfalse;
}

/*
===============
R_LoadCacheModels
===============
*/
void R_LoadCacheModels( void ) {
	int len;
	byte *buf;
	char    *token, *pString;
	char name[MAX_QPATH];

	if ( !r_cacheModels->integer ) {
		return;
	}

	// don't load the cache list in between level loads, only on startup, or after a vid_restart
	if ( numBackupModels > 0 ) {
		return;
	}

	len = ri.FS_ReadFile( "model.cache", NULL );

	if ( len <= 0 ) {
		return;
	}

	buf = (byte *)ri.Hunk_AllocateTempMemory( len );
	ri.FS_ReadFile( "model.cache", (void **)&buf );
	pString = (char *)buf;

	while ( ( token = COM_ParseExt( &pString, qtrue ) ) && token[0] ) {
		Q_strncpyz( name, token, sizeof( name ) );
		RE_RegisterModel( name );
	}

	ri.Hunk_FreeTempMemory( buf );
}
// done.
//========================================================================
