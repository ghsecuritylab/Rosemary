#include <unistd.h>
#include <sys/time.h>


//  FFMPEG Headers
#ifdef __cplusplus
 #define __STDC_CONSTANT_MACROS
 #ifdef _STDINT_H
  #undef _STDINT_H
 #endif
 # include <stdint.h>
#endif

#ifdef __cplusplus
extern "C"{
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/opt.h"
#include "libavutil/pixdesc.h"
#include "libavdevice/avdevice.h"
#ifdef __cplusplus
}
#endif


#include <nx_fourcc.h>
#include <nx_video_api.h>
#include <nx_dsp.h>
#include <nx_graphictools.h>
#include "queue.h"
#include "NX_Semaphore.h"
#include "codec_info.h"


//#define DUMP_FILE_FORMAT
#define	ENABLE_DISPLAY

//#define	CHECK_IMG_PROC_TIME

#ifdef	ENABLE_DISPLAY
	#define	DISPLAY_DEC_OUT
	// #define	DISPLAY_IMG_OUT
#endif


#define	LCD_WIDTH	1024
#define	LCD_HEIGHT	600

#define	SRC_WIDTH	1280
#define	SRC_HEIGHT	720

// #define	ENC_WIDTH	1920
// #define	ENC_HEIGHT	1080
#define	ENC_WIDTH	720
#define	ENC_HEIGHT	480

#define	RCV_V2

#define	NX_MAX_NUM_SPS		3
#define	NX_MAX_SPS_SIZE		1024
#define	NX_MAX_NUM_PPS		3
#define	NX_MAX_PPS_SIZE		1024

typedef struct {
	int32_t		version;
	int32_t		profile_indication;
	int32_t		compatible_profile;
	int32_t		level_indication;
	int32_t		nal_length_size;
	int32_t		num_sps;
	int32_t		sps_length[NX_MAX_NUM_SPS];
	uint8_t		sps_data  [NX_MAX_NUM_SPS][NX_MAX_SPS_SIZE];
	int32_t		num_pps;
	int32_t		pps_length[NX_MAX_NUM_PPS];
	uint8_t		pps_data  [NX_MAX_NUM_PPS][NX_MAX_PPS_SIZE];
} NX_AVCC_TYPE;


typedef struct tFFMPEG_STREAM_READER {
	AVFormatContext		*fmt_ctx;

	//	Video Stream Information
	int32_t				video_stream_idx;
	AVStream			*video_stream;
	AVCodec				*video_codec;

	NX_AVCC_TYPE		h264AvcCHeader;		//	for h.264

	//	Audio Stream Information
	int32_t				audio_stream_idx;
	AVStream			*audio_stream;
	AVCodec				*audio_codec;
} FFMPEG_STREAM_READER;


typedef struct TRANS_APP_DATA
{
	int32_t				srcWidth;
	int32_t				srcHeight;
	int32_t				encWidth;
	int32_t				encHeight;

	NX_QUEUE			decRelQueue;
	NX_QUEUE			decOutQueue;
	NX_QUEUE			imgOutQueue;
	NX_QUEUE			encInputQueue;

	NX_SEMAPHORE		*pSemDecOutBuf;			//	Post (Image Processing Thread), Pend(Decoding Thread), Initialize Value(Decodeable Buffer Size=4)
	NX_SEMAPHORE		*pSemImgInBuf;			//	Post (Decoding Thread) , Pend (Image Processing Thread), Initial Value (0)
	NX_SEMAPHORE		*pSemImgOutBuf;			//	Post (Encoding Thread), Pend (Image Process Thread), Initial Value(Max Out Buffer=3)
	NX_SEMAPHORE		*pSemEncInBuf;			//	Post (Image Processing Thread), Pend(Encoding Thread), Initial Value(0)

	pthread_t			hDecThread;				//	Decode Loop Thread Handler
	pthread_t			hEncThread;				//	Encode Loop Thread Handler
	pthread_t			hImgProcThread;			//	Image Processing Loop Thread Handler

	char				*inFileName;
	char				*outFileName;


	NX_VID_MEMORY_HANDLE outImageProc[3];
	NX_VID_MEMORY_INFO	decOutImage[MAX_DEC_FRAME_BUFFERS];	//	Decoder Output

} TRANS_APP_DATA;


uint8_t streamBuffer[8*1024*1024];
uint8_t seqData[1024*4];


//==============================================================================
static int32_t NX_ParseSpsPpsFromAVCC( uint8_t *extraData, int32_t extraDataSize, NX_AVCC_TYPE *avcCInfo );
static void NX_MakeH264StreamAVCCtoANNEXB( NX_AVCC_TYPE *avcc, uint8_t *pBuf, int32_t *size );
static void dumpdata( void *data, int32_t len, const char *msg );



#ifndef MKTAG
#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#endif

#define PUT_LE32(_p, _var) \
	*_p++ = (uint8_t)((_var)>>0);  \
	*_p++ = (uint8_t)((_var)>>8);  \
	*_p++ = (uint8_t)((_var)>>16); \
	*_p++ = (uint8_t)((_var)>>24); 

#define PUT_BE32(_p, _var) \
	*_p++ = (uint8_t)((_var)>>24);  \
	*_p++ = (uint8_t)((_var)>>16);  \
	*_p++ = (uint8_t)((_var)>>8); \
	*_p++ = (uint8_t)((_var)>>0); 

#define PUT_LE16(_p, _var) \
	*_p++ = (uint8_t)((_var)>>0);  \
	*_p++ = (uint8_t)((_var)>>8);  


#define PUT_BE16(_p, _var) \
	*_p++ = (uint8_t)((_var)>>8);  \
	*_p++ = (uint8_t)((_var)>>0);  




