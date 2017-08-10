/*
 * Copyright 2012 Michael Chen <omxcodec@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "NX_FFmpegExtractor"
#include <utils/Log.h>

#include <strings.h>

#include <limits.h> /* INT_MAX */
#include <inttypes.h>

#include <media/stagefright/foundation/ABitReader.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <utils/String8.h>
#include <utils/misc.h>

#include <avc_utils.h>

#include "ffmpeg_utils.h"
#include "FFmpegExtractor.h"

#define DEBUG_READ_ENTRY           0
#define DEBUG_DISABLE_VIDEO        0
#define DEBUG_DISABLE_AUDIO        0
#define WAIT_KEY_PACKET_AFTER_SEEK 1
#define DISABLE_NAL_TO_ANNEXB      0
#define DUMP_EXTRA_DATA            0

#define MAX_QUEUE_SIZE (80 * 1024 * 1024)		//	FIXME
#define MIN_AUDIOQ_SIZE (8 * 16 * 1024)			//	128 kbps, 8 seconds
#define MIN_FRAMES		10
#define EXTRACTOR_MAX_PROBE_PACKETS 200

#define FF_MAX_EXTRADATA_SIZE ((1 << 28) - FF_INPUT_BUFFER_PADDING_SIZE)

static unsigned int gstExtractorCounter = 0;

enum {
    NO_SEEK = 0,
    SEEK,
};

static AVPacket flush_pkt;

namespace android {

static status_t addVorbisCodecInfo(
        const sp<MetaData> &meta,
        const void *_codecPrivate, size_t codecPrivateSize);


typedef struct {
	const char *format;
	const char *container;
	const float confidence;
} formatmap;

static formatmap FILE_FORMATS[] = {
	{"asf",					MEDIA_MIMETYPE_CONTAINER_ASF		, 0.88f	},
	{"wmv",					MEDIA_MIMETYPE_CONTAINER_WMV		, 0.88f	},
	{"rm",					MEDIA_MIMETYPE_CONTAINER_RM			, 0.88f	},
	{"flv",					MEDIA_MIMETYPE_CONTAINER_FLV		, 0.88f	},
	{"matroska",			MEDIA_MIMETYPE_CONTAINER_MATROSKA	, 0.59f	},
	{"avi",					MEDIA_MIMETYPE_CONTAINER_AVI		, 0.88f	},
	{"mpegts",				MEDIA_MIMETYPE_CONTAINER_MPEG2TS	, 0.09f	},
	{"mpeg",				MEDIA_MIMETYPE_CONTAINER_MPEG2PS	, 0.24f	},
	{"mov",					MEDIA_MIMETYPE_CONTAINER_MPEG4		, 0.88f	},
	{"ape",					MEDIA_MIMETYPE_AUDIO_APE			, 0.88f	},
};


void NX_Delay( int ms )
{
	usleep( ms*1000 );
}

//////////////////////////////////////////////////////////////////////////////
//
//	Implementaion TrackInfo Class
//
class FFmpegExtractor::TrackInfo : public RefBase{
public:
	TrackInfo(int streamIndex, sp<MetaData> meta , bool isAVC, AVStream *pStream, PacketQueue *pQueue )
		: mMeta(meta)
		, mStreamIndex(streamIndex)
		, mIsAVC(isAVC)
		, mpStream(pStream)
		, mpQueue(pQueue)
	{
	}
	~TrackInfo(){};
	sp<MetaData>	mMeta;
	int				mStreamIndex;
	bool			mIsAVC;
	AVStream		*mpStream;
	PacketQueue		*mpQueue;

private:
	TrackInfo(const TrackInfo &);
	TrackInfo &operator=(const TrackInfo &);
};
//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//	Implementation FFMpegSource class
//
//
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FFMpegSource"

class FFMpegSource : public MediaSource {
	FFMpegSource(const sp<FFmpegExtractor> &extractor, sp<MetaData> meta, bool isAVC, AVStream *stream, PacketQueue *queue);
	virtual status_t start(MetaData *params);
	virtual status_t stop();
	virtual sp<MetaData> getFormat();
	virtual status_t read(MediaBuffer **buffer, const ReadOptions *options);

protected:
	virtual ~FFMpegSource();
	bool Mp3CheckHeader(uint32_t h);

private:
	friend struct FFmpegExtractor;

	sp<FFmpegExtractor> mExtractor;
	sp<MetaData> mMeta;

	enum AVMediaType mMediaType;

	mutable Mutex mLock;

	bool mIsAVC;
	bool mIsHEVC;
	bool mIsMp3;
	size_t mNALLengthSize;
	bool mNal2AnnexB;
	bool mNeedStartKey;

	AVStream *mStream;
	PacketQueue *mQueue;

