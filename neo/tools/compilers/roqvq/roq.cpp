/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

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
#include "../../../idlib/precompiled.h"
#pragma hdrstop

#include "roq.h"
#include "codec.h"

roq		*theRoQ;				// current roq file

roq::roq( void )
{
	image = 0;
	quietMode = false;
	encoder = 0;
	previousSize = 0;
	lastFrame = false;
	dataStuff=false;
}

roq::~roq( void )
{
	if (image) delete image;
	if (encoder) delete encoder;
	return;
}

void roq::EncodeQuietly( bool which )
{
	quietMode = which;
}

bool roq::IsQuiet( void )
{
	return quietMode;
}

bool roq::IsLastFrame( void )
{
	return lastFrame;
}

bool roq::Scaleable( void )
{
	return paramFile->IsScaleable();
}

bool roq::ParamNoAlpha( void )
{
	return paramFile->NoAlpha();
}

bool roq::MakingVideo( void )
{
	return true;	//paramFile->timecode];
}

bool roq::SearchType( void )
{
	return	paramFile->SearchType();
}

bool roq::HasSound( void )
{
	return	paramFile->HasSound();
}

int roq::PreviousFrameSize( void )
{
	return	previousSize;
}

int roq::FirstFrameSize( void )
{
	return paramFile->FirstFrameSize();
}

int roq::NormalFrameSize( void )
{
	return	paramFile->NormalFrameSize();
}

const char * roq::CurrentFilename( void )
{
	return currentFile.c_str();
}

void roq::EncodeStream( const char *paramInputFile )
{
	int		onFrame;
	idStr	f0, f1, f2;
	int		morestuff;

	onFrame = 1;

	encoder = new codec;
	paramFile = new roqParam;
	paramFile->numInputFiles = 0;

	paramFile->InitFromFile( paramInputFile );

	if (!paramFile->NumberOfFrames()) {
		return;
	}

	InitRoQFile( paramFile->outputFilename);

	numberOfFrames = paramFile->NumberOfFrames();

	if (paramFile->NoAlpha()==true) common->Printf("encodeStream: eluding alpha\n");

	f0 = "";
	f1 = paramFile->GetNextImageFilename();
	if (( paramFile->MoreFrames() == true )) {
		f2 = paramFile->GetNextImageFilename();
	}
	morestuff = numberOfFrames;

	while( morestuff ) {
		LoadAndDisplayImage( f1 );

		if (onFrame==1) {
			encoder->SparseEncode();
//			WriteLossless();
		} else {
			if (!strcmp( f0, f1 ) && strcmp( f1, f2) ) {
				WriteHangFrame();
			} else {
				encoder->SparseEncode();
			}
		}

		onFrame++;
		f0 = f1;
		f1 = f2;
		if (paramFile->MoreFrames() == true) {
			f2 = paramFile->GetNextImageFilename();
		}
		morestuff--;
		session->UpdateScreen();
	}

//	if (numberOfFrames != 1) {
//		if (image->hasAlpha() && paramFile->NoAlpha()==false) {
//			lastFrame = true;
//			encoder->SparseEncode();
//		} else {
//			WriteLossless();
//		}
//	}
	CloseRoQFile();
}

void roq::Write16Word( word *aWord, idFile *stream )
{
	byte	a, b;

	a = *aWord & 0xff;
	b = *aWord >> 8;

	stream->Write( &a, 1 );
	stream->Write( &b, 1 );
}

void roq::Write32Word( unsigned int *aWord, idFile *stream )
{
	byte	a, b, c, d;

	a = *aWord & 0xff;
	b = (*aWord >> 8) & 0xff;
	c = (*aWord >> 16) & 0xff;
	d = (*aWord >> 24) & 0xff;

	stream->Write( &a, 1 );
	stream->Write( &b, 1 );
	stream->Write( &c, 1 );
	stream->Write( &d, 1 );
}

int roq::SizeFile( idFile *ftosize )
{
	return ftosize->Length();
}

/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */

void roq::InitRoQFile( const char *RoQFilename )
{
	word i;
	static int finit = 0;

	if (!finit) {
		finit++;
		common->Printf("initRoQFile: %s\n", RoQFilename);
		RoQFile = fileSystem->OpenFileWrite( RoQFilename );
//		chmod(RoQFilename, S_IREAD|S_IWRITE|S_ISUID|S_ISGID|0070|0007 );
		if ( !RoQFile ) {
			common->Error("Unable to open output file %s.\n", RoQFilename);
		}

		i = RoQ_ID;
		Write16Word( &i, RoQFile );

		i = 0xffff;
		Write16Word( &i, RoQFile );
		Write16Word( &i, RoQFile );

		// to retain exact file format write out 32 for new roq's
		// on loading this will be noted and converted to 1000 / 30
		// as with any new sound dump avi demos we need to playback
		// at the speed the sound engine dumps the audio
		i = 30;						// framerate
		Write16Word( &i, RoQFile );
	}
	roqOutfile = RoQFilename;
}