FFMPEG_STREAM_READER *OpenMediaFile( const char *fileName )
{
	FFMPEG_STREAM_READER *streamReader = NULL;

	AVFormatContext *fmt_ctx = NULL;
	AVInputFormat *iformat = NULL;

	AVCodec *video_codec = NULL;
	AVCodec *audio_codec = NULL;
	AVStream *video_stream = NULL;
	AVStream *audio_stream = NULL;
	int32_t video_stream_idx = -1;
	int32_t audio_stream_idx = -1;
	int32_t i;

	fmt_ctx = avformat_alloc_context();

	fmt_ctx->flags |= CODEC_FLAG_TRUNCATED;

	if ( avformat_open_input(&fmt_ctx, fileName, iformat, NULL) < 0 )
	{
		printf("avformat_open_input() failed\n");
		return NULL;
	}

	/* fill the streams in the format context */
	if ( av_find_stream_info(fmt_ctx) < 0)
	{
		av_close_input_file( fmt_ctx );
		printf("av_find_stream_info() failed\n");
		return NULL;
	}

#ifdef	DUMP_FILE_FORMAT
	av_dump_format(fmt_ctx, 0, fileName, 0);
#endif

	//	Video Codec Binding
	for( i=0; i<(int32_t)fmt_ctx->nb_streams ; i++ )
	{
		AVStream *stream = fmt_ctx->streams[i];

		if( stream->codec->codec_type == AVMEDIA_TYPE_VIDEO )
		{
			if( !(video_codec=avcodec_find_decoder(stream->codec->codec_id)) )
			{
				printf( "Unsupported codec (id=%d) for input stream %d\n", stream->codec->codec_id, stream->index );
				goto ErrorExit;
			}

			if( avcodec_open2(stream->codec, video_codec, NULL)<0 )
			{
				printf( "Error while opening codec for input stream %d\n", stream->index );
				goto ErrorExit;
			}
			else
			{
				if( video_stream_idx == -1 )
				{
					video_stream_idx = i;
					video_stream = stream;;
				}
				else
				{
					avcodec_close( stream->codec );
				}
			}
		}
		else if( stream->codec->codec_type == AVMEDIA_TYPE_AUDIO )
		{
			if( !(audio_codec=avcodec_find_decoder(stream->codec->codec_id)) )
			{
				printf( "Unsupported codec (id=%d) for input stream %d\n", stream->codec->codec_id, stream->index );
				goto ErrorExit;
			}

			if( avcodec_open2(stream->codec, audio_codec, NULL)<0 )
			{
				printf( "Error while opening codec for input stream %d\n", stream->index );
				goto ErrorExit;
			}
			else
			{
				if( audio_stream_idx == -1 )
				{
					audio_stream_idx = i;
					audio_stream = stream;;
				}
				else
				{
					avcodec_close( stream->codec );
				}
			}
		}
	}

	streamReader = (FFMPEG_STREAM_READER *)malloc(sizeof(FFMPEG_STREAM_READER));
	streamReader->fmt_ctx = fmt_ctx;

	streamReader->video_stream_idx = video_stream_idx;
	streamReader->video_stream     = video_stream;
	streamReader->video_codec      = video_codec;

	streamReader->audio_stream_idx = audio_stream_idx;
	streamReader->audio_stream     = audio_stream;
	streamReader->audio_codec      = audio_codec;

	return streamReader;

ErrorExit:
	if( streamReader )
	{
		free( streamReader );
	}
	if( fmt_ctx )
	{
		av_close_input_file( fmt_ctx );
	}
	return NULL;

}

void CloseFile( FFMPEG_STREAM_READER *fmt_ctx )
{
	if( fmt_ctx )
	{
		av_close_input_file( (struct AVFormatContext*)fmt_ctx->fmt_ctx );
		free(fmt_ctx);
	}
}


