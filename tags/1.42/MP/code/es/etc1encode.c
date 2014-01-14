#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <assert.h>
#include "GLES/gl.h"
#include "GLES/glext.h"

#if !defined(max)
#define max(a, b) ((a) < (b) ? (b) : (a))
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif

#define RED_TO_Y     0.299
#define GREEN_TO_Y   0.587
#define BLUE_TO_Y    0.114

#define GREY_TO_Y    (RED_TO_Y + GREEN_TO_Y + BLUE_TO_Y)

#define RED_VALUE    0.299
#define GREEN_VALUE  0.587
#define BLUE_VALUE   0.114

#define SEARCH_RANGE 0
#define Y_SEARCH_RANGE 0
#define FULL_Y_SEARCH 0
#define COMPARE_WITH_CLIPPING 0

typedef struct
{
   float r, g, b, y;
} fcolour_t;

typedef struct
{
   int red, green, blue, table;
} base_t;

typedef struct
{
   union
   {
      struct
      {
         int red1, red2, green1, green2, blue1, blue2;
      } norm;
      struct
      {
         int red1, d_red, green1, d_green, blue1, d_blue;
      } diff;
   } colour;
   int table1, table2;
   short luma[16];
   int diffbit, flipbit;
} etc_block_t;

typedef struct
{
   fcolour_t colour[2][4];
   int red_av, green_av, blue_av;
} block_detail_t;


static fcolour_t adjtab[8][4];

static int dsize;


static int dropped, tested;
static long long ldropped, ltested;

typedef struct
{
   int r, g, b, grey;
} rgbg_t;

typedef struct
{
   rgbg_t base[2];
   int table[2];
   int flip, diff;
   int score;
} candidate_t;


#define R_WEIGHT 19
#define G_WEIGHT 37
#define B_WEIGHT 7
#define TOTAL_WEIGHT (R_WEIGHT + G_WEIGHT + B_WEIGHT)
#define MAX_LUMA (TOTAL_WEIGHT * 255)

static int bencn_diffable(rgbg_t const *c1, rgbg_t const *c2)
{
   int r1 = (c1->r * 31 + 128 * 8) / (255 * 8);
   int g1 = (c1->g * 31 + 128 * 8) / (255 * 8);
   int b1 = (c1->b * 31 + 128 * 8) / (255 * 8);
   int r2 = (c2->r * 31 + 128 * 8) / (255 * 8);
   int g2 = (c2->g * 31 + 128 * 8) / (255 * 8);
   int b2 = (c2->b * 31 + 128 * 8) / (255 * 8);

   return ((unsigned)((r2 - r1 + 4) | (g2 - g1 + 4) | (b2 - b1 + 4)) <= 7);
}


static void bencn_quantise(candidate_t *cand)
{
   int i;

   if (cand->diff == 0)
   {
      for (i = 0; i < 2; i++)
      {
         cand->base[i].r = (cand->base[i].r * 15 + 128 * 8) / (255 * 8);
         cand->base[i].g = (cand->base[i].g * 15 + 128 * 8) / (255 * 8);
         cand->base[i].b = (cand->base[i].b * 15 + 128 * 8) / (255 * 8);

         assert((unsigned)cand->base[i].r < 16);
         assert((unsigned)cand->base[i].g < 16);
         assert((unsigned)cand->base[i].b < 16);

         cand->base[i].r = cand->base[i].r << 4 | cand->base[i].r >> 0;
         cand->base[i].g = cand->base[i].g << 4 | cand->base[i].g >> 0;
         cand->base[i].b = cand->base[i].b << 4 | cand->base[i].b >> 0;
      }
   }
   else
   {
      for (i = 0; i < 2; i++)
      {
         cand->base[i].r = (cand->base[i].r * 31 + 128 * 8) / (255 * 8);
         cand->base[i].g = (cand->base[i].g * 31 + 128 * 8) / (255 * 8);
         cand->base[i].b = (cand->base[i].b * 31 + 128 * 8) / (255 * 8);

         assert((unsigned)cand->base[i].r < 32);
         assert((unsigned)cand->base[i].g < 32);
         assert((unsigned)cand->base[i].b < 32);

         cand->base[i].r = cand->base[i].r << 3 | cand->base[i].r >> 2;
         cand->base[i].g = cand->base[i].g << 3 | cand->base[i].g >> 2;
         cand->base[i].b = cand->base[i].b << 3 | cand->base[i].b >> 2;
      }
   }

   for (i = 0; i < 2; i++)
   {
      assert((unsigned)cand->base[i].r < 256);
      assert((unsigned)cand->base[i].g < 256);
      assert((unsigned)cand->base[i].b < 256);
      cand->base[i].grey = R_WEIGHT * cand->base[i].r + G_WEIGHT * cand->base[i].g + B_WEIGHT * cand->base[i].b;
   }
}