	int64_t mFirstKeyPktTimestamp;
	int64_t mPktTsPrev;

private:
	FFMpegSource(const FFMpegSource &);
	FFMpegSource &operator=(const FFMpegSource &);
};

FFMpegSource::FFMpegSource(const sp<FFmpegExtractor> &extractor, sp<MetaData> meta, bool isAVC, AVStream *stream, PacketQueue *queue)
	: mExtractor(extractor)
	, mMeta(meta)
	, mIsAVC(isAVC)
	, mIsHEVC(false)
	, mIsMp3(false)
	, mNeedStartKey(true)
	, mStream(stream)
	, mQueue(queue)
{
	const char *mime;

	/* H.264 Video Types */
	{
		mNal2AnnexB = false;

		if (mIsAVC)
		{
			uint32_t type;
			const void *data;
			size_t size;
			CHECK(meta->findData(kKeyAVCC, &type, &data, &size));

			const uint8_t *ptr = (const uint8_t *)data;

			CHECK(size >= 7);
			CHECK_EQ((unsigned)ptr[0], 1u);  // configurationVersion == 1

			// The number of bytes used to encode the length of a NAL unit.
			mNALLengthSize = 1 + (ptr[4] & 3);

			ALOGV("the stream is AVC, the length of a NAL unit: %d", mNALLengthSize);

			mNal2AnnexB = true;
		}
	}

	//	Check MP3 Codec
	if( AV_CODEC_ID_MP3 == stream->codec->codec_id )
	{
		mIsMp3 = true;
	}
	else if( AV_CODEC_ID_HEVC == stream->codec->codec_id )
	{

		/* Codec Private Data
		The format of the MKV CodecPrivate element for HEVC has been aligned with MP4 and GPAC/MP4Box.
		The definition of MP4 for HEVC has not been finalized. The version of MP4Box appears to be
		aligned with the latest version of the HEVC standard. The configuration_version field should be
		kept 0 until CodecPrivate for HEVC have been finalized. Thereafter it shall have the required value of 1.
		The CodecPrivate format is flexible and allows storage of arbitrary NAL units.
		However it is restricted by MP4 to VPS, SPS and PPS headers and SEI messages that apply to the
		whole stream as for example user data. The table below specifies the format:

		Value                               Bits  Description
		-----                               ----  -----------
		configuration_version               8	  The value should be 0 until the format has been finalized.
		                                          Thereafter is should have the specified value (probably 1).
		                                          This allows us to recognize (and ignore) non-standard CodecPrivate
		general_profile_space               2     Specifies the context for the interpretation of general_profile_idc and  general_profile_compatibility_flag
		general_tier_flag                   1     Specifies the context for the interpretation of general_level_idc
		general_profile_idc                 5     Defines the profile of the bitstream
		general_profile_compatibility_flag  32    Defines profile compatibility, see [2] for interpretation
		general_progressive_source_flag     1     Source is progressive, see [2] for interpretation.
		general_interlace_source_flag       1     Source is interlaced, see [2] for interpretation.
		general_nonpacked_constraint_flag   1     If 1 then no frame packing arrangement SEI messages, see [2] for more information
		general_frame_only_constraint_flag  1     If 1 then no fields, see [2] for interpretation
		reserved                            44    Reserved field, value TBD 0
		general_level_idc                   8     Defines the level of the bitstream
		reserved                            4     Reserved Field, value '1111'b
		min_spatial_segmentation_idc        12    Maximum possible size of distinct coded spatial segmentation regions in the pictures of the CVS
		reserved                            6     Reserved Field, value '111111'b
		parallelism_type                    2     0=unknown, 1=slices, 2=tiles, 3=WPP
		reserved                            6     Reserved field, value '111111'b
		chroma_format_idc                   2     See table 6-1, HEVC
		reserved                            5     Reserved Field, value '11111'b
		bit_depth_luma_minus8               3     Bit depth luma minus 8
		reserved                            5     Reserved Field, value '11111'b
		bit_depth_chroma_minus8             3     Bit depth chroma minus 8
		reserved                            16    Reserved Field, value 0
		reserved                            2     Reserved Field, value 0
		max_sub_layers                      3     maximum number of temporal sub-layers
		temporal_id_nesting_flag            1     Specifies whether inter prediction is additionally restricted. see [2] for interpretation.
		size_nalu_minus_one                 2     Size of field NALU Length – 1
		num_parameter_sets                  8     Number of parameter sets

		for (i=0;i<num_parameter_sets;i++) {
		  array_completeness                1     1 when there is no duplicate parameter set with same id in the stream, 0 otherwise or unknown
		  reserved                          1     Value '1'b
		  nal_unit_type                     6     Nal unit type, restricted to VPS, SPS, PPS and SEI, SEI must be of declarative nature which applies to the whole stream such as user data sei.
		  nal_unit_count                    16    Number of nal units
		  for (j=0;j<nalu_unit_count;j+) {
		    size                            16    Size of nal unit
		    for(k=0;k<size;k++) {
		      data[k]                       8     Nalu data+
		    }
		  }
		}*/
		mNal2AnnexB = false;
		if( stream->codec->extradata_size > 21 )
		{
			if( stream->codec->extradata[0]==1 || stream->codec->extradata[1]==1 )
			{
				mIsHEVC = true;
				mNALLengthSize = 1 + (stream->codec->extradata[14 + 7] & 3);
				mNal2AnnexB = true;
			}
		}
	}

	mMediaType = mStream->codec->codec_type;
	mFirstKeyPktTimestamp = AV_NOPTS_VALUE;
	mPktTsPrev	  = AV_NOPTS_VALUE;	
}

FFMpegSource::~FFMpegSource() {
	ALOGD("%s()", __func__);
}

status_t FFMpegSource::start(MetaData* /*params*/) {
	Mutex::Autolock autoLock(mLock);
	//mExtractor->startReaderThread();
	return OK;
}

status_t FFMpegSource::stop() {
	Mutex::Autolock autoLock(mLock);
	mExtractor->stopReaderThread();
	return OK;
}

sp<MetaData> FFMpegSource::getFormat() {
	Mutex::Autolock autoLock(mLock);
	return mMeta;
}

bool FFMpegSource::Mp3CheckHeader(uint32_t h){

	uint32_t v;
    if ((h&0xffe00000)^0xffe00000)
        return false;
    if (((h>>19)&3) == 1)	//version
        return false;
    if (((h>>17)&3) == 0)	//layer
        return false;
    if (((h>>10)&3) == 3)	//frequency
        return false;
    if (((v=(h>>12)&0x0f) == 0x00) || v == 0x0f)	//bit rate
        return false;

	return true;
}

status_t FFMpegSource::read(MediaBuffer **buffer, const ReadOptions *options)
{
	*buffer = NULL;

	Mutex::Autolock autoLock(mLock);

	AVPacket pkt;
	bool seeking = false;
	bool waitKeyPkt = false;
	ReadOptions::SeekMode mode;
	int64_t pktTS = AV_NOPTS_VALUE;
	int64_t seekTimeUs = AV_NOPTS_VALUE;
	int64_t timeUs;
	int key;
	status_t status = OK;

	if (options && options->getSeekTo(&seekTimeUs, &mode))
	{
		ALOGV("~~~%s seekTimeUs: %lld, mode: %d", av_get_media_type_string(mMediaType), seekTimeUs, mode);
		/* add the stream start time */
		if (mStream->start_time != AV_NOPTS_VALUE)
			seekTimeUs += mStream->start_time * av_q2d(mStream->time_base) * 1000000;
		ALOGV("~~~%s seekTimeUs[+startTime]: %lld, mode: %d", av_get_media_type_string(mMediaType), seekTimeUs, mode);

		if (mExtractor->stream_seek(seekTimeUs, mMediaType) == SEEK){
			seeking = true;
		}
	}

	if( mNeedStartKey )
	{
		mNeedStartKey = false;
		waitKeyPkt = true;
	}

retry:
	if (mExtractor->packet_queue_get(mQueue, &pkt, 1) < 0)
	{
		mExtractor->reachedEOS(mMediaType);
		return ERROR_END_OF_STREAM;
	}

	if (seeking)
	{
		if (pkt.data != flush_pkt.data)
		{
			av_free_packet(&pkt);
			goto retry;
		}
		else
		{
			seeking = false;
#if WAIT_KEY_PACKET_AFTER_SEEK
			waitKeyPkt = true;
#endif
		}
	}

	if (pkt.data == flush_pkt.data)
	{
		ALOGV("read %s flush pkt", av_get_media_type_string(mMediaType));
		av_free_packet(&pkt);
		mFirstKeyPktTimestamp = AV_NOPTS_VALUE;
		mPktTsPrev = AV_NOPTS_VALUE;
		goto retry;
	}
	else if (pkt.data == NULL && pkt.size == 0)
	{
		ALOGV("read %s eos pkt", av_get_media_type_string(mMediaType));
		av_free_packet(&pkt);
		mExtractor->reachedEOS(mMediaType);
		return ERROR_END_OF_STREAM;
	}

	key = (pkt.flags & AV_PKT_FLAG_KEY) ? 1 : 0;
	pktTS = pkt.pts;
	// use dts when AVI
	if ( (pkt.pts == AV_NOPTS_VALUE) && (pkt.dts != AV_NOPTS_VALUE) )
		pktTS = pkt.dts;
	else if ( (pkt.pts == AV_NOPTS_VALUE) && (pkt.dts == AV_NOPTS_VALUE) )
		pktTS = mPktTsPrev;

	// mPktTsPrev Update
	if( (mPktTsPrev == AV_NOPTS_VALUE) && (pktTS != AV_NOPTS_VALUE) )
		mPktTsPrev = pktTS;
	else if( (mPktTsPrev != AV_NOPTS_VALUE) && (pktTS != AV_NOPTS_VALUE) )
		mPktTsPrev = pktTS;

	if (waitKeyPkt)
	{
		if (!key)
		{
			ALOGV("drop the no key packet");
			av_free_packet(&pkt);
			goto retry;
		}
		else
		{
			ALOGV("~~~~~~ got the key packet");
			waitKeyPkt = false;
		}
	}

	if (pktTS == AV_NOPTS_VALUE)
	{
		av_free_packet(&pkt);
		goto retry;
	}


	if (mFirstKeyPktTimestamp == AV_NOPTS_VALUE)
	{
		// update the first key timestamp
		mFirstKeyPktTimestamp = pktTS;
	}

	if (pktTS < mFirstKeyPktTimestamp)
	{
		ALOGV("drop the packet with the backward timestamp, maybe them are B-frames after I-frame ^_^");
		av_free_packet(&pkt);
		goto retry;
	}

	MediaBuffer *mediaBuffer;

#if DISABLE_NAL_TO_ANNEXB
	mNal2AnnexB = false;
#endif
	if ((mIsAVC||mIsHEVC) && mNal2AnnexB)
	{
		mediaBuffer = new MediaBuffer(pkt.size + FF_INPUT_BUFFER_PADDING_SIZE + 1024);
		mediaBuffer->meta_data()->clear();
		/* Convert H.264 NAL format to annex b */
		if ( 0<mNALLengthSize && mNALLengthSize<5 )
		{
			uint8_t *dst = (uint8_t *)mediaBuffer->data();
			int32_t iAnnexBSize = 0;

			/* This only works for NAL sizes 3-4 */
			size_t len = pkt.size, i;
			uint8_t *ptr = pkt.data;
			while (len >= mNALLengthSize)
			{
				uint32_t nal_len = 0;
				for( i = 0; i < mNALLengthSize; i++ )
				{
					nal_len = (nal_len << 8) | ptr[i];
				}
				if (nal_len > INT_MAX || nal_len > (unsigned int)len)
				{
					status = ERROR_MALFORMED;
					break;
				}
				//	Add NAL Start Code
				*dst++ = 0;
				*dst++ = 0;
				*dst++ = 0;
				*dst++ = 1;
				ptr += mNALLengthSize;
				len -= mNALLengthSize;

				memcpy(dst, ptr, nal_len);

				dst += nal_len;
				ptr += nal_len;
				len -= nal_len;
				iAnnexBSize += nal_len+4;
			}
			mediaBuffer->set_range(0, iAnnexBSize);
		}
		else
		{
			status = ERROR_MALFORMED;
		}

		if (status != OK)
		{
			ALOGV("status != OK");
			mediaBuffer->release();
			mediaBuffer = NULL;
			av_free_packet(&pkt);
			return ERROR_MALFORMED;
		}
	}
	else if( mIsMp3 )
	{
		mediaBuffer = new MediaBuffer(pkt.size + FF_INPUT_BUFFER_PADDING_SIZE);
		mediaBuffer->meta_data()->clear();
		mediaBuffer->set_range(0, pkt.size);
		uint32_t mp3Header;
		bool	 bFindHeader;
		if( pkt.size < 4 )
		{
			av_free_packet(&pkt);
			return 0;
		}
		
		mp3Header = pkt.data[0]<<24 | pkt.data[1]<<16 | pkt.data[2]<<8 | pkt.data[3];
		bFindHeader = Mp3CheckHeader(mp3Header);
		if(!bFindHeader)
		{
			av_free_packet(&pkt);
			goto retry;
		}
		// ALOGD("====================MP3 Payload : Header(0x%08x) size: %d, flags: %d(key=%d), pts: %lld, dts: %lld, timeUs[-startTime]: %llu us (%.2f secs)",
		// 	mp3Header, pkt.size, pkt.flags, key, pkt.pts, pkt.dts, timeUs, timeUs/1E6);
		memcpy(mediaBuffer->data(), pkt.data, pkt.size);
	}
	else
	{
		mediaBuffer = new MediaBuffer(pkt.size + FF_INPUT_BUFFER_PADDING_SIZE);
		mediaBuffer->meta_data()->clear();
		mediaBuffer->set_range(0, pkt.size);
		memcpy(mediaBuffer->data(), pkt.data, pkt.size);
		/*
		if(AVMEDIA_TYPE_VIDEO==mMediaType)
		{
			uint8_t *buf = pkt.data;
			ALOGD("------------- Size(%6d) 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x\n",
				pkt.size,buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15] );
		}
		*/
	}

	int64_t start_time = mStream->start_time != AV_NOPTS_VALUE ? mStream->start_time : 0;
	timeUs = (int64_t)((pktTS - start_time) * av_q2d(mStream->time_base) * 1000000);

#if 0
	ALOGD("read %s pkt, size: %d, flags: %d(key=%d), pts: %lld, dts: %lld, timeUs[-startTime]: %llu us (%.2f secs)",
		av_get_media_type_string(mMediaType), pkt.size, pkt.flags, key, pkt.pts, pkt.dts, timeUs, timeUs/1E6);
#endif

	if(timeUs < 0)
	{
		timeUs = 0;
	}

	mediaBuffer->meta_data()->setInt64(kKeyTime, timeUs);
	mediaBuffer->meta_data()->setInt32(kKeyIsSyncFrame, key);

	*buffer = mediaBuffer;

	av_free_packet(&pkt);

	return OK;
}
//
//////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
//
//		Implementation FFMPEG Extractor
//

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "NX_FFmpegExtractor"

FFmpegExtractor::FFmpegExtractor(const sp<DataSource> &source)
	: mDataSource(source)
	, mInitCheck(NO_INIT)
	, mReaderThreadStarted(false)
{
	ALOGD("FFmpegExtractor::FFmpegExtractor");

	int err = 0;

	buildFileName(source);

	memset(&mAudioQ, 0, sizeof(PacketQueue));
	memset(&mVideoQ, 0, sizeof(PacketQueue));

	err = initStreams();
	if (err < 0) {
		ALOGE("failed to init ffmpeg");
		return;
	}

	// start reader here, as we want to extract extradata from bitstream if no extradata
	startReaderThread();

	while(mProbePkts <= EXTRACTOR_MAX_PROBE_PACKETS && !mEOF &&
		(mFormatCtx->pb ? !mFormatCtx->pb->error : 1) &&
		(mDefersToCreateVideoTrack || mDefersToCreateAudioTrack)) {
			// FIXME, i am so lazy! Should use pthread_cond_wait to wait conditions
			NX_Delay(5);
	}

	ALOGV("mProbePkts: %d, mEOF: %d, pb->error(if has): %d, mDefersToCreateVideoTrack: %d, mDefersToCreateAudioTrack: %d",
		mProbePkts, mEOF, mFormatCtx->pb ? mFormatCtx->pb->error : 0, mDefersToCreateVideoTrack, mDefersToCreateAudioTrack);

	mInitCheck = OK;

	mMyInstanceId = gstExtractorCounter ++;
}

FFmpegExtractor::~FFmpegExtractor() {
	ALOGD("FFmpegExtractor::~FFmpegExtractor");

	// stop reader here if no track!
	stopReaderThread();

	deInitStreams();
}

size_t FFmpegExtractor::countTracks() {
	return mInitCheck == OK ? mTrackMetas.size() : 0;
}

sp<MediaSource> FFmpegExtractor::getTrack(size_t index) {
	ALOGV("FFmpegExtractor::getTrack[%d]", index);

	if (mInitCheck != OK) {
		return NULL;
	}

	if (index >= mTrackMetas.size()) {
		return NULL;
	}

	sp<TrackInfo> pTrackInfo = mTrackMetas.valueAt(index);

	return new FFMpegSource(this, pTrackInfo->mMeta, pTrackInfo->mIsAVC, pTrackInfo->mpStream, pTrackInfo->mpQueue);
/*
	return mTrackMetas.valueAt(index);
*/
}

sp<MetaData> FFmpegExtractor::getTrackMetaData(size_t index, uint32_t /*flags*/) {
	ALOGV("FFmpegExtractor::getTrackMetaData[%d]", index);

	if (mInitCheck != OK) {
		return NULL;
	}

	if (index >= mTrackMetas.size()) {
		return NULL;
	}

	return mTrackMetas.valueAt(index)->mMeta;
}

sp<MetaData> FFmpegExtractor::getMetaData() {
	ALOGV("FFmpegExtractor::getMetaData");

	if (mInitCheck != OK || !mFormatCtx ) {
		return NULL;
	}

	sp<MetaData> meta = new MetaData;

	for (int i = 0; i < NELEM(FILE_FORMATS); ++i) {
		int len = strlen(FILE_FORMATS[i].format);
		if (!strncasecmp(mFormatCtx->iformat->name, FILE_FORMATS[i].format, len)) {
			if(MEDIA_MIMETYPE_CONTAINER_ASF == FILE_FORMATS[i].container)
			{
				if( (-1 == mVideoStreamIdx) && (mAudioStreamIdx != -1) )
				{
					meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_WMA);
					break;
				}
			}
			meta->setCString(kKeyMIMEType, FILE_FORMATS[i].container);
			break;
		}
	}


