#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <vdpau/vdpau_x11.h>
/*------------------X-variables----------------*/
static Display                           *dis;
static int                               screen;
static Window                            win;
static GC                                gc;
/*-----------------VDPAU-variables-----------------*/
static VdpOutputSurface                   output_surface;
static VdpVideoSurface                    video_surface;
static VdpPresentationQueueStatus         status;
static VdpProcamp                         procamp;
static VdpVideoMixer                      video_mixer;
static uint32_t                           vid_width, vid_height;
static VdpChromaType                      vdp_chroma_type;
static VdpDevice                          vdp_device;
static VdpYCbCrFormat                     vdp_pixel_format;
static int                                colorspace; 
static VdpPresentationQueueTarget         vdp_target;
static VdpPresentationQueue               vdp_queue;


/*-----------------VDPAU-functions-------------------*/
static VdpGetProcAddress                 *vdp_get_proc_address;

static VdpDeviceDestroy                  *vdp_device_destroy;

static VdpVideoSurfaceCreate             *vdp_video_surface_create;
static VdpVideoSurfaceDestroy            *vdp_video_surface_destroy;
static VdpVideoSurfacePutBitsYCbCr       *vdp_video_surface_put_bits_y_cb_cr;

static VdpOutputSurfaceGetBitsNative     *vdp_output_surface_get_bits_native;
static VdpOutputSurfaceCreate            *vdp_output_surface_create;
static VdpOutputSurfaceDestroy           *vdp_output_surface_destroy;

static VdpVideoMixerCreate               *vdp_video_mixer_create;
static VdpVideoMixerDestroy              *vdp_video_mixer_destroy;
static VdpVideoMixerRender               *vdp_video_mixer_render;
static VdpVideoMixerSetFeatureEnables    *vdp_video_mixer_set_feature_enables;
static VdpVideoMixerSetAttributeValues   *vdp_video_mixer_set_attribute_values;

static VdpPresentationQueueTargetDestroy *vdp_presentation_queue_target_destroy;
static VdpPresentationQueueCreate        *vdp_presentation_queue_create;
static VdpPresentationQueueDestroy       *vdp_presentation_queue_destroy;
static VdpPresentationQueueDisplay       *vdp_presentation_queue_display;
static VdpPresentationQueueTargetCreateX11       *vdp_presentation_queue_target_create_x11;

static VdpDecoderCreate                          *vdp_decoder_create;
static VdpDecoderDestroy                         *vdp_decoder_destroy;
static VdpDecoderRender                          *vdp_decoder_render;

static VdpGenerateCSCMatrix                      *vdp_generate_csc_matrix;

/*
 * Intialize the X-Window
 */
void init_x() {
	unsigned long black,white;

	dis=XOpenDisplay((char *)0);
   	screen=DefaultScreen(dis);
	black=BlackPixel(dis,screen),
	white=WhitePixel(dis, screen);
        win=XCreateSimpleWindow(dis,DefaultRootWindow(dis),0,0,
                300, 300, 5, black, white);
        XSetStandardProperties(dis,win,"VDPAU Player","VDPAU",None,NULL,0,NULL);
        XSelectInput(dis, win, ExposureMask|ButtonPressMask|KeyPressMask);
        gc=XCreateGC(dis, win, 0,NULL);
        XSetBackground(dis,gc,white);
        XSetBackground(dis,gc,black);
        XClearWindow(dis, win);
};

void close_x() {
	XFreeGC(dis, gc);
	XDestroyWindow(dis,win);
	XCloseDisplay(dis);
	exit(0);
};

/*
 * intialize VdpDevice and get pointer to required functions
 */
