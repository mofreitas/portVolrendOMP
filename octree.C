/*************************************************************************/
/*                                                                       */
/*  Copyright (c) 1994 Stanford University                               */
/*                                                                       */
/*  All rights reserved.                                                 */
/*                                                                       */
/*  Permission is given to use, copy, and modify this software for any   */
/*  non-commercial purpose as long as this copyright notice is not       */
/*  removed.  All other uses, including redistribution in whole or in    */
/*  part, are forbidden without prior written permission.                */
/*                                                                       */
/*  This software is provided with absolutely no warranty and no         */
/*  support.                                                             */
/*                                                                       */
/*************************************************************************/

/******************************************************************************
*                                                                             *
*   octree.c:  Perform hiearchical enumeration of dataset.                    *
*                                                                             *
******************************************************************************/

#include <string.h>
#include "incl.h"

#define WRITE_PYR(IBIT, ILEVEL, IZ, IY, IX) \
  (PYR_ADDRESS(ILEVEL, IZ, IY, IX),         \
   *pyr_address2 |= (IBIT) << pyr_offset2)

/* The following declarations show the layout of the .pyr file.              */
/* If changed, the version number must be incremented and code               */
/* written to handle loading of both old and current versions.               */

/* Version for new .pyr files:               */
#define PYR_CUR_VERSION 1 /*   Initial release                         */
short pyr_version;        /* Version of this .pyr file                 */

short pyr_levels; /* Number of levels in this pyramid          */

short pyr_len[MAX_PYRLEVEL + 1][NM];    /* Number of voxels on each level    */
short pyr_voxlen[MAX_PYRLEVEL + 1][NM]; /* Size of voxels on each level      */

int pyr_length[MAX_PYRLEVEL + 1];    /* Total number of bytes on this level       */
                                     /*   (= (long)((product of lens+7)/8))        */
BYTE *pyr_address[MAX_PYRLEVEL + 1]; /* Pointer to binary pyramid               */
                                     /*   (only pyr_levels sets of lens, lengths, */
                                     /*    and 3-D arrays are written to file)    */

/* End of layout of .pyr file.                                               */

/* Subscripted access to binary pyramid      */
/*   (PYR_ADDRESS only computes temporaries, */
/*    most others invoke PYR_ADDRESS, then:  */
/*      PYR returns char with bit in low bit,*/
/*      CLEAR_PYR clears bit to 0,           */
/*      SET_PYR sets bit to 1,               */
/*      WRITE_PYR ORs BOOLEAN IBIT into bit, */
/*      OVERWRITE_PYR clears, then ORs IBIT, */
/*    CURRENT_PYR returns at current address)*/
/* Warning:  do not invoke any of these      */
/*   macros more than once per statement,    */
/*   or values of temporaries may conflict,  */
/*   depending on optimization by compiler.  */
long pyr_offset1,   /* Bit offset of desired bit within pyramid  */
    pyr_offset2;    /* Bit offset of bit within byte             */
BYTE *pyr_address2; /* Pointer to byte containing bit            */

void Compute_Octree()
{
  long level, max_len;
  long i;
  max_len = 0;
  for (i = 0; i < NM; i++)
  {
    max_len = MAX(max_len, opc_len[i]);
  }
  pyr_levels = 1;
  while ((1 << (pyr_levels - 1)) < max_len)
    pyr_levels++;
  printf("    Computing binary pyramid of %d levels...\n",
         pyr_levels);

  for (i = 0; i < NM; i++)
  {
    pyr_len[0][i] = opc_len[i];
    pyr_voxlen[0][i] = 1;
  }
  pyr_length[0] = (pyr_len[0][X] * pyr_len[0][Y] * pyr_len[0][Z] + 7) / 8;
  Allocate_Pyramid_Level(&pyr_address[0], pyr_length[0]);

  printf("    Computing octree base...\n");

#ifndef SERIAL_PREPROC
  #pragma omp parallel num_threads(num_nodes)
  {
    #pragma omp single
    {
      Compute_Base();
    }
    #pragma omp taskwait
  }
#else
  Compute_Base();
#endif

  printf("    Performing OR of eight neighbors in binary mask...\n");

#ifndef SERIAL_PREPROC
  #pragma omp parallel num_threads(num_nodes)
#endif
  Or_Neighbors_In_Base();

  for (level = 1; level < pyr_levels; level++)
  {
    for (i = 0; i < NM; i++)
    {
      if (pyr_len[level - 1][i] > 1)
      {
        pyr_len[level][i] =
            (pyr_len[level - 1][i] + 1) >> 1;
        pyr_voxlen[level][i] =
            pyr_voxlen[level - 1][i] << 1;
      }
      else
      {
        pyr_len[level][i] =
            pyr_len[level - 1][i];
        pyr_voxlen[level][i] =
            pyr_voxlen[level - 1][i];
      }
    }
    pyr_length[level] = (pyr_len[level][X] *
                             pyr_len[level][Y] * pyr_len[level][Z] +
                         7) /
                        8;
    Allocate_Pyramid_Level(&pyr_address[level], pyr_length[level]);
    Compute_Pyramid_Level(level);
  }
}

