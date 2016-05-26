#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <vdpau/vdpau_x11.h>

Display *dis;
int screen;
Window win;
GC gc;

static VdpOutputSurface                   rgba_surface;

static VdpVideoMixer                      video_mixer;
static float                              min_luma;
static float                              max_luma;
static int                                lumakey;
static uint32_t                           vid_width, vid_height;
static VdpChromaType                      vdp_chroma_type;
static VdpDevice                          vdp_device;
static VdpGetProcAddress                 *vdp_get_proc_address;
static int                                colorspace;

static VdpPresentationQueueTarget         vdp_target;
static VdpPresentationQueue               vdp_queue;

static VdpDeviceDestroy                  *vdp_device_destroy;
static VdpVideoSurfaceCreate             *vdp_video_surface_create;
static VdpVideoSurfaceDestroy            *vdp_video_surface_destroy;

static VdpGetErrorString                 *vdp_get_error_string;

/* May be used in software filtering/postprocessing options
 * in MPlayer (./mplayer -vf ..) if we copy video_surface data to
 * system memory.
 */
static VdpVideoSurfacePutBitsYCbCr       *vdp_video_surface_put_bits_y_cb_cr;
static VdpOutputSurfacePutBitsNative     *vdp_output_surface_put_bits_native;

static VdpOutputSurfaceCreate            *vdp_output_surface_create;
static VdpOutputSurfaceDestroy           *vdp_output_surface_destroy;

/* VideoMixer puts video_surface data on displayable output_surface. */
static VdpVideoMixerCreate               *vdp_video_mixer_create;
static VdpVideoMixerDestroy              *vdp_video_mixer_destroy;
static VdpVideoMixerRender               *vdp_video_mixer_render;
static VdpVideoMixerSetFeatureEnables    *vdp_video_mixer_set_feature_enables;
static VdpVideoMixerSetAttributeValues   *vdp_video_mixer_set_attribute_values;

static VdpPresentationQueueTargetDestroy *vdp_presentation_queue_target_destroy;
static VdpPresentationQueueCreate        *vdp_presentation_queue_create;
static VdpPresentationQueueDestroy       *vdp_presentation_queue_destroy;
static VdpPresentationQueueDisplay       *vdp_presentation_queue_display;
static VdpPresentationQueueBlockUntilSurfaceIdle *vdp_presentation_queue_block_until_surface_idle;
static VdpPresentationQueueTargetCreateX11       *vdp_presentation_queue_target_create_x11;
static VdpPresentationQueueSetBackgroundColor    *vdp_presentation_queue_set_background_color;

static VdpOutputSurfaceRenderOutputSurface       *vdp_output_surface_render_output_surface;
static VdpOutputSurfacePutBitsIndexed            *vdp_output_surface_put_bits_indexed;
static VdpOutputSurfaceRenderBitmapSurface       *vdp_output_surface_render_bitmap_surface;

static VdpBitmapSurfaceCreate                    *vdp_bitmap_surface_create;
static VdpBitmapSurfaceDestroy                   *vdp_bitmap_surface_destroy;
static VdpBitmapSurfacePutBitsNative             *vdp_bitmap_surface_putbits_native;

static VdpDecoderCreate                          *vdp_decoder_create;
static VdpDecoderDestroy                         *vdp_decoder_destroy;
static VdpDecoderRender                          *vdp_decoder_render;

static VdpGenerateCSCMatrix                      *vdp_generate_csc_matrix;
static VdpPreemptionCallbackRegister             *vdp_preemption_callback_register;




void init_x() {
/* get the colors black and white (see section for details) */        
	unsigned long black,white;

	dis=XOpenDisplay((char *)0);
   	screen=DefaultScreen(dis);
	black=BlackPixel(dis,screen),
	white=WhitePixel(dis, screen);
   	win=XCreateSimpleWindow(dis,DefaultRootWindow(dis),0,0,	
		300, 300, 5,black, white);
	XSetStandardProperties(dis,win,"Howdy","Hi",None,NULL,0,NULL);
	XSelectInput(dis, win, ExposureMask|ButtonPressMask|KeyPressMask);
        /*gc=XCreateGC(dis, win, 0,0);        */
	/*XSetBackground(dis,gc,white);*/
	/*XSetForeground(dis,gc,black);*/
	XClearWindow(dis, win);
	XMapRaised(dis, win);
};

