/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

static int InterlacedOffset[] = { 0, 4, 2, 1 };
static int InterlacedJumps[] = { 8, 8, 4, 2 };

static int
image_gif_read_buf(GifFileType *gif, GifByteType *data, int len)
{
  image *im = (image *)gif->UserData;

  //DEBUG_TRACE("GIF read_buf wants %d bytes, %d in buffer\n", len, buffer_len(im->buf));

  if (im->fh != NULL) {
    if ( !_check_buf(im->fh, im->buf, len, MAX(len, BUFFER_SIZE)) ) {
      warn("Image::Scale not enough GIF data (%s)\n", SvPVX(im->path));
      return 0;
    }
  }
  else {
    if (len > buffer_len(im->buf)) {
      // read from SV into buffer
      int sv_readlen = len - buffer_len(im->buf);

      if (sv_readlen > sv_len(im->sv_data) - im->sv_offset) {
        warn("Image::Scale not enough GIF data (%s)\n", SvPVX(im->path));
        return 0;
      }

      DEBUG_TRACE("  Reading %d bytes of SV data @ %d\n", sv_readlen, im->sv_offset);
      buffer_append(im->buf, SvPVX(im->sv_data) + im->sv_offset, sv_readlen);
      im->sv_offset += sv_readlen;
     }
  }

  memcpy(data, buffer_ptr(im->buf), len);
  buffer_consume(im->buf, len);

  return len;
}

int
image_gif_read_header(image *im)
{
#ifdef GIFLIB_API_50
  im->gif = DGifOpen(im, image_gif_read_buf, NULL);
#else
  im->gif = DGifOpen(im, image_gif_read_buf);
#endif

  if (im->gif == NULL) {
#ifdef GIFLIB_API_41
    PrintGifError();
#endif
    warn("Image::Scale unable to open GIF file (%s)\n", SvPVX(im->path));
    image_gif_finish(im);
    return 0;
  }

  im->width  = im->gif->SWidth;
  im->height = im->gif->SHeight;

  return 1;
}