	return meta;
}

uint32_t FFmpegExtractor::flags() const {
	ALOGV("FFmpegExtractor::flags");

	if (mInitCheck != OK) {
		return 0;
	}

	uint32_t flags = CAN_PAUSE;

	if (mFormatCtx->duration != AV_NOPTS_VALUE) {
		flags |= CAN_SEEK_BACKWARD | CAN_SEEK_FORWARD | CAN_SEEK;
	}

	return flags;
}

void FFmpegExtractor::packet_queue_init(PacketQueue *q)
{
	memset(q, 0, sizeof(PacketQueue));
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
	packet_queue_put(q, &flush_pkt);
}

void FFmpegExtractor::packet_queue_flush(PacketQueue *q)
{
	AVPacketList *pkt, *pkt1;

	pthread_mutex_lock(&q->mutex);
	for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
		pkt1 = pkt->next;
		av_free_packet(&pkt->pkt);
		av_freep(&pkt);
	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	pthread_mutex_unlock(&q->mutex);
}

void FFmpegExtractor::packet_queue_end(PacketQueue *q)
{
	packet_queue_flush(q);
}

void FFmpegExtractor::packet_queue_abort(PacketQueue *q)
{
	pthread_mutex_lock(&q->mutex);

	q->abort_request = 1;

	pthread_cond_signal(&q->cond);

	pthread_mutex_unlock(&q->mutex);
}

int FFmpegExtractor::packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
	AVPacketList *pkt1;

	/* duplicate the packet */
	if (pkt != &flush_pkt && av_dup_packet(pkt) < 0)
		return -1;

	pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
	if (!pkt1)
		return -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	pthread_mutex_lock(&q->mutex);

	if (!q->last_pkt)

		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;
	q->last_pkt = pkt1;
	q->nb_packets++;
	//q->size += pkt1->pkt.size + sizeof(*pkt1);
	q->size += pkt1->pkt.size;
	pthread_cond_signal(&q->cond);

	pthread_mutex_unlock(&q->mutex);
	return 0;
}

/* packet queue handling */
/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
int FFmpegExtractor::packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
	AVPacketList *pkt1;
	int ret;

	pthread_mutex_lock(&q->mutex);

	for (;;) {
		if (q->abort_request) {
			ret = -1;
			break;
		}

		pkt1 = q->first_pkt;
		if (pkt1) {
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			q->nb_packets--;
			//q->size -= pkt1->pkt.size + sizeof(*pkt1);
			q->size -= pkt1->pkt.size;
			*pkt = pkt1->pkt;
			av_free(pkt1);
			ret = 1;
			break;
		} else if (!block) {
			ret = 0;
			break;
		} else {
			pthread_cond_wait(&q->cond, &q->mutex);
		}
	}
	pthread_mutex_unlock(&q->mutex);
	return ret;
}

static void EncodeSize14(uint8_t **_ptr, size_t size) {
	CHECK_LE(size, 0x3fff);

	uint8_t *ptr = *_ptr;

	*ptr++ = 0x80 | (size >> 7);
	*ptr++ = size & 0x7f;

	*_ptr = ptr;
}

static sp<ABuffer> MakeMPEGVideoESDS(const sp<ABuffer> &csd) {
	sp<ABuffer> esds = new ABuffer(csd->size() + 25);

	uint8_t *ptr = esds->data();
	*ptr++ = 0x03;
	EncodeSize14(&ptr, 22 + csd->size());

	*ptr++ = 0x00;  // ES_ID
	*ptr++ = 0x00;

	*ptr++ = 0x00;  // streamDependenceFlag, URL_Flag, OCRstreamFlag

	*ptr++ = 0x04;
	EncodeSize14(&ptr, 16 + csd->size());

	*ptr++ = 0x40;  // Audio ISO/IEC 14496-3

	for (size_t i = 0; i < 12; ++i) {
		*ptr++ = 0x00;
	}

	*ptr++ = 0x05;
	EncodeSize14(&ptr, csd->size());

	memcpy(ptr, csd->data(), csd->size());

	return esds;
}

static int bytesForSize(size_t size) {
    // use at most 28 bits (4 times 7)
    CHECK(size <= 0xfffffff);

    if (size > 0x1fffff) {
        return 4;
    } else if (size > 0x3fff) {
        return 3;
    } else if (size > 0x7f) {
        return 2;
    }
    return 1;
}

static void storeSize(uint8_t *data, size_t &idx, size_t size) {
    int numBytes = bytesForSize(size);
    idx += numBytes;

    data += idx;
    size_t next = 0;
    while (numBytes--) {
        *--data = (size & 0x7f) | next;
        size >>= 7;
        next = 0x80;
    }
}

static void addESDSFromCodecPrivate(
        const sp<MetaData> &meta,
        bool isAudio, const void *priv, size_t privSize) {

    int privSizeBytesRequired = bytesForSize(privSize);
    int esdsSize2 = 14 + privSizeBytesRequired + privSize;
    int esdsSize2BytesRequired = bytesForSize(esdsSize2);
    int esdsSize1 = 4 + esdsSize2BytesRequired + esdsSize2;
    int esdsSize1BytesRequired = bytesForSize(esdsSize1);
    size_t esdsSize = 1 + esdsSize1BytesRequired + esdsSize1;
    uint8_t *esds = new uint8_t[esdsSize];

    size_t idx = 0;
    esds[idx++] = 0x03;
    storeSize(esds, idx, esdsSize1);
    esds[idx++] = 0x00; // ES_ID
    esds[idx++] = 0x00; // ES_ID
    esds[idx++] = 0x00; // streamDependenceFlag, URL_Flag, OCRstreamFlag
    esds[idx++] = 0x04;
    storeSize(esds, idx, esdsSize2);
    esds[idx++] = isAudio ? 0x40   // Audio ISO/IEC 14496-3
                          : 0x20;  // Visual ISO/IEC 14496-2
    for (int i = 0; i < 12; i++) {
        esds[idx++] = 0x00;
    }
    esds[idx++] = 0x05;
    storeSize(esds, idx, privSize);
    memcpy(esds + idx, priv, privSize);

    meta->setData(kKeyESDS, 0, esds, esdsSize);

    delete[] esds;
    esds = NULL;
}

// Returns the sample rate based on the sampling frequency index
static uint32_t get_sample_rate(const uint8_t sf_index)
{
	static const uint32_t sample_rates[] =
	{
		96000, 88200, 64000, 48000, 44100, 32000,
		24000, 22050, 16000, 12000, 11025, 8000
	};

	if (sf_index < sizeof(sample_rates) / sizeof(sample_rates[0])) {
		return sample_rates[sf_index];
	}

	return 0;
}

int FFmpegExtractor::check_extradata(AVCodecContext *avctx)
{
	const char *name;
	bool *defersToCreateTrack;
	AVBitStreamFilterContext **bsfc = NULL;

	// init
	if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
		bsfc = &mVideoBsfc;
		defersToCreateTrack = &mDefersToCreateVideoTrack;
	} else if (avctx->codec_type == AVMEDIA_TYPE_AUDIO){
		bsfc = &mAudioBsfc;
		defersToCreateTrack = &mDefersToCreateAudioTrack;
	}
	else
	{
		//	ignore
		return 1;
	}

	// ignore extradata
	if (avctx->codec_id == AV_CODEC_ID_MP3 ||
		avctx->codec_id == AV_CODEC_ID_MP1  ||
		avctx->codec_id == AV_CODEC_ID_MP2  ||
		avctx->codec_id == AV_CODEC_ID_AC3  ||
		avctx->codec_id == AV_CODEC_ID_DTS  ||
		avctx->codec_id == AV_CODEC_ID_FLV1  ||
		avctx->codec_id == AV_CODEC_ID_H263  ||
		avctx->codec_id == AV_CODEC_ID_H263P ||
		avctx->codec_id == AV_CODEC_ID_H263I ||
		avctx->codec_id == AV_CODEC_ID_WMV3)
		return 1;

	// is extradata compatible with android?
	if(avctx->codec_id == AV_CODEC_ID_H264)
	{
		int is_compatible = is_extradata_compatible_with_android(avctx);
		if (!is_compatible) {
			ALOGI("%s extradata is not compatible with android, should to extract it from bitstream",
				av_get_media_type_string(avctx->codec_type));
			*defersToCreateTrack = true;
			*bsfc = NULL; // H264 don't need bsfc, only AAC?
			return 0;
		}
		return 1;
	}

	if (avctx->codec_id == AV_CODEC_ID_AAC)
	{
		printf("CodecTag = 0x%08x\n", avctx->codec_tag);
		name = "aac_adtstoasc";
		if (avctx->extradata_size <= 0) {
			ALOGI("No %s extradata found, should to extract it from bitstream",
				av_get_media_type_string(avctx->codec_type));
			*defersToCreateTrack = true;
			//CHECK(name != NULL);
			if (!*bsfc && name) {
				*bsfc = av_bitstream_filter_init(name);
				if (!*bsfc) {
					ALOGE("Cannot open the %s BSF!", name);
					*defersToCreateTrack = false;
					return -1;
				} else {
					ALOGV("open the %s bsf", name);
					return 0;
				}
			} else {
				return 0;
			}
		}
	}
	return 1;
}

void FFmpegExtractor::printTime(int64_t time)
{
	int hours, mins, secs, us;

	if (time == AV_NOPTS_VALUE)
		return;

	secs = time / AV_TIME_BASE;
	us = time % AV_TIME_BASE;
	mins = secs / 60;
	secs %= 60;
	hours = mins / 60;
	mins %= 60;
	ALOGI("the time is %02d:%02d:%02d.%02d",
		hours, mins, secs, (100 * us) / AV_TIME_BASE);
}

