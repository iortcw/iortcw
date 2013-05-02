/*
   sv_wallhack.c -- functions to prevent wallhack cheats

   Copyright (C) 2012 Laszlo Menczel

   This is free software distributed under the terms of the GNU
   General Public License version 2. NO WARRANTY, see 'LICENSE.TXT'.
*/

#include "server.h"

//======================================================================

static vec3_t pred_ppos, pred_opos;

static trajectory_t traject;

static vec3_t old_origin[MAX_CLIENTS];

static int origin_changed[MAX_CLIENTS];

static float delta_sign[8][3] =
{
	{  1,  1,  1  },
	{  1,  1,  1  },
	{  1,  1, -1  },
	{  1,  1, -1  },
	{ -1,  1,  1  },
	{  1, -1,  1  },
	{ -1,  1, -1  },
	{  1, -1, -1  }
};

static vec3_t delta[8];

static int bbox_horz;
static int bbox_vert;

//======================================================================
// local functions
//======================================================================

#define POS_LIM     1.0
#define NEG_LIM    -1.0

static int zero_vector(vec3_t v)
{
	if (v[0] > POS_LIM || v[0] < NEG_LIM)
		return 0;

	if (v[1] > POS_LIM || v[1] < NEG_LIM)
		return 0;

	if (v[2] > POS_LIM || v[2] < NEG_LIM)
		return 0;

	return 1;
}

//======================================================================
/*
   The following functions for predicting player positions
   have been adopted from 'g_unlagged.c' which is part of the
   'unlagged' system created by Neil "haste" Toronto.
   WEB site: http://www.ra.is/unlagged
*/

#define OVERCLIP                1.001f

static void predict_clip_velocity(vec3_t in, vec3_t normal, vec3_t out)
{
	float backoff;

	// find the magnitude of the vector "in" along "normal"
	backoff = DotProduct(in, normal);

	// tilt the plane a bit to avoid floating-point error issues
	if (backoff < 0)
		backoff *= OVERCLIP;
	else
		backoff /= OVERCLIP;

	// slide along
	VectorMA(in, -backoff, normal, out);
}

//======================================================================

#define MAX_CLIP_PLANES   5
#define Z_ADJUST          1

static int predict_slide_move(sharedEntity_t * ent, float frametime, trajectory_t * tr, vec3_t result)
{
	int count, numbumps, numplanes, i, j, k;

	float d, time_left, into;

	vec3_t planes[MAX_CLIP_PLANES],
	       velocity, origin, clipVelocity, endVelocity, endClipVelocity, dir, end;

	trace_t trace;

	numbumps = 4;

	VectorCopy(tr->trBase, origin);
	origin[2] += Z_ADJUST;  // move it off the floor

	VectorCopy(tr->trDelta, velocity);
	VectorCopy(tr->trDelta, endVelocity);

	time_left = frametime;

	numplanes = 0;

	for (count = 0; count < numbumps; count++)
	{
		// calculate position we are trying to move to
		VectorMA(origin, time_left, velocity, end);

		// see if we can make it there
		SV_Trace(&trace, origin, ent->r.mins, ent->r.maxs, end, ent->s.number,
		         CONTENTS_SOLID, qfalse);

		if (trace.allsolid)
		{
			// entity is completely trapped in another solid
			VectorCopy(origin, result);
			return 0;
		}

		if (trace.fraction > 0.99) // moved the entire distance
		{
			VectorCopy(trace.endpos, result);
			return 1;
		}

		if (trace.fraction > 0) // covered some distance
			VectorCopy(trace.endpos, origin);

		time_left -= time_left * trace.fraction;

		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			VectorCopy(origin, result);
			return 0;
		}

		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		for (i = 0; i < numplanes; i++)
			if (DotProduct(trace.plane.normal, planes[i]) > 0.99)
			{
				VectorAdd(trace.plane.normal, velocity, velocity);
				break;
			}

		if (i < numplanes)
			continue;

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify velocity so it parallels all of the clip planes
		// find a plane that it enters
		for (i = 0; i < numplanes; i++)
		{
			into = DotProduct(velocity, planes[i]);
			if (into >= 0.1) // move doesn't interact with the plane
				continue;

			// slide along the plane
			predict_clip_velocity(velocity, planes[i], clipVelocity);

			// slide along the plane
			predict_clip_velocity(endVelocity, planes[i], endClipVelocity);

			// see if there is a second plane that the new move enters
			for (j = 0; j < numplanes; j++)
			{
				if (j == i)
					continue;

				if (DotProduct(clipVelocity, planes[j]) >= 0.1) // move doesn't interact with the plane
					continue;

				// try clipping the move to the plane
				predict_clip_velocity(clipVelocity, planes[j], clipVelocity);
				predict_clip_velocity(endClipVelocity, planes[j], endClipVelocity);

				// see if it goes back into the first clip plane
				if (DotProduct(clipVelocity, planes[i]) >= 0)
					continue;

				// slide the original velocity along the crease
				CrossProduct(planes[i], planes[j], dir);
				VectorNormalize(dir);
				d = DotProduct(dir, velocity);
				VectorScale(dir, d, clipVelocity);

				CrossProduct(planes[i], planes[j], dir);
				VectorNormalize(dir);
				d = DotProduct(dir, endVelocity);
				VectorScale(dir, d, endClipVelocity);

				// see if there is a third plane the new move enters
				for (k = 0; k < numplanes; k++)
				{
					if (k == i || k == j)
						continue;

					if (DotProduct(clipVelocity, planes[k]) >= 0.1) // move doesn't interact with the plane
						continue;

					// stop dead at a tripple plane interaction
					VectorCopy(origin, result);
					return 1;
				}
			}

			// if we have fixed all interactions, try another move
			VectorCopy(clipVelocity, velocity);
			VectorCopy(endClipVelocity, endVelocity);
			break;
		}
	}

	VectorCopy(origin, result);

	if (count == 0)
		return 1;

	return 0;
}

