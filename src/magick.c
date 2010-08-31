/*
This file contains code from the GraphicsMagick software distributed by the
GraphicsMagick Group.

image_downsize_gm is based on GraphicsMagick's ResizeImage function.

The licenses which components of this software fall under are as follows.

1)
  In November 2002, the GraphicsMagick Group created GraphicsMagick
  from ImageMagick Studio's ImageMagick and applied the "MIT" style
  license:

  Copyright (C) 2002 - 2010 GraphicsMagick Group

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

2)
  In October 1999, ImageMagick Studio assumed the responsibility for
  the development of ImageMagick (forking from the distribution by
  E. I. du Pont de Nemours and Company) and applied a new license:

  Copyright (C) 2002 ImageMagick Studio, a non-profit organization dedicated
  to making software imaging solutions freely available.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files ("ImageMagick"),
  to deal in ImageMagick without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of ImageMagick, and to permit persons to whom the
  ImageMagick is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of ImageMagick.

  The software is provided "as is", without warranty of any kind, express or
  implied, including but not limited to the warranties of merchantability,
  fitness for a particular purpose and noninfringement.  In no event shall
  ImageMagick Studio be liable for any claim, damages or other liability,
  whether in an action of contract, tort or otherwise, arising from, out of
  or in connection with ImageMagick or the use or other dealings in
  ImageMagick.

  Except as contained in this notice, the name of the ImageMagick Studio
  shall not be used in advertising or otherwise to promote the sale, use or
  other dealings in ImageMagick without prior written authorization from the
  ImageMagick Studio.

3)
  From 1991 to October 1999 (through ImageMagick 4.2.9), ImageMagick
  was developed and distributed by E. I. du Pont de Nemours and
  Company:

  Copyright 1999 E. I. du Pont de Nemours and Company

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files
  ("ImageMagick"), to deal in ImageMagick without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of ImageMagick, and to
  permit persons to whom the ImageMagick is furnished to do so, subject
  to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of ImageMagick.

  The software is provided "as is", without warranty of any kind, express
  or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose and noninfringement.
  In no event shall E. I. du Pont de Nemours and Company be liable for
  any claim, damages or other liability, whether in an action of
  contract, tort or otherwise, arising from, out of or in connection with
  ImageMagick or the use or other dealings in ImageMagick.

  Except as contained in this notice, the name of the E. I. du Pont de
  Nemours and Company shall not be used in advertising or otherwise to
  promote the sale, use or other dealings in ImageMagick without prior
  written authorization from the E. I. du Pont de Nemours and Company.

---------------------------------------------------------------------------

| Copyright (C) 2002 - 2010 GraphicsMagick Group
*/

#include "magick.h"

static double J1(double x)
{
  double
    p,
    q;

  register long
    i;

  static const double
    Pone[] =
    {
       0.581199354001606143928050809e+21,
      -0.6672106568924916298020941484e+20,
       0.2316433580634002297931815435e+19,
      -0.3588817569910106050743641413e+17,
       0.2908795263834775409737601689e+15,
      -0.1322983480332126453125473247e+13,
       0.3413234182301700539091292655e+10,
      -0.4695753530642995859767162166e+7,
       0.270112271089232341485679099e+4
    },
    Qone[] =
    {
      0.11623987080032122878585294e+22,
      0.1185770712190320999837113348e+20,
      0.6092061398917521746105196863e+17,
      0.2081661221307607351240184229e+15,
      0.5243710262167649715406728642e+12,
      0.1013863514358673989967045588e+10,
      0.1501793594998585505921097578e+7,
      0.1606931573481487801970916749e+4,
      0.1e+1
    };

  p=Pone[8];
  q=Qone[8];
  for (i=7; i >= 0; i--)
  {
    p=p*x*x+Pone[i];
    q=q*x*x+Qone[i];
  }
  return(p/q);
}

static double P1(double x)
{
  double
    p,
    q;

  register long
    i;

  static const double
    Pone[] =
    {
      0.352246649133679798341724373e+5,
      0.62758845247161281269005675e+5,
      0.313539631109159574238669888e+5,
      0.49854832060594338434500455e+4,
      0.2111529182853962382105718e+3,
      0.12571716929145341558495e+1
    },
    Qone[] =
    {
      0.352246649133679798068390431e+5,
      0.626943469593560511888833731e+5,
      0.312404063819041039923015703e+5,
      0.4930396490181088979386097e+4,
      0.2030775189134759322293574e+3,
      0.1e+1
    };

  p=Pone[5];
  q=Qone[5];
  for (i=4; i >= 0; i--)
  {
    p=p*(8.0/x)*(8.0/x)+Pone[i];
    q=q*(8.0/x)*(8.0/x)+Qone[i];
  }
  return(p/q);
}

