// Fixed-point version of magick.c

static fixed_t BoxFixed(const fixed_t x,const fixed_t ARGUNUSED(support))
{
  if (x < -FIXED_HALF)
    return 0;
  if (x < FIXED_HALF)
    return FIXED_1;
  return 0;
}

static fixed_t TriangleFixed(const fixed_t x,const fixed_t ARGUNUSED(support))
{
  if (x < -FIXED_1)
    return 0;
  if (x < 0)
    return(FIXED_1+x);
  if (x < FIXED_1)
    return(FIXED_1-x);
  return 0;
}

// Other filters could be ported but they are increasingly more complex and
// include a lot of multiplication, divisison, and/or trig functions

static void
image_downsize_gm_horizontal_filter_fixed_point(image *im, ImageInfo *source, ImageInfo *destination,
  const fixed_t x_factor, const FilterInfoFixed *filter_info, ContributionInfoFixed *contribution, int rotate)
{
  fixed_t scale, support;
  int x;
  int dstX = 0;
  int dstW = destination->columns;
  
  if (im->width_padding) {
    dstX = im->width_padding;
    dstW = im->width_inner;
  }
  
  scale = MAX(fixed_div(FIXED_1, x_factor), FIXED_1);
  support = fixed_mul(scale, filter_info->support);
  if (support <= FIXED_HALF) {
    // Reduce to point sampling
    support = FIXED_HALF + FIXED_EPSILON;
    scale = FIXED_1;
  }
  scale = fixed_div(FIXED_1, scale);
  
  for (x = dstX; (x < dstX + dstW); x++) {
    fixed_t center, density;
    int n, start, stop, y;
    
    center  = fixed_div(int_to_fixed(x - dstX) + FIXED_HALF, x_factor);
    start   = fixed_to_int(MAX(center - support + FIXED_HALF, 0));
    stop    = fixed_to_int(MIN(center + support + FIXED_HALF, int_to_fixed(source->columns)));
    density = 0;
    
    //DEBUG_TRACE("x %d: center %.2f, start %d, stop %d\n", x, center, start, stop);
    
    for (n = 0; n < (stop - start); n++) {
      contribution[n].pixel = start + n;
      contribution[n].weight = filter_info->function(fixed_mul(scale, (int_to_fixed(start) + int_to_fixed(n) - center + FIXED_HALF)), filter_info->support);
      density += contribution[n].weight;
      //DEBUG_TRACE("  contribution[%d].pixel %d, weight %.2f, density %.2f\n", n, contribution[n].pixel, contribution[n].weight, density);
    }
    
    if ((density != 0) && (density != FIXED_1)) {
      // Normalize
      int i;
      
      density = fixed_div(FIXED_1, density);
      for (i = 0; i < n; i++) {
        contribution[i].weight = fixed_mul(contribution[i].weight, density);
        //DEBUG_TRACE("  normalize contribution[%d].weight to %.2f\n", i, fixed_to_float(contribution[i].weight));
      }
    }
    
    for (y = 0; y < destination->rows; y++) {
      fixed_t weight;
      fixed_t red = 0, green = 0, blue = 0, alpha = 0;
      pix p;
      int j;
      register int i;
      
      //DEBUG_TRACE("y %d:\n", y);
      
      if (im->has_alpha) {
        fixed_t normalize = 0;
        
        for (i = 0; i < n; i++) {          
          j = (int)((y * source->columns) + contribution[i].pixel);
          weight = contribution[i].weight;
          p = source->buf[j];
          
          // XXX The original GM code weighted based on transparency for some reason,
          // but this produces bad results, so we use only the weight
          //transparency_coeff = weight * ((float)COL_ALPHA(p) / 255);
          
          /*
          DEBUG_TRACE("    merging with pix (%d, %d) @ %d (%d %d %d %d) weight %.2f\n",
            x, contribution[i].pixel, j,
            COL_RED(p), COL_GREEN(p), COL_BLUE(p), COL_ALPHA(p),
            fixed_to_float(weight));
          */
          
          red   += fixed_mul(weight, int_to_fixed(COL_RED(p)));
          green += fixed_mul(weight, int_to_fixed(COL_GREEN(p)));
          blue  += fixed_mul(weight, int_to_fixed(COL_BLUE(p)));
          alpha += fixed_mul(weight, int_to_fixed(COL_ALPHA(p)));
          normalize += weight;
        }
      
        normalize = fixed_div(FIXED_1, (ABS(normalize) <= FIXED_EPSILON ? FIXED_1 : normalize));
        red   = fixed_mul(red, normalize);
        green = fixed_mul(green, normalize);
        blue  = fixed_mul(blue, normalize);
      }
      else {
        for (i = 0; i < n; i++) {
          j = (int)((y * source->columns) + contribution[i].pixel);
          weight = contribution[i].weight;
          p = source->buf[j];
          
          /*
          DEBUG_TRACE("    merging with pix (%d, %d) @ %d (%d %d %d) weight %.2f\n",
            contribution[i].pixel, y, j,
            COL_RED(p), COL_GREEN(p), COL_BLUE(p),
            weight);
          */
          
          red   += fixed_mul(weight, int_to_fixed(COL_RED(p)));
          green += fixed_mul(weight, int_to_fixed(COL_GREEN(p)));
          blue  += fixed_mul(weight, int_to_fixed(COL_BLUE(p)));
        }
        
        alpha = FIXED_255;
      }
      
      /*
      DEBUG_TRACE("  -> (%d, %d) @ %d (%d %d %d %d)\n",
        x, y, (y * destination->columns) + x,
        ROUND_FIXED_TO_INT(red),
        ROUND_FIXED_TO_INT(green),
        ROUND_FIXED_TO_INT(blue),
        ROUND_FIXED_TO_INT(alpha));
      */
      
      if (rotate && im->orientation != ORIENTATION_NORMAL) {
        int ox, oy; // new destination pixel coordinates after rotating

        image_get_rotated_coords(im, x, y, &ox, &oy);

        if (im->orientation >= 5) {
          // 90 and 270 rotations, width/height are swapped
          destination->buf[(oy * destination->rows) + ox] = COL_FULL(
            ROUND_FIXED_TO_INT(red),
            ROUND_FIXED_TO_INT(green),
            ROUND_FIXED_TO_INT(blue),
            ROUND_FIXED_TO_INT(alpha)
          );
        }
        else {
          destination->buf[(oy * destination->columns) + ox] = COL_FULL(
            ROUND_FIXED_TO_INT(red),
            ROUND_FIXED_TO_INT(green),
            ROUND_FIXED_TO_INT(blue),
            ROUND_FIXED_TO_INT(alpha)
          );
        }
      }
      else { 
        destination->buf[(y * destination->columns) + x] = COL_FULL(
          ROUND_FIXED_TO_INT(red),
          ROUND_FIXED_TO_INT(green),
          ROUND_FIXED_TO_INT(blue),
          ROUND_FIXED_TO_INT(alpha)
        );
      }
    }
  }
}