//======================================================================

#define STEPSIZE 18

// 'frametime' is interpreted as seconds

static void predict_move(sharedEntity_t * ent, float frametime, trajectory_t * tr, vec3_t result)
{
	float stepSize;

	vec3_t start_o, start_v, down, up;

	trace_t trace;

	VectorCopy(tr->trBase, result); // assume the move fails

	if (zero_vector(tr->trDelta)) // not moving
		return;

	if (predict_slide_move(ent, frametime, tr, result)) // move completed
		return;

	VectorCopy(tr->trBase, start_o);
	VectorCopy(tr->trDelta, start_v);

	VectorCopy(start_o, up);
	up[2] += STEPSIZE;

	// test the player position if they were a stepheight higher
	SV_Trace(&trace, start_o, ent->r.mins, ent->r.maxs, up, ent->s.number,
	         CONTENTS_SOLID, qfalse);

	if (trace.allsolid)     // can't step up
		return;

	stepSize = trace.endpos[2] - start_o[2];

	// try slidemove from this position
	VectorCopy(trace.endpos, tr->trBase);
	VectorCopy(start_v, tr->trDelta);

	predict_slide_move(ent, frametime, tr, result);

	// push down the final amount
	VectorCopy(tr->trBase, down);
	down[2] -= stepSize;
	SV_Trace(&trace, tr->trBase, ent->r.mins, ent->r.maxs, down, ent->s.number,
	         CONTENTS_SOLID, qfalse);

	if (!trace.allsolid)
		VectorCopy(trace.endpos, result);
}

//======================================================================
/*
   Calculates the view point of a player model at position 'org' using
   information in the player state 'ps' of its client, and stores the
   viewpoint coordinates in 'vp'.
*/

static void calc_viewpoint(playerState_t * ps, vec3_t org, vec3_t vp)
{
	VectorCopy(org, vp);
	
	if ( ps->leanf != 0 ) {
		vec3_t right, v3ViewAngles;
		VectorCopy( ps->viewangles, v3ViewAngles );
		v3ViewAngles[2] += ps->leanf / 2.0f;
		AngleVectors( v3ViewAngles, NULL, right, NULL );
		VectorMA( org, ps->leanf, right, org );
	}

	if (ps->pm_flags & PMF_DUCKED)
		vp[2] += CROUCH_VIEWHEIGHT;
	else
		vp[2] += DEFAULT_VIEWHEIGHT;
}

//======================================================================

#define MAX_PITCH    20

static int player_in_fov(vec3_t viewangle, vec3_t ppos, vec3_t opos)
{
	float yaw, pitch, cos_angle;
	vec3_t dir, los;
	int vofs;

	VectorSubtract(opos, ppos, los);

	/*
	   Check if the two players are roughly on the same X/Y plane
	   and skip the test if not. We only want to eliminate info that
	   would reveal the position of opponents behind the player on
	   the same X/Y plane (e.g. on the same floor in a room).
	*/
	vofs = (int) (opos[2] - ppos[2]);
	if (VectorLength(los) < 5 * abs(vofs))
		return 1;

	// calculate unit vector of the direction the player looks at
	yaw = viewangle[YAW] * (M_PI * 2 / 360);
	pitch = viewangle[PITCH] * (M_PI * 2 / 360);
	dir[0] = cos(yaw) * cos(pitch);
	dir[1] = sin(yaw);
	dir[2] = cos(yaw) * sin(pitch);

	// calculate unit vector corresponding to line of sight to opponent
	VectorNormalize(los);

	// calculate and test the angle between the two vectors
	cos_angle = DotProduct(dir, los);
	if (cos_angle > 0)      // +/- 90 degrees (fov = 180)
		return 1;

	return 0;
}

//======================================================================

static void copy_trajectory(trajectory_t *src, trajectory_t *dst)
{
	dst->trType = src->trType;
	dst->trTime = src->trTime;
	dst->trDuration = src->trDuration;
	VectorCopy(src->trBase, dst->trBase);
	VectorCopy(src->trDelta, dst->trDelta);
}

//======================================================================

static int is_visible(vec3_t start, vec3_t end)
{
	trace_t trace;

	CM_BoxTrace(&trace, start, end, NULL, NULL, 0, CONTENTS_SOLID, 0);

	if (trace.contents & CONTENTS_SOLID)
		return 0;

	return 1;
}