static int bencn_score(int greymap[2][8], int tabs[2])
{
   static const int table[8][2] = { { 2, 8 }, { 5, 17 }, { 9, 29 }, { 13, 42 }, { 18, 60 }, { 24, 80 }, { 33, 106 }, { 47, 183 } };
   int total = 0;
   int i, j;

   for (i = 0; i < 2; i++)
   {
      int bestfit = -1;
      int bestscore = 1 << 30;

      for (j = 0; j < 8; j++)
      {
         int little = table[j][0] * TOTAL_WEIGHT, big = table[j][1] * TOTAL_WEIGHT;
         int score = 0;
         int k;

         for (k = 0; k < 8; k++)
         {
            int e1, e2, emin;

            e1 = abs(greymap[i][k] - big);
            e2 = abs(greymap[i][k] - little);
            emin = min(e1, e2);
            score += emin * emin >> 2;
            if (score >= bestscore)
               break;
         }

         if (score < bestscore)
         {
            bestfit = j;
            bestscore = score;
         }
      }

      assert(bestfit != -1);
      tabs[i] = bestfit;
      total += bestscore;
   }

   return total;
}

static int bencn_vertical_score(int greymap[4][4], int tabs[2], int left_luma, int right_luma)
{
   int newmap[2][8];
   int i;

   for (i = 0; i < 8; i++)
   {
      newmap[0][i] = abs(greymap[i >> 1][(i & 1) + 0] - left_luma);
      newmap[1][i] = abs(greymap[i >> 1][(i & 1) + 2] - right_luma);
   }

   return bencn_score(newmap, tabs);
}

static int bencn_horizontal_score(int greymap[4][4], int tabs[2], int top_luma, int bottom_luma)
{
   int newmap[2][8];
   int i;

   for (i = 0; i < 8; i++)
   {
      newmap[0][i] = abs(greymap[(i & 1) + 0][i >> 1] - top_luma);
      newmap[1][i] = abs(greymap[(i & 1) + 2][i >> 1] - bottom_luma);
   }

   return bencn_score(newmap, tabs);
}


static unsigned long long block_encode_new(unsigned char src[][3], int pitch)
{
   unsigned long low, high;
   candidate_t horiz, vert, *winner;
   int greymap[4][4];
   int x, y;

   {
      rgbg_t totals[2][2] = { { { 0 } } };

      for (y = 0; y < 4; y++)
         for (x = 0; x < 4; x++)
         {
            unsigned char (*ptr)[3] = src + x + (y ^ 3) * pitch;
            rgbg_t *tptr = &totals[y >> 1][x >> 1];
            int r = ptr[0][0], g = ptr[0][1], b = ptr[0][2];
            int grey = R_WEIGHT * r + G_WEIGHT * g + B_WEIGHT * b;

            greymap[y][x] = grey;

            tptr->r += r;
            tptr->g += g;
            tptr->b += b;
            tptr->grey += grey;
         }

      vert.flip = 0;
      for (x = 0; x < 2; x++)
      {
         vert.base[x].r    = totals[0][x].r    + totals[1][x].r;
         vert.base[x].g    = totals[0][x].g    + totals[1][x].g;
         vert.base[x].b    = totals[0][x].b    + totals[1][x].b;
         vert.base[x].grey = totals[0][x].grey + totals[1][x].grey;
      }
      vert.diff = bencn_diffable(&vert.base[0], &vert.base[1]);
      bencn_quantise(&vert);

      horiz.flip = 1;
      for (y = 0; y < 2; y++)
      {
         horiz.base[y].r    = totals[y][0].r    + totals[y][1].r;
         horiz.base[y].g    = totals[y][0].g    + totals[y][1].g;
         horiz.base[y].b    = totals[y][0].b    + totals[y][1].b;
         horiz.base[y].grey = totals[y][0].grey + totals[y][1].grey;
      }
      horiz.diff = bencn_diffable(&horiz.base[0], &horiz.base[1]);
      bencn_quantise(&horiz);

      vert.score = bencn_vertical_score(greymap, vert.table, vert.base[0].grey, vert.base[1].grey);
      horiz.score = bencn_horizontal_score(greymap, horiz.table, horiz.base[0].grey, horiz.base[1].grey);
   }

   winner = horiz.score < vert.score ? &horiz : &vert;

   low = 0;
   high = 0;

   {
      static const int threshold[8] = { 5, 11, 19, 22, 39, 52, 69, 115 };
      int base_grey[2][2];
      int quadthresh[2][2];

      if (winner->flip == 0)
         for (x = 0; x < 2; x++)
         {
            base_grey [0][x] = base_grey[1][x] = winner->base[x].grey;
            quadthresh[0][x] = quadthresh[1][x] = threshold[winner->table[x]] * TOTAL_WEIGHT;
         }
      else
         for (y = 0; y < 2; y++)
         {
            base_grey [y][0] = base_grey[y][1] = winner->base[y].grey;
            quadthresh[y][0] = quadthresh[y][1] = threshold[winner->table[y]] * TOTAL_WEIGHT;
         }

      for (y = 0; y < 4; y++)
         for (x = 0; x < 4; x++)
         {
            int v = greymap[y][x] - base_grey[y >> 1][x >> 1];
            int t = quadthresh[y >> 1][x >> 1];
            int shift = x * 4 + y;

            if (v < 0)
               low |= 0x10000 << shift;
            if (abs(v) > t)
               low |= 0x1 << shift;
         }
   }

   assert((unsigned)(winner->base[0].r | winner->base[0].g | winner->base[0].b) < 256);
   assert((unsigned)(winner->base[1].r | winner->base[1].g | winner->base[1].b) < 256);

   if (winner->diff)
   {
      int dr, dg, db;
      dr = (winner->base[1].r >> 3) - (winner->base[0].r >> 3);
      dg = (winner->base[1].g >> 3) - (winner->base[0].g >> 3);
      db = (winner->base[1].b >> 3) - (winner->base[0].b >> 3);

      assert((unsigned)(dr + 4) < 8);
      assert((unsigned)(dg + 4) < 8);
      assert((unsigned)(db + 4) < 8);

      high |= (winner->base[0].r >> 3) << 27;
      high |= (dr & 7)                 << 24;
      high |= (winner->base[0].g >> 3) << 19;
      high |= (dg & 7)                 << 16;
      high |= (winner->base[0].b >> 3) << 11;
      high |= (db & 7)                 << 8;
   }
   else
   {
      high |= (winner->base[0].r >> 4) << 28;
      high |= (winner->base[1].r >> 4) << 24;
      high |= (winner->base[0].g >> 4) << 20;
      high |= (winner->base[1].g >> 4) << 16;
      high |= (winner->base[0].b >> 4) << 12;
      high |= (winner->base[1].b >> 4) << 8;
   }

   assert((unsigned)winner->table[0] < 8);
   assert((unsigned)winner->table[1] < 8);

   high |= winner->table[0] << 5;
   high |= winner->table[1] << 2;

   high |= winner->diff << 1;
   high |= winner->flip << 0;

   return (unsigned long long)high << 32 | low;
}