int FFmpegExtractor::stream_component_open(int stream_index)
{
	AVCodecContext *avctx;
	sp<MetaData> meta;
	bool isAVC = false;
	bool supported = false;
	uint32_t type;
	const void *data;
	size_t size;
	int ret;

	ALOGI("stream_index: %d", stream_index);
	if (stream_index < 0 || (uint32_t)stream_index >= mFormatCtx->nb_streams)
		return -1;
	avctx = mFormatCtx->streams[stream_index]->codec;

	switch(avctx->codec_id) {
	//
	//	Video
	//
	case AV_CODEC_ID_H264:
	case AV_CODEC_ID_MPEG4:
	case AV_CODEC_ID_FLV1:
	case AV_CODEC_ID_MSMPEG4V3:
	case AV_CODEC_ID_H263:
	case AV_CODEC_ID_H263P:
	case AV_CODEC_ID_H263I:
	case AV_CODEC_ID_MPEG1VIDEO:
	case AV_CODEC_ID_MPEG2VIDEO:
	case AV_CODEC_ID_WMV3:
	case AV_CODEC_ID_VC1:
	case AV_CODEC_ID_VP8:
	case AV_CODEC_ID_VP9:
	case AV_CODEC_ID_RV40:
	case AV_CODEC_ID_HEVC:

	//
	//	Audio
	//
	case AV_CODEC_ID_AAC:
	case AV_CODEC_ID_AC3:
	case AV_CODEC_ID_MP1:
	case AV_CODEC_ID_MP2:
	case AV_CODEC_ID_MP3:
	case AV_CODEC_ID_WMAV1:
	case AV_CODEC_ID_WMAV2:
	case AV_CODEC_ID_WMAPRO:
	case AV_CODEC_ID_WMALOSSLESS:
	case AV_CODEC_ID_COOK:
	case AV_CODEC_ID_APE:
	case AV_CODEC_ID_DTS:
	case AV_CODEC_ID_VORBIS:
	case AV_CODEC_ID_FLAC:
	case AV_CODEC_ID_PCM_S16LE:
		supported = true;
		break;
	default:
		supported = false;
		break;
	}

	if (!supported) {
		ALOGE("unsupport the codec, id: 0x%0x", avctx->codec_id);
		return -1;
	}
	ALOGV("support the codec");

	unsigned streamType;
	ssize_t index = mTrackMetas.indexOfKey(stream_index);

	if (index >= 0) {
		ALOGE("this track already exists");
		return 0;
	}

	mFormatCtx->streams[stream_index]->discard = AVDISCARD_DEFAULT;

	char tagbuf[32];
	av_get_codec_tag_string(tagbuf, sizeof(tagbuf), avctx->codec_tag);
	ALOGD("Tag %s/0x%08x with codec id '%d'\n", tagbuf, avctx->codec_tag, avctx->codec_id);

	switch (avctx->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
		if (mVideoStreamIdx == -1)
			mVideoStreamIdx = stream_index;
		if (mVideoStream == NULL)
			mVideoStream = mFormatCtx->streams[stream_index];
		if (!mVideoQInited) {
			packet_queue_init(&mVideoQ);
			mVideoQInited = true;
		}

		ret = check_extradata(avctx);
		if (ret != 1) {
			if (ret == -1) {
				// disable the stream
				mVideoStreamIdx = -1;
				mVideoStream = NULL;
				packet_queue_end(&mVideoQ);
				mVideoQInited =  false;
				mFormatCtx->streams[stream_index]->discard = AVDISCARD_ALL;
			}
			return ret;
		}

		if (avctx->extradata) {
			ALOGV("video stream extradata:");
			if( DUMP_EXTRA_DATA )
				hexdump(avctx->extradata, avctx->extradata_size);
		} else {
			ALOGV("video stream no extradata, but we can ignore it.");
		}

		meta = new MetaData;

		switch(avctx->codec_id) {
		case AV_CODEC_ID_H264:
			/**
			* H.264 Video Types
			* http://msdn.microsoft.com/en-us/library/dd757808(v=vs.85).aspx
			*/
			//if (avctx->codec_tag && avctx->codec_tag == AV_RL32("avc1")) {
			if (avctx->extradata[0] == 1 /* configurationVersion */) {
				// H.264 bitstream without start codes.
				isAVC = true;
				ALOGV("AVC");

				if (avctx->width == 0 || avctx->height == 0) {
					int32_t width, height;
					sp<ABuffer> seqParamSet = new ABuffer(avctx->extradata_size - 8);
					memcpy(seqParamSet->data(), avctx->extradata + 8, avctx->extradata_size - 8);
					FindAVCDimensions(seqParamSet, &width, &height);
					avctx->width  = width;
					avctx->height = height;
				}

				meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_AVC);
				meta->setData(kKeyAVCC, kTypeAVCC, avctx->extradata, avctx->extradata_size);
			} else {
				// H.264 bitstream with start codes.
				isAVC = false;
				ALOGV("H264");

				/* set NULL to release meta as we will new a meta in MakeAVCCodecSpecificData() fxn */
				meta->clear();
				meta = NULL;

				sp<ABuffer> buffer = new ABuffer(avctx->extradata_size);
				memcpy(buffer->data(), avctx->extradata, avctx->extradata_size);
				meta = MakeAVCCodecSpecificData(buffer);
			}
			break;
		case AV_CODEC_ID_MPEG4:
		case AV_CODEC_ID_MSMPEG4V3:
		case AV_CODEC_ID_FLV1:
			ALOGV("MPEG4");
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_MPEG4);
			if( avctx->extradata_size>0 )
			{
				sp<ABuffer> csd = new ABuffer(avctx->extradata_size);
				memcpy(csd->data(), avctx->extradata, avctx->extradata_size);
				sp<ABuffer> esds = MakeMPEGVideoESDS(csd);
				meta->setData(kKeyESDS, kTypeESDS, esds->data(), esds->size());
				meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			}
			meta->setInt32( kKeyFFCodecTag, avctx->codec_tag );
			break;
		case AV_CODEC_ID_H263:
		case AV_CODEC_ID_H263P:
		case AV_CODEC_ID_H263I:
			ALOGV("H263");
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_H263);
			{
				if( avctx->extradata_size>0 )
				{
					sp<ABuffer> csd = new ABuffer(avctx->extradata_size);
					memcpy(csd->data(), avctx->extradata, avctx->extradata_size);
					meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
				}
			}
			break;
		case AV_CODEC_ID_MPEG1VIDEO:
		case AV_CODEC_ID_MPEG2VIDEO:
			ALOGV("MPEG2VIDEO(ExtraSize = %d)\n", avctx->extradata_size);
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_MPEG2);
			if( avctx->extradata_size>0 )
			{
				meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			}
			break;
		case AV_CODEC_ID_VC1:
			ALOGV("VC1");
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_WMV);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			meta->setInt32(kKeyWMVVersion, 0);
			break;
		//case AV_CODEC_ID_WMV1:
		//	ALOGV("WMV1");
		//	meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_WMV);
		//	meta->setInt32(kKeyWMVVersion, kTypeWMVVer_7);
		//	break;
		//case AV_CODEC_ID_WMV2:
		//	ALOGV("WMV2");
		//	meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_WMV);
		//	meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
		//	meta->setInt32(kKeyWMVVersion, kTypeWMVVer_8);
		//	break;
		case AV_CODEC_ID_WMV3:
			ALOGV("WMV3");
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_WMV);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			meta->setInt32(kKeyWMVVersion, kTypeWMVVer_9);
			break;
		case AV_CODEC_ID_RV40:
			ALOGV("RV40");
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RV);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			break;
		case AV_CODEC_ID_VP8:
			ALOGV("VP8");
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_VP8);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			break;
		case AV_CODEC_ID_VP9:
			ALOGV("VP9");
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_VP9);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			break;
		case AV_CODEC_ID_HEVC:
			ALOGV("HEVC");
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_HEVC);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);

			if (avctx->extradata[0] == 1 /* configurationVersion */) {
				// H.265 bitstream without start codes.
				meta->setData(kKeyHVCC, kTypeHVCC, avctx->extradata, avctx->extradata_size);
			} else{
				//
				//	FIXME : S/W H264 Need HVCC Data, We Should be setting kKeyHVCC Data.
				//
			}
			break;
		default:
			CHECK(!"Should not be here. Unsupported codec.");
			break;
		}

		ALOGI("width: %d, height: %d, bit_rate: %d",
			avctx->width, avctx->height, avctx->bit_rate);

		meta->setInt32(kKeyWidth, avctx->width);
		meta->setInt32(kKeyHeight, avctx->height);
		if (avctx->bit_rate > 0)
			meta->setInt32(kKeyBitRate, avctx->bit_rate);
		if (mVideoStream->duration != AV_NOPTS_VALUE) {
			int64_t duration = mVideoStream->duration * av_q2d(mVideoStream->time_base) * 1000000;
			printTime(duration);
			ALOGV("video startTime: %lld", mVideoStream->start_time);
			meta->setInt64(kKeyDuration, duration);
			meta->setInt64(kKeyThumbnailTime, duration/4);
		} else {
			// default when no stream duration
			meta->setInt64(kKeyDuration, mFormatCtx->duration);
			meta->setInt64(kKeyThumbnailTime, mFormatCtx->duration/4);
		}

		ALOGV("create a video track");
		index = mTrackMetas.add( stream_index, new TrackInfo(mVideoStreamIdx, meta, isAVC, mVideoStream, &mVideoQ) );

		mDefersToCreateVideoTrack = false;

		break;
	case AVMEDIA_TYPE_AUDIO:
		if (mAudioStreamIdx == -1)
			mAudioStreamIdx = stream_index;
		if (mAudioStream == NULL)
			mAudioStream = mFormatCtx->streams[stream_index];
		if (!mAudioQInited) {
			packet_queue_init(&mAudioQ);
			mAudioQInited = true;
		}

		ret = check_extradata(avctx);
		if (ret != 1) {
			if (ret == -1) {
				// disable the stream
				mAudioStreamIdx = -1;
				mAudioStream = NULL;
				packet_queue_end(&mAudioQ);
				mAudioQInited =  false;
				mFormatCtx->streams[stream_index]->discard = AVDISCARD_ALL;
			}
			return ret;
		}

		if (avctx->extradata) {
			ALOGV("audio stream extradata:");
			if( DUMP_EXTRA_DATA )
				hexdump(avctx->extradata, avctx->extradata_size);
		} else {
			ALOGV("audio stream no extradata, but we can ignore it.");
		}

		switch(avctx->codec_id) {
		case AV_CODEC_ID_MP1:
			ALOGV("MP1");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_I);
			break;
		case AV_CODEC_ID_MP2:
			ALOGV("MP2");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_II);
			break;
		case AV_CODEC_ID_MP3:
			ALOGV("MP3");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_MPEG);
			break;
		case AV_CODEC_ID_AC3:
			ALOGV("AC3");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_AC3);
			break;
		case AV_CODEC_ID_AAC:
			ALOGV("AAC"); 
			uint32_t sr;
			const uint8_t *header;
			uint8_t profile, sf_index, channel;
			meta = new MetaData;

			header = avctx->extradata;
			CHECK(header != NULL);

			// AudioSpecificInfo follows
			// oooo offf fccc c000
			// o - audioObjectType
			// f - samplingFreqIndex
			// c - channelConfig
			profile = ((header[0] & 0xf8) >> 3);
			sf_index = (header[0] & 0x07) << 1 | (header[1] & 0x80) >> 7;
			sr = get_sample_rate(sf_index);
			if (sr == 0) {
				ALOGE("unsupport the sample rate");
				return -1;
			}
			channel = (header[1] >> 3) & 0xf;
			ALOGD("profile: %d, sf_index: %d, channel: %d", profile, sf_index, channel);

			//meta = MakeAACCodecSpecificData(profile - 1, sf_index, channel);
			addESDSFromCodecPrivate(meta, true, avctx->extradata, avctx->extradata_size );
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_AAC);
			meta->setInt32(kKeyBlockAlign, avctx->block_align);
			meta->setInt32(kKeyAACProfile, profile);
			break;
		case AV_CODEC_ID_WMAV1:  // TODO, version?
			ALOGV("WMAV1");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_WMA);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			break;
		case AV_CODEC_ID_WMAV2:
			ALOGV("WMAV2");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_WMA);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			meta->setInt32(kKeyWMAVersion, kTypeWMA);
			break;
		case AV_CODEC_ID_WMAPRO:
			ALOGV("WMAPRO");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_WMA);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			meta->setInt32(kKeyWMAVersion, kTypeWMAPro);
			break;
		case AV_CODEC_ID_WMALOSSLESS:
			ALOGV("WMALOSSLESS");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_WMA);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			meta->setInt32(kKeyWMAVersion, kTypeWMALossLess);
			break;
		case AV_CODEC_ID_COOK: // audio codec in RMVB
			ALOGV("COOK");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_RA);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			break;
		case AV_CODEC_ID_APE:
			ALOGV("APE");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_APE);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			break;
		case AV_CODEC_ID_DTS:
			ALOGV("DTS");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_DTS);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			break;
		case AV_CODEC_ID_FLAC:
			ALOGV("FLAC");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_FLAC);
			meta->setData(kKeyRawCodecSpecificData, 0, avctx->extradata, avctx->extradata_size);
			break;
		case AV_CODEC_ID_PCM_S16LE:
			ALOGV("RAW");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_RAW);
			break;
		case AV_CODEC_ID_VORBIS:
			ALOGV("VORBIS");
			meta = new MetaData;
			meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_VORBIS);
			if( avctx->extradata_size > 0 )
				addVorbisCodecInfo( meta, avctx->extradata, avctx->extradata_size);
			break;
		default:
			CHECK(!"Should not be here. Unsupported codec.");
			break;
		}

		ALOGI("bit_rate: %d, sample_rate: %d, channels: %d, bits_per_coded_sample: %d, bits_per_coded_sample=%d, block_align=%d\n",
			avctx->bit_rate, avctx->sample_rate, avctx->channels, avctx->bits_per_coded_sample, avctx->bits_per_coded_sample, avctx->block_align);

		meta->setInt32(kKeySampleRate, avctx->sample_rate);
		meta->setInt32(kKeyChannelCount, avctx->channels);
		meta->setInt32(kKeyBitspersample, avctx->bits_per_coded_sample);
		meta->setInt32(kKeyBitRate, avctx->bit_rate);
		meta->setInt32(kKeyBlockAlign, avctx->block_align);
		if (mAudioStream->duration != AV_NOPTS_VALUE) {
			int64_t duration = mAudioStream->duration * av_q2d(mAudioStream->time_base) * 1000000;
			printTime(duration);
			ALOGV("audio startTime: %lld", mAudioStream->start_time);
			meta->setInt64(kKeyDuration, duration);
		} else {
			// default when no stream duration
			meta->setInt64(kKeyDuration, mFormatCtx->duration);
		}

		ALOGV("create a audio track");
		index = mTrackMetas.add( stream_index, new TrackInfo(mAudioStreamIdx, meta, false, mAudioStream, &mAudioQ) );

		mDefersToCreateAudioTrack = false;

		break;
	case AVMEDIA_TYPE_SUBTITLE:
		/* Unsupport now */
		CHECK(!"Should not be here. Unsupported media type.");
		break;
	default:
		CHECK(!"Should not be here. Unsupported media type.");
		break;
	}
	return 0;
}