//======================================================================

static void init_horz_delta(void)
{
	int i;

	bbox_horz = wh_bbox_horz->integer;

	for (i = 0; i < 8; i++)
	{
		delta[i][0] = ((float) bbox_horz * delta_sign[i][0]) / 2.0;
		delta[i][1] = ((float) bbox_horz * delta_sign[i][1]) / 2.0;
	}
}

//======================================================================

static void init_vert_delta(void)
{
	int i;

	bbox_vert = wh_bbox_vert->integer;

	for (i = 0; i < 8; i++)
		delta[i][2] = ((float) bbox_vert * delta_sign[i][2]) / 2.0;
}

//======================================================================
// public functions
//======================================================================

void SV_InitWallhack(void)
{
	init_horz_delta();
	init_vert_delta();
}

//======================================================================
/*
   'SV_CanSee' checks if 'player' can see 'other' or not. First
   a check is made if 'other' is in the maximum allowed fov of
   'player'. If not, then zero is returned w/o any further checks.
   Next traces are carried out from the present viewpoint of 'player'
   to the corners of the bounding box of 'other'. If any of these
   traces are successful (i.e. nothing solid is between the start
   and end positions) then non-zero is returned.

   Otherwise the expected positions of the two players are calculated,
   by extrapolating their movements for PREDICT_TIME seconds and the above
   tests are carried out again. The result is reported by returning non-zero
   (expected to become visible) or zero (not expected to become visible
   in the next frame).
*/

#define PREDICT_TIME      0.1
#define VOFS              6

int SV_CanSee(int player, int other)
{
	sharedEntity_t *pent, *oent;
	playerState_t *ps;
	vec3_t viewpoint, tmp;
	int i;

	// check if bounding box has been changed
	if (wh_bbox_horz->integer != bbox_horz)
		init_horz_delta();

	if (wh_bbox_vert->integer != bbox_vert)
		init_vert_delta();

	ps = SV_GameClientNum(player);
	pent = SV_GentityNum(player);
	oent = SV_GentityNum(other);

	/* check if 'other' is in the maximum fov allowed */
	if (wh_check_fov->integer)
	{
		if (!player_in_fov(pent->s.apos.trBase, pent->s.pos.trBase, oent->s.pos.trBase))
			return 0;
	}

	/* check if visible in this frame */
	calc_viewpoint(ps, pent->s.pos.trBase, viewpoint);

	for (i = 0; i < 8; i++)
	{
		VectorCopy(oent->s.pos.trBase, tmp);
		tmp[0] += delta[i][0];
		tmp[1] += delta[i][1];
		tmp[2] += delta[i][2] + VOFS;

		if (is_visible(viewpoint, tmp))
			return 1;
	}

	/* predict player positions */
	copy_trajectory(&pent->s.pos, &traject);
	predict_move(pent, PREDICT_TIME, &traject, pred_ppos);

	copy_trajectory(&oent->s.pos, &traject);
	predict_move(oent, PREDICT_TIME, &traject, pred_opos);

	/*
	   Check again if 'other' is in the maximum fov allowed.
	   FIXME: We use the original viewangle that may have
	   changed during the move. This could introduce some
	   errors.
	*/
	if (wh_check_fov->integer)
	{
		if (!player_in_fov(pent->s.apos.trBase, pred_ppos, pred_opos))
			return 0;
	}

	/* check if expected to be visible in the next frame */
	calc_viewpoint(ps, pred_ppos, viewpoint);

	for (i = 0; i < 8; i++)
	{
		VectorCopy(pred_opos, tmp);
		tmp[0] += delta[i][0];
		tmp[1] += delta[i][1];
		tmp[2] += delta[i][2] + VOFS;

		if (is_visible(viewpoint, tmp))
			return 1;
	}

	return 0;
}

//======================================================================
/*
   Changes the position of client 'other' so that it is directly
   below 'player'. The distance is maintained so that sound scaling
   will work correctly.
*/

void SV_RandomizePos(int player, int other)
{
	sharedEntity_t *pent, *oent;
	vec3_t los;
	float dist;

	pent = SV_GentityNum(player);
	oent = SV_GentityNum(other);

	VectorCopy(oent->s.pos.trBase, old_origin[other]);
	origin_changed[other] = 1;

	// get distance (we need it for correct sound scaling)
	VectorSubtract(oent->s.pos.trBase, pent->s.pos.trBase, los);
	dist = VectorLength(los);

	// set the opponent's position directly below the player
	VectorCopy(pent->s.pos.trBase, oent->s.pos.trBase);
	oent->s.pos.trBase[2] -= dist;
}

//======================================================================

void SV_RestorePos(int cli)
{
	sharedEntity_t *ent;

	ent = SV_GentityNum(cli);
	VectorCopy(old_origin[cli], ent->s.pos.trBase);
	origin_changed[cli] = 0;
}

//======================================================================

int SV_PositionChanged(int cli)
{
	return origin_changed[cli];
}