static unsigned long long pack_block(etc_block_t const *block)
{
   unsigned long low, high;
   int x,y;

   low = 0;
      for (y = 0; y < 4; y++)
         for (x = 0; x < 4; x++)
         {
            int shift = x * 4 + y;
            int idx = x + y * 4;

            assert((unsigned)block->luma[x + y * 4] < 4);
            low |= (block->luma[idx]  & 1) << (shift + 0);
            low |= (block->luma[idx] >> 1) << (shift + 16);
         }

   if (block->diffbit)
   {
      assert((unsigned)block->colour.diff.red1 < 256);
      assert((unsigned)block->colour.diff.green1 < 256);
      assert((unsigned)block->colour.diff.blue1 < 256);
      assert((unsigned)(block->colour.diff.d_red + 32) < 64);
      assert((unsigned)(block->colour.diff.d_green + 32) < 64);
      assert((unsigned)(block->colour.diff.d_blue + 32) < 64);

      high  = (block->colour.diff.red1 >> 3)          << 27;
      high |= ((block->colour.diff.d_red >> 3) & 7)   << 24;
      high |= (block->colour.diff.green1 >> 3)        << 19;
      high |= ((block->colour.diff.d_green >> 3) & 7) << 16;
      high |= (block->colour.diff.blue1 >> 3)         << 11;
      high |= ((block->colour.diff.d_blue >> 3) & 7)  << 8;
   }
   else
   {
      assert((unsigned)block->colour.norm.red1 < 256);
      assert((unsigned)block->colour.norm.green1 < 256);
      assert((unsigned)block->colour.norm.blue1 < 256);
      assert((unsigned)block->colour.norm.red2 < 256);
      assert((unsigned)block->colour.norm.green2 < 256);
      assert((unsigned)block->colour.norm.blue2 < 256);
      high  = (block->colour.norm.red1 >> 4)   << 28;
      high |= (block->colour.norm.red2 >> 4)   << 24;
      high |= (block->colour.norm.green1 >> 4) << 20;
      high |= (block->colour.norm.green2 >> 4) << 16;
      high |= (block->colour.norm.blue1 >> 4)  << 12;
      high |= (block->colour.norm.blue2 >> 4)  << 8;
   }

   high |= block->flipbit << 0;
   high |= block->diffbit << 1;
   high |= block->table1  << 5;
   high |= block->table2  << 2;

   /* Note that this code doesn't bother about endian, and a PPM-like file
    * should be big endian rather than intel-native. */
   return (unsigned long long)high << 32 | low;
}