void Compute_Base()
{
  long outx, outy, outz;
  long zstart, zstop;
  long xstart, xstop, ystart, ystop;

  zstart = 0;
  zstop = pyr_len[0][Z];
  ystart = 0;
  ystop = pyr_len[0][Y];
  xstart = 0;
  xstop = pyr_len[0][X];
  #pragma omp task
  {
    for (outz = zstart; outz < zstop; outz++)
    {
      for (outy = ystart; outy < ystop; outy++)
      {
        for (outx = xstart; outx < xstop; outx++)
        {
        
          if (OPC(outz, outy, outx) == 0)      /* mask bit is one unless opacity */
            WRITE_PYR(0, 0, outz, outy, outx); /* value is zero.                 */
          else
            WRITE_PYR(1, 0, outz, outy, outx);
        }
      }
    }
  }
}

void Or_Neighbors_In_Base()
{
  long outx, outy, outz; /* Loop indices in image space               */
  long outx_plus_one, outy_plus_one, outz_plus_one;
  BOOLEAN bit;
  long zstop;
  
  zstop = pyr_len[0][Z];
  
#ifndef SERIAL_PREPROC
  #pragma omp for schedule(dynamic, 4)
#endif
  for (outz = 0; outz < zstop; outz++)
  {
    outz_plus_one = MIN(outz + 1, pyr_len[0][Z] - 1);
    for (outy = 0; outy < pyr_len[0][Y]; outy++)
    {
      outy_plus_one = MIN(outy + 1, pyr_len[0][Y] - 1);
      for (outx = 0; outx < pyr_len[0][X]; outx++)
      {
        outx_plus_one = MIN(outx + 1, pyr_len[0][X] - 1);

        bit = PYR(0, outz, outy, outx);
        bit |= PYR(0, outz, outy, outx_plus_one);
        bit |= PYR(0, outz, outy_plus_one, outx);
        bit |= PYR(0, outz, outy_plus_one, outx_plus_one);
        bit |= PYR(0, outz_plus_one, outy, outx);
        bit |= PYR(0, outz_plus_one, outy, outx_plus_one);
        bit |= PYR(0, outz_plus_one, outy_plus_one, outx);
        bit |= PYR(0, outz_plus_one, outy_plus_one, outx_plus_one);

        WRITE_PYR(bit, 0, outz, outy, outx);
      }
    }
  }
}

void Allocate_Pyramid_Level(address, length)
    BYTE **address;
long length;
{
  long i;

  printf("      Allocating pyramid level of %ld bytes...\n",
         length * sizeof(BYTE));

  /*  POSSIBLE ENHANCEMENT:  If you want to replicate the octree
on all processors, then replace the macro below with a regular malloc.
*/

  *address = (BYTE *)NU_MALLOC(length * sizeof(BYTE), 0);

  if (*address == NULL)
    Error("    No space available for pyramid level.\n");

  /*  POSSIBLE ENHANCEMENT:  Here's where one might distribute the
    octree among physical memories if one wanted to.
*/

  for (i = 0; i < length; i++)
    *(*address + i) = 0;
}

