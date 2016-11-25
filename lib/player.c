//
// Created by Borchers, Henry Samuel on 11/23/16.
//

#include <stdio.h>
#include <avformat.h>
#include <avcodec.h>
#include <opt.h>
#include <assert.h>
#include "player.h"
#include <SDL2/SDL.h>
#include <swscale.h>


typedef struct{
    SDL_Window      *window;
    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
    int             width;
    int             height;
} myWindow;

//static AVFormatContext *formatCtx;
//static
static AVStream *video_stream = NULL;
//static AVFrame *frame = NULL;

static void initialize_ffmpeg();
static void cleanup_ffmpeg(AVCodecContext *dec_ctx);
static void initialize_sdl2();
static void cleanup_sdl2(myWindow *window);
static int setup_window(myWindow *window, int width, int height);

static int open_input_file(const char *filename, AVFormatContext **formatCtx, AVCodecContext **dec_ctx);

int decode(AVCodecContext *pContext, AVFrame *pFrame, int *got_frame, AVPacket *pkt);

int display_frame(AVFrame *pFrame, myWindow *window);

static int video_stream_index = -1;
static int mainloop(AVFormatContext *formatCtx, AVCodecContext *dec_ctx, myWindow *window);

int playVideo(const char *filename) {
    int ret;
    AVCodecContext *dec_ctx = NULL;
    myWindow window;

    initialize_player();
    AVFormatContext *formatCtx = NULL;

    printf("Using file %s\n", filename);

    puts("Opening file");
    if ((ret = open_input_file(filename, &formatCtx, &dec_ctx)) != 0){
        fprintf(stderr, "Unable to open Video file. Aborting.\n");
        return ret;
    };
    if(NULL == dec_ctx){
        puts("dec_ctx was null again!");
        return -1;
    }
    puts("File opened");
    fflush(stdout);
//    av_dump_format(formatCtx, 0, filename, 0);
    puts("Opening video window");

    if((ret = setup_window(&window, dec_ctx->width, dec_ctx->height)) < 0){
        fprintf(stderr, "Unable to create a window\n");
        return ret;
    };
    puts("Playing video");
    if ((ret = mainloop(formatCtx, dec_ctx, &window)) != 0){
        fprintf(stderr, "Video playback failed\n");
        return ret;
    };

    puts("Video stopped");

    avformat_close_input(&formatCtx);
    puts("Cleaning up");
    cleanup_sdl2(&window);
    cleanup_ffmpeg(dec_ctx);
    return 0;
}

void cleanup_player() {

}



int mainloop(AVFormatContext *formatCtx, AVCodecContext *dec_ctx, myWindow *window) {
    AVPacket pkt;
    AVFrame *frame = NULL;
    struct SwsContext *sws_ctx;
    SDL_Event event;
    int ret;
    int got_frame;

    frame = av_frame_alloc();
    if(!frame){
        return ENOMEM;
    }
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;


    sws_ctx = sws_getContext(dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                             dec_ctx->width, dec_ctx->height, AV_PIX_FMT_YUV420P,
                             SWS_BILINEAR, NULL, NULL, NULL);

    while(av_read_frame(formatCtx, &pkt) >= 0) {
        SDL_PollEvent(&event);
        if(event.type == SDL_QUIT){
            break;
        }
        if(pkt.stream_index == video_stream_index){

            ret = decode(dec_ctx, frame, &got_frame, &pkt);
            if(ret < 0){
                fprintf(stderr, "Error with a frame %s\n",av_err2str(ret));
                return ret;
            }
            if(got_frame){
                if(frame->width != window->width || frame->height != window->height){
                    return -1;
                }
                AVFrame pict;
                pict.data[0] = frame->data[0];
                pict.data[1] = frame->data[1];
                pict.data[2] = frame->data[2];

                pict.linesize[0] = frame->linesize[0];
                pict.linesize[1] = frame->linesize[1];
                pict.linesize[2] = frame->linesize[2];

                sws_scale(sws_ctx, (uint8_t const * const *)frame->data, frame->linesize, 0,dec_ctx->height, pict.data, pict.linesize);

                if(( ret = display_frame(frame, window)) != 0){
                    fprintf(stderr, "Problem displaying frame");
                    return ret;
                };
            }
        }
    }

    return 0;
}

