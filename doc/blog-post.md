## The Problem

Several years ago I helped develop a media server that was designed to run on everything from desktop PCs to low-end Linux-based NAS devices. A common task for a media server is creating high quality thumbnails of all the album covers, photos, and videos in the user's media library. When scanning the library, the server needs to cache all of these thumbnails so they can be displayed quickly. In addition, each image must be cached at several different sizes to optimize for display on different devices. For example, a 200x200 thumbnail might work well on a web UI whereas 75x75 would fit better in a mobile app.

Unfortunately, some of the lower-end NAS devices have very slow CPUs that were designed more for simple file transfer tasks than complex math-intensive image processing. The resampling algorithm in libgd, one of the most common open-source image libraries, uses around 35 floating-point multiplies per pixel of the destination image. Creating a 200x200 thumbnail of a typical 1500x1500 album cover requires nearly 1.5 million multiplication operations in the core libgd resampling loop. Computers are pretty good at multiplying numbers together, but performing math on fractional numbers is more difficult than on whole numbers, and is generally done by a separate part of the CPU called the FPU, or floating-point unit. One way CPU manufacturers can reduce costs is by removing the FPU. The regular math unit in the CPU can perform the same calculations, but at the cost of many more operations. CPUs without FPUs are commonly used in embedded devices such as firewalls, routers, and of course, NAS devices.

One of the most popular consumer storage devices several years ago included a SPARC-based CPU running at 240MHz. Naturally, there was no room for extra frills such as an FPU in this little guy. Take a guess at how long it took this machine to resize that 1500x1500 cover down to 200x200. 5 seconds? 10? How does 34 seconds sound? Yes, over half a minute, for ONE thumbnail. The fact that people were having to wait so many hours for the artwork phase of their music scan to finish was completely unacceptable.

## The Solution

I had previously encountered the concept of fixed-point math from working with various audio codecs such as libmad (MP3), Tremor (Vorbis) and FLAC. Decoding audio is almost always performed without using floating-point math. In addition to improved performance on low-end CPUs, avoiding FPU use saves a lot of power in portable devices. Fixed-point math seemed like the perfect solution to our image problem; if it was good enough for high quality audio, it had to work for image processing as well.

We had been using the GD Perl module up until this point. GD is a frontend to the libgd C library, and while it produces very high quality thumbnails, it has a rather clunky API and is notoriously painful to build, especially on Windows. I decided to take the best part of libgd, its resampling algorithm, and port it to fixed-point math within a much simpler and easier to use module called Image::Scale. This module would support all the common image formats with a simple Perl-ish API as well as add a few features, such as using JPEG prescaling, maintaining aspect ratio by padding with transparency, and auto-rotating camera images. I also wanted to add some protection against accidentally resizing a very large image that could lead to running out of memory on these embedded systems which generally have very small amounts of RAM. libjpeg-turbo is also supported and highly recommended; it includes highly-optimized assembly routines and can improve JPEG processing 2-4x.

    my $img = Image::Scale->new(‘image.jpg’);
    $img->resize_gd_fixed_point( { width => 150 } );
    $img->save_jpeg(‘resized.jpg’); # or save_png()

## Fixed-Point Primer

Fixed-point is implemented by splitting the 32 bits in a normal integer into an integer half and a fractional half. The more bits that are allocated to the fractional side, the higher the accuracy will be. I chose to use 19.12 format, which simply means 19 bits for the integer, 12 for the fraction, and the remaining bit is used for sign. 12 bits allows for around 4 decimal places of accuracy, which is more than enough for these types of calculations. In fact, I was pleased to find that image quality is virtually indistinguishable when compared to using floating point. If you want to know more, check out the Wikipedia article on [fixed-point arithmetic](https://en.wikipedia.org/wiki/Fixed-point_arithmetic).

## Results

I benchmarked the module on 3 different types of hardware: a high-end Core i7 MacBook Pro, a 1.2GHz ARM9 with no FPU, and the aforementioned 240MHz Sparc. Each of three tests loaded a 1425x1425 JPEG, resized it to 200x200, and compressed that to a new JPEG. libjpeg-turbo was used on the x86 and ARM systems.

* The original GD module's `copyResampled` method.
* Image::Scale's `resize_gd` method. This is a straight port of the floating-point algorithm.
* Image::Scale's `resize_gd_fixed_point` method. 

On a modern Intel CPU, you would expect the floating-point version to be as fast, if not better than, the fixed-point version. Indeed, this is true, with the floating point method slightly beating the fixed point one. Image::Scale is still around twice as fast thanks to the lack of libgd overhead and the use of JPEG prescaling.

*(bar charts)*

Method | Performance
------ | -----------
GD `copyResampled` | 1x
I::S `resize_gd_fixed_point` | 1.8x
I::S `resize_gd` | 1.9x

*2.6GHz Core i7 MacBook Pro (2012 model)*

Method | Performance
------ | -----------
GD `copyResampled` | 1x
I::S `resize_gd` | 2x
I::S `resize_gd_fixed_point` | 7.4x

*Marvell SheevaPlug 1.2GHz ARM9 (no FPU)*

Method | Performance
------ | -----------
GD `copyResampled` | 1x
I::S `resize_gd` | 1.1x
I::S `resize_gd_fixed_point` | 66x

*Netgear ReadyNAS Duo (240MHz Sparc, no FPU)*

On the non-FPU systems, performance was far better than I expected. The ARM9 clocked in at 7.4x faster than GD, and the Sparc managed to be a whopping 66x faster! On that system, resizing an image now takes half a second where it used to need half a minute. 

## Summary

While the performance of low-end embedded CPUs may not be as poor as it was several years ago, Image::Scale remains a great choice for those looking for a dead-simple Perl image resizing module. It has a friendlier API, is easier to install, and can be up to twice as fast as GD on modern hardware.

## Links

* Image::Scale is available on [CPAN](https://metacpan.org/release/Image-Scale), and the git repository can be found on [GitHub](https://github.com/andygrundman/Image-Scale).
* [libgd](http://libgd.github.io)
* [GD Perl module](https://metacpan.org/release/GD)
* [libjpeg-turbo](http://www.libjpeg-turbo.org) - *x86/ARM SIMD-accelerated drop-in replacement for libjpeg, 2-4x faster*