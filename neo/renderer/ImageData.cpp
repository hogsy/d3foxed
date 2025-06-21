/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 2016 Johannes Ohlemacher

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

#include "ImageData.h"
#include "ImageProgram.h"
#include "../framework/FileSystem.h"
#include "../framework/File.h"
#include "tr_local.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

namespace {
	//The dwCaps2 member of the DDSCAPS2 structure can be set to one or more of the following values.Flag	Value
	const uint32 DDSCAPS2_CUBEMAP = 0x00000200;
	const uint32 DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400;
	const uint32 DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800;
	const uint32 DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000;
	const uint32 DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000;
	const uint32 DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000;
	const uint32 DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000;
	const uint32 DDSCAPS2_VOLUME = 0x00200000;


	uint32 DataSize( uint32 width, uint32 height, pixelFormat_t format ) {
		switch (format)	{
		case pixelFormat_t::BGR:
		case pixelFormat_t::RGB:
			return width * height * 3;
		case pixelFormat_t::RGBA:
		case pixelFormat_t::BGRA:
			return width * height * 4;
		case pixelFormat_t::DXT1_RGBA:
		case pixelFormat_t::DXT1_RGB:
			return ((width + 3) / 4) * ((height + 3) / 4) * 8;
		case pixelFormat_t::DXT3_RGBA:
		case pixelFormat_t::DXT5_RGBA:
		case pixelFormat_t::DXT5_RxGB:
		case pixelFormat_t::RGTC:
			return ((width + 3) / 4) * ((height + 3) / 4) * 16;
		default:
			assert( false && "unknown pixel format!?" );
		}

		return 0;
	}

}

bool fhImageData::TryLoadFile( const char* filename, const char* ext, fhImageData* imageData, ID_TIME_T* timestamp, bool (fhImageData::*f)(fhStaticBuffer<byte>&, bool) ) {

	idStr filenameExt = filename;

	if (ext) {
		filenameExt.SetFileExtension( ext );
	}

	fhFileHandle file = fileSystem->OpenFileRead( filenameExt );
	if (!file) {
		return false;
	}

	int	len = file->Length();

	if (timestamp) {
		*timestamp = file->Timestamp();
	}

	if (imageData) {
		fhStaticBuffer<byte> buffer;
		buffer.Allocate( len );
		file->Read( buffer.Get(), len );

		bool ret = ((*imageData).*f)(buffer, false);

		if (ret){
			idStr::Copynz( imageData->name, filenameExt.c_str(), MAX_IMAGE_NAME );
			imageData->timestamp = file->Timestamp();
		}

		return ret;
	}

	return true;
}

bool fhImageData::LoadFile( const char *filename, fhImageData *imageData, bool forceRgba, ID_TIME_T *timestamp )
{
	if ( !forceRgba )
	{
		if ( TryLoadFile( filename, "dds", imageData, timestamp, &fhImageData::LoadDDS ) )
		{
			return true;
		}
	}

	if ( TryLoadFile( filename, "tga", imageData, timestamp, &fhImageData::LoadSTB ) )
	{
		return true;
	}
	if ( TryLoadFile( filename, "png", imageData, timestamp, &fhImageData::LoadSTB ) )
	{
		return true;
	}
	if ( TryLoadFile( filename, "jpg", imageData, timestamp, &fhImageData::LoadSTB ) )
	{
		return true;
	}

	return false;
}

fhImageData::fhImageData()
	: data(nullptr)
	, format(pixelFormat_t::None)
	, numFaces(0)
	, numLevels(0)
	, timestamp(0) {
	name[0] = '\0';
}

fhImageData::fhImageData(fhImageData&& other)
	: data( other.data )
	, format( other.format )
	, numFaces( other.numFaces )
	, numLevels( other.numLevels )
	, timestamp( other.timestamp ) {

	memcpy( &this->faces, &other.faces, sizeof( this->faces ) );

	other.data = nullptr;
	other.format = pixelFormat_t::None;
	other.numFaces = 0;
	other.numLevels = 0;
	other.timestamp = 0;
}

fhImageData::~fhImageData() {
	Clear();
}

fhImageData& fhImageData::operator=(fhImageData&& other) {
	Clear();

	this->data = other.data;
	this->format = other.format;
	this->numFaces = other.numFaces;
	this->numLevels = other.numLevels;
	this->timestamp = other.timestamp;

	memcpy( &this->faces, &other.faces, sizeof( this->faces ) );

	other.data = nullptr;
	other.format = pixelFormat_t::None;
	other.numFaces = 0;
	other.numLevels = 0;
	other.timestamp = 0;

	return *this;
}

void fhImageData::Clear() {
	if (data) {
		R_StaticFree(data);
		data = nullptr;
	}

	format = pixelFormat_t::None;
	numFaces = 0;
	numLevels = 0;
	timestamp = 0;

	assert(!IsValid());
}