#if 0
#define sqdiff(a, b) (((a) - (b)) * ((a) - (b)))
#define pixdiff(x, z) (sqdiff((x).r, (z).r) + sqdiff((x).g, (z).g) + sqdiff((x).b, (z).b))
#define difftosum(x) (x)
#else
#define pixdiff(x, z) fabs((x).y - (z).y)
#define difftosum(x) ((x) * (x))
#endif

#if COMPARE_WITH_CLIPPING
static float inline evaluate(fcolour_t const colour[8], fcolour_t const *base, fcolour_t const table[4], float limit)
{
   int i, x;
   float error = 0.0;

   tested += 8;

   for (x = 0; x < 8; x++)
   {
      fcolour_t pixel;
      float delta, d2;

      pixel.r = base->r + table[0].r;
      pixel.g = base->g + table[0].g;
      pixel.b = base->b + table[0].b;
      pixel.y = base->y + table[0].y;
      if (pixel.r < 0.0) pixel.r = 0.0;
      else if (pixel.r > 255 * RED_VALUE) pixel.r = 255 * RED_VALUE;
      if (pixel.g < 0.0) pixel.g = 0.0;
      else if (pixel.g > 255 * GREEN_VALUE) pixel.g = 255 * GREEN_VALUE;
      if (pixel.b < 0.0) pixel.b = 0.0;
      else if (pixel.b > 255 * BLUE_VALUE) pixel.b = 255 * BLUE_VALUE;

      delta = pixdiff(pixel, colour[x]);
      for (i = 1; i < 4; i++)
      {
         pixel.r = base->r + table[i].r;
         pixel.g = base->g + table[i].g;
         pixel.b = base->b + table[i].b;
         pixel.y = base->y + table[i].y;
         if (pixel.r < 0.0) pixel.r = 0.0;
         else if (pixel.r > 255 * RED_VALUE) pixel.r = 255 * RED_VALUE;
         if (pixel.g < 0.0) pixel.g = 0.0;
         else if (pixel.g > 255 * GREEN_VALUE) pixel.g = 255 * GREEN_VALUE;
         if (pixel.b < 0.0) pixel.b = 0.0;
         else if (pixel.b > 255 * BLUE_VALUE) pixel.b = 255 * BLUE_VALUE;

         if ((d2 = pixdiff(pixel, colour[x])) < delta)
            delta = d2;
      }
      error += difftosum(delta);

      if (error >= limit)
      {
         dropped += 7 - x;
         return error;
      }
   }

   return error;
}
#else
static float __inline evaluate(fcolour_t const colour[8], fcolour_t const *base, fcolour_t const table[4], float limit)
{
   int i, x;
   float error = 0.0;

   /* TODO: consider the benefits of clipping */

   tested += 8;

   for (x = 0; x < 8; x++)
   {
      fcolour_t pixel;
      float delta, d2;

      pixel.r = colour[x].r - base->r;
      pixel.g = colour[x].g - base->g;
      pixel.b = colour[x].b - base->b;
      pixel.y = colour[x].y - base->y;

      delta = pixdiff(pixel, table[0]);
      for (i = 1; i < 4; i++)
         if ((d2 = pixdiff(pixel, table[i])) < delta)
            delta = d2;
      error += difftosum(delta);

      if (error >= limit)
      {
         dropped += 7 - x;
         return error;
      }
   }

   return error;
}
#endif

static void produce(short out[8], fcolour_t const colour[8], base_t const *base)
{
   int i, x;
   fcolour_t test;
   fcolour_t const *table;

   assert((unsigned)base->table < 8);
   assert((base->red & (~255 | 7)) == 0);
   assert((base->green & (~255 | 7)) == 0);
   assert((base->blue & (~255 | 7)) == 0);

   test.r = base->red * RED_VALUE;
   test.g = base->green * GREEN_VALUE;
   test.b = base->blue * BLUE_VALUE;
   test.y = base->red * RED_TO_Y + base->green * GREEN_TO_Y + base->blue * BLUE_TO_Y;
   table = adjtab[base->table];

   for (x = 0; x < 8; x++)
   {
      fcolour_t pixel;
      float delta, d2;

      pixel.r = colour[x].r - test.r;
      pixel.g = colour[x].g - test.g;
      pixel.b = colour[x].b - test.b;
      pixel.y = colour[x].y - test.y;

      delta = pixdiff(pixel, table[0]);
      out[x] = 0;
      for (i = 1; i < 4; i++)
         if ((d2 = pixdiff(pixel, table[i])) < delta)
         {
            delta = d2;
            out[x] = i;
         }
   }
}

static float __inline evaluate_all(int *index, fcolour_t const colour[8], fcolour_t const *base, float limit)
{
   int i;
   float error;

   for (i = 0; i < 8; i++)
   {
      if ((error = evaluate(colour, base, adjtab[i], limit)) < limit)
      {
         limit = error;
         *index = i;
      }
   }

   return limit;
}

