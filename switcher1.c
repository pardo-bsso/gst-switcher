#include <stdio.h>
#include <string.h>

#include <glib/gprintf.h>

#include <gst/gst.h>
#include <gst/controller/gstcontroller.h>


typedef struct _Source {
    GstElement  *source;
    GstElement  *tee;
    GstPad      *vpad;
    GstPad      *apad;
} Source;

typedef struct _TetraApp {
    GstElement  *pipeline;
    GMainLoop   *loop;

    GstElement  *vsink;
    GstElement  *asink;

    GstElement  *vmixer;
    GstElement  *amixer;

    GstElement  *muxes[2];
    GstElement  *alpha_flt[2];
    GstElement  *vconvert[2];
    Source      sources[3];
/* FIXME: separar mejor ... */
    int         current_mux;
    int         current_src;
    GstInterpolationControlSource    *alpha[2];
} TetraApp;


static void _init_pad_cb (GstElement *src, GstPad *new_pad, Source *data);
guint init_source (Source *src, const char *uri);
static gboolean handle_keyboard (GIOChannel *source, GIOCondition cond, TetraApp *data);
void mux_set_input(GstElement *mux, int input);
void mux_group_set_input(GstElement *mux, int input, GstElement *muxes[]);
void switch_to (int to, TetraApp *app);


static void _audio_link_pad_cb (GstElement *src, GstPad *new_pad, TetraApp *data) {
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    new_pad_caps = gst_pad_get_caps (new_pad);
    new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
    new_pad_type = gst_structure_get_name (new_pad_struct);

    if (g_str_has_prefix (new_pad_type, "audio/x-raw")) {
        GstElement *sink = gst_element_factory_make ("fakesink", NULL);
        gst_bin_add(GST_BIN(data->pipeline), sink);
        gst_element_link(src, sink);
    }

    if (new_pad_caps != NULL) {
        gst_caps_unref (new_pad_caps);
    }

}

static void _init_pad_cb (GstElement *src, GstPad *new_pad, Source *data) {
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    new_pad_caps = gst_pad_get_caps (new_pad);
    new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
    new_pad_type = gst_structure_get_name (new_pad_struct);

    if (g_str_has_prefix (new_pad_type, "audio/x-raw")) {
/* FIXME: linkear a un fakesink. */
        data->apad = new_pad;
    } else if (g_str_has_prefix (new_pad_type, "video/x-raw-rgb")) {
        data->vpad = new_pad;
        gst_element_link(src, data->tee);
    }

    if (new_pad_caps != NULL) {
        gst_caps_unref (new_pad_caps);
    }
}


guint init_source (Source *src, const char *uri) {
    if ( !src || !uri ) {
        return 1;
    }

    src->source = gst_element_factory_make ("uridecodebin", NULL);
    if (!(src->source) ) {
        return 2;
    }

    src->tee = gst_element_factory_make ("tee", NULL);
    if (!(src->tee) ) {
        return 4;
    }

    g_signal_connect (src->source, "pad-added", G_CALLBACK (_init_pad_cb), src);
    g_object_set(src->source, "uri", uri, NULL);

    return 0;
}

static gboolean handle_keyboard (GIOChannel *source, GIOCondition cond, TetraApp *data) {
    gchar k = '\0';

    if (g_io_channel_read_chars (source, &k, 1, NULL, NULL) != G_IO_STATUS_NORMAL) {
        return TRUE;
    }

    switch (g_ascii_tolower (k)) {
/* XXX */
        case '1':
            switch_to(0, data);
            break;
        case '2':
            switch_to(1, data);
            break;
        case '3':
            switch_to(2, data);
            break;
        case 'q':
            g_main_loop_quit (data->loop);
            break;
        default:
            break;
    }

    return TRUE;
}

void mux_set_input(GstElement *mux, int input) {
    if (!mux) {
        return;
    }

    gchar *name = g_strdup_printf("sink%d", input);
    GstPad *pad = gst_element_get_static_pad(mux, name);
    GstPad *oldpad = gst_element_get_static_pad(mux, name);


    if (!pad) {
        g_printerr("mux_set_input: no pad %d\n", input);
        goto exit;
    }

    g_object_get (mux, "active-pad", &oldpad, NULL);
    if (pad != oldpad) {
        g_object_set(mux, "active-pad", pad, NULL);
    }

exit:
    g_free(name);
    gst_object_unref(pad);
}