int32_t GetSequenceInformation( FFMPEG_STREAM_READER *streamReader, AVStream *stream, uint8_t *buffer, int32_t size )
{
	uint8_t *pbHeader = buffer;
	enum AVCodecID codecId = stream->codec->codec_id;
	int32_t fourcc;
	int32_t frameRate = 0;
	int32_t nMetaData = stream->codec->extradata_size;
	uint8_t *pbMetaData = stream->codec->extradata;
	int32_t retSize = 0;
	uint32_t tag = stream->codec->codec_tag;

	if (stream->avg_frame_rate.den && stream->avg_frame_rate.num)
		frameRate = (int32_t)((double)stream->avg_frame_rate.num/(double)stream->avg_frame_rate.den);
	if (!frameRate && stream->r_frame_rate.den && stream->r_frame_rate.num)
		frameRate = (int32_t)((double)stream->r_frame_rate.num/(double)stream->r_frame_rate.den);

	if( (codecId == CODEC_ID_H264) && (stream->codec->extradata_size>0) )
	{
		if( stream->codec->extradata[0] == 0x1 )
		{
			NX_ParseSpsPpsFromAVCC( pbMetaData, nMetaData, &streamReader->h264AvcCHeader );
			NX_MakeH264StreamAVCCtoANNEXB(&streamReader->h264AvcCHeader, buffer, &retSize );
			return retSize;
		}
	}
    else if ( (codecId == CODEC_ID_VC1) )
    {
		retSize = nMetaData;
        memcpy(pbHeader, pbMetaData, retSize);
		//if there is no seq startcode in pbMetatData. VPU will be failed at seq_init stage.
		return retSize;
	}
    else if ( (codecId == CODEC_ID_MSMPEG4V3) )
	{
		switch( tag )
		{
			case MKTAG('D','I','V','3'):
			case MKTAG('M','P','4','3'):
			case MKTAG('M','P','G','3'):
			case MKTAG('D','V','X','3'):
			case MKTAG('A','P','4','1'):
				if( !nMetaData )
				{
					PUT_LE32(pbHeader, MKTAG('C', 'N', 'M', 'V'));	//signature 'CNMV'
					PUT_LE16(pbHeader, 0x00);						//version
					PUT_LE16(pbHeader, 0x20);						//length of header in bytes
					PUT_LE32(pbHeader, MKTAG('D', 'I', 'V', '3'));	//codec FourCC
					PUT_LE16(pbHeader, stream->codec->width);		//width
					PUT_LE16(pbHeader, stream->codec->height);		//height
					PUT_LE32(pbHeader, stream->r_frame_rate.num);	//frame rate
					PUT_LE32(pbHeader, stream->r_frame_rate.den);	//time scale(?)
					PUT_LE32(pbHeader, stream->nb_index_entries);	//number of frames in file
					PUT_LE32(pbHeader, 0);							//unused
					retSize += 32;
				}
				else
				{
					PUT_BE32(pbHeader, nMetaData);
					retSize += 4;
					memcpy(pbHeader, pbMetaData, nMetaData);
					retSize += nMetaData;
				}
				return retSize;
			default:
				break;
			
		}
	}
	else if(  (codecId == CODEC_ID_WMV1) || (codecId == CODEC_ID_WMV2) || (codecId == CODEC_ID_WMV3) )
	{
#ifdef	RCV_V2	//	RCV_V2
        PUT_LE32(pbHeader, ((0xC5 << 24)|0));
        retSize += 4; //version
        PUT_LE32(pbHeader, nMetaData);
        retSize += 4;

        memcpy(pbHeader, pbMetaData, nMetaData);
		pbHeader += nMetaData;
        retSize += nMetaData;

        PUT_LE32(pbHeader, stream->codec->height);
        retSize += 4;
        PUT_LE32(pbHeader, stream->codec->width);
        retSize += 4;
        PUT_LE32(pbHeader, 12);
        retSize += 4;
        PUT_LE32(pbHeader, 2 << 29 | 1 << 28 | 0x80 << 24 | 1 << 0);
        retSize += 4; // STRUCT_B_FRIST (LEVEL:3|CBR:1:RESERVE:4:HRD_BUFFER|24)
        PUT_LE32(pbHeader, stream->codec->bit_rate);
        retSize += 4; // hrd_rate
		PUT_LE32(pbHeader, frameRate);            
        retSize += 4; // frameRate
#else	//RCV_V1
        PUT_LE32(pbHeader, (0x85 << 24) | 0x00);
        retSize += 4; //frames count will be here
        PUT_LE32(pbHeader, nMetaData);
        retSize += 4;
        memcpy(pbHeader, pbMetaData, nMetaData);
		pbHeader += nMetaData;
        retSize += nMetaData;
        PUT_LE32(pbHeader, stream->codec->height);
        retSize += 4;
        PUT_LE32(pbHeader, stream->codec->width);
        retSize += 4;
#endif
		return retSize;
	}
	else if( (stream->codec->codec_id == CODEC_ID_RV40 || stream->codec->codec_id == CODEC_ID_RV30 )
		&& (stream->codec->extradata_size>0) )
	{
		if( CODEC_ID_RV40 == stream->codec->codec_id )
		{
			fourcc = MKTAG('R','V','4','0');
		}
		else
		{
			fourcc = MKTAG('R','V','3','0');
		}
        retSize = 26 + nMetaData;
        PUT_BE32(pbHeader, retSize); //Length
        PUT_LE32(pbHeader, MKTAG('V', 'I', 'D', 'O')); //MOFTag
		PUT_LE32(pbHeader, fourcc); //SubMOFTagl
        PUT_BE16(pbHeader, stream->codec->width);
        PUT_BE16(pbHeader, stream->codec->height);
        PUT_BE16(pbHeader, 0x0c); //BitCount;
        PUT_BE16(pbHeader, 0x00); //PadWidth;
        PUT_BE16(pbHeader, 0x00); //PadHeight;
        PUT_LE32(pbHeader, frameRate);
        memcpy(pbHeader, pbMetaData, nMetaData);
		return retSize;
	}
	memcpy( buffer, stream->codec->extradata, stream->codec->extradata_size );

	return stream->codec->extradata_size;
}




static int32_t PasreAVCStream( AVPacket *pkt, int32_t nalLengthSize, uint8_t *buffer, int32_t outBufSize )
{
	int32_t nalLength;

	//	input
	uint8_t *inBuf = pkt->data;
	int32_t inSize = pkt->size;
	int32_t pos=0;

	//	'avcC' format
	do{
		nalLength = 0;

		if( nalLengthSize == 2 )
		{
			nalLength = inBuf[0]<< 8 | inBuf[1];
		}
		else if( nalLengthSize == 3 )
		{
			nalLength = inBuf[0]<<16 | inBuf[1]<<8  | inBuf[2];
		}
		else if( nalLengthSize == 4 )
		{
			nalLength = inBuf[0]<<24 | inBuf[1]<<16 | inBuf[2]<<8 | inBuf[3];
		}
		else if( nalLengthSize == 1 )
		{
			nalLength = inBuf[0];
		}

		inBuf  += nalLengthSize;
		inSize -= nalLengthSize;

		if( 0==nalLength || inSize<(int32_t)nalLength )
		{
			printf("Error : avcC type nal length error (nalLength = %d, inSize=%d, nalLengthSize=%d)\n", nalLength, inSize, nalLengthSize);
			return -1;
		}

		//	put nal start code
		buffer[pos + 0] = 0x00;
		buffer[pos + 1] = 0x00;
		buffer[pos + 2] = 0x00;
		buffer[pos + 3] = 0x01;
		pos += 4;

		memcpy( buffer + pos, inBuf, nalLength );
		pos += nalLength;

		inSize -= nalLength;
		inBuf += nalLength;
	}while( 2<inSize );
	return pos;
}

static int32_t MakeRvStream( AVPacket *pkt, AVStream *stream, uint8_t *buffer, int32_t outBufSize )
{
	uint8_t *p = pkt->data;
    int32_t cSlice, nSlice;
    int32_t i, val, offset;
	int32_t size;

	cSlice = p[0] + 1;
	nSlice =  pkt->size - 1 - (cSlice * 8);
	size = 20 + (cSlice*8);

	PUT_BE32(buffer, nSlice);
	if (AV_NOPTS_VALUE == (unsigned long long)pkt->pts)
	{
		PUT_LE32(buffer, 0);
	}
	else
	{
		PUT_LE32(buffer, (int32_t)((double)(pkt->pts/stream->time_base.den))); // milli_sec
	}
	PUT_BE16(buffer, stream->codec->frame_number);
	PUT_BE16(buffer, 0x02); //Flags
	PUT_BE32(buffer, 0x00); //LastPacket
	PUT_BE32(buffer, cSlice); //NumSegments
	offset = 1;
	for (i = 0; i < (int32_t) cSlice; i++)
	{
		val = (p[offset+3] << 24) | (p[offset+2] << 16) | (p[offset+1] << 8) | p[offset];
		PUT_BE32(buffer, val); //isValid
		offset += 4;
		val = (p[offset+3] << 24) | (p[offset+2] << 16) | (p[offset+1] << 8) | p[offset];
		PUT_BE32(buffer, val); //Offset
		offset += 4;
	}

	memcpy(buffer, pkt->data+(1+(cSlice*8)), nSlice);		
	size += nSlice;

	//printf("size = %6d, nSlice = %6d, cSlice = %4d, pkt->size=%6d, frameNumber=%d\n", size, nSlice, cSlice, pkt->size, stream->codec->frame_number );

	return size;
}

