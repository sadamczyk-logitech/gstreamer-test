#include <gst/gst.h>
#include <notify.h>

#define SWITCH_TIMEOUT 1
#define NUM_VIDEO_BUFFERS 10000000

static GMainLoop *loop;

/* Output selector src pads */
static GstPad *osel_src1 = NULL;
static GstPad *osel_src2 = NULL;

static gboolean
my_bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
    g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

    switch (GST_MESSAGE_TYPE (message)) {
        case GST_MESSAGE_ERROR:{
            GError *err;
            gchar *debug;

            gst_message_parse_error (message, &err, &debug);
            g_print ("Error: %s\n", err->message);
            g_error_free (err);
            g_free (debug);

            g_main_loop_quit (loop);
            break;
        }
        case GST_MESSAGE_EOS:
            /* end-of-stream */
            g_main_loop_quit (loop);
            break;
        default:
            /* unhandled message */
            break;
    }
    /* we want to be notified again the next time there is a message
     * on the bus, so returning TRUE (FALSE means we want to stop watching
     * for messages on the bus and our callback should not be called again)
     */
    return TRUE;
}

static gboolean
switch_cb (gpointer user_data)
{
    GstElement *sel = GST_ELEMENT (user_data);
    GstPad *old_pad, *new_pad = NULL;

    g_object_get (G_OBJECT (sel), "active-pad", &old_pad, NULL);

    if (old_pad == osel_src1)
        new_pad = osel_src2;
    else
        new_pad = osel_src1;

    g_object_set (G_OBJECT (sel), "active-pad", new_pad, NULL);

    g_print ("switched from %s:%s to %s:%s\n", GST_DEBUG_PAD_NAME (old_pad),
             GST_DEBUG_PAD_NAME (new_pad));

    gst_object_unref (old_pad);

    return TRUE;

}

static void
on_bin_element_added (GstBin * bin, GstElement * element, gpointer user_data)
{
//    sleep(3);
    GST_ERROR("Element addedd callback");
    g_object_set (G_OBJECT (element), "sync", FALSE, "async", FALSE, NULL);
}

gint
main (gint argc, gchar * argv[])
{
    GstElement *pipeline, *src, *osel, *sink1, *sink2;
    GstPad *sinkpad;

    /* init GStreamer */
    gst_init (&argc, &argv);
    GST_ERROR("AFTER INIT");
    loop = g_main_loop_new (NULL, FALSE);


    /* Create pipeline */
    pipeline = gst_element_factory_make ("pipeline", "pipeline");
    osel = gst_element_factory_make ("output-selector", "osel");
    sink1 = gst_element_factory_make ("autovideosink", "sink1"); //THIS IS GONNA BE OUR USB
    sink2 = gst_element_factory_make ("fakesink", "sink2");
    GstElement* bin = gst_bin_new("mybin");

    src = gst_element_factory_make ("videotestsrc", "src");

    if (!pipeline || !src || !osel || !sink1 ||
        !sink2 || !bin) {
        g_print ("missing element\n");
        return -1;
    }
    gst_bin_add_many((GstBin*)bin, osel, sink1, sink2,  NULL);
    /* handle deferred properties This is because autosink creates child that has to have properties to be set*/
    g_signal_connect (G_OBJECT (sink1), "element-added",
                      G_CALLBACK (on_bin_element_added), NULL);
    g_object_set (G_OBJECT (sink2), "sync", FALSE, "async", FALSE, NULL);

    sinkpad = gst_element_get_static_pad (osel, "sink");
    GstPad *ghost_pad = gst_ghost_pad_new("sink", sinkpad);
    gst_pad_set_active(ghost_pad,  TRUE);
    gst_element_add_pad(bin, ghost_pad);
    gst_object_unref(sinkpad);

    /* link output 1 */
    sinkpad = gst_element_get_static_pad (sink1, "sink");
    osel_src1 = gst_element_get_request_pad (osel, "src_%u");
    if (gst_pad_link (osel_src1, sinkpad) != GST_PAD_LINK_OK) {
        g_print ("linking output 1 converter failed\n");
        return -1;
    }
    gst_object_unref (sinkpad);

    /* link output 2 */
    sinkpad = gst_element_get_static_pad (sink2, "sink");
    osel_src2 = gst_element_get_request_pad (osel, "src_%u");
    if (gst_pad_link (osel_src2, sinkpad) != GST_PAD_LINK_OK) {
        g_print ("linking output 2 converter failed\n");
        return -1;
    }
    gst_object_unref (sinkpad);
    g_object_set (G_OBJECT (osel), "active-pad", osel_src2, NULL);

    /* add them to bin */
    gst_bin_add_many (GST_BIN (pipeline), src, bin, NULL);

    /* set properties */
    g_object_set (G_OBJECT (src), "num-buffers", NUM_VIDEO_BUFFERS, NULL);
    /* link src ! bin  */
    if (!gst_element_link (src, bin)) {
        g_print ("linking failed\n");
        return -1;
    }

    /* change to playing */

    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, my_bus_callback, loop);
    gst_object_unref (bus);
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* Some test stuff */
    g_timeout_add_seconds (SWITCH_TIMEOUT, switch_cb, osel);

    /* now run */
    g_main_loop_run (loop);

    /* also clean up */
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_element_release_request_pad (osel, osel_src1);
    gst_element_release_request_pad (osel, osel_src2);
    gst_object_unref (GST_OBJECT (pipeline));

    return 0;
}