void mux_group_set_input(GstElement *mux, int input, GstElement *muxes[]) {
/* XXX copypasta */
    if (!mux || !muxes) {
        return;
    }

    gchar *name = g_strdup_printf("sink%d", input);
    GstPad *pad = gst_element_get_static_pad(mux, name);
    GstPad *oldpad = gst_element_get_static_pad(mux, name);


    if (!pad) {
        g_printerr("mux_group_set_input: no pad %d\n", input);
        goto exit;
    }

    g_object_get (mux, "active-pad", &oldpad, NULL);
    if (pad != oldpad) {
        gint64 stop_time = 0;
/* XXX: limite hardcodeado... */
        for (int idx=0; idx < 2; idx++) {
            gint64 tmp = 0;
            g_signal_emit_by_name(muxes[idx], "block", &tmp);
            if (tmp > stop_time)
                stop_time = tmp;
        }
        //g_object_set(mux, "active-pad", pad, NULL);
        g_signal_emit_by_name(mux, "switch", pad, stop_time, -1);
    }

/* XXX: limite hardcodeado... */
    for (int idx=0; idx < 2; idx++) {
        if (mux == muxes[idx])
            continue;
        gst_element_set_state(muxes[idx], GST_STATE_PLAYING);
    }

exit:
    g_free(name);
    gst_object_unref(pad);
}

void switch_to (int to, TetraApp *app) {
    if (!app) {
        return;
    }

    if (to == app->current_src) {
        return;
    }

    GValue start_alpha = {0,};
    GValue end_alpha = {0,};

    GstClock *clock;
    GstClockTime now, end;

    int cidx, oidx;
    GstElement *curr_mux, *other_mux;

    clock = gst_pipeline_get_clock (GST_PIPELINE (app->pipeline));
    now = gst_clock_get_time(clock);
/* XXX: limite hardcodeado... */
    end = now + 500*GST_MSECOND;

/* XXX: limite hardcodeado... */
    cidx = app->current_mux;
    oidx = 1 - cidx;

g_printerr("switch_to cidx: %d oidx:%d \n", cidx, oidx);
    curr_mux = app->muxes[cidx];
    other_mux = app->muxes[oidx];

/* XXX FIXME: en secuencia (123 - 321) ok, rompe salto. */
    mux_set_input(other_mux, to);

/* ok segun docs, no anda 3 */
    //mux_group_set_input(other_mux, to, app->muxes);

/* XXX: limite hardcodeado... */
/* XXX: por que pasa esto: (switcher1:PID): GLib-GObject-WARNING **: value "-nan" of type `gdouble' is invalid or out of range for property `alpha' of type `gdouble'
 * */
/* probar con gst-head ... */
/*

    g_value_init (&start_alpha, G_TYPE_DOUBLE);
    g_value_set_double(&start_alpha, 1.0);

    g_value_init (&end_alpha, G_TYPE_DOUBLE);
    g_value_set_double(&end_alpha, 0.0);

    gst_interpolation_control_source_set(app->alpha[cidx], now, &start_alpha);
    gst_interpolation_control_source_set(app->alpha[cidx], end, &end_alpha);

    gst_interpolation_control_source_set(app->alpha[oidx], end, &start_alpha);
    gst_interpolation_control_source_set(app->alpha[oidx], now, &end_alpha);
*/

/* XXX: no, tiene que andar los tweens */
    g_object_set(app->alpha_flt[cidx], "alpha", 0.0, NULL);
    g_object_set(app->alpha_flt[oidx], "alpha", 1.0, NULL);

    app->current_src = to;
    app->current_mux = oidx;

}


