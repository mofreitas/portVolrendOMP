void Ray_Trace_Adaptive_Box(long outx, long outy, long boxlen)
{
  long i, j;
  long half_boxlen = boxlen >> 1;
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
      //Verifica se raiofoi traçado para o pixel (outx + i, outy + i)
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
      //min_volume_color = MIN(IMAGE(outy + i, outx + j), min_volume_color);
      //max_volume_color = MAX(IMAGE(outy + i, outx + j), max_volume_color);
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