bool fhImageData::IsValid() const {
	if (!data)
		return false;

	if (numFaces == 0)
		return false;

	if (numLevels == 0)
		return false;

	if (format == pixelFormat_t::None)
		return false;

	return true;
}

bool fhImageData::LoadRgbaFromMemory( const byte* pic, uint32 width, uint32 height ) {
	Clear();

	const int numBytes = width * height * 4;

	this->format = pixelFormat_t::RGBA;
	this->numFaces = 1;
	this->numLevels = 1;
	this->data = (byte*)R_StaticAlloc( numBytes );
	memcpy( this->data, pic, numBytes );

	auto& level = this->faces[0].levels[0];
	level.height = height;
	level.width = width;
	level.size = numBytes;
	level.offset = 0;

	return true;
}

bool fhImageData::LoadFile(const char* filename, bool toRgba /* = false */) {
	idStr name = filename;
	name.DefaultFileExtension(".tga");

	if (name.Length()<5) {
		return false;
	}

	name.ToLower();
	idStr ext;
	name.ExtractFileExtension(ext);

	bool ok = false;
	if ( ext == "tga" || ext == "png" || ext == "psd" || ext == "gif" || ext == "bmp" )
	{
		ok = LoadSTB( name.c_str(), toRgba );
	}
	else if ( ext == "dds" )
	{
		if ( toRgba )
		{
			common->Warning( "Cannot convert compressed data to RGBA" );
			return false;
		}
		ok = LoadDDS( name.c_str() );
	}

	//
	// convert to exact power of 2 sizes
	//
	if (ok && this->data && this->numFaces == 1 && this->numLevels == 1 && this->format == pixelFormat_t::RGBA) {

		int	scaled_width, scaled_height;

		int w = GetWidth();
		int h = GetHeight();

		for (scaled_width = 1; scaled_width < w; scaled_width <<= 1)
			;
		for (scaled_height = 1; scaled_height < h; scaled_height <<= 1)
			;

		if (scaled_width != w || scaled_height != h) {
			if (globalImages->image_roundDown.GetBool() && scaled_width > w) {
				scaled_width >>= 1;
			}
			if (globalImages->image_roundDown.GetBool() && scaled_height > h) {
				scaled_height >>= 1;
			}

			byte* resampledBuffer = R_ResampleTexture(GetData(), w, h, scaled_width, scaled_height);
			R_StaticFree(this->data);
			this->data = resampledBuffer;
			this->faces[0].levels[0].width = scaled_width;
			this->faces[0].levels[0].height = scaled_height;
			this->faces[0].levels[0].offset = 0;
			this->faces[0].levels[0].size = scaled_width * scaled_height * 4;
		}
	}

	return ok;
}

bool fhImageData::LoadDDS( const char* filename, bool toRgba ) {
	fhStaticBuffer<byte> buffer;
	if (LoadFileIntoBuffer(filename, buffer)) {
		return LoadDDS(buffer, toRgba);
	}

	return false;
}

bool fhImageData::LoadSTB( const char *filename, bool toRgba )
{
	fhStaticBuffer< byte > buffer;
	if ( LoadFileIntoBuffer( filename, buffer ) )
	{
		return LoadSTB( buffer, toRgba );
	}

	return false;
}