int init_vdpau() {

    VdpStatus vdp_st;

    struct vdp_function {
        const int id;
        void *pointer;
    };

    const struct vdp_function *dsc;

    static const struct vdp_function vdp_func[] = {
        {VDP_FUNC_ID_DEVICE_DESTROY,                    &vdp_device_destroy},
        {VDP_FUNC_ID_VIDEO_SURFACE_CREATE,              &vdp_video_surface_create},
        {VDP_FUNC_ID_VIDEO_SURFACE_DESTROY,             &vdp_video_surface_destroy},
        {VDP_FUNC_ID_VIDEO_SURFACE_PUT_BITS_Y_CB_CR,    &vdp_video_surface_put_bits_y_cb_cr},
        {VDP_FUNC_ID_OUTPUT_SURFACE_GET_BITS_NATIVE,    &vdp_output_surface_get_bits_native},
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
        {VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11,
                        &vdp_presentation_queue_target_create_x11},
        {VDP_FUNC_ID_DECODER_CREATE,                    &vdp_decoder_create},
        {VDP_FUNC_ID_DECODER_RENDER,                    &vdp_decoder_render},
        {VDP_FUNC_ID_DECODER_DESTROY,                   &vdp_decoder_destroy},
        {VDP_FUNC_ID_GENERATE_CSC_MATRIX,               &vdp_generate_csc_matrix},
        {0, NULL}
    };

    vdp_st = vdp_device_create_x11(dis, screen,
                               &vdp_device, &vdp_get_proc_address);

    if (vdp_st != VDP_STATUS_OK) {
        return -1;
    }

    for (dsc = vdp_func; dsc->pointer; dsc++) {
        vdp_st = vdp_get_proc_address(vdp_device, dsc->id, dsc->pointer);
        if (vdp_st != VDP_STATUS_OK) {
            return -1;
        }
    }

    return 0;
}

/*
 * intialize VdpPresentationQueue
 */
int init_vdpau_queue()
{
    VdpStatus vdp_st;

    vdp_st = vdp_presentation_queue_target_create_x11(vdp_device, win,
                                                      &vdp_target);
    if(vdp_st != VDP_STATUS_OK) return -1;

    XMapRaised(dis, win);

    vdp_st = vdp_presentation_queue_create(vdp_device, vdp_target,
                                           &vdp_queue);
    if(vdp_st != VDP_STATUS_OK) return -1;

    return 0;
}

/*
 * Set the Color Space Conversion Matrix
 */
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

    vdp_st = vdp_generate_csc_matrix(&procamp, vdp_colors[csp], &matrix);

    if(vdp_st != VDP_STATUS_OK) return -1;

    vdp_st = vdp_video_mixer_set_attribute_values(video_mixer, 1, attributes,
                                                  attribute_values);
    if(vdp_st != VDP_STATUS_OK) return -1;
 
    return 0;
}

/*
 * initalize VdpVideoMixer
 */
static int create_vdp_mixer(VdpChromaType vdp_chroma_type)
{
#define VDP_NUM_MIXER_PARAMETER 3
#define MAX_NUM_FEATURES 6
    int i;
    VdpStatus vdp_st;
    int feature_count = 0;

    VdpVideoMixerFeature features[MAX_NUM_FEATURES];        
    VdpBool feature_enables[MAX_NUM_FEATURES];

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

    vdp_st = vdp_video_mixer_create(vdp_device, feature_count, features,
                                    VDP_NUM_MIXER_PARAMETER,
                                    parameters, parameter_values,
                                    &video_mixer);

    if(vdp_st != VDP_STATUS_OK) return -1;

    return update_csc_matrix();
}

/*
 * Create a Video Surface
 */
int create_video_surface(){
 
    VdpStatus vdp_st;

    vdp_st = vdp_video_surface_create(vdp_device, vdp_chroma_type,
                                           vid_width, vid_height,
                                           &video_surface);

    if (vdp_st != VDP_STATUS_OK) return -1;

    return 0;
}

/*
 * create a VdpOutputSurface
 */
