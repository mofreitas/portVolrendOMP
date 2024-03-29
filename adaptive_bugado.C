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

/*************************************************************************
*                                                                        *
*     adaptive.c:  Render dataset via raytracing.                        *
*                                                                        *
*************************************************************************/

#include "incl.h"
#include <stdio.h>
#include <omp.h>

float invjacobian[NM][NM];       /* Jacobian matrix showing object space      */
                                 /*   d{x,y,z} per image space d{x,y,z}       */
                                 /*   [0][0] is dx(object)/dx(image)          */
                                 /*   [0][2] is dz(object)/dx(image)          */
                                 /*   [2][0] is dx(object)/dz(image)          */
float invinvjacobian[NM][NM];    /*   [i][j] = 1.0 / invjacobian[i][j]        */
                                 /* For gathering statistics:                 */
long num_rays_traced;            /*   number of calls to Trace_Ray            */
long num_traced_rays_hit_volume; /*   number of traced rays that hit volume   */
long num_samples_trilirped;      /*   number of samples trilirped             */
long itest;

#define RAY_TRACED ((MAX_PIXEL + 1) / 2) /* Ray traced at this pixel  */
#define START_RAY 1
#define INTERPOLATED ((MAX_PIXEL + 1) / 32) /* This pixel interpolated   */

//EXTERN_ENV

#include "anl.h"

void Ray_Trace(long my_node)
{
  long i, j;
  long starttime, stoptime, exectime, exectime1;

  /* Assumptions made by ray tracer:                                   */
  /*   o Frustrum clipping is performed.                               */
  /*     All viewing frustums will be handled correctly.               */
  /*   o Contributions are obtained only from nearest 8 neighbors.     */
  /*     If downsizing was specified, some input voxels will be        */
  /*     unsampled, but upsizing may be specified and will be          */
  /*     handled correctly.                                            */

  /* Compute inverse Jacobian matrix from                              */
  /* coordinates of output map unit voxel in object space,             */
  /* then make a copy of object space d{x,y,z} per image space dz,     */
  /* which controls number of ray samples per object space voxel       */
  /* (i.e. Z-sampling rate), to allow recomputation per region.        */
  for (i = 0; i < NM; i++)
  {
    invjacobian[X][i] = uout_invvertex[0][0][1][i] -
                        uout_invvertex[0][0][0][i];
    invjacobian[Y][i] = uout_invvertex[0][1][0][i] -
                        uout_invvertex[0][0][0][i];
    invjacobian[Z][i] = uout_invvertex[1][0][0][i] -
                        uout_invvertex[0][0][0][i];
  }

  /* Compute multiplicative inverse of inverse Jacobian matrix.        */
  /* If any Jacobian is zero, compute no inverse for that element.     */
  /* This test must be repeated before access to any inverse element.  */
  for (i = 0; i < NM; i++)
  {
    for (j = 0; j < NM; j++)
    {
      if (ABS(invjacobian[i][j]) > SMALL)
        invinvjacobian[i][j] = 1.0 / invjacobian[i][j];
    }
  }

  num_rays_traced = 0;
  num_traced_rays_hit_volume = 0;
  num_samples_trilirped = 0;

  /* Invoke adaptive or non-adaptive ray tracer                          */
  if (adaptive)
  {
    //#pragma omp barrier

/*   BARRIER(Global->TimeBarrier,num_nodes); */

    CLOCK(starttime);
    Pre_Shade(my_node);

    /*   Espera threads acabarem
      LOCK(Global->CountLock);
      Global->Counter--;
      UNLOCK(Global->CountLock);
      while (Global->Counter); */

      //#pragma omp barrier
    Ray_Trace_Adaptively(my_node);

    CLOCK(stoptime);
    //#pragma omp barrier

 /*  BARRIER(Global->TimeBarrier,num_nodes); */

    mclock(stoptime, starttime, &exectime);

    /* If adaptively ray tracing and highest sampling size is greater    */
    /* than lowest size for volume data if polygon list exists or        */
    /* display pixel size if it does not, recursively interpolate to     */
    /* fill in any missing samples down to lowest size for volume data.  */

    if (highest_sampling_boxlen > 1)
    {

/* BARRIER(Global->TimeBarrier,num_nodes); */
//#pragma omp barrier
      CLOCK(starttime);
      Interpolate_Recursively(my_node);

      CLOCK(stoptime);

/* BARRIER(Global->TimeBarrier,num_nodes); */
//#pragma omp barrier 
      mclock(stoptime, starttime, &exectime1);
    }
  }
  else
  {

/*   BARRIER(Global->TimeBarrier,num_nodes);*/
  //#pragma omp barrier 
    CLOCK(starttime);

    Pre_Shade(my_node);

/*   LOCK(Global->CountLock);
  Global->Counter--;
  UNLOCK(Global->CountLock);
  while (Global->Counter);*/
  //#pragma omp barrier 
    Ray_Trace_Non_Adaptively(my_node);

    CLOCK(stoptime);

/*   BARRIER(Global->TimeBarrier,num_nodes);*/
  //#pragma omp barrier 
    mclock(stoptime, starttime, &exectime);
    exectime1 = 0;
  }

  /*   LOCK(Global->CountLock); */
  /*printf("%3ld\t%3ld\t%6ld\t%6ld\t%6ld\t%6ld\t%8ld\n",my_node,frame,exectime,
	   exectime1,num_rays_traced,num_traced_rays_hit_volume,
	   num_samples_trilirped);*/

 /*    UNLOCK(Global->CountLock); */
/*   #pragma omp barrier */
/*     BARRIER(Global->TimeBarrier, num_nodes); */
}