void roq::InitRoQPatterns( void )
{
uint j;
word direct;

	direct = RoQ_QUAD_INFO;
	Write16Word( &direct, RoQFile );

	j = 8;

	Write32Word( &j, RoQFile );
	common->Printf("initRoQPatterns: outputting %d bytes to RoQ_INFO\n", j);
	direct = image->hasAlpha();
	if (ParamNoAlpha() == true) direct = 0;

	Write16Word( &direct, RoQFile );

	direct = image->pixelsWide();
	Write16Word( &direct, RoQFile );
	direct = image->pixelsHigh();
	Write16Word( &direct, RoQFile );
	direct = 8;
	Write16Word( &direct, RoQFile );
	direct = 4;
	Write16Word( &direct, RoQFile );
}

void roq::CloseRoQFile( void )
{
	common->Printf("closeRoQFile: closing RoQ file\n");
	fileSystem->CloseFile( RoQFile );
}

void roq::WriteHangFrame( void )
{
uint j;
word direct;
	common->Printf("*******************************************************************\n");
	direct = RoQ_QUAD_HANG;
	Write16Word( &direct, RoQFile);
	j = 0;
	Write32Word( &j, RoQFile);
	direct = 0;
	Write16Word( &direct, RoQFile);
}

void roq::WriteCodeBookToStream( byte *codebook, int csize, word cflags )
{
uint j;
word direct;

	if (!csize) {
		common->Printf("writeCodeBook: false VQ DATA!!!!\n");
		return;
	}

	direct = RoQ_QUAD_CODEBOOK;

	Write16Word( &direct, RoQFile);

	j = csize;

	Write32Word( &j, RoQFile);
	common->Printf("writeCodeBook: outputting %d bytes to RoQ_QUAD_CODEBOOK\n", j);

	direct = cflags;
	Write16Word( &direct, RoQFile);

	RoQFile->Write( codebook, j );
}

void roq::WriteCodeBook( byte *codebook )
{
	memcpy( codes, codebook, 4096 );
}