int create_output_surface(){
 
    VdpStatus vdp_st;

    vdp_st = vdp_output_surface_create(vdp_device, VDP_RGBA_FORMAT_B8G8R8A8,
                                           vid_width, vid_height,
                                           &output_surface);

    if (vdp_st != VDP_STATUS_OK) return -1;

    return 0;
}

/*
 * put frame data on VdpVideoSurface
 */
int put_bits() {
    uint32_t **data;
    int i,j;
    const uint32_t a[1] = {vid_width*4};
    VdpStatus vdp_st;

    data = (uint32_t * * )calloc(1, sizeof(uint32_t *));

    for(i = 0; i < 1; i++)
        data[i] = (uint32_t *)calloc(vid_width*vid_height, sizeof(uint32_t *));
 
    for(i = 0; i < 1; i++) {
        for(j = 0; j < vid_width*vid_height; j++)
            scanf("%ld", &data[i][j]);
    }

    vdp_st = vdp_video_surface_put_bits_y_cb_cr(video_surface, vdp_pixel_format,
                                                (void const*const*)data,
                                                 a);
 
    for (i = 0; i < 1; i++) {
        free(data[i]);
    }

    if (vdp_st != VDP_STATUS_OK) return -1;

    return 0;
}

/*
 * retrieve data from VdpOutputSurface
 */
int get_bits() {
    uint32_t **data;
    int i,j;
    const uint32_t a[1] = {vid_width*4};
    VdpStatus vdp_st;
    
    data = (uint32_t * * )calloc(1, sizeof(uint32_t *));
    
    for(i = 0; i < 1; i++)
        data[i] = (uint32_t *)calloc(vid_width*vid_height, sizeof(uint32_t *));
    
    vdp_st = vdp_output_surface_get_bits_native(output_surface,NULL,
                                                (void * const*)data,
                                                 a);
 
    if (vdp_st != VDP_STATUS_OK) return -1;

    return 0; 
}

int main(){
    KeySym key;
    XEvent event;
    
    vdp_chroma_type = VDP_CHROMA_TYPE_420;
    vid_width = 300;
    vid_height = 300;
    colorspace =1;  
    VdpStatus vdp_st;
    int field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
    vdp_pixel_format = VDP_YCBCR_FORMAT_Y8U8V8A8;
    procamp.struct_version = VDP_PROCAMP_VERSION;
    procamp.brightness = 0.0;
    procamp.contrast   = 1.0;
    procamp.saturation = 1.0;
    procamp.hue        = 0.0;
    uint32_t rgba_format = 5;
    int status;

    init_x();

    status = init_vdpau();
    if(status == -1)
        printf("Error in initializing VdpDevice\n");

    status = create_video_surface();
    if(status == -1)
        printf("Error in creating VdpVideoSurface\n");

    status = create_output_surface();
    if(status == -1)
        printf("Error in creating VdpOutputSurface\n");

    status = create_vdp_mixer(vdp_chroma_type);
    if(status == -1)
        printf("Error in creating VdpVideoMixer\n");

    status = init_vdpau_queue();    
    if(status == -1)
        printf("Error in initializing VdpPresentationQueue\n");

    status = put_bits();
    if(status == -1)
        printf("Error in Putting data on VdpVideoSurface\n");

   
    vdp_st = vdp_video_mixer_render(video_mixer, VDP_INVALID_HANDLE, 0,
                                        field, 0, (VdpVideoSurface*)VDP_INVALID_HANDLE,
                                        video_surface,
                                        0, (VdpVideoSurface*)VDP_INVALID_HANDLE,
                                        NULL,
                                        output_surface,
                                        NULL, NULL, 0, NULL);
    

    get_bits();
    int i = 500;
    while(i) 
    {
           vdp_st = vdp_presentation_queue_display(vdp_queue,
                                                  output_surface, 
                                                  0, 0,
                                                  0);
           i--;
    }

    while(1){
       XNextEvent(dis, &event);

       if(event.type == KeyPress)
           close_x();
    }
    
}