static int32_t MakeVC1Stream( AVPacket *pkt, AVStream *stream, uint8_t *buffer, int32_t outBufSize )
{
	int32_t size=0;
	uint8_t *p = pkt->data;

	if( stream->codec->codec_id == CODEC_ID_VC1 )
	{
		if (p[0] != 0 || p[1] != 0 || p[2] != 1) // check start code as prefix (0x00, 0x00, 0x01)
		{
			*buffer++ = 0x00;
			*buffer++ = 0x00;
			*buffer++ = 0x01;
			*buffer++ = 0x0D;
			size = 4;
			memcpy(buffer, pkt->data, pkt->size);
			size += pkt->size;
		}
		else
		{
			memcpy(buffer, pkt->data, pkt->size);
			size = pkt->size; // no extra header size, there is start code in input stream.
		}
	}
	else
	{
		PUT_LE32(buffer, pkt->size | ((pkt->flags & AV_PKT_FLAG_KEY) ? 0x80000000 : 0));
		size += 4;
#ifdef RCV_V2	//	RCV_V2
		if (AV_NOPTS_VALUE == (unsigned long long)pkt->pts)
		{
			PUT_LE32(buffer, 0);
		}
		else
		{
			PUT_LE32(buffer, (int32_t)((double)(pkt->pts/stream->time_base.den))); // milli_sec
		}
		size += 4;
#endif
		memcpy(buffer, pkt->data, pkt->size);
		size += pkt->size;
	}
	return size;
}