void roq::WriteFrame( quadcel *pquad )
{
word action, direct;
int	onCCC, onAction, i, code;
uint j;
byte *cccList;
bool *use2, *use4;
int dx,dy,dxMean,dyMean,index2[256],index4[256], dimension;

	cccList = (byte *)Mem_Alloc( numQuadCels * 8);					// maximum length
	use2 = (bool *)Mem_Alloc(256*sizeof(bool));
	use4 = (bool *)Mem_Alloc(256*sizeof(bool));

	for(i=0;i<256;i++) {
		use2[i] = false;
		use4[i] = false;
	}

	action = 0;
	j = onAction = 0;
	onCCC = 2;											// onAction going to go at zero

	dxMean = encoder->MotMeanX();
	dyMean = encoder->MotMeanY();

	if (image->hasAlpha()) dimension = 10; else dimension = 6;

	for (i=0; i<numQuadCels; i++) {
	if ( pquad[i].size && pquad[i].size < 16 ) {
		switch( pquad[i].status ) {
			case	SLD:
				use4[pquad[i].patten[0]] = true;
				use2[codes[dimension*256+(pquad[i].patten[0]*4)+0]] = true;
				use2[codes[dimension*256+(pquad[i].patten[0]*4)+1]] = true;
				use2[codes[dimension*256+(pquad[i].patten[0]*4)+2]] = true;
				use2[codes[dimension*256+(pquad[i].patten[0]*4)+3]] = true;
				break;
			case	PAT:
				use4[pquad[i].patten[0]] = true;
				use2[codes[dimension*256+(pquad[i].patten[0]*4)+0]] = true;
				use2[codes[dimension*256+(pquad[i].patten[0]*4)+1]] = true;
				use2[codes[dimension*256+(pquad[i].patten[0]*4)+2]] = true;
				use2[codes[dimension*256+(pquad[i].patten[0]*4)+3]] = true;
				break;
			case	CCC:
				use2[pquad[i].patten[1]] = true;
				use2[pquad[i].patten[2]] = true;
				use2[pquad[i].patten[3]] = true;
				use2[pquad[i].patten[4]] = true;
		}
	}
	}

	if (!dataStuff) {
		dataStuff=true;
		InitRoQPatterns();
		if (image->hasAlpha()) i = 3584; else i = 2560;
		WriteCodeBookToStream( codes, i, 0 );
		for(i=0;i<256;i++) {
			index2[i] = i;
			index4[i] = i;
		}
	} else {
		j = 0;
		for(i=0;i<256;i++) {
			if (use2[i]) {
				index2[i] = j;
				for(dx=0;dx<dimension;dx++) cccList[j*dimension+dx] = codes[i*dimension+dx];
				j++;
			}
		}
		code = j*dimension;
		direct = j;
		common->Printf("writeFrame: really used %d 2x2 cels\n", j);
		j = 0;
		for(i=0;i<256;i++) {
			if (use4[i]) {
				index4[i] = j;
				for(dx=0;dx<4;dx++) cccList[j*4+code+dx] = index2[codes[i*4+(dimension*256)+dx]];
				j++;
			}
		}
		code += j*4;
		direct = (direct<<8) + j;
		common->Printf("writeFrame: really used %d 4x4 cels\n", j);
		if (image->hasAlpha()) i = 3584; else i = 2560;
		if ( code == i || j == 256) {
			WriteCodeBookToStream( codes, i, 0 );
		} else {
			WriteCodeBookToStream( cccList, code, direct );
		}
	}

	action = 0;
	j = onAction = 0;

	for (i=0; i<numQuadCels; i++) {
	if ( pquad[i].size && pquad[i].size < 16 ) {
		code = -1;
		switch( pquad[i].status ) {
			case	DEP:
				code = 3;
				break;
			case	SLD:
				code = 2;
				cccList[onCCC++] = index4[pquad[i].patten[0]];
				break;
			case	MOT:
				code = 0;
				break;
			case	FCC:
				code = 1;
				dx = ((pquad[i].domain >> 8  )) - 128 - dxMean + 8;
				dy = ((pquad[i].domain & 0xff)) - 128 - dyMean + 8;
				if (dx>15 || dx<0 || dy>15 || dy<0 ) {
					common->Error("writeFrame: FCC error %d,%d mean %d,%d at %d,%d,%d rmse %f\n", dx,dy, dxMean, dyMean,pquad[i].xat,pquad[i].yat,pquad[i].size, pquad[i].snr[FCC] );
				}
				cccList[onCCC++] = (dx<<4)+dy;
				break;
			case	PAT:
				code = 2;
				cccList[onCCC++] = index4[pquad[i].patten[0]];
				break;
			case	CCC:
				code = 3;
				cccList[onCCC++] = index2[pquad[i].patten[1]];
				cccList[onCCC++] = index2[pquad[i].patten[2]];
				cccList[onCCC++] = index2[pquad[i].patten[3]];
				cccList[onCCC++] = index2[pquad[i].patten[4]];
				break;
			case	DEAD:
				common->Error("dead cels in picture\n");
				break;
		}
		if (code == -1) {
			common->Error( "writeFrame: an error occurred writing the frame\n");
		}

		action = (action<<2)|code;
		j++;
		if (j == 8) {
			j = 0;
			cccList[onAction+0] = (action & 0xff);
			cccList[onAction+1] = ((action >> 8) & 0xff);
			onAction = onCCC;
			onCCC += 2;
		}
	}
	}

	if (j) {
		action <<= ((8-j)*2);
		cccList[onAction+0] = (action & 0xff);
		cccList[onAction+1] = ((action >> 8) & 0xff);
	}

	direct = RoQ_QUAD_VQ;

	Write16Word( &direct, RoQFile);

	j = onCCC;
	Write32Word( &j, RoQFile);

	direct  = dyMean;
	direct &= 0xff;
	direct += (dxMean<<8);		// flags

	Write16Word( &direct, RoQFile);

	common->Printf("writeFrame: outputting %d bytes to RoQ_QUAD_VQ\n", j);

	previousSize = j;

	RoQFile->Write( cccList, onCCC );

	Mem_Free( cccList );
	Mem_Free( use2 );
	Mem_Free( use4 );
}

//
// load a frame, create a window (if neccesary) and display the frame
//
void roq::LoadAndDisplayImage( const char * filename )
{
	if (image) delete image;

	common->Printf("loadAndDisplayImage: %s\n", filename);

	currentFile = filename;

	image = new NSBitmapImageRep( filename );

	numQuadCels  = ((image->pixelsWide() & 0xfff0)*(image->pixelsHigh() & 0xfff0))/(MINSIZE*MINSIZE);
	numQuadCels += numQuadCels/4 + numQuadCels/16;

//	if (paramFile->deltaFrames] == true && cleared == false && [image isPlanar] == false) {
//		cleared = true;
//		imageData = [image data];
//		memset( imageData, 0, image->pixelsWide()*image->pixelsHigh()*[image samplesPerPixel]);
//	}

	if (!quietMode) common->Printf("loadAndDisplayImage: %dx%d\n", image->pixelsWide(), image->pixelsHigh());
}

void roq::MarkQuadx( int xat, int yat, int size, float cerror, int choice ) {
}

NSBitmapImageRep* roq::CurrentImage( void )
{
	return	image;
}

int roq::NumberOfFrames( void ) {
	return numberOfFrames;
}

void RoQFileEncode_f( const idCmdArgs &args ) {
	if ( args.Argc() != 2 ) {
		common->Printf( "Usage: roq <paramfile>\n" );
		return;
	}
	theRoQ = new roq;
	int		startMsec = Sys_Milliseconds();
	theRoQ->EncodeStream( args.Argv( 1 ) );
	int		stopMsec = Sys_Milliseconds();
	common->Printf( "total encoding time: %i second\n", ( stopMsec - startMsec ) / 1000 );

}