void FFmpegExtractor::stream_component_close(int stream_index)
{
	AVCodecContext *avctx;

	if (stream_index < 0 || (uint32_t)stream_index >= mFormatCtx->nb_streams)
		return;
	avctx = mFormatCtx->streams[stream_index]->codec;

	switch (avctx->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
		ALOGV("packet_queue_abort videoq");
		packet_queue_abort(&mVideoQ);
		/* wait until the end */
		while (!mAbortRequest && !mVideoEOSReceived) {
			ALOGV("wait for video received");
			NX_Delay(10);
		}
		ALOGV("packet_queue_end videoq");
		packet_queue_end(&mVideoQ);
		break;
	case AVMEDIA_TYPE_AUDIO:
		ALOGV("packet_queue_abort audioq");
		packet_queue_abort(&mAudioQ);
		while (!mAbortRequest && !mAudioEOSReceived) {
			// 20170104 added by ray park for seek hangup.
			if( mFormatCtx->ctx_flags & 0x1000 )
				break;
			ALOGV("wait for audio received");
			NX_Delay(10);
		}
		ALOGV("packet_queue_end audioq");
		packet_queue_end(&mAudioQ);
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		break;
	default:
		break;
	}

	mFormatCtx->streams[stream_index]->discard = AVDISCARD_ALL;
	switch (avctx->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
		mVideoStream    = NULL;
		mVideoStreamIdx = -1;
		if (mVideoBsfc) {
			av_bitstream_filter_close(mVideoBsfc);
			mVideoBsfc  = NULL;
		}
		break;
	case AVMEDIA_TYPE_AUDIO:
		mAudioStream    = NULL;
		mAudioStreamIdx = -1;
		if (mAudioBsfc) {
			av_bitstream_filter_close(mAudioBsfc);
			mAudioBsfc  = NULL;
		}
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		break;
	default:
		break;
	}
}

void FFmpegExtractor::reachedEOS(enum AVMediaType media_type)
{
	Mutex::Autolock autoLock(mExtractorLock);

	if (media_type == AVMEDIA_TYPE_VIDEO) {
		mVideoEOSReceived = true;
	} else if (media_type == AVMEDIA_TYPE_AUDIO) {
		mAudioEOSReceived = true;
	}
}

/* seek in the stream */
int FFmpegExtractor::stream_seek(int64_t pos, enum AVMediaType media_type)
{
	Mutex::Autolock autoLock(mExtractorLock);

	if (mVideoStreamIdx >= 0 &&
		mAudioStreamIdx >= 0 &&
		media_type == AVMEDIA_TYPE_AUDIO &&
		!mVideoEOSReceived) {
			return NO_SEEK;
	}

	// flush immediately
	if (mAudioStreamIdx >= 0)
		packet_queue_flush(&mAudioQ);
	if (mVideoStreamIdx >= 0)
		packet_queue_flush(&mVideoQ);

	mSeekPos = pos;
	mSeekFlags |= AVSEEK_FLAG_FRAME;
	mSeekFlags &= ~AVSEEK_FLAG_BYTE;
	mSeekReq = 1;
	mEOF2 = false;

	return SEEK;
}

// staitc
int FFmpegExtractor::decode_interrupt_cb(void *ctx)
{
	FFmpegExtractor *extrator = static_cast<FFmpegExtractor *>(ctx);
	return extrator->mAbortRequest;
}

void FFmpegExtractor::print_error_ex(const char *filename, int err)
{
	char errbuf[128];
	const char *errbuf_ptr = errbuf;

	if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
		errbuf_ptr = strerror(AVUNERROR(err));
	ALOGI("%s: %s\n", filename, errbuf_ptr);
}

void FFmpegExtractor::buildFileName(const sp<DataSource> &source)
{
#if 1
	ALOGI("android source: %x", (int)(&source));
	// pass the addr of smart pointer("source")
	snprintf(mFilename, sizeof(mFilename), "android-source:%x", (int)&source);
	ALOGI("build mFilename: %s", mFilename);
#else
	const char *url = mDataSource->getNamURI();
	if (url == NULL) {
		ALOGI("url is error!");
		return;
	}
	// is it right?
	if (!strcmp(url, "-")) {
		av_strlcpy(mFilename, "pipe:", strlen("pipe:") + 1);
	} else {
		av_strlcpy(mFilename, url, strlen(url) + 1);
	}
	ALOGI("build url: %s, mFilename: %s", url, mFilename);
#endif
}

void FFmpegExtractor::setFFmpegDefaultOpts()
{
	mGenPTS       = 1;
#if DEBUG_DISABLE_VIDEO
	mVideoDisable = 1;
#else
	mVideoDisable = 0;
#endif
#if DEBUG_DISABLE_AUDIO
	mAudioDisable = 1;
#else
	mAudioDisable = 0;
#endif
	mShowStatus   = 0;
	mSeekByBytes  = 0; /* seek by bytes 0=off 1=on -1=auto" */
	mDuration     = AV_NOPTS_VALUE;
	mSeekPos      = AV_NOPTS_VALUE;
	mAutoExit     = 1;
	mLoop         = 1;

	mVideoStreamIdx = -1;
	mAudioStreamIdx = -1;
	mVideoStream  = NULL;
	mAudioStream  = NULL;
	mVideoQInited = false;
	mAudioQInited = false;
	mDefersToCreateVideoTrack = false;
	mDefersToCreateAudioTrack = false;
	mVideoBsfc = NULL;
	mAudioBsfc = NULL;

	mAbortRequest = 0;
	mPaused       = 0;
	mLastPaused   = 0;
	mSeekReq      = 0;

	mProbePkts    = 0;
	mEOF          = false;
}