bool fhImageData::LoadDDS( fhStaticBuffer< byte > &buffer, bool toRgba )
{

	if ( toRgba )
	{
		//TODO(johl): currently we just assume, that data from dds files can not be
		//            converted to RGBA.
		return false;
	}

	if ( buffer.Num() < sizeof( ddsFileHeader_t ) )
	{
		return false;
	}

	data = buffer.Release();

	unsigned long    magic  = LittleLong( *( unsigned long * ) data );
	ddsFileHeader_t *header = ( ddsFileHeader_t * ) ( data + 4 );

	// ( not byte swapping dwReserved1 dwReserved2 )
	header->dwSize              = LittleLong( header->dwSize );
	header->dwFlags             = LittleLong( header->dwFlags );
	header->dwHeight            = LittleLong( header->dwHeight );
	header->dwWidth             = LittleLong( header->dwWidth );
	header->dwPitchOrLinearSize = LittleLong( header->dwPitchOrLinearSize );
	header->dwDepth             = LittleLong( header->dwDepth );
	header->dwMipMapCount       = LittleLong( header->dwMipMapCount );
	header->dwCaps1             = LittleLong( header->dwCaps1 );
	header->dwCaps2             = LittleLong( header->dwCaps2 );

	header->ddspf.dwSize        = LittleLong( header->ddspf.dwSize );
	header->ddspf.dwFlags       = LittleLong( header->ddspf.dwFlags );
	header->ddspf.dwFourCC      = LittleLong( header->ddspf.dwFourCC );
	header->ddspf.dwRGBBitCount = LittleLong( header->ddspf.dwRGBBitCount );
	header->ddspf.dwRBitMask    = LittleLong( header->ddspf.dwRBitMask );
	header->ddspf.dwGBitMask    = LittleLong( header->ddspf.dwGBitMask );
	header->ddspf.dwBBitMask    = LittleLong( header->ddspf.dwBBitMask );
	header->ddspf.dwABitMask    = LittleLong( header->ddspf.dwABitMask );

	const char *fourcc = ( const char * ) &header->ddspf.dwFourCC;

	if ( header->ddspf.dwFlags & DDSF_FOURCC )
	{
		switch ( header->ddspf.dwFourCC )
		{
			case DDS_MAKEFOURCC( 'D', 'X', 'T', '1' ):
				if ( header->ddspf.dwFlags & DDSF_ALPHAPIXELS )
				{
					format = pixelFormat_t::DXT1_RGBA;
				}
				else
				{
					format = pixelFormat_t::DXT1_RGB;
				}
				break;
			case DDS_MAKEFOURCC( 'D', 'X', 'T', '3' ):
				format = pixelFormat_t::DXT3_RGBA;
				break;
			case DDS_MAKEFOURCC( 'D', 'X', 'T', '5' ):
				format = pixelFormat_t::DXT5_RGBA;
				break;
			case DDS_MAKEFOURCC( 'A', 'T', 'I', '2' ):
				format = pixelFormat_t::RGTC;
				break;
			case DDS_MAKEFOURCC( 'R', 'X', 'G', 'B' ):
				format = pixelFormat_t::DXT5_RxGB;
				break;
			default:
				common->Warning( "Invalid compressed internal format\n" );
				return false;
		}
	}
	else if ( ( header->ddspf.dwFlags & DDSF_RGBA ) && header->ddspf.dwRGBBitCount == 32 )
	{
		format = pixelFormat_t::BGRA;
	}
	else if ( ( header->ddspf.dwFlags & DDSF_RGB ) && header->ddspf.dwRGBBitCount == 32 )
	{
		format = pixelFormat_t::BGRA;
	}
	else if ( ( header->ddspf.dwFlags & DDSF_RGB ) && header->ddspf.dwRGBBitCount == 24 )
	{
		if ( header->ddspf.dwFlags & DDSF_ID_INDEXCOLOR )
		{
			common->Warning( "Invalid uncompressed internal format\n" );
			return false;
		}
		else
		{
			format = pixelFormat_t::BGR;
		}
	}
	else if ( header->ddspf.dwRGBBitCount == 8 )
	{
		assert( false && "not supported" );
		common->Warning( "Invalid uncompressed internal format\n" );
		return false;
	}
	else
	{
		common->Warning( "Invalid uncompressed internal format\n" );
		return false;
	}

	if ( header->dwCaps2 & DDSCAPS2_CUBEMAP )
	{
		const char *missingFace = nullptr;
		if ( !( header->dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEX ) )
			missingFace = "positiv x";
		else if ( !( header->dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX ) )
			missingFace = "negative x";
		else if ( !( header->dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEY ) )
			missingFace = "positiv y";
		else if ( !( header->dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY ) )
			missingFace = "negative y";
		else if ( !( header->dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ ) )
			missingFace = "positiv z";
		else if ( !( header->dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ ) )
			missingFace = "negative z";

		if ( missingFace )
		{
			common->Warning( "not a valid cube map, face '%s' is missing", missingFace );
			return false;
		}

		this->numFaces = 6;
	}
	else
	{
		this->numFaces = 1;
	}

	this->numLevels = header->dwMipMapCount;

	byte *imagedata = this->data + sizeof( ddsFileHeader_t ) + 4;

	for ( uint32 j = 0; j < this->numFaces; ++j )
	{
		int uw = header->dwWidth;
		int uh = header->dwHeight;

		for ( uint32 i = 0; i < numLevels; i++ )
		{
			uint32 numBytes = DataSize( uw, uh, format );

			uint32 faceIndex = j;// ddsFace2doomFace[j];

			faces[ faceIndex ].levels[ i ].offset = static_cast< uint32 >( ( uintptr_t ) imagedata - ( uintptr_t ) this->data );
			faces[ faceIndex ].levels[ i ].width  = uw;
			faces[ faceIndex ].levels[ i ].height = uh;
			faces[ faceIndex ].levels[ i ].size   = numBytes;

			imagedata += numBytes;
			uw /= 2;
			uh /= 2;
			if ( uw < 1 )
			{
				uw = 1;
			}
			if ( uh < 1 )
			{
				uh = 1;
			}
		}
	}

	return true;
}