void Compute_Pyramid_Level(level) long level;
{
  long outx, outy, outz; /* Loop indices in image space               */
  long inx, iny, inz;
  long inx_plus_one, iny_plus_one, inz_plus_one;
  BOOLEAN bit;

  printf("      Computing pyramid level %ld from level %ld...\n",
         level, level - 1);
  for (outz = 0; outz < pyr_len[level][Z]; outz++)
  {
    inz = outz << 1;
    inz_plus_one = MIN(inz + 1, pyr_len[level - 1][Z] - 1);
    for (outy = 0; outy < pyr_len[level][Y]; outy++)
    {
      iny = outy << 1;
      iny_plus_one = MIN(iny + 1, pyr_len[level - 1][Y] - 1);
      for (outx = 0; outx < pyr_len[level][X]; outx++)
      {
        inx = outx << 1;
        inx_plus_one = MIN(inx + 1, pyr_len[level - 1][X] - 1);

        bit = PYR(level - 1, inz, iny, inx);
        bit |= PYR(level - 1, inz, iny, inx_plus_one);
        bit |= PYR(level - 1, inz, iny_plus_one, inx);
        bit |= PYR(level - 1, inz, iny_plus_one, inx_plus_one);
        bit |= PYR(level - 1, inz_plus_one, iny, inx);
        bit |= PYR(level - 1, inz_plus_one, iny, inx_plus_one);
        bit |= PYR(level - 1, inz_plus_one, iny_plus_one, inx);
        bit |= PYR(level - 1, inz_plus_one, iny_plus_one, inx_plus_one);

        WRITE_PYR(bit, level, outz, outy, outx);
      }
    }
  }
}

void Load_Octree(filename) char filename[];
{
  char local_filename[FILENAME_STRING_SIZE];
  int fd;
  long level;

  strcpy(local_filename, filename);
  strcat(local_filename, ".pyr");
  fd = Open_File(local_filename);

  Read_Shorts(fd, (unsigned char *)&pyr_version, (long)sizeof(pyr_version));
  if (pyr_version != PYR_CUR_VERSION)
    Error("    Can't load version %ld file\n", pyr_version);

  Read_Shorts(fd, (unsigned char *)&pyr_levels, (long)sizeof(pyr_levels));
  Read_Shorts(fd, (unsigned char *)pyr_len, (long)(pyr_levels * NM * sizeof(long)));
  Read_Shorts(fd, (unsigned char *)pyr_voxlen, (long)(pyr_levels * NM * sizeof(long)));
  Read_Longs(fd, (unsigned char *)pyr_length, (long)(pyr_levels * sizeof(pyr_length[0])));

  printf("    Loading binary pyramid of %d levels...\n", pyr_levels);
  for (level = 0; level < pyr_levels; level++)
  {
    Allocate_Pyramid_Level(&pyr_address[level], pyr_length[level]);
    printf("      Loading pyramid level %ld from .pyr file...\n", level);
    Read_Bytes(fd, (unsigned char *)pyr_address[level], (long)(pyr_length[level] * sizeof(BYTE)));
  }
  Close_File(fd);
}

void Store_Octree(filename) char filename[];
{
  char local_filename[FILENAME_STRING_SIZE];
  int fd;
  long level;

  strcpy(local_filename, filename);
  strcat(local_filename, ".pyr");
  fd = Create_File(local_filename);

  pyr_version = PYR_CUR_VERSION;
  Write_Shorts(fd, (unsigned char *)&pyr_version, (long)sizeof(pyr_version));

  Write_Shorts(fd, (unsigned char *)&pyr_levels, (long)sizeof(pyr_levels));
  Write_Shorts(fd, (unsigned char *)pyr_len, (long)(pyr_levels * NM * sizeof(long)));
  Write_Shorts(fd, (unsigned char *)pyr_voxlen, (long)(pyr_levels * NM * sizeof(long)));
  Write_Longs(fd, (unsigned char *)pyr_length, (long)(pyr_levels * sizeof(pyr_length[0])));

  printf("    Storing binary pyramid of %d levels...\n", pyr_levels);
  for (level = 0; level < pyr_levels; level++)
  {
    printf("      Storing pyramid level %ld into .pyr file...\n", level);

    Write_Bytes(fd, (unsigned char *)pyr_address[level], (long)(pyr_length[level] * sizeof(BYTE)));
  }
  Close_File(fd);
}