void Ray_Trace_Adaptively(long my_node)
{
  long outx, outy, yindex, xindex;

  long num_xqueue, num_yqueue, num_queue, lnum_xblocks, lnum_yblocks, lnum_blocks;
  long xstart, xstop, ystart, ystop, local_node, work;

  itest = 0;

  /*   image_len[eixo] - numero de pixels da imagem dado um eixo
    image_section[eixo] - numero de seçõs em que a imagem em dado eixo
    é dividida
    num_queue - numero de pixels de cada "superbloco"
    Global->Queue[num_nodes][0] - indica quantas threads ainda estão processando dados */

  /* Calcula numero de pixels de cada "super bloco" ou região dado o numero */
  /* de seções que a imagem tem (definido basedo no número de threads) */
  num_xqueue = ROUNDUP((float)image_len[X] / (float)image_section[X]);
  num_yqueue = ROUNDUP((float)image_len[Y] / (float)image_section[Y]);
  num_queue = num_xqueue * num_yqueue;

  /* Calcula números de tiles para cada superbloco
  lnum_xblocks = ROUNDUP((float)num_xqueue / (float)block_xlen);
  lnum_yblocks = ROUNDUP((float)num_yqueue / (float)block_ylen);
  lnum_blocks = lnum_xblocks * lnum_yblocks;
 */
  lnum_xblocks = ROUNDUP((float)image_len[X] / (float)block_xlen);
  lnum_yblocks = ROUNDUP((float)image_len[Y] / (float)block_ylen);
  lnum_blocks = lnum_xblocks * lnum_yblocks;

/*     Define thread que estará executando codigo a seguinr
    local_node = my_node;

    Serve para indicar em qual tile a thread "localnode" está trabalhando
    Global->Queue[local_node][0] = 0;

    Executa enquanto ainda há threads com dados para serem processados
    while (Global->Queue[num_nodes][0] > 0)
    {
      Define pixels de começo e fim de cada ""superbloco""
    xstart = (local_node % image_section[X]) * num_xqueue;
    xstart = ROUNDUP((float)xstart / (float)highest_sampling_boxlen);
    xstart = xstart * highest_sampling_boxlen;
    xstop = MIN(xstart + num_xqueue, image_len[X]);
    ystart = (local_node / image_section[X]) * num_yqueue;
    ystart = ROUNDUP((float)ystart / (float)highest_sampling_boxlen);
    ystart = ystart * highest_sampling_boxlen;
    ystop = MIN(ystart + num_yqueue, image_len[Y]);

      Caso um superbloco esteja sendo processado por duas thread diferentes,
      Esse mutex garante que cada uma irá trabalhar em um tile (work) diferente
      ALOCK(Global->QLock, local_node);
      work = Global->Queue[local_node][0];
      Global->Queue[local_node][0] += 1;
      AULOCK(Global->QLock, local_node);
    
      Executa enquanto o número de tiles trabalhadas for menor
      que o número de tiles do superbloco (percorre superbloco tile a tile) */
    xstart = 0;
    xstop = image_len[X];
    ystart = 0;
    ystop = image_len[Y];
    int i = 0;
#   pragma omp for schedule(dynamic, 1)
    for(work = 0; work < lnum_blocks; work++){
    /*   while (work < lnum_blocks)
      {
        Percorre tile com passos de highest_sampling_boxlen */
      xindex = xstart + (work % lnum_xblocks) * block_xlen;
      yindex = ystart + (work / lnum_xblocks) * block_ylen;
      for (outy = yindex; outy < yindex + block_ylen && outy < ystop;
           outy += highest_sampling_boxlen)
      {
        for (outx = xindex; outx < xindex + block_xlen && outx < xstop;
             outx += highest_sampling_boxlen)
        {

          /* Trace rays within square box of highest sampling size     */
          /* whose lower-left corner is current image space location.  */
          Ray_Trace_Adaptive_Box(outx, outy, highest_sampling_boxlen);
        }
      }
      /*   Equivalente ao work++
        Quando um superbloco está sendo processado por duas threads, 
        o work é compartilhado entre as duas
        ALOCK(Global->QLock, local_node);
        work = Global->Queue[local_node][0];
        Global->Queue[local_node][0] += 1;
        AULOCK(Global->QLock, local_node); */
    }
   /*    Se a thread terminou seu "superbloco", remove um da lista de "superblocos"
      faltantes
      if (my_node == local_node)
      {
        ALOCK(Global->QLock, num_nodes);
        Global->Queue[num_nodes][0]--;
        AULOCK(Global->QLock, num_nodes);
      }
      desloca a thread ciclicamente, de forma que se a thread 0 acabou, vai para a thread 1
      local_node = (local_node + 1) % num_nodes;

      Procura por uma thread (local_node) que não acabou suas tiles, 
      perconrrendo-as ciclicamente
      while (Global->Queue[local_node][0] >= lnum_blocks &&
             Global->Queue[num_nodes][0] > 0)
        local_node = (local_node + 1) % num_nodes;
    } */
}