static int32_t MakeDIVX3Stream( AVPacket *pkt, AVStream *stream, uint8_t *buffer, int32_t outBufSize )
{
	int32_t size = pkt->size;
	uint32_t tag = stream->codec->codec_tag;
	if( tag == MKTAG('D', 'I', 'V', '3') || tag == MKTAG('M', 'P', '4', '3') ||
		tag == MKTAG('M', 'P', 'G', '3') || tag == MKTAG('D', 'V', 'X', '3') || tag == MKTAG('A', 'P', '4', '1') )
	{
 		PUT_LE32(buffer,pkt->size);
 		PUT_LE32(buffer,0);
 		PUT_LE32(buffer,0);
 		size += 12;
	}
	memcpy( buffer, pkt->data, pkt->size );
	return size;
}
int32_t ReadStream( FFMPEG_STREAM_READER *streamReader, AVStream *stream, uint8_t *buffer, int32_t *size, int32_t *isKey )
{
	int32_t ret;
	AVPacket pkt;
	enum AVCodecID codecId = stream->codec->codec_id;
	*size = 0;
	do{
		ret = av_read_frame( streamReader->fmt_ctx, &pkt );
		if( ret < 0 )
			return -1;
		if( pkt.stream_index == stream->index )
		{
			//	check codec type
			if( codecId == CODEC_ID_H264 && stream->codec->extradata_size > 0 && stream->codec->extradata[0]==1 )
			{
				*size = PasreAVCStream( &pkt, streamReader->h264AvcCHeader.nal_length_size, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				av_free_packet( &pkt );
				return 0;
			}
			else if(  (codecId == CODEC_ID_VC1) || (codecId == CODEC_ID_WMV1) || (codecId == CODEC_ID_WMV2) || (codecId == CODEC_ID_WMV3) )
			{
				*size = MakeVC1Stream( &pkt, stream, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				av_free_packet( &pkt );
				return 0;
			}
			else if( codecId == CODEC_ID_RV30 || codecId == CODEC_ID_RV40 )
			{
				*size = MakeRvStream( &pkt, stream, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				av_free_packet( &pkt );
				return 0;
			}
			else if( codecId == CODEC_ID_MSMPEG4V3 )
			{
				*size = MakeDIVX3Stream( &pkt, stream, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				av_free_packet( &pkt );
				return 0;
			}
			else
			{
				memcpy(buffer, pkt.data, pkt.size );
				*size = pkt.size;
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				av_free_packet( &pkt );
				return 0;
			}
		}
		av_free_packet( &pkt );
	}while(1);
	return -1;
}

int32_t ReadStream2( FFMPEG_STREAM_READER *streamReader, AVStream *stream,  AVPacket *pkt )
{
	int32_t ret;
	do{
		ret = av_read_frame( streamReader->fmt_ctx, pkt );
		if( ret < 0 )
			return -1;

		if( pkt->stream_index == stream->index )
		{
			return 0;
		}

		av_free_packet( pkt );
		av_init_packet( pkt );
	}while(1);
	return -1;
}


static void dumpdata( void *data, int32_t len, const char *msg )
{
	int32_t i=0;
	uint8_t *byte = (uint8_t *)data;
	printf("Dump Data : %s", msg);
	for( i=0 ; i<len ; i ++ )
	{
		if( i%32 == 0 )	printf("\n\t");
		printf("%.2x", byte[i] );
		if( i%4 == 3 ) printf(" ");
	}
	printf("\n");
}

int32_t isIdrFrame( uint8_t *buf, int32_t size )
{
	int32_t i;
	int32_t isIdr=0;

	for( i=0 ; i<size-4; i++ )
	{
		if( buf[i]==0 && buf[i+1]==0 && ((buf[i+2]&0x1F)==0x05) )	//	Check Nal Start Code & Nal Type
		{
			isIdr = 1;
			break;
		}
	}

	return isIdr;
}


static int32_t NX_ParseSpsPpsFromAVCC( uint8_t *extraData, int32_t extraDataSize, NX_AVCC_TYPE *avcCInfo )
{
	int32_t pos = 0;
	int32_t i;
	int32_t length;
	if( 1!=extraData[0] || 11>extraDataSize ){
		printf( "Error : Invalid \"avcC\" data\n" );
		return -1;
	}

	//	Parser "avcC" format data
	avcCInfo->version				= (int32_t)extraData[pos];			pos++;
	avcCInfo->profile_indication	= (int32_t)extraData[pos];			pos++;
	avcCInfo->compatible_profile	= (int32_t)extraData[pos];			pos++;
	avcCInfo->level_indication		= (int32_t)extraData[pos];			pos++;
	avcCInfo->nal_length_size		= (int32_t)(extraData[pos]&0x03)+1;	pos++;
	//	parser SPSs
	avcCInfo->num_sps				= (int32_t)(extraData[pos]&0x1f);	pos++;
	for( i=0 ; i<avcCInfo->num_sps ; i++){
		length = avcCInfo->sps_length[i] = (int32_t)(extraData[pos]<<8)|extraData[pos+1];
		pos+=2;
		if( (pos+length) > extraDataSize ){
			printf("Error : extraData size too small(SPS)\n" );
			return -1;
		}
		memcpy( avcCInfo->sps_data[i], extraData+pos, length );
		pos += length;
	}

	//	parse PPSs
	avcCInfo->num_pps				= (int32_t)extraData[pos];			pos++;
	for( i=0 ; i<avcCInfo->num_pps ; i++ ){
		length = avcCInfo->pps_length[i] = (int32_t)(extraData[pos]<<8)|extraData[pos+1];
		pos+=2;
		if( (pos+length) > extraDataSize ){
			printf( "Error : extraData size too small(PPS)\n");
			return -1;
		}
		memcpy( avcCInfo->pps_data[i], extraData+pos, length );
		pos += length;
	}
	return 0;
}

static void NX_MakeH264StreamAVCCtoANNEXB( NX_AVCC_TYPE *avcc, uint8_t *pBuf, int32_t *size )
{
	int32_t i;
	int32_t pos = 0;
	for( i=0 ; i<avcc->num_sps ; i++ )
	{
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x01;
		memcpy( pBuf + pos, avcc->sps_data[i], avcc->sps_length[i] );
		pos += avcc->sps_length[i];
	}
	for( i=0 ; i<avcc->num_pps ; i++ )
	{
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x01;
		memcpy( pBuf + pos, avcc->pps_data[i], avcc->pps_length[i] );
		pos += avcc->pps_length[i];
	}
	*size = pos;
}


static int32_t CodecIdToVpuType( int32_t codecId, uint32_t fourcc )
{
	int32_t vpuCodecType =-1;
	printf("codecId = %d, fourcc=%c%c%c%c\n", codecId, fourcc, fourcc>>8, fourcc>>16, fourcc>>24);
	if( codecId == CODEC_ID_MPEG4 || codecId == CODEC_ID_FLV1 )
	{
		vpuCodecType = NX_MP4_DEC;
	}
	else if( codecId == CODEC_ID_MSMPEG4V3 )
	{
		switch( fourcc )
		{
			case MKTAG('D','I','V','3'):
			case MKTAG('M','P','4','3'):
			case MKTAG('M','P','G','3'):
			case MKTAG('D','V','X','3'):
			case MKTAG('A','P','4','1'):
				vpuCodecType = NX_DIV3_DEC;
				break;
			default:
				vpuCodecType = NX_MP4_DEC;
				break;
		}
	}
	else if( codecId == CODEC_ID_H263 || codecId == CODEC_ID_H263P || codecId == CODEC_ID_H263I )
	{
		vpuCodecType = NX_H263_DEC;
	}
	else if( codecId == CODEC_ID_H264 )
	{
		vpuCodecType = NX_AVC_DEC;
	}
	else if( codecId == CODEC_ID_MPEG2VIDEO )
	{
		vpuCodecType = NX_MP2_DEC;
	}
	else if( (codecId == CODEC_ID_WMV3) || (codecId == CODEC_ID_VC1) )
	{
		vpuCodecType = NX_VC1_DEC;
	}
	else if( (codecId == CODEC_ID_RV30) || (codecId == CODEC_ID_RV40) )
	{
		vpuCodecType = NX_RV_DEC;
	}
	else
	{
		printf("Cannot supprot codecid(%d)\n", codecId);
		exit(-1);
	}
	return vpuCodecType;
}


long long GetCurrentTime( void )
{
	long long msec;
	struct timeval tv;
	gettimeofday( &tv, NULL );

	msec = ((long long)tv.tv_sec*1000) + tv.tv_usec*1000;
	return msec;
}



//
//                                |--- Scaling 0
//	Deinterlacing --> Scaling  ------- Scaling 1
//                                |--- Scaling 2
//
void *ImageProcessingThread( void *arg )
{
	uint32_t inIndex;
	TRANS_APP_DATA *pAppData = (TRANS_APP_DATA *)arg;
	NX_SEMAPHORE *pSemDecOutBuf = pAppData->pSemDecOutBuf;	//	Post (Image Processing Thread), Pend(Decoding Thread), Initialize Value(Decodeable Buffer Size=4)
	NX_SEMAPHORE *pSemImgInBuf  = pAppData->pSemImgInBuf ;	//	Post (Decoding Thread) , Pend (Image Processing Thread), Initial Value (0)
	NX_SEMAPHORE *pSemImgOutBuf = pAppData->pSemImgOutBuf;	//	Post (Encoding Thread), Pend (Image Process Thread), Initial Value(Max Out Buffer=3)
	NX_SEMAPHORE *pSemEncInBuf  = pAppData->pSemEncInBuf ;	//	Post (Image Processing Thread), Pend(Encoding Thread), Initial Value(0)

	NX_GT_DEINT_HANDLE hDeint = NULL;
	NX_GT_SCALER_HANDLE hScale = NULL;
	NX_VID_MEMORY_HANDLE pDeintOutImg = NX_VideoAllocateMemory( 4096, pAppData->srcWidth, pAppData->srcHeight,
											NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
	NX_VID_MEMORY_HANDLE pImgOutBuf = NULL;

#ifdef CHECK_IMG_PROC_TIME
	struct timeval startTime, endTime;
	long long fpsTime = 0;
	long long totalTime = 0;
	uint32_t outCount=0;
#endif

#ifdef ENABLE_DISPLAY
	DISPLAY_HANDLE hDsp;
	DISPLAY_INFO dspInfo;
	DSP_IMG_RECT dspSrcRect;
	DSP_IMG_RECT dspDstRect;

	int32_t displayCnt = 0;

	memset( &dspInfo, 0x00, sizeof(dspInfo) );
	dspInfo.port	= 0;
	dspInfo.module	= 0;
#ifdef DISPLAY_DEC_OUT	
	dspInfo.width	= pAppData->srcWidth;
	dspInfo.height	= pAppData->srcHeight;
#endif	
#ifdef DISPLAY_IMG_OUT
	dspInfo.width	= pAppData->encWidth;
	dspInfo.height	= pAppData->encHeight;
#endif

	dspInfo.numPlane	= 1;

	dspSrcRect.left		= 0;
	dspSrcRect.top		= 0;
#ifdef DISPLAY_DEC_OUT		
	dspSrcRect.right	= pAppData->srcWidth;
	dspSrcRect.bottom	= pAppData->srcHeight;
#endif
#ifdef DISPLAY_IMG_OUT	
	dspSrcRect.right	= pAppData->srcWidth;
	dspSrcRect.bottom	= pAppData->srcHeight;
#endif

	dspDstRect.left		= 0;
	dspDstRect.top		= 0;
	dspDstRect.right	= 1280;
	dspDstRect.bottom	= 800;

	dspInfo.dspSrcRect = dspSrcRect;
	dspInfo.dspDstRect = dspDstRect;	

	NX_DspVideoSetPriority( 0, 0 );

	hDsp = NX_DspInit( &dspInfo );
	if( hDsp == NULL )
	{
		printf("Display Failed!!!\n");
		exit(-1);
	}
#endif	//	ENABLE_DISPLAY

	hDeint = NX_GTDeintOpen( pAppData->srcWidth, pAppData->srcHeight, MAX_GRAPHIC_BUF_SIZE );
	hScale = NX_GTSclOpen( pAppData->srcWidth, pAppData->srcHeight, pAppData->encWidth, pAppData->encHeight, MAX_GRAPHIC_BUF_SIZE );

	while( 1 )
	{
		//	Wait Output Buffer
		NX_PendSem(pSemImgOutBuf);
		//	Wait Input Buffer
		NX_PendSem(pSemImgInBuf);

		//	Pop Input Buffer Index
		if( 0 != NX_PopQueue( &pAppData->decOutQueue, (void**)&inIndex ) )
		{
			printf("NX_PopQueue Error decOutQueue\n");
		}
		//	Pop Output Buffer Index
		if( 0 != NX_PopQueue( &pAppData->imgOutQueue, (void**)&pImgOutBuf ) )
		{
			printf("NX_PopQueue Error imgOutQueue\n");
		}

#ifdef CHECK_IMG_PROC_TIME
		gettimeofday(&startTime, NULL);
#endif // CHECK_IMG_PROC_TIME

		//	Decoded Image Buffer(Interlace) --> Temporal Output Buffer
		NX_GTDeintDoDeinterlace( hDeint, &pAppData->decOutImage[inIndex], pDeintOutImg );
		//	Temporal Deinterlace Output Buffer --> Scaled Output Buffer
		NX_GTSclDoScale( hScale, pDeintOutImg, pImgOutBuf );

#ifdef CHECK_IMG_PROC_TIME
		gettimeofday(&endTime, NULL);
		fpsTime += (endTime.tv_sec - startTime.tv_sec)*1000000 + (endTime.tv_usec - startTime.tv_usec);
		outCount ++;
		if( outCount%30 == 0)
		{
			totalTime += fpsTime;
			printf("Total Duration = %lldusec, Lastest 30frame = %lldusec\n", totalTime/outCount, fpsTime/30 );
			fpsTime = 0;
		}
#endif // CHECK_IMG_PROC_TIME

#ifdef DISPLAY_DEC_OUT
		NX_DspQueueBuffer( hDsp, &pAppData->decOutImage[inIndex] );
#endif
#ifdef DISPLAY_IMG_OUT
		NX_DspQueueBuffer( hDsp, pDeintOutImg );
#endif

#ifdef ENABLE_DISPLAY
		if( displayCnt != 0 )
		{
			NX_DspDequeueBuffer( hDsp );
		}
		displayCnt ++;
#endif

		NX_PushQueue( &pAppData->encInputQueue, pImgOutBuf );
		//	Post Semaphore
		NX_PostSem(pSemEncInBuf);		//	Send buffer to encoding thread

		NX_PushQueue( &pAppData->decRelQueue, (void*)inIndex );
		//	Decoder Buffer Use Done
		NX_PostSem(pSemDecOutBuf);		//	Start Encoding
	}
	NX_GTDeintClose( hDeint );
	NX_GTSclClose( hScale );

	return (void*)0xDEADDEAD;
}

#define	MAX_SEQ_BUF_SIZE 1024

void *EncodeThread( void *arg )
{
	TRANS_APP_DATA *pAppData = (TRANS_APP_DATA *)arg;
	//	Encoder Parameters
	NX_VID_ENC_INIT_PARAM encInitParam;
	uint8_t *seqBuffer = (uint8_t *)malloc( MAX_SEQ_BUF_SIZE );
	NX_VID_ENC_HANDLE hEnc;
	NX_VID_ENC_IN encIn;
	NX_VID_ENC_OUT encOut;
	FILE *fdOut = NULL;
	NX_VID_MEMORY_HANDLE encInputImg = NULL;
	NX_SEMAPHORE *pSemImgOutBuf = pAppData->pSemImgOutBuf;	//	Post (Encoding Thread), Pend (Image Process Thread), Initial Value(Max Out Buffer=3)
	NX_SEMAPHORE *pSemEncInBuf  = pAppData->pSemEncInBuf ;	//	Post (Image Processing Thread), Pend(Encoding Thread), Initial Value(0)

	//	Output
	fdOut = fopen( (const char*)pAppData->outFileName, "wb" );

	//	Initialize Encoder
	hEnc = NX_VidEncOpen( NX_AVC_ENC, NULL );

	memset( &encInitParam, 0, sizeof(encInitParam) );
	encInitParam.width = pAppData->encWidth;
	encInitParam.height = pAppData->encHeight;
	encInitParam.gopSize = 30/2;
	encInitParam.bitrate = 5000000;
	encInitParam.fpsNum = 30;
	encInitParam.fpsDen = 1;
	//	Rate Control
	encInitParam.enableRC = 1;		//	Enable Rate Control
	encInitParam.disableSkip = 0;	//	Enable Skip
	encInitParam.maximumQp = 51;	//	Max Qunatization Scale
	encInitParam.initialQp = 23;	//	Default Encoder API ( enableRC == 0 )

	memset( &encIn, 0, sizeof(encIn) );

	NX_VidEncInit( hEnc, &encInitParam );

	if( fdOut )
	{
		int32_t size;
		//	Write Sequence Data
		NX_VidEncGetSeqInfo( hEnc, seqBuffer, &size );
		fwrite( seqBuffer, 1, size, fdOut );
		dumpdata( seqBuffer, size, "sps pps" );
	}

	while(1)
	{
		//	Waiting Sempahore
		NX_PendSem(pSemEncInBuf);

		if( NX_GetQueueCnt(&pAppData->encInputQueue) <= 0 )
		{
			printf("Have No Queue Member!!!\n");
			continue;
		}

		NX_PopQueue( &pAppData->encInputQueue, (void*)&encInputImg );
		encIn.pImage = encInputImg;

		NX_VidEncEncodeFrame( hEnc, &encIn, &encOut );
		if( fdOut && encOut.bufSize>0 )
		{
			//	Write Sequence Data
			fwrite( encOut.outBuf, 1, encOut.bufSize, fdOut );
			//printf("outSize = %d\n", encOut.bufSize);
		}
		//	Writing Semaphore
		NX_PushQueue(&pAppData->imgOutQueue, (void*)encInputImg);
		NX_PostSem(pSemImgOutBuf);
	}
	if( fdOut )
	{
		fclose(fdOut);
	}
	return (void*)0xDEADDEAD;
}


void *DecodeThread( void *arg )
{
	FFMPEG_STREAM_READER *pReader;
	int32_t vpuCodecType;
	VID_ERROR_E vidRet;
	NX_VID_SEQ_IN seqIn;
	NX_VID_SEQ_OUT seqOut;
	NX_VID_DEC_HANDLE hDec;
	NX_VID_DEC_IN decIn;
	NX_VID_DEC_OUT decOut;
	int32_t seqSize = 0;
	int32_t bInit=0, pos=0;
	int32_t readSize, frameCount = 0;
	int32_t isKey = 0;
	int32_t needKey = 1;
	int32_t mp4Class=0;
	int32_t relIndex;
	int32_t seqNeedMoreBuffer = 0;

	TRANS_APP_DATA *pAppData = (TRANS_APP_DATA *)arg;
	NX_SEMAPHORE *pSemDecOutBuf = pAppData->pSemDecOutBuf;	//	Post (Image Processing Thread), Pend(Decoding Thread), Initialize Value(Decodeable Buffer Size=4)
	NX_SEMAPHORE *pSemImgInBuf  = pAppData->pSemImgInBuf ;	//	Post (Decoding Thread) , Pend (Image Processing Thread), Initial Value (0)

	pReader = OpenMediaFile( pAppData->inFileName );
	if( !pReader )
	{
		printf("Cannot open file!!!(%s)\n", pAppData->inFileName);
		exit(-1);
	}

	if( pReader->video_stream_idx == -1 )
	{
		printf("Cannot found video stream!!!\n");
		exit(-1);
	}

	vpuCodecType = CodecIdToVpuType( pReader->video_stream->codec->codec_id, pReader->video_stream->codec->codec_tag );
	if( vpuCodecType < 0 )
	{
		exit(-1);
	}

	mp4Class = fourCCToMp4Class( pReader->video_stream->codec->codec_tag );
	if( mp4Class == -1 )
		mp4Class = codecIdToMp4Class( pReader->video_stream->codec->codec_id );

	hDec = NX_VidDecOpen(vpuCodecType, mp4Class, 0, NULL);
	if( hDec == NULL )
	{
		printf("NX_VidDecOpen(%d) failed!!!\n", vpuCodecType);
		exit(-1);
	}

	seqSize = GetSequenceInformation( pReader, pReader->video_stream, seqData, sizeof(seqData) );
	if( seqSize == 0 )
	{
		printf("Have no SeqData!!!(%d)\n", vpuCodecType);
		exit(-1);
	}

	while( 1 )
	{
		if( 0 == bInit )
		{
			if( seqNeedMoreBuffer == 0 )
			{
				if( seqSize > 0 )
					memcpy( streamBuffer, seqData, seqSize );

				if( 0 != ReadStream( pReader, pReader->video_stream,  (streamBuffer+seqSize), &readSize, &isKey ) )
				{
					break;
				}

				if( needKey )
				{
					if( !isKey )
					{
						continue;
					}
					needKey = 0;
				}
				memset( &seqIn, 0, sizeof(seqIn) );
				seqIn.seqInfo = streamBuffer;
				seqIn.seqSize = readSize+seqSize;
				seqIn.enableUserData = 0;
				seqIn.addNumBuffers = 4;
				vidRet = NX_VidDecParseVideoCfg(hDec, &seqIn, &seqOut);
				seqIn.width = seqOut.width;
				seqIn.height = seqOut.height;
				vidRet = NX_VidDecInit( hDec, &seqIn );
			}
			else
			{
				if( 0 != ReadStream( pReader, pReader->video_stream, streamBuffer, &readSize, &isKey ) )
				{
					break;
				}
				memset( &seqIn, 0, sizeof(seqIn) );
				seqIn.seqInfo = streamBuffer;
				seqIn.seqSize = readSize;
				seqIn.enableUserData = 0;
				seqIn.addNumBuffers = 4;
				vidRet = NX_VidDecParseVideoCfg(hDec, &seqIn, &seqOut);
				seqIn.width = seqOut.width;
				seqIn.height = seqOut.height;
				vidRet = NX_VidDecInit( hDec, &seqIn );
			}

			if( vidRet == 1 )
			{
				seqNeedMoreBuffer = 1;
				continue;
			}

			if( 0 != vidRet )
			{
				printf("Initialize Failed!!!\n");
				exit(-1);
			}

			pos = 0;
			bInit = 1;
		}

		//	Stream Reading
		if( 0 != ReadStream( pReader, pReader->video_stream,  (streamBuffer+pos), &readSize, &isKey ) )
		{
			break;
		}
		pos += readSize;

		//	Release Buffer
		while( NX_GetQueueCnt(&pAppData->decRelQueue) > 0 )
		{
			relIndex = 0;
			if( 0 == NX_PopQueue(&pAppData->decRelQueue, (void**)&relIndex) )
			{
				NX_VidDecClrDspFlag( hDec, &pAppData->decOutImage[relIndex], relIndex );
				break;
			}
		}

		//	Decoding Frame
		decIn.strmBuf = streamBuffer;
		decIn.strmSize = pos;
		decIn.timeStamp = 0;
		decIn.eos = 0;
		vidRet = NX_VidDecDecodeFrame( hDec, &decIn, &decOut );
		pos = 0;
		if( vidRet == VID_NEED_STREAM )
		{
			frameCount ++;
			printf("VID_NEED_STREAM\n");
			continue;
		}
		if( vidRet < 0 )
		{
			printf("Decoding Error!!!\n");
			exit(-2);
		}
		if( decOut.outImgIdx >= 0  )
		{
			NX_PendSem(pSemDecOutBuf);

			//	Push Decoding Image to Decoding Queue
			pAppData->decOutImage[decOut.outImgIdx] = decOut.outImg;
			NX_PushQueue( &pAppData->decOutQueue, (void*)decOut.outImgIdx );
			NX_PostSem(pSemImgInBuf);
		}
	}

	NX_VidDecClose( hDec );
	return (void*)0xDEADDEAD;
}


static int32_t multi_thread_transcoding( TRANS_APP_DATA *appData )
{
	int32_t i;

	//	Create Queue
	NX_InitQueue(&appData->decOutQueue,   16);
	NX_InitQueue(&appData->decRelQueue,   16);
	NX_InitQueue(&appData->imgOutQueue,   16);
	NX_InitQueue(&appData->encInputQueue, 16);

	//	Create Semaphore
	appData->pSemDecOutBuf= NX_CreateSem( 2, 3 );	//	Post (Image Processing Thread), Pend(Decoding Thread), Initialize Value(Decodeable Buffer Size=4)
	appData->pSemImgInBuf = NX_CreateSem( 0, 3 );	//	Post (Decoding Thread) , Pend (Image Processing Thread), Initial Value (0)
	appData->pSemImgOutBuf= NX_CreateSem( 3, 3 );	//	Post (Encoding Thread), Pend (Image Process Thread), Initial Value(Max Out Buffer=3)
	appData->pSemEncInBuf = NX_CreateSem( 0, 3 );	//	Post (Image Processing Thread), Pend(Encoding Thread), Initial Value(0)


	//	Allocate Scaled Image Buffer & Push allocated buffer to Image Output Queue
	for( i=0 ; i<3 ; i++ )
	{
		appData->outImageProc[i] = NX_VideoAllocateMemory( 4096,
									appData->encWidth,
									appData->encHeight,
									NX_MEM_MAP_LINEAR,
									FOURCC_MVS0 );
		NX_PushQueue(&appData->imgOutQueue, (void*)appData->outImageProc[i]);
	}

	if( pthread_create( &appData->hEncThread, NULL, EncodeThread, appData ) < 0 )
	{
		printf("Cannot Create Encoding Thread!!!\n");
		return -1;
	}

	if( pthread_create( &appData->hImgProcThread, NULL, ImageProcessingThread, appData ) < 0 )
	{
		printf("Cannot Create Image Processing Thread!!!\n");
		return -1;
	}
	
	if( pthread_create( &appData->hDecThread, NULL, DecodeThread, appData ) < 0 )
	{
		printf("Cannot Create Decoding Thread!!!\n");
		return -1;
	}
	while(1)
	{
		usleep(1000);
	}

	return 0;
}

void print_usage( const char *appName )
{
	printf(" Usage : %s -i [input file] -o [output file] -r [x,y]\n", appName);
	printf("    -i [input file]      : Input source filename.\n");
	printf("    -o [input file]      : Output source filename.\n");
	printf("    -r [x,y]             : Encoder output resolution.\n\n");
	printf(" example> \n");
	printf("    %s -i 1920x1080.mp4 -o 1280x720 -r 1280,720\n", appName);
}

int32_t main( int32_t argc, char *argv[] )
{
	int32_t opt;
	FFMPEG_STREAM_READER *pReader = NULL;
	TRANS_APP_DATA *appData = (TRANS_APP_DATA *)malloc( sizeof(TRANS_APP_DATA) );
	memset( appData, 0, sizeof(TRANS_APP_DATA) );

	av_register_all();

	appData->encWidth  = ENC_WIDTH;
	appData->encHeight = ENC_HEIGHT;

	while( -1 != (opt=getopt(argc, argv, "hi:o:r:")))
	{
		switch( opt )
		{
			case 'h':	print_usage( argv[0] );	break;
			case 'i':	appData->inFileName  = strdup( optarg );  break;
			case 'o':	appData->outFileName = strdup( optarg );  break;
			case 'r':	sscanf(optarg, "%d,%d", &appData->encWidth, &appData->encHeight);  break;
		}
	}

	if( NULL==appData->inFileName || NULL==appData->outFileName )
	{
		print_usage( argv[0] );
		exit(-1);
	}

	pReader = OpenMediaFile( appData->inFileName );
	if( !pReader )
	{
		printf("Cannot open input file (%s)\n", appData->inFileName);
		exit(-1);
	}
	if( pReader && pReader->video_stream )
	{
		appData->srcWidth  = pReader->video_stream->codec->width;
		appData->srcHeight = pReader->video_stream->codec->height;
	}
	CloseFile(pReader);


	printf("=============================================\n");
	printf("  Input File  : %s\n", appData->inFileName);
	printf("  Output File : %s\n", appData->outFileName);
	printf("  Resolution  : %dx%d\n", appData->encWidth, appData->encHeight);
	printf("=============================================\n");

	multi_thread_transcoding( appData );

	if( appData )
		free( appData );
}
