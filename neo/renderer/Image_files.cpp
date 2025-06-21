/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.
Copyright (C) 2016 Johannes Ohlemacher (http://github.com/eXistence/fhDOOM)

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "ImageData.h"

/*

This file only has a single entry point:

void R_LoadImage( const char *name, byte **pic, int *width, int *height, bool makePowerOf2 );

*/

/*
================
R_WriteTGA
================
*/
void R_WriteTGA( const char *filename, const byte *data, int width, int height, bool flipVertical ) {
	byte	*buffer;
	int		i;
	int		bufferSize = width*height*4 + 18;
	int     imgStart = 18;

	buffer = (byte *)Mem_Alloc( bufferSize );
	memset( buffer, 0, 18 );
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width&255;
	buffer[13] = width>>8;
	buffer[14] = height&255;
	buffer[15] = height>>8;
	buffer[16] = 32;	// pixel size
	if ( !flipVertical ) {
		buffer[17] = (1<<5);	// flip bit, for normal top to bottom raster order
	}

	// swap rgb to bgr
	for ( i=imgStart ; i<bufferSize ; i+=4 ) {
		buffer[i] = data[i-imgStart+2];		// blue
		buffer[i+1] = data[i-imgStart+1];		// green
		buffer[i+2] = data[i-imgStart+0];		// red
		buffer[i+3] = data[i-imgStart+3];		// alpha
	}

	fileSystem->WriteFile( filename, buffer, bufferSize );

	Mem_Free (buffer);
}


/*
================
R_WritePalTGA
================
*/
void R_WritePalTGA( const char *filename, const byte *data, const byte *palette, int width, int height, bool flipVertical ) {
	byte	*buffer;
	int		i;
	int		bufferSize = (width * height) + (256 * 3) + 18;
	int     palStart = 18;
	int     imgStart = 18 + (256 * 3);

	buffer = (byte *)Mem_Alloc( bufferSize );
	memset( buffer, 0, 18 );
	buffer[1] = 1;		// color map type
	buffer[2] = 1;		// uncompressed color mapped image
	buffer[5] = 0;		// number of palette entries (lo)
	buffer[6] = 1;		// number of palette entries (hi)
	buffer[7] = 24;		// color map bpp
	buffer[12] = width&255;
	buffer[13] = width>>8;
	buffer[14] = height&255;
	buffer[15] = height>>8;
	buffer[16] = 8;	// pixel size
	if ( !flipVertical ) {
		buffer[17] = (1<<5);	// flip bit, for normal top to bottom raster order
	}

	// store palette, swapping rgb to bgr
	for ( i=palStart ; i<imgStart ; i+=3 ) {
		buffer[i] = palette[i-palStart+2];		// blue
		buffer[i+1] = palette[i-palStart+1];		// green
		buffer[i+2] = palette[i-palStart+0];		// red
	}

	// store the image data
	for ( i=imgStart ; i<bufferSize ; i++ ) {
		buffer[i] = data[i-imgStart];
	}

	fileSystem->WriteFile( filename, buffer, bufferSize );

	Mem_Free (buffer);
}

/*
========================================================================

PCX files are used for 8 bit images

========================================================================
*/

typedef struct {
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} pcx_t;


/*
========================================================================

TGA files are used for 24/32 bit images

========================================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
=========================================================

BMP LOADING

=========================================================
*/
typedef struct
{
	char id[2];
	unsigned long fileSize;
	unsigned long reserved0;
	unsigned long bitmapDataOffset;
	unsigned long bitmapHeaderSize;
	unsigned long width;
	unsigned long height;
	unsigned short planes;
	unsigned short bitsPerPixel;
	unsigned long compression;
	unsigned long bitmapDataSize;
	unsigned long hRes;
	unsigned long vRes;
	unsigned long colors;
	unsigned long importantColors;
	unsigned char palette[256][4];
} BMPHeader_t;

/*
==============
LoadBMP
==============
*/
static void LoadBMP( const char *name, byte **pic, int *width, int *height, ID_TIME_T *timestamp )
{
	int		columns, rows, numPixels;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	int		length;
	BMPHeader_t bmpHeader;
	byte		*bmpRGBA;

	if ( !pic ) {
		fileSystem->ReadFile ( name, NULL, timestamp );
		return;	// just getting timestamp
	}

	*pic = NULL;

	//
	// load the file
	//
	length = fileSystem->ReadFile( name, (void **)&buffer, timestamp );
	if ( !buffer ) {
		return;
	}

	buf_p = buffer;

	bmpHeader.id[0] = *buf_p++;
	bmpHeader.id[1] = *buf_p++;
	bmpHeader.fileSize = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.reserved0 = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.bitmapDataOffset = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.bitmapHeaderSize = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.width = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.height = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.planes = LittleShort( * ( short * ) buf_p );
	buf_p += 2;
	bmpHeader.bitsPerPixel = LittleShort( * ( short * ) buf_p );
	buf_p += 2;
	bmpHeader.compression = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.bitmapDataSize = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.hRes = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.vRes = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.colors = LittleLong( * ( long * ) buf_p );
	buf_p += 4;
	bmpHeader.importantColors = LittleLong( * ( long * ) buf_p );
	buf_p += 4;

	memcpy( bmpHeader.palette, buf_p, sizeof( bmpHeader.palette ) );

	if ( bmpHeader.bitsPerPixel == 8 )
		buf_p += 1024;

	if ( bmpHeader.id[0] != 'B' && bmpHeader.id[1] != 'M' )
	{
		common->Error( "LoadBMP: only Windows-style BMP files supported (%s)\n", name );
	}
	if ( bmpHeader.fileSize != length )
	{
		common->Error( "LoadBMP: header size does not match file size (%lu vs. %d) (%s)\n", bmpHeader.fileSize, length, name );
	}
	if ( bmpHeader.compression != 0 )
	{
		common->Error( "LoadBMP: only uncompressed BMP files supported (%s)\n", name );
	}
	if ( bmpHeader.bitsPerPixel < 8 )
	{
		common->Error( "LoadBMP: monochrome and 4-bit BMP files not supported (%s)\n", name );
	}

	columns = bmpHeader.width;
	rows = bmpHeader.height;
	if ( rows < 0 )
		rows = -rows;
	numPixels = columns * rows;

	if ( width )
		*width = columns;
	if ( height )
		*height = rows;

	bmpRGBA = (byte *)R_StaticAlloc( numPixels * 4 );
	*pic = bmpRGBA;


	for ( row = rows-1; row >= 0; row-- )
	{
		pixbuf = bmpRGBA + row*columns*4;

		for ( column = 0; column < columns; column++ )
		{
			unsigned char red, green, blue, alpha;
			int palIndex;
			unsigned short shortPixel;

			switch ( bmpHeader.bitsPerPixel )
			{
			case 8:
				palIndex = *buf_p++;
				*pixbuf++ = bmpHeader.palette[palIndex][2];
				*pixbuf++ = bmpHeader.palette[palIndex][1];
				*pixbuf++ = bmpHeader.palette[palIndex][0];
				*pixbuf++ = 0xff;
				break;
			case 16:
				shortPixel = * ( unsigned short * ) pixbuf;
				pixbuf += 2;
				*pixbuf++ = ( shortPixel & ( 31 << 10 ) ) >> 7;
				*pixbuf++ = ( shortPixel & ( 31 << 5 ) ) >> 2;
				*pixbuf++ = ( shortPixel & ( 31 ) ) << 3;
				*pixbuf++ = 0xff;
				break;

			case 24:
				blue = *buf_p++;
				green = *buf_p++;
				red = *buf_p++;
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = 255;
				break;
			case 32:
				blue = *buf_p++;
				green = *buf_p++;
				red = *buf_p++;
				alpha = *buf_p++;
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alpha;
				break;
			default:
				common->Error( "LoadBMP: illegal pixel_size '%d' in file '%s'\n", bmpHeader.bitsPerPixel, name );
				break;
			}
		}
	}

	fileSystem->FreeFile( buffer );

}


/*
=================================================================

PCX LOADING

=================================================================
*/


/*
==============
LoadPCX
==============
*/
static void LoadPCX ( const char *filename, byte **pic, byte **palette, int *width, int *height,
					 ID_TIME_T *timestamp ) {
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte	*out, *pix;
	int		xmax, ymax;

	if ( !pic ) {
		fileSystem->ReadFile( filename, NULL, timestamp );
		return;	// just getting timestamp
	}

	*pic = NULL;
	*palette = NULL;

	//
	// load the file
	//
	len = fileSystem->ReadFile( filename, (void **)&raw, timestamp );
	if (!raw) {
		return;
	}

	//
	// parse the PCX file
	//
	pcx = (pcx_t *)raw;
	raw = &pcx->data;

  	xmax = LittleShort(pcx->xmax);
    ymax = LittleShort(pcx->ymax);

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| xmax >= 1024
		|| ymax >= 1024)
	{
		common->Printf( "Bad pcx file %s (%i x %i) (%i x %i)\n", filename, xmax+1, ymax+1, pcx->xmax, pcx->ymax);
		return;
	}

	out = (byte *)R_StaticAlloc( (ymax+1) * (xmax+1) );

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = (byte *)R_StaticAlloc(768);
		memcpy (*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
		*width = xmax+1;
	if (height)
		*height = ymax+1;
// FIXME: use bytes_per_line here?

	for (y=0 ; y<=ymax ; y++, pix += xmax+1)
	{
		for (x=0 ; x<=xmax ; )
		{
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if ( raw - (byte *)pcx > len)
	{
		common->Printf( "PCX file %s was malformed", filename );
		R_StaticFree (*pic);
		*pic = NULL;
	}

	fileSystem->FreeFile( pcx );
}


/*
==============
LoadPCX32
==============
*/
static void LoadPCX32 ( const char *filename, byte **pic, int *width, int *height, ID_TIME_T *timestamp) {
	byte	*palette;
	byte	*pic8;
	int		i, c, p;
	byte	*pic32;

	if ( !pic ) {
		fileSystem->ReadFile( filename, NULL, timestamp );
		return;	// just getting timestamp
	}
	LoadPCX (filename, &pic8, &palette, width, height, timestamp);
	if (!pic8) {
		*pic = NULL;
		return;
	}

	c = (*width) * (*height);
	pic32 = *pic = (byte *)R_StaticAlloc(4 * c );
	for (i = 0 ; i < c ; i++) {
		p = pic8[i];
		pic32[0] = palette[p*3];
		pic32[1] = palette[p*3 + 1];
		pic32[2] = palette[p*3 + 2];
		pic32[3] = 255;
		pic32 += 4;
	}

	R_StaticFree( pic8 );
	R_StaticFree( palette );
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

/*
=============
LoadTGA
=============
*/
static void LoadTGA( const char *name, byte **pic, int *width, int *height, ID_TIME_T *timestamp ) {
	int		columns, rows, numPixels, fileSize, numBytes;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	TargaHeader	targa_header;
	byte		*targa_rgba;

	if ( !pic ) {
		fileSystem->ReadFile( name, NULL, timestamp );
		return;	// just getting timestamp
	}

	*pic = NULL;

	//
	// load the file
	//
	fileSize = fileSystem->ReadFile( name, (void **)&buffer, timestamp );
	if ( !buffer ) {
		return;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_length = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.y_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.width = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.height = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if ( targa_header.image_type != 2 && targa_header.image_type != 10 && targa_header.image_type != 3 ) {
		common->Error( "LoadTGA( %s ): Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n", name );
	}

	if ( targa_header.colormap_type != 0 ) {
		common->Error( "LoadTGA( %s ): colormaps not supported\n", name );
	}

	if ( ( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) && targa_header.image_type != 3 ) {
		common->Error( "LoadTGA( %s ): Only 32 or 24 bit images supported (no colormaps)\n", name );
	}

	if ( targa_header.image_type == 2 || targa_header.image_type == 3 ) {
		numBytes = targa_header.width * targa_header.height * ( targa_header.pixel_size >> 3 );
		if ( numBytes > fileSize - 18 - targa_header.id_length ) {
			common->Error( "LoadTGA( %s ): incomplete file\n", name );
		}
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if ( width ) {
		*width = columns;
	}
	if ( height ) {
		*height = rows;
	}

	targa_rgba = (byte *)R_StaticAlloc(numPixels*4);
	*pic = targa_rgba;

	if ( targa_header.id_length != 0 ) {
		buf_p += targa_header.id_length;  // skip TARGA image comment
	}

	if ( targa_header.image_type == 2 || targa_header.image_type == 3 )
	{
		// Uncompressed RGB or gray scale image
		for( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = targa_rgba + row*columns*4;
			for( column = 0; column < columns; column++)
			{
				unsigned char red,green,blue,alphabyte;
				switch( targa_header.pixel_size )
				{

				case 8:
					blue = *buf_p++;
					green = blue;
					red = blue;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;

				case 24:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alphabyte = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				default:
					common->Error( "LoadTGA( %s ): illegal pixel_size '%d'\n", name, targa_header.pixel_size );
					break;
				}
			}
		}
	}
	else if ( targa_header.image_type == 10 ) {   // Runlength encoded RGB images
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;

		red = 0;
		green = 0;
		blue = 0;
		alphabyte = 0xff;

		for( row = rows - 1; row >= 0; row-- ) {
			pixbuf = targa_rgba + row*columns*4;
			for( column = 0; column < columns; ) {
				packetHeader= *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if ( packetHeader & 0x80 ) {        // run-length packet
					switch( targa_header.pixel_size ) {
						case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = 255;
								break;
						case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								break;
						default:
							common->Error( "LoadTGA( %s ): illegal pixel_size '%d'\n", name, targa_header.pixel_size );
							break;
					}

					for( j = 0; j < packetSize; j++ ) {
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if ( column == columns ) { // run spans across rows
							column = 0;
							if ( row > 0) {
								row--;
							}
							else {
								goto breakOut;
							}
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else {                            // non run-length packet
					for( j = 0; j < packetSize; j++ ) {
						switch( targa_header.pixel_size ) {
							case 24:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									alphabyte = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
							default:
								common->Error( "LoadTGA( %s ): illegal pixel_size '%d'\n", name, targa_header.pixel_size );
								break;
						}
						column++;
						if ( column == columns ) { // pixel packet run spans across rows
							column = 0;
							if ( row > 0 ) {
								row--;
							}
							else {
								goto breakOut;
							}
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
			}
			breakOut: ;
		}
	}

	if ( (targa_header.attributes & (1<<5)) ) {			// image flp bit
		R_VerticalFlip( *pic, *width, *height );
	}

	fileSystem->FreeFile( buffer );
}

//===================================================================

/*
=================
R_LoadImage

Loads any of the supported image types into a cannonical
32 bit format.

Automatically attempts to load .jpg files if .tga files fail to load.

*pic will be NULL if the load failed.

Anything that is going to make this into a texture would use
makePowerOf2 = true, but something loading an image as a lookup
table of some sort would leave it in identity form.

It is important to do this at image load time instead of texture load
time for bump maps.

Timestamp may be NULL if the value is going to be ignored

If pic is NULL, the image won't actually be loaded, it will just find the
timestamp.
=================
*/
void R_LoadImage( const char *cname, byte **pic, int *width, int *height, ID_TIME_T *timestamp, bool makePowerOf2 ) {
	idStr name = cname;

	if ( pic ) {
		*pic = NULL;
	}
	if ( timestamp ) {
		*timestamp = 0xFFFFFFFF;
	}
	if ( width ) {
		*width = 0;
	}
	if ( height ) {
		*height = 0;
	}

	name.DefaultFileExtension( ".tga" );

	if (name.Length()<5) {
		return;
	}

	name.ToLower();
	idStr ext;
	name.ExtractFileExtension( ext );
#if 1
	fhImageData imageData;
	bool ok = imageData.LoadFile(name.c_str(), true);
	if (ok && imageData.IsValid()) {
		if (pic) {
			*pic = (byte*)R_StaticAlloc(imageData.GetSize(0));
			memcpy(*pic, imageData.GetData(0, 0), imageData.GetSize(0));
		}

		if (width) {
			*width = imageData.GetWidth(0);
		}

		if (height) {
			*height = imageData.GetHeight(0);
		}

		if (timestamp) {
			*timestamp = imageData.GetTimeStamp();
		}

		imageData.Clear();
	}
	else {
#endif
		if ( ext == "pcx" )
		{
			LoadPCX32( name.c_str(), pic, width, height, timestamp );
		}
#if 1
	}
#endif

	if ((width && *width < 1) || (height && *height < 1)) {
		if (pic && *pic) {
			R_StaticFree(*pic);
			*pic = 0;
		}
	}

	//
	// convert to exact power of 2 sizes
	//
	if ( pic && *pic && makePowerOf2 ) {
		int		w, h;
		int		scaled_width, scaled_height;
		byte	*resampledBuffer;

		w = *width;
		h = *height;

		for (scaled_width = 1 ; scaled_width < w ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < h ; scaled_height<<=1)
			;

		if ( scaled_width != w || scaled_height != h ) {
			if ( globalImages->image_roundDown.GetBool() && scaled_width > w ) {
				scaled_width >>= 1;
			}
			if ( globalImages->image_roundDown.GetBool() && scaled_height > h ) {
				scaled_height >>= 1;
			}

			resampledBuffer = R_ResampleTexture( *pic, w, h, scaled_width, scaled_height );
			R_StaticFree( *pic );
			*pic = resampledBuffer;
			*width = scaled_width;
			*height = scaled_height;
		}
	}
}