static double Q1(double x)
{
  double
    p,
    q;

  register long
    i;

  static const double
    Pone[] =
    {
      0.3511751914303552822533318e+3,
      0.7210391804904475039280863e+3,
      0.4259873011654442389886993e+3,
      0.831898957673850827325226e+2,
      0.45681716295512267064405e+1,
      0.3532840052740123642735e-1
    },
    Qone[] =
    {
      0.74917374171809127714519505e+4,
      0.154141773392650970499848051e+5,
      0.91522317015169922705904727e+4,
      0.18111867005523513506724158e+4,
      0.1038187585462133728776636e+3,
      0.1e+1
    };

  p=Pone[5];
  q=Qone[5];
  for (i=4; i >= 0; i--)
  {
    p=p*(8.0/x)*(8.0/x)+Pone[i];
    q=q*(8.0/x)*(8.0/x)+Qone[i];
  }
  return(p/q);
}

static double BesselOrderOne(double x)
{
  double
    p,
    q;

  if (x == 0.0)
    return(0.0);
  p=x;
  if (x < 0.0)
    x=(-x);
  if (x < 8.0)
    return(p*J1(x));
  q=sqrt(2.0/(PI*x))*(P1(x)*(1.0/sqrt(2.0)*(sin(x)-cos(x)))-8.0/x*Q1(x)*
    (-1.0/sqrt(2.0)*(sin(x)+cos(x))));
  if (p < 0.0)
    q=(-q);
  return(q);
}

static float Bessel(const float x,const float ARGUNUSED(support))
{
  if (x == 0.0)
    return(PI/4.0);
  return(BesselOrderOne(PI*x)/(2.0*x));
}

static float Sinc(const float x,const float ARGUNUSED(support))
{
  if (x == 0.0)
    return(1.0);
  return(sin(PI*x)/(PI*x));
}

static float Blackman(const float x,const float ARGUNUSED(support))
{
  return(0.42+0.5*cos(PI*x)+0.08*cos(2*PI*x));
}

static float BlackmanBessel(const float x,const float support)
{
  return(Blackman(x/support,support)*Bessel(x,support));
}

static float BlackmanSinc(const float x,const float support)
{
  return(Blackman(x/support,support)*Sinc(x,support));
}

static float Box(const float x,const float ARGUNUSED(support))
{
  if (x < -0.5)
    return(0.0);
  if (x < 0.5)
    return(1.0);
  return(0.0);
}

static float Catrom(const float x,const float ARGUNUSED(support))
{
  if (x < -2.0)
    return(0.0);
  if (x < -1.0)
    return(0.5*(4.0+x*(8.0+x*(5.0+x))));
  if (x < 0.0)
    return(0.5*(2.0+x*x*(-5.0-3.0*x)));
  if (x < 1.0)
    return(0.5*(2.0+x*x*(-5.0+3.0*x)));
  if (x < 2.0)
    return(0.5*(4.0+x*(-8.0+x*(5.0-x))));
  return(0.0);
}

static float Cubic(const float x,const float ARGUNUSED(support))
{
  if (x < -2.0)
    return(0.0);
  if (x < -1.0)
    return((2.0+x)*(2.0+x)*(2.0+x)/6.0);
  if (x < 0.0)
    return((4.0+x*x*(-6.0-3.0*x))/6.0);
  if (x < 1.0)
    return((4.0+x*x*(-6.0+3.0*x))/6.0);
  if (x < 2.0)
    return((2.0-x)*(2.0-x)*(2.0-x)/6.0);
  return(0.0);
}

static float Gaussian(const float x,const float ARGUNUSED(support))
{
  return(exp(-2.0*x*x)*sqrt(2.0/PI));
}

static float Hanning(const float x,const float ARGUNUSED(support))
{
  return(0.5+0.5*cos(PI*x));
}

static float Hamming(const float x,const float ARGUNUSED(support))
{
  return(0.54+0.46*cos(PI*x));
}

static float Hermite(const float x,const float ARGUNUSED(support))
{
  if (x < -1.0)
    return(0.0);
  if (x < 0.0)
    return((2.0*(-x)-3.0)*(-x)*(-x)+1.0);
  if (x < 1.0)
    return((2.0*x-3.0)*x*x+1.0);
  return(0.0);
}

