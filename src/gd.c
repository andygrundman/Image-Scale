// Port of GD copyResampled
#define floor2(exp) ((int) exp)

void
image_downsize_gd(image *im)
{
  int x, y;
  float sy1, sy2, sx1, sx2;
  int dstX = 0, dstY = 0, srcX = 0, srcY = 0;
  float width_scale, height_scale;
  
  int dstW = im->target_width;
  int dstH = im->target_height;
  int srcW = im->width;
  int srcH = im->height;
  
  if (im->height_padding) {
    dstY = im->height_padding;
    dstH = im->height_inner;
  }
  
  if (im->width_padding) {
    dstX = im->width_padding;
    dstW = im->width_inner;
  }
  
  width_scale = (float)srcW / dstW;
  height_scale = (float)srcH / dstH;
  
  for (y = dstY; (y < dstY + dstH); y++) {
    sy1 = (float)(y - dstY) * height_scale;
    sy2 = (float)((y + 1) - dstY) * height_scale;
    
    for (x = dstX; (x < dstX + dstW); x++) {
      float sx, sy;
  	  float spixels = 0;
  	  float red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
  	  
  	  if (!im->has_alpha)
        alpha = 255.0;
  	  
  	  sx1 = (float)(x - dstX) * width_scale;
  	  sx2 = (float)((x + 1) - dstX) * width_scale;
  	  sy = sy1;
  	  
      //DEBUG_TRACE("sx1 %.2f, sx2 %.2f, sy1 %.2f, sy2 %.2f\n", sx1, sx2, sy1, sy2); 
  	  
  	  do {
        float yportion;
        
        //DEBUG_TRACE("  yportion(sy %.2f, sy1 %.2f, sy2 %.2f) = ", sy, sy1, sy2);
        if (floor2(sy) == floor2(sy1)) {
          yportion = 1.0 - (sy - floor2(sy));
    		  if (yportion > sy2 - sy1) {
            yportion = sy2 - sy1;
    		  }
    		  sy = floor2(sy);
    		}
    		else if (sy == floor2(sy2)) {
          yportion = sy2 - floor2(sy2);
        }
        else {
          yportion = 1.0;
        }
        //DEBUG_TRACE("%.2f\n", yportion);
        
        sx = sx1;
        
        do {
          float xportion = 1.0;
    		  float pcontribution;
    		  pix p;

    		  //DEBUG_TRACE("  xportion(sx %.2f, sx1 %.2f, sx2 %.2f) = ", sx, sx1, sx2);
    		  if (floor2(sx) == floor2(sx1)) {
    	      xportion = 1.0 - (sx - floor2(sx));
    	      if (xportion > sx2 - sx1)	{
              xportion = sx2 - sx1;
    			  }
    		    sx = floor2(sx);
    		  }
    		  else if (sx == floor2(sx2)) {
            xportion = sx2 - floor2(sx2);
          }
    		  else {
    		    xportion = 1.0;
    		  }
    		  //DEBUG_TRACE("%.2f\n", xportion);
  		    
    		  pcontribution = xportion * yportion;
  		  
    		  p = get_pix(im, (int32_t)sx + srcX, (int32_t)sy + srcY);
    		  
    		  /*
    		  DEBUG_TRACE("  merging with pix %d, %d: src %x (%d %d %d %d), pcontribution %.2f\n",
            (int32_t)sx + srcX, (int32_t)sy + srcY,
            p, COL_RED(p), COL_GREEN(p), COL_BLUE(p), COL_ALPHA(p), pcontribution);
          */
  		    
    		  red   += COL_RED(p)   * pcontribution;
    		  green += COL_GREEN(p) * pcontribution;
    		  blue  += COL_BLUE(p)  * pcontribution;
    		  
    		  if (im->has_alpha)
    		    alpha += COL_ALPHA(p) * pcontribution;
    		  
    		  spixels += pcontribution;
    		  sx += 1.0;
    		} while (sx < sx2);
    		
    		sy += 1.0;
      } while (sy < sy2);
      
      if (spixels != 0.0) {
        //DEBUG_TRACE("  rgba (%.2f %.2f %.2f %.2f) spixels %.2f\n", red, green, blue, alpha, spixels);
        spixels = 1 / spixels;
	      red   *= spixels;
	      green *= spixels;
	      blue  *= spixels;
	      
	      if (im->has_alpha)
	        alpha *= spixels;
	    }
	    
	    /* Clamping to allow for rounding errors above */
      if (red > 255.0)   red = 255.0;
      if (green > 255.0) green = 255.0;
      if (blue > 255.0)  blue = 255.0;
      if (im->has_alpha && alpha > 255.0) alpha = 255.0;
      
      /*
      DEBUG_TRACE("  -> %d, %d %x (%d %d %d %d)\n",
        x, y, COL_FULL((int)red, (int)green, (int)blue, (int)alpha),
        (int)red, (int)green, (int)blue, (int)alpha);
      */
      
      if (im->orientation != ORIENTATION_NORMAL) {
        int ox, oy; // new destination pixel coordinates after rotating
        
        image_get_rotated_coords(im, x, y, &ox, &oy);
        
        if (im->orientation >= 5) {
          // 90 and 270 rotations, width/height are swapped so we have to use alternate put_pix method
          put_pix_rotated(
            im, ox, oy, im->target_height,
            COL_FULL(ROUND_FLOAT_TO_INT(red), ROUND_FLOAT_TO_INT(green), ROUND_FLOAT_TO_INT(blue), ROUND_FLOAT_TO_INT(alpha))
          );
        }
        else {
          put_pix(
            im, ox, oy,
            COL_FULL(ROUND_FLOAT_TO_INT(red), ROUND_FLOAT_TO_INT(green), ROUND_FLOAT_TO_INT(blue), ROUND_FLOAT_TO_INT(alpha))
          );
        }
      }
      else {
        put_pix(
          im, x, y,
          COL_FULL(ROUND_FLOAT_TO_INT(red), ROUND_FLOAT_TO_INT(green), ROUND_FLOAT_TO_INT(blue), ROUND_FLOAT_TO_INT(alpha))
        );
      }
    }
	}
}

