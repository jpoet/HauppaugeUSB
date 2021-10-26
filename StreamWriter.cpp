#include "StreamWriter.h"
#include "Logger.h"
#include "Transcoder.h"
#include <thread>

extern "C" {
#include <libavutil/timestamp.h>
#include <libavutil/channel_layout.h>
}

extern "C" int staticWriteBuffer(void * ctx, uint8_t * buf, int buf_size)
{
    return ((StreamWriter *)ctx)->WriteBuffer(buf, buf_size);
}

StreamWriter::StreamWriter(Transcoder * transcoder,
                           const std::string & filename,
                           DataTransfer::callback_t * cb)
: m_transcoder(transcoder)
, m_filename(filename)
, m_cb(cb)
, m_oAVFContext(nullptr)
, m_avioContext(nullptr)
, m_oFormat(nullptr)
, m_initialized(false)
, m_fd(-1)
{
    if (!filename.empty() || cb)
    {
        unsigned char * buffer = (unsigned char *)av_malloc(1024 * 1024);
        m_oAVFContext = avformat_alloc_context();

        m_avioContext =
            avio_alloc_context(buffer, 1024 * 1024, 1, (void *)this, nullptr,
                               &staticWriteBuffer, nullptr);

        m_oFormat = av_guess_format("mpegts", nullptr, nullptr);
    }
}

StreamWriter::~StreamWriter()
{
    if (m_fd > 1)
        close(m_fd);

    if (m_avioContext)
    {
        av_free(m_avioContext->buffer);
        avio_context_free(&m_avioContext);
    }

    if (m_oAVFContext)
    {
        avformat_free_context(m_oAVFContext);
        m_oAVFContext = nullptr;
    }
}

static void log_packet(const AVFormatContext * fmt_ctx, const AVPacket * pkt,
                       const char * tag)
{
    AVRational * time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    char buf1[64];
    char buf2[64];
    char buf3[64];
    char buf4[64];
    char buf5[64];
    char buf6[64];

    const AVCodec * codec = avcodec_find_decoder(
        fmt_ctx->streams[pkt->stream_index]->codecpar->codec_id);

    DEBUGLOG << tag << ": pts:" << av_ts_make_string(buf1, pkt->pts)
             << " pts_time:"
             << av_ts_make_time_string(buf2, pkt->pts, time_base)
             << " dts:" << av_ts_make_string(buf3, pkt->dts) << " dts_time:"
             << av_ts_make_time_string(buf4, pkt->dts, time_base)
             << " duration:" << av_ts_make_string(buf5, pkt->duration)
             << " duration_time:"
             << av_ts_make_time_string(buf6, pkt->duration, time_base)
             << " stream_index:" << pkt->stream_index
             << " codec:" << codec->name;
}

void StreamWriter::Reset()
{
    if (m_fd > 1)
        close(m_fd);

    if (m_avioContext)
    {
        av_free(m_avioContext->buffer);
        avio_context_free(&m_avioContext);
    }

    if (m_oAVFContext)
    {
        avformat_free_context(m_oAVFContext);
        m_oAVFContext = nullptr;
    }

    if (!m_filename.empty() || m_cb)
    {
        unsigned char * buffer = (unsigned char *)av_malloc(1024 * 1024);
        m_oAVFContext = avformat_alloc_context();

        m_avioContext =
            avio_alloc_context(buffer, 1024 * 1024, 1, (void *)this, nullptr,
                               &staticWriteBuffer, nullptr);

        m_oFormat = av_guess_format("mpegts", nullptr, nullptr);
    }

    m_initialized = false;
    WARNLOG << "StreamWriter::Reset called";
}

void StreamWriter::Pause()
{
    m_paused = true;
}

void StreamWriter::Resume()
{
    m_paused = false;
}

bool StreamWriter::WritePacket(AVPacket * pkt)
{
    int ret;

    if (m_paused)
        return true;

    if (!m_initialized)
    {
        m_oAVFContext->pb = m_avioContext;
        m_oAVFContext->oformat = m_oFormat;

        AVStream * out_stream;
        AVStream * in_stream =
            m_transcoder->m_streamBuffer->m_iAVFContext->streams[0];
        AVCodecParameters * in_codecpar = in_stream->codecpar;

        out_stream = avformat_new_stream(m_oAVFContext, NULL);
        if (!out_stream)
        {
            ERRORLOG << "Failed allocating output stream";
            return false;
        }

        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0)
        {
            ERRORLOG << "Failed to copy codec parameters";
            return false;
        }
        out_stream->codecpar->codec_tag = 0;

        out_stream = avformat_new_stream(m_oAVFContext, NULL);
        if (!out_stream)
        {
            ERRORLOG << "Failed allocating output stream";
            return false;
        }

        out_stream->codecpar->channel_layout = AV_CH_LAYOUT_5POINT1;
        out_stream->codecpar->channels = 6;
        out_stream->codecpar->codec_id = AV_CODEC_ID_AC3;
        out_stream->codecpar->format = AV_SAMPLE_FMT_FLTP;
        out_stream->codecpar->codec_tag = 0;

        m_oAVFContext->streams[0]->time_base =
            m_transcoder->m_streamBuffer->m_iAVFContext->streams[0]->time_base;
        m_oAVFContext->streams[1]->time_base =
            m_transcoder->m_streamBuffer->m_iAVFContext->streams[1]->time_base;

        // av_dump_format(m_oAVFContext, 0, "out", 1);

        ret = avformat_write_header(m_oAVFContext, NULL);
        if (ret < 0)
        {
            ERRORLOG << "Error occurred when opening output file";
            return false;
        }

        if (!m_filename.empty())
        {
            if (m_filename == "stdout" || m_filename == "-")
            {
                m_fd = 1;
            }
            else
            {
                m_fd = open(m_filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                            S_IRWXU | S_IRGRP | S_IROTH);
                if (m_fd < 0)
                {
                    ERRORLOG << "Could not open file '" << m_filename
                             << "' for writing: " << errno;
                    return false;
                }
            }
        }

        m_initialized = true;
    }

    log_packet(m_oAVFContext, pkt, "out");
    av_packet_rescale_ts(
        pkt,
        m_transcoder->m_streamBuffer->m_iAVFContext->streams[pkt->stream_index]
            ->time_base,
        m_oAVFContext->streams[pkt->stream_index]->time_base);
    if (pkt->pts < pkt->dts)
        pkt->pts = pkt->dts;


    ret = av_interleaved_write_frame(m_oAVFContext, pkt);
    if (ret == -22) {
        if (pkt->pts < pkt->dts)
            pkt->pts = pkt->dts;
        ret = av_interleaved_write_frame(m_oAVFContext, pkt);
    }
    if (ret < 0)
    {
        ERRORLOG << "Error writing frame for stream " << pkt->stream_index << " error: " << ret;
        return false;
    }

    return true;
}

int StreamWriter::WriteBuffer(uint8_t * buf, int buf_size)
{
    if (m_cb)
        (*m_cb)(buf, buf_size);

    if (m_fd >= 1)
        return write(m_fd, buf, buf_size);

    return buf_size;
}