static float Lanczos(const float x,const float support)
{
  if (x < -3.0)
    return(0.0);
  if (x < 0.0)
    return(Sinc(-x,support)*Sinc(-x/3.0,support));
  if (x < 3.0)
    return(Sinc(x,support)*Sinc(x/3.0,support));
  return(0.0);
}

static float Mitchell(const float x,const float ARGUNUSED(support))
{
#define B   (1.0/3.0)
#define C   (1.0/3.0)
#define P0  ((  6.0- 2.0*B       )/6.0)
#define P2  ((-18.0+12.0*B+ 6.0*C)/6.0)
#define P3  (( 12.0- 9.0*B- 6.0*C)/6.0)
#define Q0  ((       8.0*B+24.0*C)/6.0)
#define Q1  ((     -12.0*B-48.0*C)/6.0)
#define Q2  ((       6.0*B+30.0*C)/6.0)
#define Q3  ((     - 1.0*B- 6.0*C)/6.0)

  if (x < -2.0)
    return(0.0);
  if (x < -1.0)
    return(Q0-x*(Q1-x*(Q2-x*Q3)));
  if (x < 0.0)
    return(P0+x*x*(P2-x*P3));
  if (x < 1.0)
    return(P0+x*x*(P2+x*P3));
  if (x < 2.0)
    return(Q0+x*(Q1+x*(Q2+x*Q3)));
  return(0.0);
}

static float Quadratic(const float x,const float ARGUNUSED(support))
{
  if (x < -1.5)
    return(0.0);
  if (x < -0.5)
    return(0.5*(x+1.5)*(x+1.5));
  if (x < 0.5)
    return(0.75-x*x);
  if (x < 1.5)
    return(0.5*(x-1.5)*(x-1.5));
  return(0.0);
}

static float Triangle(const float x,const float ARGUNUSED(support))
{
  if (x < -1.0)
    return(0.0);
  if (x < 0.0)
    return(1.0+x);
  if (x < 1.0)
    return(1.0-x);
  return(0.0);
}

static void
image_downsize_gm_horizontal_filter(image *im, ImageInfo *source, ImageInfo *destination,
  const float x_factor, const FilterInfo *filter_info, ContributionInfo *contribution, int rotate)
{
  float scale, support;
  int x;
  int dstX = 0;
  int dstW = destination->columns;
  
  if (im->width_padding) {
    dstX = im->width_padding;
    dstW = im->width_inner;
  }
  
  scale = BLUR * MAX(1.0 / x_factor, 1.0);
  support = scale * filter_info->support;
  if (support <= 0.5) {
    // Reduce to point sampling
    support = 0.5 + EPSILON;
    scale = 1.0;
  }
  scale = 1.0 / scale;
  
  for (x = dstX; (x < dstX + dstW); x++) {
    float center, density;
    int n, start, stop, y;
    
    center  = (float)(x - dstX + 0.5) / x_factor;
    start   = (int)MAX(center - support + 0.5, 0);
    stop    = (int)MIN(center + support + 0.5, source->columns);
    density = 0.0;
    
    //DEBUG_TRACE("x %d: center %.2f, start %d, stop %d\n", x, center, start, stop);
    
    for (n = 0; n < (stop - start); n++) {
      contribution[n].pixel = start + n;
      contribution[n].weight = filter_info->function(scale * (start + n - center + 0.5), filter_info->support);
      density += contribution[n].weight;
      //DEBUG_TRACE("  contribution[%d].pixel %d, weight %.2f, density %.2f\n", n, contribution[n].pixel, contribution[n].weight, density);
    }
    
    if ((density != 0.0) && (density != 1.0)) {
      // Normalize
      int i;
      
      density = 1.0 / density;
      for (i = 0; i < n; i++) {
        contribution[i].weight *= density;
        //DEBUG_TRACE("  normalize contribution[%d].weight to %.2f\n", i, contribution[i].weight);
      }
    }
    
    for (y = 0; y < destination->rows; y++) {
      float weight;
      float red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
      pix p;
      int j;
      register int i;
      
      //DEBUG_TRACE("y %d:\n", y);
      
      if (im->has_alpha) {
        float normalize;
      
        normalize = 0.0;
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
            weight);
          */
          
          red   += weight * COL_RED(p);
          green += weight * COL_GREEN(p);
          blue  += weight * COL_BLUE(p);
          alpha += weight * COL_ALPHA(p);
          normalize += weight;
        }
      
        normalize = 1.0 / (ABS(normalize) <= EPSILON ? 1.0 : normalize);
        red   *= normalize;
        green *= normalize;
        blue  *= normalize;
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
          
          red   += weight * COL_RED(p);
          green += weight * COL_GREEN(p);
          blue  += weight * COL_BLUE(p);
        }
        
        alpha = 255.0;
      }
      
      /*
      DEBUG_TRACE("  -> (%d, %d) @ %d (%d %d %d %d)\n",
        x, y, (y * destination->columns) + x,
        ROUND_FLOAT_TO_INT(red),
        ROUND_FLOAT_TO_INT(green),
        ROUND_FLOAT_TO_INT(blue),
        ROUND_FLOAT_TO_INT(alpha));
      */
      
      if (rotate && im->orientation != ORIENTATION_NORMAL) {
        int ox, oy; // new destination pixel coordinates after rotating

        image_get_rotated_coords(im, x, y, &ox, &oy);

        if (im->orientation >= 5) {
          // 90 and 270 rotations, width/height are swapped
          destination->buf[(oy * destination->rows) + ox] = COL_FULL(
            ROUND_FLOAT_TO_INT(red),
            ROUND_FLOAT_TO_INT(green),
            ROUND_FLOAT_TO_INT(blue),
            ROUND_FLOAT_TO_INT(alpha)
          );
        }
        else {
          destination->buf[(oy * destination->columns) + ox] = COL_FULL(
            ROUND_FLOAT_TO_INT(red),
            ROUND_FLOAT_TO_INT(green),
            ROUND_FLOAT_TO_INT(blue),
            ROUND_FLOAT_TO_INT(alpha)
          );
        }
      }
      else { 
        destination->buf[(y * destination->columns) + x] = COL_FULL(
          ROUND_FLOAT_TO_INT(red),
          ROUND_FLOAT_TO_INT(green),
          ROUND_FLOAT_TO_INT(blue),
          ROUND_FLOAT_TO_INT(alpha)
        );
      }
    }
  }
}