void Ray_Trace_Adaptive_Box(long outx, long outy, long boxlen)
{
  long i, j;
  long half_boxlen;
  long min_volume_color, max_volume_color;
  float foutx, fouty;
  volatile long imask;

  PIXEL *pixel_address;

  /* Trace rays from all four corners of the box into the map,         */
  /* being careful not to exceed the boundaries of the output image,   */
  /* and using a flag array to avoid retracing any rays.               */
  /* For diagnostic display, flag is set to a light gray.              */
  /* If mipmapping, flag is light gray minus current mipmap level.     */
  /* If mipmapping and ray has already been traced,                    */
  /* retrace it if current mipmap level is lower than                  */
  /* mipmap level in effect when ray was last traced,                  */
  /* thus replacing crude approximation with better one.               */
  /* Meanwhile, compute minimum and maximum geometry/volume colors.    */
  /* If polygon list exists, compute geometry-only colors              */
  /* and volume-attenuated geometry-only colors as well.               */
  /* If stochastic sampling and box is smaller than a display pixel,   */
  /* distribute the rays uniformly across a square centered on the     */
  /* nominal ray location and of size equal to the image array spacing.*/
  /* This scheme interpolates the jitter size / sample spacing ratio   */
  /* from zero at one sample per display pixel, avoiding jitter noise, */
  /* to one at the maximum sampling rate, insuring complete coverage,  */
  /* all the while building on previously traced rays where possible.  */
  /* The constant radius also prevents overlap of jitter squares from  */
  /* successive subdivision levels, preventing sample clumping noise.  */

  min_volume_color = MAX_PIXEL;
  max_volume_color = MIN_PIXEL;

  for (i = 0; i <= boxlen && outy + i < image_len[Y]; i += boxlen)
  {
    for (j = 0; j <= boxlen && outx + j < image_len[X]; j += boxlen)
    {

      /*reschedule processes here if rescheduling only at synch points on simulator*/

      if (MASK_IMAGE(outy + i, outx + j) == 0)
      {

        /*reschedule processes here if rescheduling only at synch points on simulator*/

        MASK_IMAGE(outy + i, outx + j) = START_RAY;

        /*reschedule processes here if rescheduling only at synch points on simulator*/

        foutx = (float)(outx + j);
        fouty = (float)(outy + i);
        pixel_address = IMAGE_ADDRESS(outy + i, outx + j);

        /*reschedule processes here if rescheduling only at synch points on simulator*/

        Trace_Ray(foutx, fouty, pixel_address);

        /*reschedule processes here if rescheduling only at synch points on simulator*/

        MASK_IMAGE(outy + i, outx + j) = RAY_TRACED;
      }
    }
  }
  for (i = 0; i <= boxlen && outy + i < image_len[Y]; i += boxlen)
  {
    for (j = 0; j <= boxlen && outx + j < image_len[X]; j += boxlen)
    {
      imask = MASK_IMAGE(outy + i, outx + j);

      /*reschedule processes here if rescheduling only at synch points on simulator*/

      while (imask == START_RAY)
      {

        /*reschedule processes here if rescheduling only at synch points on simulator*/

        imask = MASK_IMAGE(outy + i, outx + j);

        /*reschedule processes here if rescheduling only at synch points on simulator*/
      }
      min_volume_color = MIN(IMAGE(outy + i, outx + j), min_volume_color);
      max_volume_color = MAX(IMAGE(outy + i, outx + j), max_volume_color);
    }
  }

  /* If size of current box is above lowest size for volume data and   */
  /* magnitude of geometry/volume color difference is significant, or  */
  /* size of current box is above lowest size for geometric data and   */
  /* magnitudes of geometry-only and volume-attenuated geometry-only   */
  /* are both significant, thus detecting only visible geometry events,*/
  /* invoke this function recursively to trace rays within the         */
  /* four equal-sized square sub-boxes enclosed by the current box,    */
  /* being careful not to exceed the boundaries of the output image.   */
  /* Use of geometry-only color difference suppressed in accordance    */
  /* with hybrid.trf as published in IEEE CG&A, March, 1990.           */
  if (boxlen > lowest_volume_boxlen &&
      max_volume_color - min_volume_color >=
          volume_color_difference)
  {
    half_boxlen = boxlen >> 1;
    for (i = 0; i < boxlen && outy + i < image_len[Y]; i += half_boxlen)
    {
      for (j = 0; j < boxlen && outx + j < image_len[X]; j += half_boxlen)
      {
        Ray_Trace_Adaptive_Box(outx + j, outy + i, half_boxlen);
      }
    }
  }
}