int display_frame(AVFrame *pFrame, myWindow *window) {
    int res;

    SDL_RenderClear(window->renderer);
    if((res = SDL_UpdateYUVTexture(window->texture, NULL,
                                   pFrame->data[0], pFrame->linesize[0],
                                   pFrame->data[1], pFrame->linesize[1],
                                   pFrame->data[2], pFrame->linesize[2])) != 0){
        return res;
    };

    SDL_RenderCopy(window->renderer, window->texture, NULL, NULL);
    SDL_RenderPresent(window->renderer);
//    SDL_Delay(20);
    return 0;
}

int decode(AVCodecContext *pContext, AVFrame *pFrame, int *got_frame, AVPacket *pkt) {
    int ret;

    *got_frame = 0;

    if(pkt) {
        ret = avcodec_send_packet(pContext, pkt);
        if(ret < 0){
            return ret == AVERROR_EOF ? 0 : ret;
        }
    }

    ret = avcodec_receive_frame(pContext, pFrame);
    if(ret < 0 && ret != AVERROR(EAGAIN) && ret !=AVERROR_EOF) {
        return ret;
    } if (ret >= 0){
        *got_frame = 1;
    }

    return 0;
}


static int open_input_file(const char *filename, AVFormatContext **formatCtx, AVCodecContext **dec_ctx) {
    int ret;
    AVCodec *dec;
    fflush(stdout);

    if((ret = avformat_open_input(formatCtx, filename, NULL, NULL)) < 0){
        av_log(NULL, AV_LOG_ERROR, "Cannot open input\n");
        return ret;
    };


    if((ret = avformat_find_stream_info(*formatCtx, NULL)) < 0){
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream\n");
        return ret;
    }

    ret = av_find_best_stream(*formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec,0);
    if(ret < 0){
        av_log(NULL, AV_LOG_ERROR, "Cannot find video stream\n");
        return ret;
    }

    video_stream_index = ret;
    video_stream = (*formatCtx)->streams[video_stream_index];
    AVCodecContext *tmp = avcodec_alloc_context3(dec);
    *dec_ctx = tmp;
    if(*dec_ctx == NULL){
        av_log(NULL, AV_LOG_ERROR, "Unable to allocate codec context\n");
        return -1;
    }

    if((ret = avcodec_parameters_to_context(*dec_ctx, video_stream->codecpar)) < 0){
        return ret;
    }
    av_opt_set_int(dec_ctx, "refcounted_frames", 1, 0);
//
    if((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0){
        av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
        return ret;
    }

    return 0;
}

void initialize_player() {
    initialize_ffmpeg();
    initialize_sdl2();
}
static void initialize_ffmpeg() {
    av_register_all();
}


static void initialize_sdl2() {
    assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);
}

static void cleanup_sdl2(myWindow *window) {
    SDL_DestroyTexture(window->texture);
    SDL_DestroyRenderer(window->renderer);
    SDL_DestroyWindow(window->window);

}

static void cleanup_ffmpeg(AVCodecContext *dec_ctx) {
    avcodec_free_context(&dec_ctx);


}

static int setup_window(myWindow *window, int width, int height) {
    window->renderer    = NULL;
    window->texture     = NULL;
    window->window      = NULL;

    SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_SHOWN, &window->window, &window->renderer);
    window->texture = SDL_CreateTexture(window->renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STATIC, width, height);
    if(!window->renderer || !window->window || !window->texture ) {
        return -1;
    }


    window->width = width;
    window->height = height;
    return 0;
}

