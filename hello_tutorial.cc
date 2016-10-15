#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <ppapi_simple/ps_main.h>
#include <ppapi_simple/ps_event.h>
#include <ppapi_simple/ps_interface.h>

#include <ppapi/c/ppb_var_array_buffer.h>

#pragma clang diagnostic ignored "-Wswitch"

PPB_InputEvent* g_pInputEvent;
struct PP_Var data; void *p;

int valueR;

extern "C" 
{
    #include "libavutil/opt.h"
    #include "libavcodec/avcodec.h"
    #include "libavutil/channel_layout.h"
    #include "libavutil/common.h"
    #include "libavutil/imgutils.h"
    #include "libavutil/mathematics.h"
    #include "libavutil/samplefmt.h"
    #include "libavformat/avformat.h" 
    //#include "libavcodec/avcodec.h"
    #include "libavutil/error.h" 
    #include "libavdevice/avdevice.h"
    #include "libavformat/avformat.h" 
    #include "libavutil/error.h"
    #include "libswscale/swscale.h" 
    #include "libavutil/pixfmt.h"
}

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFIL_THRESH 4096

AVFormatContext *formatContext = NULL;
AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
int width, height;
enum AVPixelFormat pix_fmt;
AVStream *video_stream = NULL, *audio_stream = NULL;
const char *src_filename = NULL;
//const char *video_dst_filename = "video";
//const char *audio_dst_filename = "audio";
//FILE *video_dst_file = NULL;
//FILE *audio_dst_file = NULL;
uint8_t *video_dst_data[4] = { NULL };
int      video_dst_linesize[4];
int video_dst_bufsize;
int video_stream_index, audio_stream_index;
AVFrame *frame = NULL;
AVPacket pkt;
int video_frame_count = 0;
int audio_frame_count = 0;
int result = -1;
int got_frame;
int got_pkt;

int sourceX, sourceY, destX, destY;
//AVFrame avFrameRGB;
uint8_t * src_data[4], *dst_data[4];
int src_linesize[4], dst_linesize[4];

struct SwsContext *sws_ctx;

//process the event from javascript
void ProcessEvent(PSEvent* event) {
  switch(event->type) {
    /* If the view updates, build a new Graphics 2D Context */
    case PSE_INSTANCE_DIDCHANGEVIEW: {
      //struct PP_Rect rect;
      //g_pView->GetRect(event->as_resource, &rect);
      //UpdateContext(rect.size.width, rect.size.height);
      break;
    }
    case PSE_INSTANCE_HANDLEMESSAGE:{
        if (event->as_var.type == PP_VARTYPE_STRING) {
            //const char* message;
            //uint32_t len;
            //message = PSInterfaceVar()->VarToUtf8(event->as_var, &len);
            //std::cout << "This was the message: " <<std::endl;
            //std::cout << message << std::endl;
            /*for (int i = 0; i < 1000 ; i++){
                usleep(2000);
                int num_bytes = 240*120*4;
                uint8_t* array = (uint8_t*) malloc (num_bytes);
                for (int i = 0 ; i < num_bytes ; i++){
                    array[i] = rand() % 256;
                }
                struct PP_Var data = PSInterfaceVarArrayBuffer()->Create(num_bytes);
                void *p = PSInterfaceVarArrayBuffer()->Map(data);
                p = memcpy(p, array, num_bytes);
                PSInterfaceVarArrayBuffer()->Unmap(data);
                PSInterfaceMessaging()->PostMessage(PSGetInstanceId(), data);
                //free(p);
                //free(array);
            }*/
          // Do something with the message. Note that it is NOT null-terminated.
        break;
      }
      break;
    }
    default:
      break;
  }
}


//printing of the error string
  void message_error(int errnum){
        char errbuf[1024];
        //av_make_error_string (errbuf, sizeof(errbuf), errnum);
        av_strerror (errnum, errbuf, sizeof(errbuf));
        //std::cerr << errbuf << std::endl;
  }

//log messages on the output console
void log(std::string message){
    std::cout << message << std::endl;
}