void close_x() {
	XFreeGC(dis, gc);
	XDestroyWindow(dis,win);
	XCloseDisplay(dis);	
	exit(1);				
};

static int update_csc_matrix(void)
{
    VdpStatus vdp_st;
    VdpCSCMatrix matrix;
    static const VdpVideoMixerAttribute attributes[] = {VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX};
    const void *attribute_values[] = {&matrix};
    static const VdpColorStandard vdp_colors[] = {0, VDP_COLOR_STANDARD_ITUR_BT_601, VDP_COLOR_STANDARD_ITUR_BT_709, VDP_COLOR_STANDARD_SMPTE_240M};
    static const char * const vdp_names[] = {NULL, "BT.601", "BT.709", "SMPTE-240M"};
    int csp = colorspace;

    if (!csp)
        csp = vid_width >= 1280 || vid_height > 576 ? 2 : 1;

    /*mp_msg(MSGT_VO, MSGL_V, "[vdpau] Updating CSC matrix for %s\n",*/
           /*vdp_names[csp]);*/

    /*vdp_st = vdp_generate_csc_matrix(&procamp, vdp_colors[csp], &matrix);*/
    /*CHECK_ST_WARNING("Error when generating CSC matrix")*/

    /*vdp_st = vdp_video_mixer_set_attribute_values(video_mixer, 1, attributes,*/
                                                  /*attribute_values);*/
    /*CHECK_ST_WARNING("Error when setting CSC matrix")*/
    /*return VO_TRUE;*/
}

int init_vdpau_queue(void)
{
    VdpStatus vdp_st;
    // {0, 0, 0, 0} makes the video shine through any black window on top
    VdpColor vdp_bg = {0.1, 0.2, 0.3, 1};

    vdp_st = vdp_presentation_queue_target_create_x11(vdp_device, win,
                                                      &vdp_target);

    vdp_st = vdp_presentation_queue_create(vdp_device, vdp_target,
                                           &vdp_queue);

    vdp_st = vdp_presentation_queue_set_background_color(vdp_queue, &vdp_bg);
    return 0;
}


static int create_vdp_mixer(VdpChromaType vdp_chroma_type)
{
#define VDP_NUM_MIXER_PARAMETER 3
#define MAX_NUM_FEATURES 6
    int i;
    VdpStatus vdp_st;
    int feature_count = 0;
   
    VdpVideoMixerFeature features[MAX_NUM_FEATURES];
    VdpBool feature_enables[MAX_NUM_FEATURES];
    static const VdpVideoMixerAttribute lumakey_attrib[] = {VDP_VIDEO_MIXER_ATTRIBUTE_LUMA_KEY_MIN_LUMA, VDP_VIDEO_MIXER_ATTRIBUTE_LUMA_KEY_MAX_LUMA};
    const void * const lumakey_value[] = {&min_luma, &max_luma};
    static const VdpVideoMixerParameter parameters[VDP_NUM_MIXER_PARAMETER] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE
    };
    const void *const parameter_values[VDP_NUM_MIXER_PARAMETER] = {
        &vid_width,
        &vid_height,
        &vdp_chroma_type
    };
    if (lumakey)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_LUMA_KEY;

    vdp_st = vdp_video_mixer_create(vdp_device, feature_count, features,
                                    VDP_NUM_MIXER_PARAMETER,
                                    parameters, parameter_values,
                                    &video_mixer);

    for (i = 0; i < feature_count; i++)
        feature_enables[i] = VDP_TRUE;
    if (feature_count)
        vdp_video_mixer_set_feature_enables(video_mixer, feature_count, features, feature_enables);
    if (lumakey)
        vdp_video_mixer_set_attribute_values(video_mixer, 2, lumakey_attrib, lumakey_value);
    if(vdp_st == VDP_STATUS_OK) printf("mixer created");
    /*update_csc_matrix();*/
    return 0;
}

