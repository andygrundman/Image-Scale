#ifdef _MSC_VER
#define NO_XSLOCKS // Needed to avoid PerlProc_setjmp/PerlProc_longjmp unresolved symbols
#endif

// On Debian, pngconf.h might complain about setjmp.h being loaded before PNG
// so we have to load png.h first
#ifdef HAVE_PNG
#include <png.h>
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "common.c"
#include "image.c"

MODULE = Image::Scale		PACKAGE = Image::Scale

PROTOTYPES: ENABLE

void
__init(HV *self)
PPCODE:
{
  SV *pv = NEWSV(0, sizeof(image));
  image *im = (image *)SvPVX(pv);

  SvPOK_only(pv);

  if ( !image_init(self, im) ) {
    // Return undef on any errors during header reading
    SvREFCNT_dec(pv);
    XSRETURN_UNDEF;
  }

  XPUSHs( sv_2mortal( sv_bless(
    newRV_noinc(pv),
    gv_stashpv("Image::Scale::XS", 1)
  ) ) );
}

int
width(HV *self)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));

  RETVAL = im->width;
}
OUTPUT:
  RETVAL

int
height(HV *self)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));

  RETVAL = im->height;
}
OUTPUT:
  RETVAL

int
resized_width(HV *self)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));

  RETVAL = im->target_width;
}
OUTPUT:
  RETVAL

int
resized_height(HV *self)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));

  RETVAL = im->target_height;
}
OUTPUT:
  RETVAL

int
resize(HV *self, HV *opts)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));

  // Reset options if resize is being called multiple times
  if (im->target_width) {
    im->target_width  = 0;
    im->target_height = 0;
    im->keep_aspect   = 0;
    im->orientation   = im->orientation_orig;
    im->bgcolor       = 0;
    im->memory_limit  = 0;
    im->resize_type   = IMAGE_SCALE_TYPE_GD;
    im->filter        = 0;
  }

  if (my_hv_exists(opts, "width"))
    im->target_width = SvIV(*(my_hv_fetch(opts, "width")));

  if (my_hv_exists(opts, "height"))
    im->target_height = SvIV(*(my_hv_fetch(opts, "height")));

  if (!im->target_width && !im->target_height) {
    croak("Image::Scale->resize requires at least one of height or width");
  }

  if (my_hv_exists(opts, "keep_aspect"))
    im->keep_aspect = SvIV(*(my_hv_fetch(opts, "keep_aspect")));

  if (my_hv_exists(opts, "ignore_exif")) {
    if (SvIV(*(my_hv_fetch(opts, "ignore_exif"))) != 0)
      im->orientation = ORIENTATION_NORMAL;
  }

  if (my_hv_exists(opts, "bgcolor"))
    im->bgcolor = SvIV(*(my_hv_fetch(opts, "bgcolor"))) << 8 | 0xFF;

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

  // If the image will be rotated 90 degrees, swap the target values
  if (im->orientation >= 5) {
    if (!im->target_height) {
      // Only width was specified, but this will actually be the target height
      im->target_height = im->target_width;
      im->target_width = 0;
    }
    else if (!im->target_width) {
      // Only height was specified, but this will actually be the target width
      im->target_width = im->target_height;
      im->target_height = 0;
    }
  }

  if (!im->target_height) {
    // Only width was specified
    im->target_height = (int)((float)im->height / im->width * im->target_width);
    if (im->target_height < 1)
      im->target_height = 1;
  }
  else if (!im->target_width) {
    // Only height was specified
    im->target_width = (int)((float)im->width / im->height * im->target_height);
    if (im->target_width < 1)
      im->target_width = 1;
  }

  DEBUG_TRACE("Resizing from %d x %d -> %d x %d\n", im->width, im->height, im->target_width, im->target_height);

  RETVAL = image_resize(im);
}
OUTPUT:
  RETVAL

#ifdef HAVE_JPEG
void
save_jpeg(HV *self, SV *path, ...)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));

  int quality = DEFAULT_JPEG_QUALITY;

  if (items == 3 && SvOK(ST(2))) {
    quality = SvIV(ST(2));
  }

  image_jpeg_save(im, SvPV_nolen(path), quality);
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

#endif

#ifdef HAVE_PNG
void
save_png(HV *self, SV *path)
CODE:
{
  image *im = (image *)SvPVX(SvRV(*(my_hv_fetch(self, "_image"))));

  image_png_save(im, SvPV_nolen(path));
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

#endif

void
__cleanup(HV *self, image *im)
CODE:
{
  image_finish(im);
}

SV *
jpeg_version(void)
CODE:
{
#ifdef JPEG_VERSION
  RETVAL = newSVpv( STRINGIFY(JPEG_VERSION), 0 );
#else
  RETVAL = &PL_sv_undef;
#endif
}
OUTPUT:
  RETVAL

SV *
png_version(void)
CODE:
{
#ifdef PNG_VERSION
  RETVAL = newSVpv( STRINGIFY(PNG_VERSION), 0 );
#else
  RETVAL = &PL_sv_undef;
#endif
}
OUTPUT:
  RETVAL

SV *
gif_version(void)
CODE:
{
#ifdef GIF_VERSION
  RETVAL = newSVpv( STRINGIFY(GIF_VERSION), 0 );
#else
  RETVAL = &PL_sv_undef;
#endif
}
OUTPUT:
  RETVAL