static float search_yrb(base_t *base, float limit, fcolour_t const colour[8], int step, int ymin, int ymax, int rmin, int rmax, int bmin, int bmax)
{
   int ro, bo, y;

   rmin =  rmin & ~(step - 1);
   rmax = (rmax + step - 1) & ~(step - 1);
   bmin =  bmin & ~(step - 1);
   bmax = (bmax + step - 1) & ~(step - 1);
   ymin =  ymin & ~(step - 1);
   ymax = (ymax + step - 1) & ~(step - 1);

   for (ro = rmin; ro <= rmax; ro += step)
   {
      for (bo = bmin; bo <= bmax; bo += step)
      {
         for (y = ymin; y <= ymax; y += step)
         {
            int r = y + ro, g = y, b = y + bo;
            fcolour_t test;
            float error;

            if ((unsigned)r > 255 || (unsigned)g > 255 || (unsigned)b > 255)
               continue;

            test.r = r * RED_VALUE;
            test.g = g * GREEN_VALUE;
            test.b = b * BLUE_VALUE;
            test.y = r * RED_TO_Y + g * GREEN_TO_Y + b * BLUE_TO_Y;

            if ((error = evaluate_all(&base->table, colour, &test, limit)) < limit)
            {
               base->red = r;
               base->green = g;
               base->blue = b;
               limit = error;
            }
         }
      }
   }
   return limit;
}

static float full_y_search(base_t *base, block_detail_t const *detail, int step, float limit)
{
   int red_off, blue_off;

   red_off = detail->red_av - detail->green_av;
   blue_off = detail->blue_av - detail->green_av;

   /* initial guess to set a reasonable limit */
   limit = search_yrb(base, limit, &detail->colour[0][0], step,
               detail->green_av, detail->green_av,
               red_off, red_off,
               blue_off, blue_off);

#if FULL_Y_SEARCH
   limit = search_yrb(base, limit, &detail->colour[0][0], step,
               0, 255,
               red_off - SEARCH_RANGE * step, red_off + SEARCH_RANGE * step,
               blue_off - SEARCH_RANGE * step, blue_off + SEARCH_RANGE * step);
#else
   limit = search_yrb(base, limit, &detail->colour[0][0], step,
               detail->green_av - Y_SEARCH_RANGE * step, detail->green_av + Y_SEARCH_RANGE * step,
               red_off - SEARCH_RANGE * step, red_off + SEARCH_RANGE * step,
               blue_off - SEARCH_RANGE * step, blue_off + SEARCH_RANGE * step);
#endif

   return limit;
}

static float block_encode_abs(etc_block_t *block, block_detail_t const *detail, float limit)
{
   base_t base[2];
   float error1, error2;

   memset(block, 0, sizeof(*block));

   error1 = full_y_search(&base[0], &detail[0], 16, limit);
   error2 = full_y_search(&base[1], &detail[1], 16, limit);

   if (error1 + error2 >= limit)
      return error1 + error2;

   block->flipbit = 1;
   block->diffbit = 0;
   block->colour.norm.red1 = base[0].red &= 0xF0;
   block->colour.norm.red2 = base[1].red &= 0xF0;
   block->colour.norm.green1 = base[0].green &= 0xF0;
   block->colour.norm.green2 = base[1].green &= 0xF0;
   block->colour.norm.blue1 = base[0].blue &= 0xF0;
   block->colour.norm.blue2 = base[1].blue &= 0xF0;

   block->table1 = base[0].table;
   block->table2 = base[1].table;
   produce(&block->luma[0], &detail[0].colour[0][0], &base[0]);
   produce(&block->luma[8], &detail[1].colour[0][0], &base[1]);

   return error1 + error2;
}