static void
image_downsize_gm_vertical_filter(image *im, ImageInfo *source, ImageInfo *destination,
  const float y_factor, const FilterInfo *filter_info, ContributionInfo *contribution, int rotate)
{
  float scale, support;
  int y;
  int dstY = 0;
  int dstH = destination->rows;
  
  if (im->height_padding) {
    dstY = im->height_padding;
    dstH = im->height_inner;
  }
  
  //DEBUG_TRACE("y_factor %.2f\n", y_factor);
  
  // Apply filter to resize vertically from source to destination
  scale = BLUR * MAX(1.0 / y_factor, 1.0);
  support = scale * filter_info->support;
  if (support <= 0.5) {
    // Reduce to point sampling
    support = 0.5 + EPSILON;
    scale = 1.0;
  }
  scale = 1.0 / scale;
  
  for (y = dstY; (y < dstY + dstH); y++) {
    float center, density;
    int n, start, stop, x;
    
    center  = (float)(y - dstY + 0.5) / y_factor;
    start   = (int)MAX(center - support + 0.5, 0);
    stop    = (int)MIN(center + support + 0.5, source->rows);
    density = 0.0;
    
    //DEBUG_TRACE("y %d: center %.2f, start %d, stop %d\n", y, center, start, stop);
    
    for (n = 0; n < (stop - start); n++) {
      contribution[n].pixel = start + n;
      contribution[n].weight = filter_info->function(scale * (start + n - center + 0.5), filter_info->support);
      density += contribution[n].weight;
      //DEBUG_TRACE("  contribution[%d].pixel %d, weight %.2f, density %.2f\n", n, contribution[n].pixel, contribution[n].weight, density);
    }
    
    if ((density != 0.0) && (density != 1.0)) {
      // Normalize
      int i;
      
      density = 1.0 / density;
      for (i = 0; i < n; i++) {
        contribution[i].weight *= density;
        //DEBUG_TRACE("  normalize contribution[%d].weight to %.2f\n", i, contribution[i].weight);
      }
    }
    
    for (x = 0; x < destination->columns; x++) {
      float weight;
      float red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
      pix p;
      int j;
      register int i;
      
      //DEBUG_TRACE("x %d:\n", x);
      
      if (im->has_alpha) {
        float normalize;
      
        normalize = 0.0;
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
            weight
          );
          */
          
          red   += weight * COL_RED(p);
          green += weight * COL_GREEN(p);
          blue  += weight * COL_BLUE(p);
          alpha += weight * COL_ALPHA(p);
          normalize += weight;
        }
      
        normalize = 1.0 / (ABS(normalize) <= EPSILON ? 1.0 : normalize);
        red   *= normalize;
        green *= normalize;
        blue  *= normalize;
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
            weight);
          */
          
          red   += weight * COL_RED(p);
          green += weight * COL_GREEN(p);
          blue  += weight * COL_BLUE(p);
        }
        
        alpha = 255.0;
      }
      
      /*
      DEBUG_TRACE("  -> (%d, %d) @ %d (%d %d %d %d)\n",
        x, y, (y * destination->columns) + x,
        ROUND_FLOAT_TO_INT(red),
        ROUND_FLOAT_TO_INT(green),
        ROUND_FLOAT_TO_INT(blue),
        ROUND_FLOAT_TO_INT(alpha));
      */
      
      if (rotate && im->orientation != ORIENTATION_NORMAL) {
        int ox, oy; // new destination pixel coordinates after rotating

        image_get_rotated_coords(im, x, y, &ox, &oy);

        if (im->orientation >= 5) {
          // 90 and 270 rotations, width/height are swapped
          destination->buf[(oy * destination->rows) + ox] = COL_FULL(
            ROUND_FLOAT_TO_INT(red),
            ROUND_FLOAT_TO_INT(green),
            ROUND_FLOAT_TO_INT(blue),
            ROUND_FLOAT_TO_INT(alpha)
          );
        }
        else {
          destination->buf[(oy * destination->columns) + ox] = COL_FULL(
            ROUND_FLOAT_TO_INT(red),
            ROUND_FLOAT_TO_INT(green),
            ROUND_FLOAT_TO_INT(blue),
            ROUND_FLOAT_TO_INT(alpha)
          );
        }
      }
      else {
        destination->buf[(y * destination->columns) + x] = COL_FULL(
          ROUND_FLOAT_TO_INT(red),
          ROUND_FLOAT_TO_INT(green),
          ROUND_FLOAT_TO_INT(blue),
          ROUND_FLOAT_TO_INT(alpha)
        );
      }
    }
  }
}