static void
image_downsize_gm_vertical_filter_fixed_point(image *im, ImageInfo *source, ImageInfo *destination,
  const fixed_t y_factor, const FilterInfoFixed *filter_info, ContributionInfoFixed *contribution, int rotate)
{
  fixed_t scale, support;
  int y;
  int dstY = 0;
  int dstH = destination->rows;
  
  if (im->height_padding) {
    dstY = im->height_padding;
    dstH = im->height_inner;
  }
  
  //DEBUG_TRACE("y_factor %.2f\n", y_factor);
  
  // Apply filter to resize vertically from source to destination
  scale = MAX(fixed_div(FIXED_1, y_factor), FIXED_1);
  support = fixed_mul(scale, filter_info->support);
  if (support <= FIXED_HALF) {
    // Reduce to point sampling
    support = FIXED_HALF + FIXED_EPSILON;
    scale = FIXED_1;
  }
  scale = fixed_div(FIXED_1, scale);
  
  for (y = dstY; (y < dstY + dstH); y++) {
    fixed_t center, density;
    int n, start, stop, x;
    
    center  = fixed_div(int_to_fixed(y - dstY) + FIXED_HALF, y_factor);
    start   = fixed_to_int(MAX(center - support + FIXED_HALF, 0));
    stop    = fixed_to_int(MIN(center + support + FIXED_HALF, int_to_fixed(source->rows)));
    density = 0;
    
    //DEBUG_TRACE("y %d: center %.2f, start %d, stop %d\n", y, fixed_to_float(center), start, stop);
    
    for (n = 0; n < (stop - start); n++) {
      contribution[n].pixel = start + n;
      contribution[n].weight = filter_info->function(fixed_mul(scale, (int_to_fixed(start) + int_to_fixed(n) - center + FIXED_HALF)), filter_info->support);
      density += contribution[n].weight;
      //DEBUG_TRACE("  contribution[%d].pixel %d, weight %.2f, density %.2f\n", n, contribution[n].pixel, contribution[n].weight, density);
    }
    
    if ((density != 0) && (density != FIXED_1)) {
      // Normalize
      int i;
      
      density = fixed_div(FIXED_1, density);
      for (i = 0; i < n; i++) {
        contribution[i].weight = fixed_mul(contribution[i].weight, density);
        //DEBUG_TRACE("  normalize contribution[%d].weight to %.2f\n", i, fixed_to_float(contribution[i].weight));
      }
    }
    
    for (x = 0; x < destination->columns; x++) {
      fixed_t weight;
      fixed_t red = 0, green = 0, blue = 0, alpha = 0;
      pix p;
      int j;
      register int i;
      
      //DEBUG_TRACE("x %d:\n", x);
      
      if (im->has_alpha) {
        fixed_t normalize = 0;
        
        for (i = 0; i < n; i++) {          
          j = (int)((contribution[i].pixel * source->columns) + x);
          weight = contribution[i].weight;
          p = source->buf[j];
        
          // XXX The original GM code weighted based on transparency for some reason,
          // but this produces bad results, so we use only the weight
          //transparency_coeff = weight * ((float)COL_ALPHA(p) / 255);
          
          /*
          DEBUG_TRACE("    merging with pix (%d, %d) @ %d (%d %d %d %d) weight %.2f\n",
            x, contribution[i].pixel, j,
            COL_RED(p), COL_GREEN(p), COL_BLUE(p), COL_ALPHA(p),
            fixed_to_float(weight));
          */
          
          red   += fixed_mul(weight, int_to_fixed(COL_RED(p)));
          green += fixed_mul(weight, int_to_fixed(COL_GREEN(p)));
          blue  += fixed_mul(weight, int_to_fixed(COL_BLUE(p)));
          alpha += fixed_mul(weight, int_to_fixed(COL_ALPHA(p)));
          normalize += weight;
        }
      
        normalize = fixed_div(FIXED_1, (ABS(normalize) <= FIXED_EPSILON ? FIXED_1 : normalize));
        red   = fixed_mul(red, normalize);
        green = fixed_mul(green, normalize);
        blue  = fixed_mul(blue, normalize);
      }
      else {
        for (i = 0; i < n; i++) {
          j = (int)((contribution[i].pixel * source->columns) + x);
          weight = contribution[i].weight;
          p = source->buf[j];
        
          /*
          DEBUG_TRACE("    merging with pix (%d, %d) @ %d (%d %d %d) weight %.2f\n",
            x, contribution[i].pixel, j,
            COL_RED(p), COL_GREEN(p), COL_BLUE(p),
            fixed_to_float(weight));
          */
          
          red   += fixed_mul(weight, int_to_fixed(COL_RED(p)));
          green += fixed_mul(weight, int_to_fixed(COL_GREEN(p)));
          blue  += fixed_mul(weight, int_to_fixed(COL_BLUE(p)));
        }
        
        alpha = FIXED_255;
      }
      
      /*
      DEBUG_TRACE("  -> (%d, %d) @ %d (%d %d %d %d)\n",
        x, y, (y * destination->columns) + x,
        ROUND_FIXED_TO_INT(red),
        ROUND_FIXED_TO_INT(green),
        ROUND_FIXED_TO_INT(blue),
        ROUND_FIXED_TO_INT(alpha));
      */
      
      if (rotate && im->orientation != ORIENTATION_NORMAL) {
        int ox, oy; // new destination pixel coordinates after rotating

        image_get_rotated_coords(im, x, y, &ox, &oy);

        if (im->orientation >= 5) {
          // 90 and 270 rotations, width/height are swapped
          destination->buf[(oy * destination->rows) + ox] = COL_FULL(
            ROUND_FIXED_TO_INT(red),
            ROUND_FIXED_TO_INT(green),
            ROUND_FIXED_TO_INT(blue),
            ROUND_FIXED_TO_INT(alpha)
          );
        }
        else {
          destination->buf[(oy * destination->columns) + ox] = COL_FULL(
            ROUND_FIXED_TO_INT(red),
            ROUND_FIXED_TO_INT(green),
            ROUND_FIXED_TO_INT(blue),
            ROUND_FIXED_TO_INT(alpha)
          );
        }
      }
      else { 
        destination->buf[(y * destination->columns) + x] = COL_FULL(
          ROUND_FIXED_TO_INT(red),
          ROUND_FIXED_TO_INT(green),
          ROUND_FIXED_TO_INT(blue),
          ROUND_FIXED_TO_INT(alpha)
        );
      }
    }
  }
}