void Ray_Trace_Non_Adaptively(long my_node)
{
  //#   pragma omp parallel num_threads(4) default(shared) 
//{
  PIXEL *pixel_address;

  long num_xqueue, num_yqueue, num_queue, lnum_xblocks, lnum_yblocks, lnum_blocks;
  long xstart, xstop, ystart, ystop, local_node, work;
  
  num_xqueue = ROUNDUP((float)image_len[X] / (float)image_section[X]);
  num_yqueue = ROUNDUP((float)image_len[Y] / (float)image_section[Y]);
  num_queue = num_xqueue * num_yqueue;
  
  lnum_xblocks = ROUNDUP((float)image_len[X] / (float)block_xlen);
  lnum_yblocks = ROUNDUP((float)image_len[Y] / (float)block_ylen);
  lnum_blocks = lnum_xblocks * lnum_yblocks;
  
 /*  local_node = my_node;
    Global->Queue[local_node][0] = 0; */
  /* while (Global->Queue[num_nodes][0] > 0)
  { */
    /* xstart = (local_node % image_section[X]) * num_xqueue;
    xstop = MIN(xstart + num_xqueue, image_len[X]);
    ystart = (local_node / image_section[X]) * num_yqueue;
    ystop = MIN(ystart + num_yqueue, image_len[Y]);
    ALOCK(Global->QLock, local_node);
    work = Global->Queue[local_node][0]++;
    AULOCK(Global->QLock, local_node); */
    
    xstart = 0;
    xstop = image_len[X];
    ystart = 0;
    ystop = image_len[Y];
    int i = 0;


    
    /* printf("%i, ", omp_get_num_threads()); */
    #pragma omp parallel for schedule(dynamic, 16) num_threads(num_nodes) default(shared) private(work)
    for(work = 0; work < lnum_blocks; work++)
    {
      long outx, outy, xindex, yindex;
      float foutx, fouty;
   /*  while (work < lnum_blocks)
    { */
      xindex = xstart + (work % lnum_xblocks) * block_xlen;
      yindex = ystart + (work / lnum_xblocks) * block_ylen;
      for (outy = yindex; outy < yindex + block_ylen && outy < ystop; outy++)
      {
        for (outx = xindex; outx < xindex + block_xlen && outx < xstop; outx++)
        {

          /* Trace ray from specified image space location into map.   */
          /* Stochastic sampling is as described in adaptive code.     */
          foutx = (float)(outx);
          fouty = (float)(outy);
          pixel_address = IMAGE_ADDRESS(outy, outx);
          Trace_Ray(foutx, fouty, pixel_address);
        }
      }
      /* ALOCK(Global->QLock, local_node);
      work = Global->Queue[local_node][0]++;
      AULOCK(Global->QLock, local_node); */
    }
//}
/* printf("fim bloco \n"); */

    /* }
    if (my_node == local_node)
    {
      ALOCK(Global->QLock, num_nodes);
      Global->Queue[num_nodes][0]--;
      AULOCK(Global->QLock, num_nodes);
    }
    local_node = (local_node + 1) % num_nodes;
    while (Global->Queue[local_node][0] >= lnum_blocks &&
           Global->Queue[num_nodes][0] > 0)
      local_node = (local_node + 1) % num_nodes; */
  /* } */
}