void
image_downsize_gd_fixed_point(image *im)
{
  int x, y;
  fixed_t sy1, sy2, sx1, sx2;
  int dstX = 0, dstY = 0, srcX = 0, srcY = 0;
  fixed_t width_scale, height_scale;
  
  int dstW = im->target_width;
  int dstH = im->target_height;
  int srcW = im->width;
  int srcH = im->height;
  
  if (im->height_padding) {
    dstY = im->height_padding;
    dstH = im->height_inner;
  }
  
  if (im->width_padding) {
    dstX = im->width_padding;
    dstW = im->width_inner;
  }
  
  width_scale = fixed_div(int_to_fixed(srcW), int_to_fixed(dstW));
  height_scale = fixed_div(int_to_fixed(srcH), int_to_fixed(dstH));
  
  for (y = dstY; (y < dstY + dstH); y++) {
    sy1 = fixed_mul(int_to_fixed(y - dstY), height_scale);
    sy2 = fixed_mul(int_to_fixed((y + 1) - dstY), height_scale);
    
    for (x = dstX; (x < dstX + dstW); x++) {
      fixed_t sx, sy;
  	  fixed_t spixels = 0;
  	  fixed_t red = 0, green = 0, blue = 0, alpha = 0;
  	  
  	  if (!im->has_alpha)
        alpha = FIXED_255;
  	  
      sx1 = fixed_mul(int_to_fixed(x - dstX), width_scale);
      sx2 = fixed_mul(int_to_fixed((x + 1) - dstX), width_scale);  	  
  	  sy = sy1;
  	  
  	  /*
      DEBUG_TRACE("sx1 %f, sx2 %f, sy1 %f, sy2 %f\n",
        fixed_to_float(sx1), fixed_to_float(sx2), fixed_to_float(sy1), fixed_to_float(sy2));
      */
  	  
  	  do {
        fixed_t yportion;
        
        //DEBUG_TRACE("  yportion(sy %f, sy1 %f, sy2 %f) = ", fixed_to_float(sy), fixed_to_float(sy1), fixed_to_float(sy2));
        
        if (fixed_floor(sy) == fixed_floor(sy1)) {
          yportion = FIXED_1 - (sy - fixed_floor(sy));
    		  if (yportion > sy2 - sy1) {
            yportion = sy2 - sy1;
    		  }
    		  sy = fixed_floor(sy);
    		}
    		else if (sy == fixed_floor(sy2)) {
          yportion = sy2 - fixed_floor(sy2);
        }
        else {
          yportion = FIXED_1;
        }
        
        //DEBUG_TRACE("%f\n", fixed_to_float(yportion));
        
        sx = sx1;
        
        do {
          fixed_t xportion;
    		  fixed_t pcontribution;
    		  pix p;
    		  
    		  //DEBUG_TRACE("  xportion(sx %f, sx1 %f, sx2 %f) = ", fixed_to_float(sx), fixed_to_float(sx1), fixed_to_float(sx2));
  		  
    		  if (fixed_floor(sx) == fixed_floor(sx1)) {
    	      xportion = FIXED_1 - (sx - fixed_floor(sx));
    	      if (xportion > sx2 - sx1)	{
              xportion = sx2 - sx1;
    			  }
    		    sx = fixed_floor(sx);
    		  }
    		  else if (sx == fixed_floor(sx2)) {
            xportion = sx2 - fixed_floor(sx2);
          }
    		  else {
    		    xportion = FIXED_1;
    		  }
    		  
    		  //DEBUG_TRACE("%f\n", fixed_to_float(xportion));
  		  
    		  pcontribution = fixed_mul(xportion, yportion);
  		  
    		  p = get_pix(im, fixed_to_int(sx + srcX), fixed_to_int(sy + srcY));
    		  
    		  /*
    		  DEBUG_TRACE("  merging with pix %d, %d: src %x (%d %d %d %d), pcontribution %f\n",
            fixed_to_int(sx + srcX), fixed_to_int(sy + srcY),
            p, COL_RED(p), COL_GREEN(p), COL_BLUE(p), COL_ALPHA(p), fixed_to_float(pcontribution));
          */
  		    
          red   += fixed_mul(int_to_fixed(COL_RED(p)), pcontribution);
          green += fixed_mul(int_to_fixed(COL_GREEN(p)), pcontribution);
    		  blue  += fixed_mul(int_to_fixed(COL_BLUE(p)), pcontribution);
    		  
    		  if (im->has_alpha)
    		    alpha += fixed_mul(int_to_fixed(COL_ALPHA(p)), pcontribution);
    		  
    		  spixels += pcontribution;
    		  sx += FIXED_1;
    		} while (sx < sx2);
    		
        sy += FIXED_1;
      } while (sy < sy2);
      
  	  // If rgba get too large for the fixed-point representation, fallback to the floating point routine
		  // This should only happen with very large images
		  if (red < 0 || green < 0 || blue < 0 || alpha < 0) {
        warn("fixed-point overflow: %d %d %d %d\n", red, green, blue, alpha);
        return image_downsize_gd(im);
      }
      
      if (spixels != 0) {
        /*
        DEBUG_TRACE("  rgba (%f %f %f %f) spixels %f\n",
          fixed_to_float(red), fixed_to_float(green), fixed_to_float(blue), fixed_to_float(alpha), fixed_to_float(spixels));
        */
        
        spixels = fixed_div(FIXED_1, spixels);
        
        red   = fixed_mul(red, spixels);
        green = fixed_mul(green, spixels);
        blue  = fixed_mul(blue, spixels);
        
        if (im->has_alpha)
          alpha = fixed_mul(alpha, spixels);
	    }
	    
	    /* Clamping to allow for rounding errors above */
      if (red > FIXED_255)   red = FIXED_255;
      if (green > FIXED_255) green = FIXED_255;
      if (blue > FIXED_255)  blue = FIXED_255;
      if (im->has_alpha && alpha > FIXED_255) alpha = FIXED_255;
      
      /*
      DEBUG_TRACE("  -> %d, %d %x (%d %d %d %d)\n",
        x, y, COL_FULL(fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue), fixed_to_int(alpha)),
        fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue), fixed_to_int(alpha));
      */
      
      if (im->orientation != ORIENTATION_NORMAL) {
        int ox, oy; // new destination pixel coordinates after rotating
        
        image_get_rotated_coords(im, x, y, &ox, &oy);
        
        if (im->orientation >= 5) {
          // 90 and 270 rotations, width/height are swapped so we have to use alternate put_pix method
          put_pix_rotated(
            im, ox, oy, im->target_height,
            COL_FULL(fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue), fixed_to_int(alpha))
          );
        }
        else {
          put_pix(
            im, ox, oy,
            COL_FULL(fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue), fixed_to_int(alpha))
          );
        }
      }
      else {
        put_pix(
          im, x, y,
          COL_FULL(fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue), fixed_to_int(alpha))
        );
      }
	  }
	}
}