static float block_encode(etc_block_t *block, const block_detail_t detail[2], float limit)
{
   base_t base[2];
   float error1, error2;

   memset(block, 0, sizeof(*block));

   error1 = full_y_search(&base[0], &detail[0], 8, limit);
   error2 = full_y_search(&base[1], &detail[1], 8, limit);

   if ((unsigned)((base[1].red - base[0].red + 32)
                | (base[1].green - base[0].green + 32)
                | (base[1].blue - base[0].blue + 32))
            >= 64)
   {
      int red_av, green_av, blue_av;
      int red_off, blue_off;

      /* TODO: think harder about this case.  This is just some random guess at
       * something that might come out reasonable, but it's certainly not a
       * comprehensive search of all the good places to look.  It's also
       * limited by the fact that it's using search_yrb() which has an
       * inappropriate shape at the ends, and so the search space in the second
       * call has unfortunate dead zones.
       */

      red_av = (detail[0].red_av + detail[1].red_av) >> 1;
      green_av = (detail[0].green_av + detail[1].green_av) >> 1;
      blue_av = (detail[0].blue_av + detail[1].blue_av) >> 1;

      red_off = red_av - green_av;
      blue_off = blue_av - green_av;

      error1 = search_yrb(&base[0], limit, &detail[0].colour[0][0], 8,
                  green_av - 3*8, green_av + 3*8,
                  red_off - 8, red_off + 8,
                  blue_off - 8, blue_off + 8);

      red_off = base[0].red - base[0].green;
      blue_off = base[0].blue - base[0].green;

      error2 = search_yrb(&base[1], limit, &detail[1].colour[0][0], 8,
                  base[0].green - 3*8, base[0].green + 2*8,
                  red_off - 8, red_off + 8,
                  blue_off - 8, blue_off + 8);

      /* Having made our best guess, we try it out against absolute encoding.. */

      if (error1 + error2 >= limit)
         return block_encode_abs(block, detail, limit);
      else
      {
         float error3;
      
         if ((error3 = block_encode_abs(block, detail, error1 + error2)) < error1 + error2)
            return error3;
      }
   }

   if (error1 + error2 >= limit)
      return error1 + error2; /* don't even bother with the answer */

   if ((unsigned)((base[1].red - base[0].red + 32)
                | (base[1].green - base[0].green + 32)
                | (base[1].blue - base[0].blue + 32))
            >= 64)
   {
      assert(!"Failed to find approximate delta.");
      return FLT_MAX;
   }

   block->flipbit = 1;
   block->diffbit = 1;
   block->table1 = base[0].table;
   block->table2 = base[1].table;
   block->colour.diff.red1 = base[0].red;
   block->colour.diff.d_red = base[1].red - base[0].red;
   block->colour.diff.green1 = base[0].green;
   block->colour.diff.d_green = base[1].green - base[0].green;
   block->colour.diff.blue1 = base[0].blue;
   block->colour.diff.d_blue = base[1].blue - base[0].blue;

   produce(&block->luma[0], &detail[0].colour[0][0], &base[0]);
   produce(&block->luma[8], &detail[1].colour[0][0], &base[1]);

   return error1 + error2;
}

static unsigned long long block_encode_search(etc_block_t *block, unsigned char src[][3], int pitch)
{
   etc_block_t tmp;
   block_detail_t detail[2];
   float tmp_score, block_score = FLT_MAX;
   int x, y;

   for (y = 0; y < 2; y++)
      detail[y].red_av = detail[y].green_av = detail[y].blue_av = 0;
   for (y = 3; y >= 0; y--)
   {
      for (x = 0; x < 4; x++)
      {
         int red = src[x][0], green = src[x][1], blue = src[x][2];
         detail[y >> 1].colour[y & 1][x].r = red * RED_VALUE;
         detail[y >> 1].colour[y & 1][x].g = green * GREEN_VALUE;
         detail[y >> 1].colour[y & 1][x].b = blue * BLUE_VALUE;
         detail[y >> 1].colour[y & 1][x].y = red * RED_TO_Y + green * GREEN_TO_Y + blue * BLUE_TO_Y;

         detail[y >> 1].red_av += red;
         detail[y >> 1].green_av += green;
         detail[y >> 1].blue_av += blue;
      }
      src += pitch;
   }
   src -= 4 * pitch;
   for (y = 0; y < 2; y++)
   {
      detail[y].red_av   = ((detail[y].red_av >> 3) + 4) & ~7;
      detail[y].green_av = ((detail[y].green_av >> 3) + 4) & ~7;
      detail[y].blue_av  = ((detail[y].blue_av >> 3) + 4) & ~7;
      if (detail[y].red_av > (255 & ~7)) detail[y].red_av = (255 & ~7);
      if (detail[y].green_av > (255 & ~7)) detail[y].green_av = (255 & ~7);
      if (detail[y].blue_av > (255 & ~7)) detail[y].blue_av = (255 & ~7);
   }

   block_score = block_encode(block, detail, block_score);

   /* transpose the data so we can re-use the same code to try a vertical block */
   for (y = 0; y < 2; y++)
      detail[y].red_av = detail[y].green_av = detail[y].blue_av = 0;
   for (y = 0; y < 4; y++)
   {
      for (x = 0; x < 4; x++)
      {
         int red = src[x][0], green = src[x][1], blue = src[x][2];
         detail[x >> 1].colour[x & 1][y].r = red * RED_VALUE;
         detail[x >> 1].colour[x & 1][y].g = green * GREEN_VALUE;
         detail[x >> 1].colour[x & 1][y].b = blue * BLUE_VALUE;
         detail[x >> 1].colour[x & 1][y].y = red * RED_TO_Y + green * GREEN_TO_Y + blue * BLUE_TO_Y;

         detail[x >> 1].red_av += red;
         detail[x >> 1].green_av += green;
         detail[x >> 1].blue_av += blue;
      }
      src += pitch;
   }
   for (y = 0; y < 2; y++)
   {
      detail[y].red_av   = ((detail[y].red_av >> 3) + 4) & ~7;
      detail[y].green_av = ((detail[y].green_av >> 3) + 4) & ~7;
      detail[y].blue_av  = ((detail[y].blue_av >> 3) + 4) & ~7;
      if (detail[y].red_av > (255 & ~7)) detail[y].red_av = (255 & ~7);
      if (detail[y].green_av > (255 & ~7)) detail[y].green_av = (255 & ~7);
      if (detail[y].blue_av > (255 & ~7)) detail[y].blue_av = (255 & ~7);
   }

   tmp_score = block_encode(&tmp, detail, block_score);
   tmp.flipbit = 0;
   if (tmp_score < block_score)
   {
      *block = tmp;
      block_score = tmp_score;
   }

   /* un-transpose the data if we transposed it above */
   if (block->flipbit == 0)
   {
      for (y = 0; y < 4; y++)
         for (x = y + 1; x < 4; x++)
         {
            int tmp;

            tmp = block->luma[x + y * 4];
            block->luma[x + y * 4] = block->luma[y + x * 4];
            block->luma[y + x * 4] = tmp;
         }
      /* and un-vflip it, too... */
      for (y = 0; y < 2; y++)
      {
         for (x = 0; x < 4; x++)
         {
            int tmp = block->luma[y * 4 + x];
            block->luma[y * 4 + x] = block->luma[(y ^ 3) * 4 + x];
            block->luma[(y ^ 3) * 4 + x] = tmp;
         }
      }
   }

   return pack_block(block);
}