int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    message_error(ret);
    //ret = find_stream_index(fmt_ctx, type);
    if (ret < 0) {
        //fprintf(stderr, "Could not find %s stream in input file '%s'\n",av_get_media_type_string(type), src_filename);
        message_error(ret);
        return ret;
    }
    else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        /* find decoder for the stream */
        dec_ctx = st->codec;
        dec = avcodec_find_decoder(dec_ctx->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        /* Init the decoders, with or without reference counting */
        //if (api_mode == API_MODE_NEW_API_REF_COUNT)
            //av_dict_set(&opts, "refcounted_frames", "1", 0);
        if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }
    return 0;
}

int decode_packet(int *got_frame, int cached)
{
    //log("inside decode packet ...");
    int ret = 0;
    int decoded = pkt.size;
    *got_frame = 0;
    if (pkt.stream_index == video_stream_index) {
        //log("inside video stream index");
        /* decode video frame */
        //std::cout<<"V_b";
        ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            //fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
            std::cerr<<"Cannot decode video: "<<std::endl;
            message_error(ret);
            return ret;
        }else{
            //log("Video packet successfully decoded ...");
            //std::cout << "|";
        }
        if (*got_frame) {
            if (frame->width != width || frame->height != height ||
                frame->format != pix_fmt) {
                /* To handle this change, one could call av_image_alloc again and
                * decode the following frames into another rawvideo file. */
                fprintf(stderr, "Error: Width, height and pixel format have to be "
                    "constant in a rawvideo file, but the width, height or "
                    "pixel format of the input video changed:\n"
                    "old: width = %d, height = %d, format = %s\n"
                    "new: width = %d, height = %d, format = %s\n",
                    width, height, av_get_pix_fmt_name(pix_fmt),
                    frame->width, frame->height,
                    av_get_pix_fmt_name((AVPixelFormat)frame->format));
                return -1;
            }
            //video_frame_count++;
            //std::cout << "a: " << video_frame_count << ": ";



            av_image_copy(src_data, src_linesize, (const uint8_t**)(frame->data) , frame->linesize , video_dec_ctx->pix_fmt, width , height);
            //std::cout << "c"<<*got_frame;
            sws_scale(sws_ctx , (const uint8_t * const*) src_data, src_linesize, 0 , sourceY, (uint8_t * const*) dst_data , dst_linesize);
            //std::cout << "s";

            int num_bytes = video_dst_bufsize;
            //int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGR32, destX, destY , 1);
            //void * q = NULL;

            //std::cout << "pp";
            //data = PSInterfaceVarArrayBuffer()->Create(num_bytes);
            //std::cout << "_create_";
            //p = PSInterfaceVarArrayBuffer()->Map(data);
            //std::cout << "_map_";
            p = memcpy(p, dst_data[0], num_bytes);
            //p = memcpy(p, frame->data[0], num_bytes);
            //q = memcpy(q, dst_data[0], num_bytes);
            //av_image_copy_to_buffer((uint8_t*)p, num_bytes, (const uint8_t * const*)(dst_data) , dst_linesize, AV_PIX_FMT_BGR32, destX , destY, 16);
            //std::cout << "_memcpy_";
            //PSInterfaceVarArrayBuffer()->Unmap(data);
            //std::cout << "_unmap_";
            //free(p);
            PSInterfaceMessaging()->PostMessage(PSGetInstanceId(), data);
            usleep(2000);
            //std::cout << "_post_   ";

            
        }
    }
    else if (pkt.stream_index == audio_stream_index) {
        /* decode audio frame */
        //std::cout<<"A_b";
        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
        //std::cout<<"a";
        if (ret < 0) {
            std::cerr<<"Cannot decode audio: "<<std::endl;
            message_error(ret);
            return ret;
        }else{
            //std::cout << "*";
        }
        /* Some audio decoders decode only part of the packet, and have to be
        * called again with the remainder of the packet data.
        * Sample: fate-suite/lossless-audio/luckynight-partial.shn
        * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, pkt.size);
        //std::cout<<"_decoded_";
        if (*got_frame) {
            //size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
            //std::cerr << unpadded_linesize;
            audio_frame_count++;
            //std::cout<<"__got_frame " << audio_frame_count <<"    ";
            
            //printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",   cached ? "(cached)" : "", audio_frame_count++, frame->nb_samples,av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));
            /* Write the raw audio data samples of the first plane. This works
            * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
            * most audio decoders output planar audio, which uses a separate
            * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
            * In other words, this code will write only the first audio channel
            * in these cases.
            * You should use libswresample or libavfilter to convert the frame
            * to packed data. */
            //fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
            
        }
    }
    //std::cout<<"{ret}  ";
    return decoded;
}

