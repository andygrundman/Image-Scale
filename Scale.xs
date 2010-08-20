// On Debian, pngconf.h might complain about setjmp.h being loaded before PNG
// so we have to load png.h first
#include <png.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "common.c"
#include "image.c"

MODULE = Image::Scale		PACKAGE = Image::Scale

void
__init(HV *self)
PPCODE:
{
  SV *pv = NEWSV(0, sizeof(image));
  image *im = (image *)SvPVX(pv);
  
  SvPOK_only(pv);
  
  image_init(self, im);
  
  XPUSHs( sv_2mortal( sv_bless(
    newRV_noinc(pv),
    gv_stashpv("Image::Scale::XS", 1)
  ) ) );
}

void
resize(HV *self, HV *opts)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));
  
  if (my_hv_exists(opts, "width"))
    im->target_width = SvIV(*(my_hv_fetch(opts, "width")));
  
  if (my_hv_exists(opts, "height"))
    im->target_height = SvIV(*(my_hv_fetch(opts, "height")));
  
  if (!im->target_width && !im->target_height) {
    croak("Image::Scale->resize requires at least one of height or width");
  }
  
  if (!im->target_height) {
    // Only width was specified
    im->target_height = (int)((float)im->height / im->width * im->target_width);
  }
  else if (!im->target_width) {
    // Only height was specified
    im->target_width = (int)((float)im->width / im->height * im->target_height);
  }
  
  DEBUG_TRACE("Resizing from %d x %d -> %d x %d\n", im->width, im->height, im->target_width, im->target_height);
  
  if (my_hv_exists(opts, "keep_aspect"))
    im->keep_aspect = SvIV(*(my_hv_fetch(opts, "keep_aspect")));
  
  if (my_hv_exists(opts, "ignore_exif")) {
    if (SvIV(*(my_hv_fetch(opts, "ignore_exif"))) != 0)
      im->orientation = ORIENTATION_NORMAL;
  }
  
  if (my_hv_exists(opts, "memory_limit"))
    im->memory_limit = SvIV(*(my_hv_fetch(opts, "memory_limit")));
  
  if (my_hv_exists(opts, "type"))
    im->resize_type = SvIV(*(my_hv_fetch(opts, "type")));
  
  if (my_hv_exists(opts, "filter")) {
    char *filterstr = SvPVX(*(my_hv_fetch(opts, "filter")));
    if (strEQ("Point", filterstr))
      im->filter = PointFilter;
    else if (strEQ("Box", filterstr))
      im->filter = BoxFilter;
    else if (strEQ("Triangle", filterstr))
      im->filter = TriangleFilter;
    else if (strEQ("Hermite", filterstr))
      im->filter = HermiteFilter;
    else if (strEQ("Hanning", filterstr))
      im->filter = HanningFilter;
    else if (strEQ("Hamming", filterstr))
      im->filter = HammingFilter;
    else if (strEQ("Blackman", filterstr))
      im->filter = BlackmanFilter;
    else if (strEQ("Gaussian", filterstr))
      im->filter = GaussianFilter;
    else if (strEQ("Quadratic", filterstr))
      im->filter = QuadraticFilter;
    else if (strEQ("Cubic", filterstr))
      im->filter = CubicFilter;
    else if (strEQ("Catrom", filterstr))
      im->filter = CatromFilter;
    else if (strEQ("Mitchell", filterstr))
      im->filter = MitchellFilter;
    else if (strEQ("Lanczos", filterstr))
      im->filter = LanczosFilter;
    else if (strEQ("Bessel", filterstr))
      im->filter = BesselFilter;
    else if (strEQ("Sinc", filterstr))
      im->filter = SincFilter;
  }
  
  image_resize(im);
}

void
save_jpeg(HV *self, SV *path, ...)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));
  int quality = DEFAULT_JPEG_QUALITY;
  
  if ( !SvPOK(path) ) {
    croak("Image::Scale->save_jpeg requires a path");
  }
  
  if (items == 3 && SvOK(ST(2))) {
    quality = SvIV(ST(2));
  }
  
  image_jpeg_save(im, SvPVX(path), quality);
}

SV *
as_jpeg(HV *self, ...)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));
  int quality = DEFAULT_JPEG_QUALITY;
  
  if (items == 2 && SvOK(ST(1))) {
    quality = SvIV(ST(1));
  }
  
  RETVAL = newSVpvn("", 0);
  
  image_jpeg_to_sv(im, quality, RETVAL);
}
OUTPUT:
  RETVAL

void
save_png(HV *self, SV *path)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));
  
  if ( !SvPOK(path) ) {
    croak("Image::Scale->save_jpeg requires a path");
  }
  
  image_png_save(im, SvPVX(path));
}

SV *
as_png(HV *self)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));

  RETVAL = newSVpvn("", 0);
  
  image_png_to_sv(im, RETVAL);
}
OUTPUT:
  RETVAL

void
__cleanup(HV *self, image *im)
CODE:
{
  image_finish(im);
}