void Ray_Trace_Fast_Non_Adaptively(long my_node)
{
  long i, outx, outy, xindex, yindex;
  float foutx, fouty;
  PIXEL *pixel_address;

  for (i = 0; i < num_blocks; i += num_nodes)
  {
    yindex = ((my_node + i) / num_xblocks) * block_ylen;
    xindex = ((my_node + i) % num_xblocks) * block_xlen;

    for (outy = yindex; outy < yindex + block_ylen &&
                        outy < image_len[Y];
         outy += lowest_volume_boxlen)
    {
      for (outx = xindex; outx < xindex + block_xlen &&
                          outx < image_len[X];
           outx += lowest_volume_boxlen)
      {

        /* Trace ray from specified image space location into map.   */
        /* Stochastic sampling is as described in adaptive code.     */
        MASK_IMAGE(outy, outx) += RAY_TRACED;
        foutx = (float)(outx);
        fouty = (float)(outy);
        pixel_address = IMAGE_ADDRESS(outy, outx);
        Trace_Ray(foutx, fouty, pixel_address);
        num_rays_traced++;
      }
    }
  }
}

void Interpolate_Recursively(long my_node)
{
  long i, outx, outy, xindex, yindex;

  //#pragma omp parallel for schedule(static, 20) num_threads(8)
  for (i = 0; i < num_blocks; i += num_nodes)
  {
    yindex = ((my_node + i) / num_xblocks) * block_ylen;
    xindex = ((my_node + i) % num_xblocks) * block_xlen;

    for (outy = yindex; outy < yindex + block_ylen &&
                        outy < image_len[Y];
         outy += highest_sampling_boxlen)
    {
      for (outx = xindex; outx < xindex + block_xlen &&
                          outx < image_len[X];
           outx += highest_sampling_boxlen)
      {

        /* Fill in image within square box of highest sampling size  */
        /* whose lower-left corner is current image space location.  */
        Interpolate_Recursive_Box(outx, outy, highest_sampling_boxlen);
      }
    }
  }
}