int main(int argc, char *argv[]) {
    TetraApp data;
    GstStateChangeReturn ret;
    GIOChannel *io_stdin;

    gst_init (&argc, &argv);
    gst_controller_init (&argc, &argv);

    memset (&data, 0, sizeof (data));

#if 1
    const char *uris[] = {
        "file:///media/datos/videos/Cannonball_The_Breeders-Qpoqzt2EHaA.mp4",
        "file:///media/datos/videos/Veruca_Salt_-_Born_Entertainer-OU7IWZ-ClW8.mp4",
        "file:///media/datos/videos/INFORMATION_SOCIETY_-_WHAT'S_ON_YOUR_MIND-UPuXvpkOLmM.mp4",
    };
#else
    /* XXX: init flexible aca ... */
#endif

    data.pipeline = gst_pipeline_new ("tetra-pipeline");
    if (!data.pipeline) {
        g_printerr("Error no pipeline\n");
        return 1;
    }

/* XXX: limite hardcodeado... */
    for (int idx=0; idx < 2; idx++) {
        GstElement *el = gst_element_factory_make("input-selector", NULL);
        if (!el) {
            g_printerr("Error no input-selector\n");
            return 2;
        }
        //g_object_set(el, "sync-streams", TRUE, NULL);
        data.muxes[idx] = el;
        gst_bin_add(GST_BIN(data.pipeline), el);
    }

    data.vsink = gst_element_factory_make ("autovideosink", "vsink0");
    data.asink = gst_element_factory_make ("autoaudiosink", "asink0");
    data.vmixer = gst_element_factory_make ("videomixer", "vmixer0");
    data.alpha_flt[0] = gst_element_factory_make ("alpha", "alpha0");
    data.alpha_flt[1] = gst_element_factory_make ("alpha", "alpha1");
    data.vconvert[0] = gst_element_factory_make ("ffmpegcolorspace", "videoconvert0");
    data.vconvert[1] = gst_element_factory_make ("autovideoconvert", "videoconvert1");
/* XXX: no olvidar que clampea los canales... */
    data.amixer = gst_element_factory_make ("liveadder", "amixer0");

/* XXX: error checking ... */
    gst_bin_add(GST_BIN(data.pipeline), data.vsink);
    gst_bin_add(GST_BIN(data.pipeline), data.vmixer);
    gst_bin_add(GST_BIN(data.pipeline), data.alpha_flt[0]);
    gst_bin_add(GST_BIN(data.pipeline), data.alpha_flt[1]);
    gst_bin_add(GST_BIN(data.pipeline), data.vconvert[0]);
    gst_bin_add(GST_BIN(data.pipeline), data.vconvert[1]);
    //gst_bin_add(GST_BIN(data.pipeline), data.asink);
    //gst_bin_add(GST_BIN(data.pipeline), data.amixer);

/* XXX: limite hardcodeado... */
    for (int idx=0; idx < 3; idx++) {
        if (init_source(&data.sources[idx], uris[idx])) {
            g_printerr("Error init_source %d uri: %s\n", idx, uris[idx]);
            return 4;
        }
/* XXX: factor out ... */
        gst_bin_add(GST_BIN(data.pipeline), data.sources[idx].source);
        gst_bin_add(GST_BIN(data.pipeline), data.sources[idx].tee);
        for (int im=0; im < 2 ; im++) {
            gst_element_link(data.sources[idx].tee, data.muxes[im]);
        }
        //g_signal_connect (data.sources[idx].source, "pad-added", G_CALLBACK (_audio_link_pad_cb), &data);
    }

/* XXX: error checking ... */
    gst_element_link_many(data.muxes[1], data.alpha_flt[1], data.vmixer, NULL);
    gst_element_link_many(data.muxes[0], data.alpha_flt[0], data.vmixer, data.vconvert[0], data.vsink, NULL);
    //gst_element_link_many(data.amixer, data.asink, NULL);

//    gst_child_proxy_set(GST_OBJECT(data.vmixer), "sink_0::alpha", .5, NULL);
//    gst_child_proxy_set(GST_OBJECT(data.vmixer), "sink_1::alpha", .5, NULL);
//   g_object_set(data.alpha_flt[0], "alpha", 0.5, NULL);
//    g_object_set(data.alpha_flt[1], "alpha", 0.5, NULL);

    mux_set_input(data.muxes[0], 0);
    mux_set_input(data.muxes[1], 1);



/* XXX: error checking + factor ... */
    data.alpha[0] =  gst_interpolation_control_source_new ();
    data.alpha[1] =  gst_interpolation_control_source_new ();
    gst_interpolation_control_source_set_interpolation_mode( data.alpha[0], GST_INTERPOLATE_CUBIC);
    gst_interpolation_control_source_set_interpolation_mode( data.alpha[1], GST_INTERPOLATE_CUBIC);


    GstController   *ctrl;

    ctrl = gst_object_control_properties(G_OBJECT(data.alpha_flt[0]), "alpha",NULL);
    gst_controller_set_control_source (ctrl, "alpha", GST_CONTROL_SOURCE (data.alpha[0]));

    ctrl = gst_object_control_properties(G_OBJECT(data.alpha_flt[1]), "alpha",NULL);
    gst_controller_set_control_source (ctrl, "alpha", GST_CONTROL_SOURCE (data.alpha[1]));

    io_stdin = g_io_channel_unix_new (fileno (stdin));
    g_io_add_watch (io_stdin, G_IO_IN | G_IO_PRI, (GIOFunc)handle_keyboard, &data);



    ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (data.pipeline);
        return -1;
    }
g_printerr("play...\n");
//data.current_src = 0x4444;
//switch_to(0, &data);


GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS | GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "pipeline_switcher");
    data.loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (data.loop);

    g_main_loop_unref (data.loop);
    g_io_channel_unref (io_stdin);
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
    return 0;
}