int
image_gif_load(image *im)
{
  int x, y, ofs;
  GifRecordType RecordType;
  GifPixelType *line = NULL;
  int ExtFunction = 0;
  GifByteType *ExtData;
  SavedImage *sp;
  SavedImage temp_save;
  int BackGround = 0;
  int trans_index = 0; // transparent index if any
  ColorMapObject *ColorMap;
  GifColorType *ColorMapEntry;

  temp_save.ExtensionBlocks = NULL;
  temp_save.ExtensionBlockCount = 0;

  // If reusing the object a second time, start over
  if (im->used) {
    DEBUG_TRACE("Recreating giflib objects\n");
    image_gif_finish(im);

    if (im->fh != NULL) {
      // reset file to begining of image
      PerlIO_seek(im->fh, im->image_offset, SEEK_SET);
    }
    else {
      // reset SV read
      im->sv_offset = im->image_offset;
    }

    buffer_clear(im->buf);

    image_gif_read_header(im);
  }

  do {
    if (DGifGetRecordType(im->gif, &RecordType) == GIF_ERROR) {
      warn("Image::Scale unable to read GIF file (%s)\n", SvPVX(im->path));
      image_gif_finish(im);
      return 0;
    }

    switch (RecordType) {
      case IMAGE_DESC_RECORD_TYPE:
        if (DGifGetImageDesc(im->gif) == GIF_ERROR) {
          warn("Image::Scale unable to read GIF file (%s)\n", SvPVX(im->path));
          image_gif_finish(im);
          return 0;
        }

        sp = &im->gif->SavedImages[im->gif->ImageCount - 1];

        im->width  = sp->ImageDesc.Width;
        im->height = sp->ImageDesc.Height;

        BackGround = im->gif->SBackGroundColor; // XXX needed?
        ColorMap = im->gif->Image.ColorMap ? im->gif->Image.ColorMap : im->gif->SColorMap;

        if (ColorMap == NULL) {
          warn("Image::Scale GIF image has no colormap (%s)\n", SvPVX(im->path));
          image_gif_finish(im);
          return 0;
        }

        // Allocate storage for decompressed image
        image_alloc(im, im->width, im->height);

        New(0, line, im->width, GifPixelType);

        if (im->gif->Image.Interlace) {
          int i;
          for (i = 0; i < 4; i++) {
            for (x = InterlacedOffset[i]; x < im->height; x += InterlacedJumps[i]) {
              ofs = x * im->width;
              if (DGifGetLine(im->gif, line, 0) != GIF_OK) {
                warn("Image::Scale unable to read GIF file (%s)\n", SvPVX(im->path));
                image_gif_finish(im);
                return 0;
              }

              for (y = 0; y < im->width; y++) {
                ColorMapEntry = &ColorMap->Colors[line[y]];
                im->pixbuf[ofs++] = COL_FULL(
                  ColorMapEntry->Red,
                  ColorMapEntry->Green,
                  ColorMapEntry->Blue,
                  trans_index == line[y] ? 0 : 255
                );
              }
            }
          }
        }
        else {
          ofs = 0;
          for (x = 0; x < im->height; x++) {
            if (DGifGetLine(im->gif, line, 0) != GIF_OK) {
              warn("Image::Scale unable to read GIF file (%s)\n", SvPVX(im->path));
              image_gif_finish(im);
              return 0;
            }

            for (y = 0; y < im->width; y++) {
              ColorMapEntry = &ColorMap->Colors[line[y]];
              im->pixbuf[ofs++] = COL_FULL(
                ColorMapEntry->Red,
                ColorMapEntry->Green,
                ColorMapEntry->Blue,
                trans_index == line[y] ? 0 : 255
              );
            }
          }
        }

        Safefree(line);
        break;

      case EXTENSION_RECORD_TYPE:
        if (DGifGetExtension(im->gif, &ExtFunction, &ExtData) == GIF_ERROR) {
          warn("Image::Scale unable to read GIF file (%s)\n", SvPVX(im->path));
          image_gif_finish(im);
          return 0;
        }

        if (ExtFunction == 0xF9) { // transparency info
          if (ExtData[1] & 1)
            trans_index = ExtData[4];
          else
            trans_index = -1;
          im->has_alpha = 1;
          DEBUG_TRACE("GIF transparency index: %d\n", trans_index);
        }

        while (ExtData != NULL) {
          /* Create an extension block with our data */
#ifdef GIFLIB_API_50
          if (GifAddExtensionBlock(&im->gif->ExtensionBlockCount, &im->gif->ExtensionBlocks, ExtFunction, ExtData[0], &ExtData[1]) == GIF_ERROR) {
#else
          temp_save.Function = ExtFunction;
          if (AddExtensionBlock(&temp_save, ExtData[0], &ExtData[1]) == GIF_ERROR) {
#endif
#ifdef GIFLIB_API_41
            PrintGifError();
#endif
            warn("Image::Scale unable to read GIF file (%s)\n", SvPVX(im->path));
            image_gif_finish(im);
            return 0;
          }

          if (DGifGetExtensionNext(im->gif, &ExtData) == GIF_ERROR) {
#ifdef GIFLIB_API_41
            PrintGifError();
#endif
            warn("Image::Scale unable to read GIF file (%s)\n", SvPVX(im->path));
            image_gif_finish(im);
            return 0;
          }

          ExtFunction = 0; // CONTINUE_EXT_FUNC_CODE
        }
        break;

      case TERMINATE_RECORD_TYPE:
      default:
        break;
    }
  } while (RecordType != TERMINATE_RECORD_TYPE);

  return 1;
}

void
image_gif_finish(image *im)
{
  if (im->gif != NULL) {
#ifdef GIFLIB_API_51
    if (DGifCloseFile(im->gif, NULL) != GIF_OK) {
#else
    if (DGifCloseFile(im->gif) != GIF_OK) {
#endif
#ifdef GIFLIB_API_41
      PrintGifError();
#endif
      warn("Image::Scale unable to close GIF file (%s)\n", SvPVX(im->path));
    }
    im->gif = NULL;

    DEBUG_TRACE("image_gif_finish\n");
  }
}
