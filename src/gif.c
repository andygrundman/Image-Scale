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
image_gif_read_sv(GifFileType *gif, GifByteType *data, int len)
{
  image *im = (image *)gif->UserData;

  memcpy(data, SvPVX(im->sv_data) + im->sv_offset, len);
  im->sv_offset += len;

  DEBUG_TRACE("image_gif_read_sv read %d bytes\n", len);
  
  return len;
}

void
image_gif_read_header(image *im, const char *file)
{
  if (file != NULL) {
    im->gif = DGifOpenFileName((char *)file);
  }
  else {
    im->sv_offset = 0;
    im->gif = DGifOpen(im, image_gif_read_sv);
  }
  
  if (im->gif == NULL) {
    PrintGifError();
    croak("Image::Scale unable to open GIF file %s\n", file);
  }
  
  im->width  = im->gif->SWidth;
  im->height = im->gif->SHeight;  
}

void image_gif_load(image *im)
{
  int x, y, ofs;
  GifRecordType RecordType;
  GifPixelType *line = NULL;
  GifByteType *ExtData;
  SavedImage *sp;
  SavedImage temp_save;
  int BackGround = 0;
  int trans_index = 0; // transparent index if any
  ColorMapObject *ColorMap;
  GifColorType *ColorMapEntry;

  temp_save.ExtensionBlocks = NULL;
  temp_save.ExtensionBlockCount = 0;
  
  do { 
    if (DGifGetRecordType(im->gif, &RecordType) == GIF_ERROR) {
      PrintGifError();
      croak("Image::Scale unable to read GIF file\n");
    }
    
    switch (RecordType) {
      case IMAGE_DESC_RECORD_TYPE:      
        if (DGifGetImageDesc(im->gif) == GIF_ERROR) {
          PrintGifError();
          croak("Image::Scale unable to read GIF file\n");
        }
        
        sp = &im->gif->SavedImages[im->gif->ImageCount - 1];
        
        im->width  = sp->ImageDesc.Width;
        im->height = sp->ImageDesc.Height;
        
        BackGround = im->gif->SBackGroundColor; // XXX needed?
        ColorMap = im->gif->Image.ColorMap ? im->gif->Image.ColorMap : im->gif->SColorMap;
        
        if (ColorMap == NULL) {
          croak("Image::Scale GIF image has no colormap\n");
        }
        
        // Allocate storage for decompressed image
        image_alloc(im, im->width, im->height);
        
        New(0, line, im->width, GifPixelType);
        
        if (im->gif->Image.Interlace) {
          int i;
          for (i = 0; i < 4; i++) {
            for (x = InterlacedOffset[i]; x < im->height; x += InterlacedJumps[i]) {
              ofs = x * im->height;
              if (DGifGetLine(im->gif, line, 0) != GIF_OK) {
                PrintGifError();
                croak("Image::Scale unable to read GIF file\n");
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
              PrintGifError();
              croak("Image::Scale unable to read GIF file\n");
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
        if (DGifGetExtension(im->gif, &temp_save.Function, &ExtData) == GIF_ERROR) {
          PrintGifError();
          croak("Image::Scale unable to read GIF file\n");
        }
        
        if (temp_save.Function == 0xF9) { // transparency info
          if (ExtData[1] & 1)
            trans_index = ExtData[4];
          else
            trans_index = -1;
          DEBUG_TRACE("GIF transparency index: %d\n", trans_index);
        }
        
        while (ExtData != NULL) {
          /* Create an extension block with our data */
          if (AddExtensionBlock(&temp_save, ExtData[0], &ExtData[1]) == GIF_ERROR) {
            PrintGifError();
            croak("Image::Scale unable to read GIF file\n");
          }

          if (DGifGetExtensionNext(im->gif, &ExtData) == GIF_ERROR) {
            PrintGifError();
            croak("Image::Scale unable to read GIF file\n");
          }
          
          temp_save.Function = 0;
        }
        break;
        
      case TERMINATE_RECORD_TYPE:
      default:
        break;
    }
  } while (RecordType != TERMINATE_RECORD_TYPE);
}

void
image_gif_finish(image *im)
{
  if (im->gif != NULL) {
    if (DGifCloseFile(im->gif) != GIF_OK) {
      PrintGifError();
      croak("Image::Scale unable to close GIF file\n");
    }
    im->gif = NULL;
  }
}