void Interpolate_Recursive_Box(long outx, long outy, long boxlen)
{
  long i, j;
  long half_boxlen;
  long corner_color[2][2], color;
  long outx_plus_boxlen, outy_plus_boxlen;

  float one_over_boxlen;
  float xalpha, yalpha;
  float one_minus_xalpha, one_minus_yalpha;

  /* Fill in the four pixels at the midpoints of the sides and at      */
  /* the center of the box by bilirping between the four corners,      */
  /* being careful not to exceed the boundaries of the output image,   */
  /* and using a flag array to avoid recomputing any pixels.           */
  /* For diagnostic display, flag is set to a dark gray.               */
  /* By making interpolation follow ray tracing, no pixels along the   */
  /* perimeter are filled in using interpolation, just to be replaced  */
  /* by a more accurate color when adjacent boxes are ray traced.      */
  /* By making the interpolation recursive, it is able to fill in      */
  /* large boxes using all pixels available along their perimeters,    */
  /* rather than just at their four corners.  This prevents "creases", */
  /* or, stated another way, insures zero-order continuity everywhere. */
  half_boxlen = boxlen >> 1;
  one_over_boxlen = 1.0 / (float)boxlen;

  outx_plus_boxlen = outx + boxlen < image_len[X] ? outx + boxlen : outx;
  outy_plus_boxlen = outy + boxlen < image_len[Y] ? outy + boxlen : outy;

  corner_color[0][0] = IMAGE(outy, outx);
  corner_color[0][1] = IMAGE(outy, outx_plus_boxlen);
  corner_color[1][0] = IMAGE(outy_plus_boxlen, outx);
  corner_color[1][1] = IMAGE(outy_plus_boxlen, outx_plus_boxlen);
  for (i = 0; i <= boxlen && outy + i < image_len[Y]; i += half_boxlen)
  {
    yalpha = (float)i * one_over_boxlen;
    one_minus_yalpha = 1.0 - yalpha;
    for (j = 0; j <= boxlen && outx + j < image_len[X]; j += half_boxlen)
    {
      xalpha = (float)j * one_over_boxlen;
      one_minus_xalpha = 1.0 - xalpha;
      if (MASK_IMAGE(outy + i, outx + j) == 0)
      {
        color = corner_color[0][0] * one_minus_xalpha * one_minus_yalpha +
                corner_color[0][1] * xalpha * one_minus_yalpha +
                corner_color[1][0] * one_minus_xalpha * yalpha +
                corner_color[1][1] * xalpha * yalpha;
        IMAGE(outy + i, outx + j) = color;
        MASK_IMAGE(outy + i, outx + j) += INTERPOLATED;
      }
    }
  }

  /* If size of sub-boxes is above lowest size for volume data         */
  /* if polygon list exists or display pixel size if it does not,      */
  /* invoke this function recursively to fill in image within the      */
  /* four equal-sized square sub-boxes enclosed by the current box,    */
  /* being careful not to exceed the boundaries of the output image.   */
  if (half_boxlen > 1)
  {
    for (i = 0; i < boxlen && outy + i < image_len[Y]; i += half_boxlen)
    {
      for (j = 0; j < boxlen && outx + j < image_len[X]; j += half_boxlen)
      {
        Interpolate_Recursive_Box(outx + j, outy + i, half_boxlen);
      }
    }
  }
}