int av_find_best_audio_stream(AVFormatContext *ic,
	                          enum AVMediaType type,
	                          int wanted_stream_nb,
	                          int related_stream,
	                          AVCodec ** /*decoder_ret*/,
	                          int /*flags*/)
{
	int i, nb_streams = ic->nb_streams;
	int ret = AVERROR_STREAM_NOT_FOUND, best_count = -1, best_bitrate = -1, best_multiframe = -1, count, bitrate, multiframe;
	unsigned *program = NULL;

	if (related_stream >= 0 && wanted_stream_nb < 0)
	{
		AVProgram *p = av_find_program_from_stream(ic, NULL, related_stream);
		if (p) {
			program = p->stream_index;
			nb_streams = p->nb_stream_indexes;
		}
	}

	for (i = 0; i < nb_streams; i++)
	{
		int real_stream_index = program ? program[i] : i;
		AVStream *st = ic->streams[real_stream_index];
		AVCodecContext *avctx = st->codec;
		if (avctx->codec_type != type)
			continue;
		if (wanted_stream_nb >= 0 && real_stream_index != wanted_stream_nb)
			continue;
		if (st->disposition & (AV_DISPOSITION_HEARING_IMPAIRED|AV_DISPOSITION_VISUAL_IMPAIRED))
			continue;

		switch(avctx->codec_id)
		{
			case AV_CODEC_ID_AAC:
			case AV_CODEC_ID_AC3:
			case AV_CODEC_ID_MP1:
			case AV_CODEC_ID_MP2:
			case AV_CODEC_ID_WMAV1:
			case AV_CODEC_ID_WMAV2:
			case AV_CODEC_ID_WMAPRO:
			case AV_CODEC_ID_WMALOSSLESS:
			case AV_CODEC_ID_COOK:
			case AV_CODEC_ID_APE:
			case AV_CODEC_ID_DTS:
			case AV_CODEC_ID_MP3:
			case AV_CODEC_ID_FLAC:
			case AV_CODEC_ID_VORBIS:
			case AV_CODEC_ID_PCM_S16LE:
				break;
			default:
				continue;
		}

		count = st->codec_info_nb_frames;
		bitrate = avctx->bit_rate;
		multiframe = FFMIN(5, count);
		if ((best_multiframe >  multiframe) ||
			(best_multiframe == multiframe && best_bitrate >  bitrate) ||
			(best_multiframe == multiframe && best_bitrate == bitrate && best_count >= count))
			continue;

		best_count = count;
		best_bitrate = bitrate;
		best_multiframe = multiframe;
		ret = real_stream_index;
		if (program && i == nb_streams - 1 && ret < 0) {
			program = NULL;
			nb_streams = ic->nb_streams;
			i = 0; /* no related stream found, try again with everything */
		}
	}
	return ret;
}


int FFmpegExtractor::initStreams()
{
	int err, i;
	status_t status;
	int ret = 0, audio_ret = 0, video_ret = 0;
	int pkt_in_play_range = 0;
	int orig_nb_streams;
	int st_index[AVMEDIA_TYPE_NB] = {0};
	int wanted_stream[AVMEDIA_TYPE_NB] = {0};
	st_index[AVMEDIA_TYPE_AUDIO]  = -1;
	st_index[AVMEDIA_TYPE_VIDEO]  = -1;
	wanted_stream[AVMEDIA_TYPE_AUDIO]  = -1;
	wanted_stream[AVMEDIA_TYPE_VIDEO]  = -1;

	setFFmpegDefaultOpts();

	status = initFFmpeg();
	if (status != OK) {
		ret = -1;
		return -1;
		//goto fail;
	}

	av_init_packet(&flush_pkt);
	flush_pkt.data = (uint8_t *)"FLUSH";
	flush_pkt.size = 0;

	mFormatCtx = avformat_alloc_context();

	mFormatCtx->interrupt_callback.callback = decode_interrupt_cb;
	mFormatCtx->interrupt_callback.opaque = this;
	ALOGV("mFilename: %s", mFilename);
	err = avformat_open_input(&mFormatCtx, mFilename, NULL, NULL);
	if (err < 0) {
		print_error_ex(mFilename, err);
		ret = -1;
		goto fail;
	}

	err = avformat_find_stream_info(mFormatCtx, NULL);
	if (err < 0) {
		print_error_ex(mFilename, err);
		ret = -1;
		goto fail;
	}

	if (mGenPTS)
		mFormatCtx->flags |= AVFMT_FLAG_GENPTS;

	orig_nb_streams = mFormatCtx->nb_streams;

	if (mFormatCtx->pb)
		mFormatCtx->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use url_feof() to test for the end

	if (mSeekByBytes < 0)
		mSeekByBytes = !!(mFormatCtx->iformat->flags & AVFMT_TS_DISCONT);

	for (i = 0; (uint32_t)i < mFormatCtx->nb_streams; i++)
		mFormatCtx->streams[i]->discard = AVDISCARD_ALL;
	if (!mVideoDisable)
	{
		st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(mFormatCtx, AVMEDIA_TYPE_VIDEO,
														   wanted_stream[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
	}
	if (!mAudioDisable)
	{
		// st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(mFormatCtx, AVMEDIA_TYPE_AUDIO,
		// 												   wanted_stream[AVMEDIA_TYPE_AUDIO], st_index[AVMEDIA_TYPE_VIDEO], NULL, 0);
		st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_audio_stream(mFormatCtx, AVMEDIA_TYPE_AUDIO,
																wanted_stream[AVMEDIA_TYPE_AUDIO], st_index[AVMEDIA_TYPE_VIDEO], NULL, 0);
	}
	if (mShowStatus) {
		av_dump_format(mFormatCtx, 0, mFilename, 0);
	}

	if (mFormatCtx->duration != AV_NOPTS_VALUE && mFormatCtx->start_time != AV_NOPTS_VALUE)
	{
		int hours, mins, secs, us;
		ALOGV("file startTime: %lld", mFormatCtx->start_time);
		mDuration = mFormatCtx->duration;
		secs = mDuration / AV_TIME_BASE;
		us = mDuration % AV_TIME_BASE;
		mins = secs / 60;
		secs %= 60;
		hours = mins / 60;
		mins %= 60;
		ALOGI("the duration is %02d:%02d:%02d.%02d", hours, mins, secs, (100 * us) / AV_TIME_BASE);
	}

	if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
		audio_ret = stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
	}

	if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
		video_ret = stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);
	}

	if ( audio_ret < 0 && video_ret < 0) {
		ALOGE("%s: could not open codecs\n", mFilename);
		ret = -1;
		goto fail;
	}

	ret = 0;

fail:
	return ret;
}

void FFmpegExtractor::deInitStreams()
{
	if (mFormatCtx) {
		avformat_close_input(&mFormatCtx);
	}

	deInitFFmpeg();
}

status_t FFmpegExtractor::startReaderThread() {
	ALOGD("Starting reader thread");
	Mutex::Autolock autoLock(mExtractorLock);

	if (mReaderThreadStarted)
		return OK;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&mReaderThread, &attr, ReaderWrapper, this);
	pthread_attr_destroy(&attr);
	mReaderThreadStarted = true;
	ALOGD("Reader thread started");

	return OK;
}

void FFmpegExtractor::stopReaderThread() {
	ALOGD("Stopping reader thread");
	Mutex::Autolock autoLock(mExtractorLock);

	if (!mReaderThreadStarted) {
		ALOGD("Reader thread have been stopped");
		return;
	}

	mAbortRequest = 1;

	void *dummy;
	pthread_join(mReaderThread, &dummy);
	mReaderThreadStarted = false;
	ALOGD("Reader thread stopped");
}

// static
void *FFmpegExtractor::ReaderWrapper(void *me) {
	((FFmpegExtractor *)me)->readerEntry();

	return NULL;
}

void FFmpegExtractor::startSeekMonitor( int32_t mSec )
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	exitSeekMonitor = false;
	mSeekTimeoutTime = mSec;
	pthread_create(&mSeekMonitorThread, &attr, SeekMonitorThreadStub, this);
	pthread_attr_destroy(&attr);
}

void FFmpegExtractor::stopSeekMonitor()
{
	exitSeekMonitor = true;
	pthread_join( mSeekMonitorThread, NULL );
}

static int64_t __gettime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

void *FFmpegExtractor::SeekMonitorThreadStub(void*me){
	FFmpegExtractor *pObj = (FFmpegExtractor*)me;
	return pObj->SeekMonitorThread();
}

void *FFmpegExtractor::SeekMonitorThread(){
	int64_t endTime, curTime;
	if( mSeekTimeoutTime == -1 ){
		endTime = -1;
	} else {
		endTime = __gettime() + mSeekTimeoutTime * 1000;
	}

	while( !exitSeekMonitor )
	{
		curTime = __gettime();
		if( -1 == mSeekTimeoutTime ){
			usleep(1000);
		} else{
			if( curTime > endTime )
			{
				break;
			}
			usleep(1000);
		}
	}

	if( !exitSeekMonitor )
	{
		mFormatCtx->ctx_flags |= 0x1000;
		ALOGI("Forced make EOS !!!!");
	}
	return (void*)0xdeadface;
}