#if 0
static int get_block_offset(int x, int y, int pitch)
{
   int offset_row, offset_col, offset_min, offset;

   /* re-hash things to match 32-bit t-format coordinates */
   x >>= 2;
   y >>= 2;
   x <<= 1;
   pitch <<= 1;

   offset_row = ((y + 32) & ~63) * pitch;

   offset_col = (x >> 4) * 512
              + ((y >> 4) & 1) * 256;
   offset_col ^= (y & 32) ? -512 : 0;
   offset_col ^= (x << 4) & 256;

   offset_min = ((y >> 2) & 3) * 64
              + ((x >> 2) & 3) * 16
              + (y & 3) * 4
              + (x & 3) * 1;

   offset = offset_row + offset_col + offset_min;

   /* de-hash things back to 64-bit */
   offset >>= 1;

   if ((unsigned)offset >= dsize)
   {
      fprintf(stderr, "calculation went wrong: coord: (%d,%d), pitch: %d  offset: %d\n", x, y, pitch, offset);
      return 0;
   }

   return offset;
}


static void STC_encode(unsigned long long *dest, int dpitch, unsigned char src[][3], int s_width, int s_height)
{
   int pitch = s_width;
   etc_block_t tmp;
   int x, y;

   src += s_height * pitch;
   pitch = -pitch;
   src += pitch;

   s_width &= ~3;
   s_height &= ~3;

   for (y = 0; y < s_height; y += 4)
   {
      for (x = 0; x < s_width; x += 4)
      {
         unsigned long long result = (x + x >= 0 * s_width) ? block_encode_new(src + x + y * pitch, pitch) : block_encode_search(&tmp, src + x + y * pitch, pitch);

#if 0
         if ((result & 0x0000000200000000ull) == 0)
         {
            result = 0;
         }
#endif

         /* TODO: write this out in big-endian format. */
         dest[get_block_offset(x, y, dpitch)] = result;
      }

      ldropped += dropped;
      dropped = 0;
      ltested += tested;
      tested = 0;
#if 0
      if ((y & 15) == 12)
      {
         fprintf(stderr, "%d%% complete  early-out speedup: %.3f%%\r", 100 * y / s_height, 100.0 * ldropped / ltested);
         fflush(stderr);
      }
#endif
   }
   fprintf(stderr, "100%% complete  early-out speedup: %.6f%%\n\n", 100.0 * ldropped / ltested);
}
#endif

static void setup_adjtab(void)
{
   static const int tabvalues[8][2] =
   {
      {   2,   8 },
      {   5,  17 },
      {   9,  29 },
      {  13,  42 },
      {  18,  60 },
      {  24,  80 },
      {  33, 106 },
      {  47, 183 }
   };
   int i;

   for (i = 0; i < 8; i++)
   {
      int l = tabvalues[i][0], h = tabvalues[i][1];

      adjtab[i][0].r = l * RED_VALUE;
      adjtab[i][0].g = l * GREEN_VALUE;
      adjtab[i][0].b = l * BLUE_VALUE;
      adjtab[i][0].y = l * GREY_TO_Y;
      adjtab[i][1].r = h * RED_VALUE;
      adjtab[i][1].g = h * GREEN_VALUE;
      adjtab[i][1].b = h * BLUE_VALUE;
      adjtab[i][1].y = h * GREY_TO_Y;
      adjtab[i][2].r = l * -RED_VALUE;
      adjtab[i][2].g = l * -GREEN_VALUE;
      adjtab[i][2].b = l * -BLUE_VALUE;
      adjtab[i][2].y = l * -GREY_TO_Y;
      adjtab[i][3].r = h * -RED_VALUE;
      adjtab[i][3].g = h * -GREEN_VALUE;
      adjtab[i][3].b = h * -BLUE_VALUE;
      adjtab[i][3].y = h * -GREY_TO_Y;
   }
}