bool fhImageData::LoadSTB( fhStaticBuffer< byte > &buffer, bool toRgba )
{
	int   width, height, channels;
	byte *buf = stbi_load_from_memory( buffer.Get(), buffer.Num(), &width, &height, &channels, 4 );
	if ( buf == nullptr )
	{
		return false;
	}

	level_t level = {};
	level.width   = width;
	level.height  = height;
	level.size    = width * height * 4;
	level.offset  = 0;

	fhStaticBuffer< byte > rgba( level.size );
	memcpy( rgba.Get(), buf, level.size );

	this->faces[ 0 ].levels[ 0 ] = level;
	this->numFaces               = 1;
	this->numLevels              = 1;
	this->data                   = rgba.Release();
	this->format                 = pixelFormat_t::RGBA;

	STBI_FREE( buf );

	return true;
}

uint32 fhImageData::GetWidth(uint32 level) const {
	assert(numFaces > 0);
	assert(numLevels > level);
	return faces[0].levels[level].width;
}
uint32 fhImageData::GetHeight(uint32 level) const {
	assert(numFaces > 0);
	assert(numLevels > level);
	return faces[0].levels[level].height;
}

uint32 fhImageData::GetSize(uint32 level) const {
	assert(numFaces > 0);
	assert(numLevels > level);
	return faces[0].levels[level].size;
}

uint32 fhImageData::GetNumFaces() const {
	return numFaces;
}

uint32 fhImageData::GetNumLevels() const {
	return numLevels;
}

uint32 fhImageData::GetMaxNumLevels() const {
	uint32 levels = 0;
	uint32 w = GetWidth();
	uint32 h = GetHeight();

	while (w > 0 || h > 0) {
		levels += 1;
		w /= 2;
		h /= 2;
	}

	return levels;
}

pixelFormat_t fhImageData::GetPixelFormat() const {
	return format;
}

ID_TIME_T fhImageData::GetTimeStamp() const {
	return timestamp;
}

const byte* fhImageData::GetData(uint32 face, uint32 level) const {
	if (face >= numFaces)
		return nullptr;

	if (level >= numLevels)
		return nullptr;

	return data + faces[face].levels[level].offset;
}

byte* fhImageData::GetData( uint32 face, uint32 level ) {
	if (face >= numFaces)
		return nullptr;

	if (level >= numLevels)
		return nullptr;

	return data + faces[face].levels[level].offset;
}

bool fhImageData::LoadFileIntoBuffer(const char* filename, fhStaticBuffer<byte>& buffer) {

	fhFileHandle file = fileSystem->OpenFileRead(filename);
	if (!file) {
		return false;
	}

	int	len = file->Length();

	ID_TIME_T time = file->Timestamp();
	if (time > this->timestamp) {
		timestamp = time;
	}

	strncpy(this->name, filename, Min(strlen(filename), sizeof(this->name) - 1));

	buffer.Free();
	buffer.Allocate(len);
	file->Read(buffer.Get(), len);
	file.Close();

	return true;
}

bool fhImageData::LoadCubeMap( const fhImageData sides[6], const char* name ) {
	Clear();
	strncpy( this->name, name, MAX_IMAGE_NAME );

	int	size = 0;
	int bytesPerSide = 0;
	ID_TIME_T timestamp = 0;
	pixelFormat_t format = pixelFormat_t::None;
	fhStaticBuffer<byte> buffer;

	for (int i = 0; i < 6; i++) {
		const fhImageData& side = sides[i];

		if (i == 0) {
			timestamp = side.GetTimeStamp();
			size = side.GetWidth();
			bytesPerSide = size * size * 4; //4 bytes, due to RGBA
			format = side.GetPixelFormat();
			buffer.Allocate( bytesPerSide * 6 );
		}

		if (side.GetWidth() != size || side.GetHeight() != size) {
			common->Warning( "Mismatched sizes on cube map '%s'", side.GetName() );
			return false;
		}

		if (side.GetPixelFormat() != format) {
			common->Warning( "Mismatched pixel format on cube map '%s'", side.GetName() );
			return false;
		}

		if (side.GetSize( 0 ) != bytesPerSide) {
			common->Warning( "Unexpected size of cube map side '%s'", side.GetName() );
			return false;
		}

		memcpy( &buffer.Get()[i * bytesPerSide], side.GetData(), bytesPerSide );
	}

	this->data = buffer.Release();
	this->numFaces = 6;
	this->numLevels = 1;
	this->format = format;
	this->timestamp = timestamp;

	for (int i = 0; i < 6; ++i) {
		auto& level = this->faces[i].levels[0];
		level.width = size;
		level.height = size;
		level.offset = bytesPerSide * i;
		level.size = bytesPerSide;
	}

	return true;
}

bool fhImageData::LoadProgram( const char* program ) {
	Clear();

	fhImageProgram imageProgram;
	idStr::Copynz( name, program, sizeof( name ) - 1 );
	return imageProgram.LoadImageProgram( program, this, nullptr );
}