void
image_downsize_gm_fixed_point(image *im)
{
  // This intentionally still uses floating-point because these variables are only calculated once
  float x_factor, y_factor;
  float support, x_support, y_support;
  int columns, rows;
  int order;
  int filter;
  ContributionInfoFixed *contribution;
  ImageInfo source, destination;
  
  static const FilterInfoFixed
    filters[SincFilter+1] =
    {
      { BoxFixed, 0 },
      { BoxFixed, 0 },
      { BoxFixed, FIXED_HALF },
      { TriangleFixed, FIXED_1 },
      // Triangle seems good enough
    };
  
  columns = im->target_width;
  rows = im->target_height;
  filter = TriangleFilter; // Force Triangle for fixed-point mode, best quality and performance
  
  DEBUG_TRACE("Resizing with filter %d\n", filter);
  
  // Determine which dimension to resize first
  order = (((float)columns * (im->height + rows)) >
         ((float)rows * (im->width + columns)));
  
   if (im->width_padding)
     x_factor = (float)im->width_inner / im->width;
   else
     x_factor = (float)im->target_width / im->width;

   if (im->height_padding)
     y_factor = (float)im->height_inner / im->height;
   else
     y_factor = (float)im->target_height / im->height;
  
  x_support = BLUR * MAX(1.0 / x_factor, 1.0) * fixed_to_int(filters[filter].support);
  y_support = BLUR * MAX(1.0 / y_factor, 1.0) * fixed_to_int(filters[filter].support);
  support = MAX(x_support, y_support);
  if (support < fixed_to_int(filters[filter].support))
    support = fixed_to_int(filters[filter].support);
  
  DEBUG_TRACE("ContributionInfoFixed allocated for %ld items\n", (size_t)(2.0 * MAX(support, 0.5) + 3));
  New(0, contribution, (size_t)(2.0 * MAX(support, 0.5) + 3), ContributionInfoFixed);
  
  DEBUG_TRACE("order %d, x_factor %f, y_factor %f, support %f\n", order, x_factor, y_factor, support);
  
  source.rows    = im->height;
  source.columns = im->width;
  source.buf     = im->pixbuf;
  
  if (order) {
    DEBUG_TRACE("Allocating temporary buffer size %ld\n", im->target_width * im->height * sizeof(pix));
    New(0, im->tmpbuf, im->target_width * im->height, pix);
    
    // Fill new space with the bgcolor or zeros
    image_bgcolor_fill(im->tmpbuf, im->target_width * im->height, im->bgcolor);
    
    // Resize horizontally from source -> tmp
    destination.rows    = im->height;
    destination.columns = im->target_width;
    destination.buf     = im->tmpbuf;
    image_downsize_gm_horizontal_filter_fixed_point(im, &source, &destination, float_to_fixed(x_factor), &filters[filter], contribution, 0);
    
    // Resize vertically from tmp -> out
    source.rows    = destination.rows;
    source.columns = destination.columns;
    source.buf     = destination.buf;
    
    destination.rows = im->target_height;
    destination.buf  = im->outbuf;
    image_downsize_gm_vertical_filter_fixed_point(im, &source, &destination, float_to_fixed(y_factor), &filters[filter], contribution, 1);
  }
  else {
    DEBUG_TRACE("Allocating temporary buffer size %ld\n", im->width * im->target_height * sizeof(pix));
    New(0, im->tmpbuf, im->width * im->target_height, pix);
    
    // Fill new space with the bgcolor or zeros
    image_bgcolor_fill(im->tmpbuf, im->width * im->target_height, im->bgcolor);
    
    // Resize vertically from source -> tmp
    destination.rows    = im->target_height;
    destination.columns = im->width;
    destination.buf     = im->tmpbuf;
    image_downsize_gm_vertical_filter_fixed_point(im, &source, &destination, float_to_fixed(y_factor), &filters[filter], contribution, 0);

    // Resize horizontally from tmp -> out
    source.rows    = destination.rows;
    source.columns = destination.columns;
    source.buf     = destination.buf;
    
    destination.columns = im->target_width;
    destination.buf     = im->outbuf;
    image_downsize_gm_horizontal_filter_fixed_point(im, &source, &destination, float_to_fixed(x_factor), &filters[filter], contribution, 1);
  }
  
  Safefree(im->tmpbuf);
  Safefree(contribution);
}
