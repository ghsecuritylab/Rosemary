#ifndef __MEDIA_READER_H__
#define	__MEDIA_READER_H__

#include <stdint.h>

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
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>
#ifdef __cplusplus
}
#endif


typedef struct PACKET_TYPE{
	uint8_t		*buf;
	int32_t		size;
	int32_t		key;
	int64_t		time;
}PACKET_TYPE;

class CMediaReader
{
public:
	CMediaReader();
	~CMediaReader();

public:
	bool OpenFile(const char *fileName);
	void CloseFile();
	int32_t ReadStream( int32_t type, uint8_t *buf, int32_t *size, int32_t *key, int64_t *timeStamp );
	int32_t GetVideoSeqInfo( uint8_t *buf );
	int32_t GetAudioSeqInfo( uint8_t *buf );

	enum{
		MEDIA_TYPE_VIDEO,
		MEDIA_TYPE_AUDIO,
	};

	bool GetCodecTagId( int32_t type, int32_t *tag, int32_t *id )
	{
		if( type == MEDIA_TYPE_VIDEO && m_VideoStream )
		{
			*tag = m_VideoStream->codec->codec_tag;
			*id = m_VideoStream->codec->codec_id;
			return true;
		}
		else if( type == MEDIA_TYPE_AUDIO )
		{
			*tag = m_AudioStream->codec->codec_tag;
			*id = m_AudioStream->codec->codec_id;
			return true;
		}
		return false;
	}

	bool GetVideoResolution( int32_t *width, int32_t *height )
	{
		if( m_VideoStream )
		{
			*width = m_VideoStream->codec->coded_width;
			*height = m_VideoStream->codec->coded_height;
			return true;
		}
		return false;
	}

	bool GetVideoFramerate( int32_t *fpsNum, int32_t *fpsDen )
	{
		if( m_VideoStream )
		{
			*fpsNum = m_VideoStream->avg_frame_rate.num;
			*fpsDen = m_VideoStream->avg_frame_rate.den;
			return true;
		}
		return false;
	}

private:
	AVFormatContext		*m_pFormatCtx;
	//	Video Stream Information
	int32_t				m_VideoStreamIndex;
	AVStream			*m_VideoStream;
	AVCodec				*m_pVideoCodec;
	int32_t				m_NalLengthSize;

	int32_t				m_CodecType;

	//	Audio Stream Information
	int32_t				m_AudioStreamIndex;
	AVStream			*m_AudioStream;
	AVCodec				*m_pAudioCodec;

	//	Private Functions
	int32_t GetSequenceInformation( AVStream *stream, uint8_t *buf );

};

#endif	//	__MEDIA_READER_H__