int init_vdpau(){
    
    VdpStatus vdp_st;

    struct vdp_function {
        const int id;
        void *pointer;
    };

    const struct vdp_function *dsc;

    static const struct vdp_function vdp_func[] = {
        {VDP_FUNC_ID_GET_ERROR_STRING,                  &vdp_get_error_string},
        {VDP_FUNC_ID_DEVICE_DESTROY,                    &vdp_device_destroy},
        {VDP_FUNC_ID_VIDEO_SURFACE_CREATE,              &vdp_video_surface_create},
        {VDP_FUNC_ID_VIDEO_SURFACE_DESTROY,             &vdp_video_surface_destroy},
        {VDP_FUNC_ID_VIDEO_SURFACE_PUT_BITS_Y_CB_CR,    &vdp_video_surface_put_bits_y_cb_cr},
        {VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_NATIVE,    &vdp_output_surface_put_bits_native},
        {VDP_FUNC_ID_OUTPUT_SURFACE_CREATE,             &vdp_output_surface_create},
        {VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY,            &vdp_output_surface_destroy},
        {VDP_FUNC_ID_VIDEO_MIXER_CREATE,                &vdp_video_mixer_create},
        {VDP_FUNC_ID_VIDEO_MIXER_DESTROY,               &vdp_video_mixer_destroy},
        {VDP_FUNC_ID_VIDEO_MIXER_RENDER,                &vdp_video_mixer_render},
        {VDP_FUNC_ID_VIDEO_MIXER_SET_FEATURE_ENABLES,   &vdp_video_mixer_set_feature_enables},
        {VDP_FUNC_ID_VIDEO_MIXER_SET_ATTRIBUTE_VALUES,  &vdp_video_mixer_set_attribute_values},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_DESTROY, &vdp_presentation_queue_target_destroy},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE,         &vdp_presentation_queue_create},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY,        &vdp_presentation_queue_destroy},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY,        &vdp_presentation_queue_display},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE,
                        &vdp_presentation_queue_block_until_surface_idle},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11,
                        &vdp_presentation_queue_target_create_x11},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_SET_BACKGROUND_COLOR,
                        &vdp_presentation_queue_set_background_color},
        {VDP_FUNC_ID_OUTPUT_SURFACE_RENDER_OUTPUT_SURFACE,
                        &vdp_output_surface_render_output_surface},
        {VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_INDEXED,   &vdp_output_surface_put_bits_indexed},
        {VDP_FUNC_ID_DECODER_CREATE,                    &vdp_decoder_create},
        {VDP_FUNC_ID_DECODER_RENDER,                    &vdp_decoder_render},
        {VDP_FUNC_ID_DECODER_DESTROY,                   &vdp_decoder_destroy},
        {VDP_FUNC_ID_BITMAP_SURFACE_CREATE,             &vdp_bitmap_surface_create},
        {VDP_FUNC_ID_BITMAP_SURFACE_DESTROY,            &vdp_bitmap_surface_destroy},
        {VDP_FUNC_ID_BITMAP_SURFACE_PUT_BITS_NATIVE,    &vdp_bitmap_surface_putbits_native},
        {VDP_FUNC_ID_OUTPUT_SURFACE_RENDER_BITMAP_SURFACE,
                        &vdp_output_surface_render_bitmap_surface},
        {VDP_FUNC_ID_GENERATE_CSC_MATRIX,               &vdp_generate_csc_matrix},
        {VDP_FUNC_ID_PREEMPTION_CALLBACK_REGISTER,      &vdp_preemption_callback_register},
        {0, NULL}
    };
    vdp_st = vdp_device_create_x11(dis, screen,
                               &vdp_device, &vdp_get_proc_address);
    
    if (vdp_st != VDP_STATUS_OK) {
        return -1;
    }

    vdp_get_error_string = NULL;
    for (dsc = vdp_func; dsc->pointer; dsc++) {
        vdp_st = vdp_get_proc_address(vdp_device, dsc->id, dsc->pointer);
        if (vdp_st != VDP_STATUS_OK) {
            return -1;
        }
    }

    return 0;


}

int create_surface(){
     
    VdpStatus vdp_st;

    vdp_st = vdp_output_surface_create(vdp_device, VDP_RGBA_FORMAT_B8G8R8A8,
                                           vid_width, vid_height,
                                           &rgba_surface);
    if (vdp_st == VDP_STATUS_OK) return 1;
    else return 0;
}

int main(){
    VdpStatus vdp_st; 
    VdpGetProcAddress *vdp_get_proc_address;
    init_x();
    vdp_chroma_type = VDP_CHROMA_TYPE_420;
    vid_width = 300;
    vid_height = 300;
    init_vdpau();
    create_vdp_mixer(vdp_chroma_type);
    init_vdpau_queue();    
    while(1) {
        XEvent event;        
        XNextEvent(dis, &event);

    }
    return 0;
}