void FFmpegExtractor::readerEntry() {
	int err, i, ret;
	AVPacket pkt1, *pkt = &pkt1;
//	int eof = 0;
	mEOF2 = false;
	int pkt_in_play_range = 0;

	ALOGV("FFmpegExtractor::readerEntry");

	mVideoEOSReceived = false;
	mAudioEOSReceived = false;

	for (;;) {
		if (mAbortRequest)
			break;

		if (mPaused != mLastPaused) {
			mLastPaused = mPaused;
			if (mPaused)
				mReadPauseReturn = av_read_pause(mFormatCtx);
			else
				av_read_play(mFormatCtx);
		}
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
		if (mPaused &&
			(!strcmp(mFormatCtx->iformat->name, "rtsp") ||
			(mFormatCtx->pb && !strncmp(mFilename, "mmsh:", 5)))) {
				/* wait 10 ms to avoid trying to get another packet */
				/* XXX: horrible */
				NX_Delay(10);
				continue;
		}
#endif

		if (mSeekReq) {
			ALOGI("readerEntry() : start seek ~~");
			startSeekMonitor(5*1000);	//	5 sec
			ret = avformat_seek_file(mFormatCtx, -1, INT64_MIN, mSeekPos, INT64_MAX, mSeekFlags);
			stopSeekMonitor();
			if (ret < 0) {
				ALOGE("%s: error while seeking", mFormatCtx->filename);
				mEOF2 = true;
				goto fail;
			} else {
				if (mAudioStreamIdx >= 0) {
					packet_queue_flush(&mAudioQ);
					packet_queue_put(&mAudioQ, &flush_pkt);
				}
				if (mVideoStreamIdx >= 0) {
					packet_queue_flush(&mVideoQ);
					packet_queue_put(&mVideoQ, &flush_pkt);
				}
				mEOF2 = false;
			}
			mSeekReq = 0;
		}

		/* if the queue are full, no need to read more */
		if (   mAudioQ.size + mVideoQ.size > MAX_QUEUE_SIZE
			|| (   (mAudioQ   .size  > MIN_AUDIOQ_SIZE || mAudioStreamIdx < 0)
			&& (mVideoQ   .nb_packets > MIN_FRAMES || mVideoStreamIdx < 0))) {
#if DEBUG_READ_ENTRY
				ALOGD("readerEntry, is full!!!(%d)", mMyInstanceId);
#endif
				/* wait 10 ms */
				NX_Delay(10);
				continue;
		}

		if (mEOF2) {
			if (mVideoStreamIdx >= 0) {
				av_init_packet(pkt);
				pkt->data = NULL;
				pkt->size = 0;
				pkt->stream_index = mVideoStreamIdx;
				packet_queue_put(&mVideoQ, pkt);
			}
			if (mAudioStreamIdx >= 0) {
				av_init_packet(pkt);
				pkt->data = NULL;
				pkt->size = 0;
				pkt->stream_index = mAudioStreamIdx;
				packet_queue_put(&mAudioQ, pkt);
			}
			NX_Delay(10);
			{
				Mutex::Autolock autoLock(mSeekLock);
				if( mSeekReq )
					continue;
			}
#if DEBUG_READ_ENTRY
			ALOGD("readerEntry, mEOF2 = 1, mVideoQ.size: %d, mVideoQ.nb_packets: %d, mAudioQ.size: %d, mAudioQ.nb_packets: %d",
				mVideoQ.size, mVideoQ.nb_packets, mAudioQ.size, mAudioQ.nb_packets);
#endif
			if (mAudioQ.size + mVideoQ.size  == 0) {
				if (mAutoExit) {
					ret = AVERROR_EOF;
					goto fail;
				}
			}
			mEOF2=false;
			continue;
		}

		if( mSeekReq )
			continue;
		ret = av_read_frame(mFormatCtx, pkt);
		mProbePkts++;
		if (ret < 0) {
			if (ret == AVERROR_EOF || url_feof(mFormatCtx->pb))
				if (ret == AVERROR_EOF) {
					//ALOGV("ret == AVERROR_EOF");
				}
				if (url_feof(mFormatCtx->pb)) {
					//ALOGV("url_feof(mFormatCtx->pb)");
				}

				mEOF2 = 1;
				mEOF = true;
				if (mFormatCtx->pb && mFormatCtx->pb->error) {
					ALOGE("mFormatCtx->pb->error: %d", mFormatCtx->pb->error);
					break;
				}
				NX_Delay(10);
				continue;
		}

		if (pkt->stream_index == mVideoStreamIdx)
		{
			if (mDefersToCreateVideoTrack)
			{
				AVCodecContext *avctx = mFormatCtx->streams[mVideoStreamIdx]->codec;

				if( avctx->codec_id == AV_CODEC_ID_H264 )
				{
					int i = parser_split(avctx, pkt->data, pkt->size);
					if (i > 0 && i < FF_MAX_EXTRADATA_SIZE) {
						if (avctx->extradata)
							av_freep(&avctx->extradata);
						avctx->extradata_size= i;
						avctx->extradata = (uint8_t *)av_malloc(avctx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
						if (!avctx->extradata) {
							//return AVERROR(ENOMEM);
							ret = AVERROR(ENOMEM);
							goto fail;
						}
						// sps + pps(there may be sei in it)
						memcpy(avctx->extradata, pkt->data, avctx->extradata_size);
						memset(avctx->extradata + i, 0, FF_INPUT_BUFFER_PADDING_SIZE);
					}
					else
					{
						av_free_packet(pkt);
						continue;
					}
				}

				stream_component_open(mVideoStreamIdx);
				if (!mDefersToCreateVideoTrack)
					ALOGI("probe packet counter: %d when create video track ok", mProbePkts);
				if (mProbePkts == EXTRACTOR_MAX_PROBE_PACKETS)
					ALOGI("probe packet counter to max: %d, create video track: %d",
					mProbePkts, !mDefersToCreateVideoTrack);
			}
			packet_queue_put(&mVideoQ, pkt);
		}
		else if (pkt->stream_index == mAudioStreamIdx)
		{
			int ret;
			uint8_t *outbuf;
			int   outbuf_size;
			AVCodecContext *avctx = mFormatCtx->streams[mAudioStreamIdx]->codec;
			if (mAudioBsfc && pkt && pkt->data) {
				ret = av_bitstream_filter_filter(mAudioBsfc, avctx, NULL, &outbuf, &outbuf_size,
					pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);

				if (ret < 0 ||!outbuf_size) {
					av_free_packet(pkt);
					continue;
				}
				if (outbuf && outbuf != pkt->data) {
					memmove(pkt->data, outbuf, outbuf_size);
					pkt->size = outbuf_size;
				}
			}
			if (mDefersToCreateAudioTrack) {
				if (avctx->extradata_size <= 0) {
					av_free_packet(pkt);
					continue;
				}
				stream_component_open(mAudioStreamIdx);
				if (!mDefersToCreateAudioTrack)
					ALOGI("probe packet counter: %d when create audio track ok", mProbePkts);
				if (mProbePkts == EXTRACTOR_MAX_PROBE_PACKETS)
					ALOGI("probe packet counter to max: %d, create audio track: %d",
					mProbePkts, !mDefersToCreateAudioTrack);
			}
			packet_queue_put(&mAudioQ, pkt);
		}
		else
		{
			av_free_packet(pkt);
		}
	}
	/* wait until the end */
	while (!mAbortRequest) {
		NX_Delay(100);
	}

	ret = 0;
fail:
	ALOGI("reader thread goto end...");

	/* close each stream */
	if (mAudioStreamIdx >= 0)
		stream_component_close(mAudioStreamIdx);
	if (mVideoStreamIdx >= 0)
		stream_component_close(mVideoStreamIdx);
	if (mFormatCtx) {
		avformat_close_input(&mFormatCtx);
	}
}
//
//////////////////////////////////////////////////////////////////////////////







//////////////////////////////////////////////////////////////////////////////
//
//			Implementation Sniffer Application
//

//
//	Find number of tracks that can playable.
//
static int get_num_supported_audio_tracks(AVFormatContext *avfctx)
{
	int count = 0;
	unsigned int i;
	for( i=0; i<avfctx->nb_streams ; i++ )
	{
		switch ( avfctx->streams[i]->codec->codec_id )
		{
			case AV_CODEC_ID_AAC:
			case AV_CODEC_ID_AC3:
			case AV_CODEC_ID_MP1:
			case AV_CODEC_ID_MP2:
			case AV_CODEC_ID_MP3:
			case AV_CODEC_ID_WMAV1:
			case AV_CODEC_ID_WMAV2:
			case AV_CODEC_ID_WMAPRO:
			case AV_CODEC_ID_WMALOSSLESS:
			case AV_CODEC_ID_COOK:
			case AV_CODEC_ID_DTS:
			case AV_CODEC_ID_FLAC:
			case AV_CODEC_ID_APE:
			case AV_CODEC_ID_VORBIS:
			case AV_CODEC_ID_PCM_S16LE:
				count ++;
				break;
			default:
				break;
		}
	}
	return count;
}

static int get_num_supported_video_tracks(AVFormatContext *avfctx)
{
	int count = 0;
	unsigned int i;
	for( i=0; i<avfctx->nb_streams ; i++ )
	{
		switch ( avfctx->streams[i]->codec->codec_id )
		{
			case AV_CODEC_ID_H264:
			case AV_CODEC_ID_MPEG4:
			case AV_CODEC_ID_FLV1:
			case AV_CODEC_ID_MSMPEG4V3:
			case AV_CODEC_ID_H263:
			case AV_CODEC_ID_H263P:
			case AV_CODEC_ID_H263I:
			case AV_CODEC_ID_MPEG1VIDEO:
			case AV_CODEC_ID_MPEG2VIDEO:
			case AV_CODEC_ID_WMV3:
			case AV_CODEC_ID_VC1:
			case AV_CODEC_ID_VP8:
			case AV_CODEC_ID_VP9:
			case AV_CODEC_ID_RV40:
			case AV_CODEC_ID_HEVC:
				count ++;
				break;
			default:
				break;
		}
	}
	return count;
}


// LegacySniffFFMPEG
typedef struct {
	const char *extension;
	const char *container;
} extmap;

static extmap FILE_EXTS[] = {
#if 0
	{".mp3", MEDIA_MIMETYPE_AUDIO_MPEG},
#endif
	{".mov", MEDIA_MIMETYPE_CONTAINER_MOV},
	{".3gp", MEDIA_MIMETYPE_CONTAINER_MPEG4},
	{".mp4", MEDIA_MIMETYPE_CONTAINER_MPEG4},
	{".mkv", MEDIA_MIMETYPE_CONTAINER_MATROSKA},
	{".ts",  MEDIA_MIMETYPE_CONTAINER_MPEG2TS},
	{".avi", MEDIA_MIMETYPE_CONTAINER_AVI},
	{".divx", MEDIA_MIMETYPE_CONTAINER_AVI},
	{".asf", MEDIA_MIMETYPE_CONTAINER_ASF},
	{".rm ", MEDIA_MIMETYPE_CONTAINER_RM},
	{".rmvb", MEDIA_MIMETYPE_CONTAINER_RM},
	{".wmv", MEDIA_MIMETYPE_CONTAINER_WMV},
	{".flv", MEDIA_MIMETYPE_CONTAINER_FLV},
#if 0
	{".wma", MEDIA_MIMETYPE_CONTAINER_WMA},
	{".mpg", MEDIA_MIMETYPE_CONTAINER_MPG},
	{".divx", MEDIA_MIMETYPE_CONTAINER_DIVX},
	{".mp2", MEDIA_MIMETYPE_CONTAINER_MP2},
	{".ape", MEDIA_MIMETYPE_CONTAINER_APE},
	{".ra",  MEDIA_MIMETYPE_CONTAINER_RA},
#endif
};

const char *LegacySniffFFMPEG(const char * uri)
{
	size_t i;
	const char *container = NULL;

	ALOGI("list the file extensions suppoted by ffmpeg: ");
	ALOGI("========================================");
	for (i = 0; i < NELEM(FILE_EXTS); ++i) {
		ALOGV("file_exts[%02d]: %s", i, FILE_EXTS[i].extension);
	}
	ALOGI("========================================");

	int lenURI = strlen(uri);
	for (i = 0; i < NELEM(FILE_EXTS); ++i) {
		int len = strlen(FILE_EXTS[i].extension);
		int start = lenURI - len;
		if (start > 0) {
			if (!strncasecmp(uri + start, FILE_EXTS[i].extension, len)) {
				container = FILE_EXTS[i].container;
				break;
			}
		}
	}

	return container;
}

// BetterSniffFFMPEG

const char *BetterSniffFFMPEG(const char * uri, bool &useFFMPEG, bool dumpInfo)
{
	size_t i;
	int err;
	const char *container = NULL;
	AVFormatContext *ic = NULL;

	status_t status = initFFmpeg();
	if (status != OK) {
		ALOGE("could not init ffmpeg");
		return NULL;
	}

	ic = avformat_alloc_context();
	err = avformat_open_input(&ic, uri, NULL, NULL);
	if (err < 0) {
		ALOGE("avformat_open_input faild, err: %d(%s)", err, uri);
		return NULL;
	}

	err = avformat_find_stream_info(ic, NULL);
	if (err < 0) {
		goto ErrorExit;
	}
#if 0
	if( get_num_supported_video_tracks(ic) < 1 )
	{
		ALOGI("BetterSniffFFMPEG() : Have no video stream for playable.!!");
		goto ErrorExit;
	}
#else  //Audio File Support (2016.09.07)
	if( get_num_supported_video_tracks(ic) < 1 )
	{
		ALOGI("BetterSniffFFMPEG() : Have no video stream for playable.!!");
	}
#endif	

	if( dumpInfo )
		av_dump_format(ic, 0, uri, 0);

	for( i=0 ; i < ic->nb_streams ;  i++ )
	{
		AVCodecID codec_id = ic->streams[i]->codec->codec_id;
		if( codec_id == AV_CODEC_ID_MP1         ||
			codec_id == AV_CODEC_ID_MP2         ||
			codec_id == AV_CODEC_ID_MP3         ||
			codec_id == AV_CODEC_ID_FLAC        ||
			codec_id == AV_CODEC_ID_DTS         ||
			codec_id == AV_CODEC_ID_WMAV1       ||
			codec_id == AV_CODEC_ID_WMAV2       ||
			codec_id == AV_CODEC_ID_WMAPRO      ||
			codec_id == AV_CODEC_ID_WMALOSSLESS ||
			codec_id == AV_CODEC_ID_COOK        ||
			codec_id == AV_CODEC_ID_AC3 )
		{
			useFFMPEG= true;
		}
		if( codec_id == AV_CODEC_ID_AAC )
		{
			if( ic->streams[i]->codec->profile == FF_PROFILE_AAC_MAIN )
			{
				useFFMPEG= true;
			}
		}
		if( codec_id == AV_CODEC_ID_WMV3 ||
			codec_id == AV_CODEC_ID_VC1 ||
			codec_id == AV_CODEC_ID_RV40 )
		{
			useFFMPEG= true;
		}
	}

	ALOGI("FFmpegExtrator::BetterSniffFFMPEG uri: %s, format_name: %s, format_long_name: %s", uri, ic->iformat->name, ic->iformat->long_name);
#if 0
	ALOGI("list the format suppoted by ffmpeg: ");
	ALOGI("========================================");
	for (i = 0; i < NELEM(FILE_FORMATS); ++i) {cc
		ALOGV("format_names[%02d]: %s", i, FILE_FORMATS[i].format);
	}
	ALOGI("========================================");
#endif
	for (i = 0; i < NELEM(FILE_FORMATS); ++i) {
		int len = strlen(FILE_FORMATS[i].format);
		if (!strncasecmp(ic->iformat->name, FILE_FORMATS[i].format, len)) {
			container = FILE_FORMATS[i].container;
			break;
		}
	}
ErrorExit:
	avformat_close_input(&ic);
	av_free(ic);

	return container;
}


const char *Better2SniffFFMPEG(const sp<DataSource> &source, bool &useFFMPEG, bool dumpInfo)
{
	size_t i;
	int err;
	size_t len = 0;
	char url[128] = {0};
	const char *container = NULL;
	AVFormatContext *ic = NULL;
	useFFMPEG= false;

	status_t status = initFFmpeg();
	if (status != OK) {
		ALOGE("could not init ffmpeg");
		return NULL;
	}

	ALOGI("android source: %x", (int)&source);

	// pass the addr of smart pointer("source")
	snprintf(url, sizeof(url), "android-source:%x", (int)&source);

	ic = avformat_alloc_context();
	err = avformat_open_input(&ic, url, NULL, NULL);
	if (err < 0) {
		ALOGE("avformat_open_input faild, err: %d(url=%s)", err, url);
		return NULL;
	}

	err = avformat_find_stream_info(ic, NULL);
	if (err < 0) {
		goto ErrorExit;
	}

	if( dumpInfo )
		av_dump_format(ic, 0, url, 0);

	ALOGI("FFmpegExtrator::Better2SniffFFMPEG url: %s, format_name: %s, format_long_name: %s", url, ic->iformat->name, ic->iformat->long_name);

#if 0
	if( get_num_supported_video_tracks(ic) < 1 )
	{
		ALOGI("Better2SniffFFMPEG() :Have no video stream for playable.!!");
		goto ErrorExit;
	}
#else  //Audio File Support (2016.09.07)
	if( get_num_supported_video_tracks(ic) < 1 )
	{
		ALOGI("Better2SniffFFMPEG() :Have no video stream for playable.!!");
	}
#endif	

	for( i=0 ; i < ic->nb_streams ;  i++ )
	{
		AVCodecID codec_id = ic->streams[i]->codec->codec_id;
		if( codec_id == AV_CODEC_ID_MP1         ||
			codec_id == AV_CODEC_ID_MP2         ||
			codec_id == AV_CODEC_ID_MP3         ||
			codec_id == AV_CODEC_ID_FLAC        ||
			codec_id == AV_CODEC_ID_DTS         ||
			codec_id == AV_CODEC_ID_WMAV1       ||
			codec_id == AV_CODEC_ID_WMAV2       ||
			codec_id == AV_CODEC_ID_WMAPRO      ||
			codec_id == AV_CODEC_ID_WMALOSSLESS ||
			codec_id == AV_CODEC_ID_COOK        ||
			codec_id == AV_CODEC_ID_APE         ||
			codec_id == AV_CODEC_ID_AC3 )
		{
			useFFMPEG= true;
		}
		if( codec_id == AV_CODEC_ID_AAC )
		{
			if( ic->streams[i]->codec->profile == FF_PROFILE_AAC_MAIN )
			{
				useFFMPEG= true;
			}
		}
		if( codec_id == AV_CODEC_ID_WMV3 ||
			codec_id == AV_CODEC_ID_VC1 ||
			codec_id == AV_CODEC_ID_RV40 )
		{
			useFFMPEG= true;
		}
	}

#if 0
	ALOGI("list the format suppoted by ffmpeg: ");
	ALOGI("========================================");
	for (i = 0; i < NELEM(FILE_FORMATS); ++i) {
		ALOGV("format_names[%02d]: %s", i, FILE_FORMATS[i].format);
	}
	ALOGI("========================================");
#endif

	for (i = 0; i < NELEM(FILE_FORMATS); ++i) {
		int len = strlen(FILE_FORMATS[i].format);
		if (!strncasecmp(ic->iformat->name, FILE_FORMATS[i].format, len)) {
			container = FILE_FORMATS[i].container;
			break;
		}
	}

ErrorExit:

	avformat_close_input(&ic);
	av_free(ic);

	return container;
}

bool SniffAVIFFMPEG(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *) {
    char tmp[12];
    if (source->readAt(0, tmp, 12) < 12) {
        return false;
    }

    if (!memcmp(tmp, "RIFF", 4) && !memcmp(&tmp[8], "AVI ", 4)) {
        mimeType->setTo(MEDIA_MIMETYPE_CONTAINER_AVI);

        // Just a tad over the mp3 extractor's confidence, since
        // these .avi files may contain .mp3 content that otherwise would
        // mistakenly lead to us identifying the entire file as a .mp3 file.
        *confidence = 0.21;

        return true;
    }

    return false;
}

bool SniffFFMPEG( const sp<DataSource> &source, String8 *mimeType, float *confidence, sp<AMessage> *meta)
{
	ALOGV("SniffFFMPEG");
	const char *uri = NULL;
	const char *container = NULL;
	bool bUseFfmpeg = false;
	uri = source->getUri();

	if (!uri)
		return false;

	ALOGI("ffmpeg uri: %s", uri);

	if( strncmp("file://", uri, 7) && strlen(uri)!=0 )
	{
		return false;
	}

	container = Better2SniffFFMPEG(source, bUseFfmpeg, false);
	if (!container)
	{
		ALOGW("sniff through Better2SniffFFMPEG failed, try BetterSniffFFMPEG");
#if 0
		container = BetterSniffFFMPEG(uri, bUseFfmpeg, false);
		if (!container)
		{
			ALOGW("sniff through BetterSniffFFMPEG failed!");
		}
		else
		{
			ALOGI("sniff through BetterSniffFFMPEG success");
		}
#endif
	}
	else
	{
		ALOGI("sniff through Better2SniffFFMPEG success");
	}

	if (container == NULL)
		return false;

	if( !strcmp( container, MEDIA_MIMETYPE_CONTAINER_MPEG4 ) && !bUseFfmpeg )
		*confidence = 0.39f;
	else if( !strcmp( container, MEDIA_MIMETYPE_CONTAINER_MPEG2TS ) )
		*confidence = 0.09f;
	else if( !strcmp( container, MEDIA_MIMETYPE_CONTAINER_MATROSKA ) && !bUseFfmpeg )
		*confidence = 0.59f;
	else if( !strcmp( container, MEDIA_MIMETYPE_CONTAINER_MPEG2PS ) )
		*confidence = 0.24f;
	else
		*confidence = 0.88f;  // Slightly larger than other extractor's confidence

	ALOGV("found container: %s, bUseFfmpeg = %d, confidence = %f", container, bUseFfmpeg, *confidence);

	mimeType->setTo(container);

	/* use MPEG4Extractor(not extended extractor) for HTTP source only */
	// if (!strcasecmp(container, MEDIA_MIMETYPE_CONTAINER_MPEG4) && (source->flags() & DataSource::kIsCachingDataSource))
	// {
	// 	return true;
	// }

	*meta = new AMessage;
	(*meta)->setString("extended-extractor", "extended-extractor");
	(*meta)->setString("extended-extractor-subtype", "ffmpegextractor");
	(*meta)->setInt32("isFFMpegExtractor", 1);

	return true;
}
//
//////////////////////////////////////////////////////////////////////////////


static status_t addVorbisCodecInfo(
        const sp<MetaData> &meta,
        const void *_codecPrivate, size_t codecPrivateSize) {
    // hexdump(_codecPrivate, codecPrivateSize);

    if (codecPrivateSize < 1) {
        return ERROR_MALFORMED;
    }

    const uint8_t *codecPrivate = (const uint8_t *)_codecPrivate;

    if (codecPrivate[0] != 0x02) {
        return ERROR_MALFORMED;
    }

    // codecInfo starts with two lengths, len1 and len2, that are
    // "Xiph-style-lacing encoded"...

    size_t offset = 1;
    size_t len1 = 0;
    while (offset < codecPrivateSize && codecPrivate[offset] == 0xff) {
        len1 += 0xff;
        ++offset;
    }
    if (offset >= codecPrivateSize) {
        return ERROR_MALFORMED;
    }
    len1 += codecPrivate[offset++];

    size_t len2 = 0;
    while (offset < codecPrivateSize && codecPrivate[offset] == 0xff) {
        len2 += 0xff;
        ++offset;
    }
    if (offset >= codecPrivateSize) {
        return ERROR_MALFORMED;
    }
    len2 += codecPrivate[offset++];

    if (codecPrivateSize < offset + len1 + len2) {
        return ERROR_MALFORMED;
    }

    if (codecPrivate[offset] != 0x01) {
        return ERROR_MALFORMED;
    }
    meta->setData(kKeyVorbisInfo, 0, &codecPrivate[offset], len1);

    offset += len1;
    if (codecPrivate[offset] != 0x03) {
        return ERROR_MALFORMED;
    }

    offset += len2;
    if (codecPrivate[offset] != 0x05) {
        return ERROR_MALFORMED;
    }

    meta->setData(
            kKeyVorbisBooks, 0, &codecPrivate[offset],
            codecPrivateSize - offset);

    return OK;
}

}  // namespace android