#if 0
int main(int argc, char *argv[])
{
   FILE *infile, *outfile;
   unsigned char (*sbuf)[3];
   unsigned long long *dbuf;
   int version, width, height, maxval;
   int dpitch, hpad;
   int count;

   setup_adjtab();

   if ((infile = fopen(argv[1], "rb")) == NULL || fscanf(infile, "P%d %d %d %d", &version, &width, &height, &maxval) != 4 || fgetc(infile) != '\n')
   {
      fprintf(stderr, "Problem with input file '%s': %s\n", argv[1], strerror(errno));
      exit(1);
   }
   if ((outfile = fopen(argv[2], "wb")) == NULL)
   {
      fprintf(stderr, "Problem with output file '%s': %s\n", argv[2], strerror(errno));
      exit(1);
   }

   dpitch = ((width >> 2) + 15) & ~15;
   hpad = ((height >> 2) + 31) & ~31;
   dsize = dpitch * hpad;

   if ((sbuf = malloc(width * height * sizeof(*sbuf))) == NULL || (dbuf = calloc(dpitch * hpad, sizeof(*dbuf))) == NULL)
   {
      fprintf(stderr, "allocation problem: %s\n", strerror(errno));
      exit(1);
   }

   fprintf(outfile, "ETC1\n %4d %4d\n", width & ~3, height & ~3);

   if ((count = fread(sbuf, sizeof(*sbuf), width * height, infile)) != width * height)
   {
      fprintf(stderr, "Problem reading input file: %s (got %d pixels, not %d)\n", strerror(errno), count, width * height);
      exit(1);
   }

   fclose(infile);

   STC_encode(dbuf, dpitch, sbuf, width, height);

   if ((count = fwrite(dbuf, sizeof(*dbuf), dpitch * hpad, outfile)) != dpitch * hpad)
   {
      fprintf(stderr, "Problem writing output file: %s (wrote %d blocks, not %d)\n", strerror(errno), count, dpitch * hpad);
      exit(1);
   }

   fclose(outfile);

   return 0;
}
#endif

void etc1_compress_tex_image(
   GLenum target,
   GLint level,
   GLenum internalformat,
   GLsizei width,
   GLsizei height,
   GLint border,
   GLenum format,
   GLenum type,
   const GLvoid *pixels)
{
   int x0,x1,y0,y1;
   int w0,h0;
   unsigned char const *cpixels = (unsigned char const *)pixels;
   unsigned char src[16][3];
   size_t etc1size;
   unsigned long long *etc1data;

   w0 = (width + 3)/4;
   h0 = (height + 3)/4;

   etc1size = 8*w0*h0;
   etc1data = malloc(etc1size);
   assert(etc1data);

   setup_adjtab();

   for (y0 = 0; y0 < h0; y0++)
   {
      for (x0 = 0; x0 < w0; x0++)
      {
         unsigned long long etc1block;
         int i;

         /* Load 4x4 block */
         for (y1 = 0; y1 < 4; y1++)
         {
            for (x1 = 0; x1 < 4; x1++)
            {
               unsigned char r,g,b;
               int x = 4*x0+x1;
               int y = 4*y0+y1;

               if (format == GL_RGBA && type == GL_UNSIGNED_BYTE)
               {
                  r = cpixels[4*(width*y+x)];
                  g = cpixels[4*(width*y+x)+1];
                  b = cpixels[4*(width*y+x)+2];
               }
               else if (format == GL_RGB && type == GL_UNSIGNED_BYTE)
               {
                  r = cpixels[3*(width*y+x)];
                  g = cpixels[3*(width*y+x)+1];
                  b = cpixels[3*(width*y+x)+2];
               }
               else
               {
                  assert(0);    /* Cannot currently handle this texture format */
               }
               src[4*(3-y1)+x1][0] = r;
               src[4*(3-y1)+x1][1] = g;
               src[4*(3-y1)+x1][2] = b;
            }
         }
         etc1block = block_encode_new(src/*((x0^y0)&1)?src3:src2*/, 4);
         for (i = 0; i < 8; i++)
         {
            ((char*)etc1data)[8*(w0*y0 + x0) + i] = ((char*)&etc1block)[i^7];
         }
      }
   }

   glCompressedTexImage2D(
      target,
      level,
      GL_ETC1_RGB8_OES,
      width,
      height,
      0,
      etc1size,
      etc1data);

   free(etc1data);
}