void
image_downsize_gm(image *im)
{
  float x_factor, y_factor;
  float support, x_support, y_support;
  int columns, rows;
  int order;
  int filter;
  ContributionInfo *contribution;
  ImageInfo source, destination;
  
  static const FilterInfo
    filters[SincFilter+1] =
    {
      { Box, 0.0 },
      { Box, 0.0 },
      { Box, 0.5 },
      { Triangle, 1.0 },
      { Hermite, 1.0 },
      { Hanning, 1.0 },
      { Hamming, 1.0 },
      { Blackman, 1.0 },
      { Gaussian, 1.25 },
      { Quadratic, 1.5 },
      { Cubic, 2.0 },
      { Catrom, 2.0 },
      { Mitchell, 2.0 },
      { Lanczos, 3.0 },
      { BlackmanBessel, 3.2383 },
      { BlackmanSinc, 4.0 }
    };
  
  columns = im->target_width;
  rows = im->target_height;
  filter = im->filter;

  if (!filter) {
    // Lanczos for downsizing, Mitchell for upsizing or if transparent
    if (im->has_alpha || columns > im->width || rows > im->height)
      filter = MitchellFilter;
    else
      filter = LanczosFilter;
  }
  
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
  
  x_support = BLUR * MAX(1.0 / x_factor, 1.0) * filters[filter].support;
  y_support = BLUR * MAX(1.0 / y_factor, 1.0) * filters[filter].support;
  support = MAX(x_support, y_support);
  if (support < filters[filter].support)
    support = filters[filter].support;
  
  DEBUG_TRACE("ContributionInfo allocated for %ld items\n", (size_t)(2.0 * MAX(support, 0.5) + 3));
  New(0, contribution, (size_t)(2.0 * MAX(support, 0.5) + 3), ContributionInfo);
  
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
    image_downsize_gm_horizontal_filter(im, &source, &destination, x_factor, &filters[filter], contribution, 0);
    
    // Resize vertically from tmp -> out
    source.rows    = destination.rows;
    source.columns = destination.columns;
    source.buf     = destination.buf;
    
    destination.rows = im->target_height;
    destination.buf  = im->outbuf;
    image_downsize_gm_vertical_filter(im, &source, &destination, y_factor, &filters[filter], contribution, 1);    
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
    image_downsize_gm_vertical_filter(im, &source, &destination, y_factor, &filters[filter], contribution, 0);

    // Resize horizontally from tmp -> out
    source.rows    = destination.rows;
    source.columns = destination.columns;
    source.buf     = destination.buf;
    
    destination.columns = im->target_width;
    destination.buf     = im->outbuf;
    image_downsize_gm_horizontal_filter(im, &source, &destination, x_factor, &filters[filter], contribution, 1);
  }
  
  Safefree(im->tmpbuf);
  Safefree(contribution);
}