int ppapi_simple_main(int argc, char* argv[]) {
    

    for (int i=0; i<argc; ++i) {
        std::cout << "Argument " << i << ": " << argv[i] << std::endl;
    }

    std::cerr << "Standard error output appears in the debug console\n";

    AVDictionary * avdic = NULL;
    char option_key[]="rtsp_transport";
	char option_value[]="tcp";
	av_dict_set(&avdic,option_key,option_value,0);	

	av_register_all();
    avformat_network_init();
    log("Everything registered ...");
    formatContext = avformat_alloc_context();
    log("Context intialized");
  
    //http://qthttp.apple.com.edgesuite.net/1010qwoeiuryfg/sl.m3u8
    src_filename = argv[0];
    //src_filename = "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_115k.mov";
    //src_filename = "http://www.sample-videos.com/video/mp4/720/big_buck_bunny_720p_1mb.mp4";
    //src_filename = "rtsp://r2---sn-4g57kuek.c.youtube.com/CiILENy73wIaGQmQKAHy11zSfRMYESARFEgGUgZ2aWRlb3MM/0/0/0/video.3gp";
    //src_filename = "rtsp://52.29.248.13:8554/desktop";
    //src_filename = "http://cdn-fms.rbs.com.br/vod/hls_sample1_manifest.m3u8";
    result = avformat_open_input( &formatContext , src_filename , NULL , NULL);
    if(result< 0){
    	log("format not opened");
    	std::cout << result << std::endl;
    	message_error(result);
    }else{
    	log("Format successfully opened ... ");
    }

    result = avformat_find_stream_info(formatContext, NULL);
    if(result< 0){
        std::cout << "Stream info not found" << std::endl;
        std::cout << result << std::endl;
        message_error(result);
    }else{
        log("Stream successfully found ... ");
    }


    for(int i = 0 ; i <formatContext->nb_streams; i++){
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            video_stream_index = i;
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
            audio_stream_index = i;
    }


    std::cout<< "Video stream index: " << video_stream_index << std::endl;
    std::cout<< "Audio stream index: " << audio_stream_index << std::endl;

    result = open_codec_context(&video_stream_index , formatContext , AVMEDIA_TYPE_VIDEO);
    if (result >= 0){
        video_stream = formatContext->streams[video_stream_index];
        video_dec_ctx = video_stream->codec;
        //video_dst_file = fopen(video_dst_filename, "wb");
        //if (!video_dst_filename){
            //std::cerr << "Could not open destination file: " << video_dst_filename << std::endl;
        //}
        /* allocate image where the decoded image will be put*/
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        result = av_image_alloc(video_dst_data, video_dst_linesize, width, height, pix_fmt, 1);
        if (result < 0){
            std::cerr << "Could not allocate raw frame" << std::endl;
        }
        video_dst_bufsize = result;
    }else{
        std::cerr<<"Video stream not opened ... "<< result << std::endl;
    }

    result = open_codec_context(&audio_stream_index, formatContext, AVMEDIA_TYPE_AUDIO);
    if (result >= 0) {
        audio_stream = formatContext->streams[audio_stream_index];
        audio_dec_ctx = audio_stream->codec;
        //audio_dst_file = fopen(audio_dst_filename, "wb");
        //if (!audio_dst_file) {
            //std::cerr << "Could not open destination file: " << audio_dst_filename << std::endl;
        //}
    }else{
        std::cerr<<"Audio stream not opened ... "<< result << std::endl;
    }

    log("before dumpting ...");
    /* dump input information to stderr */
    av_dump_format(formatContext, 0, src_filename , 0);
    log("After dumping ... ");
    if (!audio_stream && !video_stream){
        std::cerr << " Could not find audio or video stream in the input " <<std::endl;
    }

    log("This needs to proceed further ... ");

    sourceX = video_dec_ctx->width;
    sourceY = video_dec_ctx->height;
    destX = video_dec_ctx->width;
    destY = video_dec_ctx->height;

    if (!sws_ctx){
        //SWS_LANCZOS | SWS_ACCURATE_RND
        sws_ctx = sws_getContext(width , height , AV_PIX_FMT_YUV420P , width , height , AV_PIX_FMT_BGR32 , 0 , 0 , 0 , 0);
        if (!sws_ctx){
            std::cerr << "Could not initialize the conversion context" << std::endl;
        }else{
            std::cout<< "intialized the codec conversion context" << std::endl;
        }

    }

    if ((result = av_image_alloc(src_data, src_linesize, video_dec_ctx->width , video_dec_ctx->height , video_dec_ctx->pix_fmt, 16))  < 0){
        std::cout << result << ": could not allocate source image" << std::endl;
    }

    if ((result = av_image_alloc(dst_data, dst_linesize, video_dec_ctx->width, video_dec_ctx->height, AV_PIX_FMT_BGR32 , 1))<0){
        std::cout<< result << " : could not allocate destination image" << std::endl;
    }

    video_dst_bufsize = result;

    data = PSInterfaceVarArrayBuffer()->Create(video_dst_bufsize);
    p = PSInterfaceVarArrayBuffer()->Map(data);

    /*initialize packet, set data to NULL, let the demuxer fill it*/
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    if (video_stream){
        log("Demuxing video to the screen ... ");
    }

    if (audio_stream){
        log("Demuxing audio to the speakers");
    }

    frame = av_frame_alloc();
    log("KEY: AUDIO: *, VIDEO: |");
    got_pkt = 0;
    got_pkt = av_read_frame(formatContext, &pkt);
    std::cout << "Got packet flag: " << got_pkt << std::endl;
    //int packet_count = 0;
    while(av_read_frame(formatContext, &pkt) >= 0){
        AVPacket orig_pkt = pkt;
        do {
            result = decode_packet(&got_frame , 0);
            if (result < 0)
                break;
            pkt.data += result;
            pkt.size -= result;
        } while (pkt.size > 0);
        av_free_packet(&orig_pkt);
        //packet_count++;
        //if (packet_count % 100 == 0){
            //log("\n");
        //}
        //got_pkt = av_read_frame(formatContext, &pkt);
    }
    /*flush cached frames*/
    pkt.data = NULL;
    pkt.size = 0;
    do {
        decode_packet(&got_frame, 1);
    }while(got_frame);
    std::cout << "Demuxing successfully done" << std::endl;


    PSEventSetFilter(PSE_ALL);
    while (1) {
        /* Process all waiting events without blocking */
        PSEvent* event;
        while ((event = PSEventTryAcquire()) != NULL) {
            ProcessEvent(event);
            PSEventRelease(event);
        }
        /* Render a frame, blocking until complete. */
        //if (g_Context.bound) {
          //Render();
        //}
    }

    PSInterfaceVarArrayBuffer()->Unmap(data);
    avcodec_close(video_dec_ctx);
    avcodec_close(audio_dec_ctx);
    avformat_close_input(&formatContext);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);
    av_freep(&src_data[0]);
    av_freep(&dst_data[0]);
    sws_freeContext(sws_ctx);
    

    sleep(10);
    std::cout << "After 10 seconds..." <<std::endl;

    return 0;
}

// ... but we need to tell ppapi_simple about it:
PPAPI_SIMPLE_REGISTER_MAIN(ppapi_simple_main)