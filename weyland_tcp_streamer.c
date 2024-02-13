#include <gst/gst.h>
#include <stdlib.h>
#include <stdio.h>


/* SRC Streaming elements. */ 
static GstElement *pipeline;
static GstElement *filesrc;
static GstElement *tsdemux;
static GstElement *queue;
static GstElement *parse;
static GstElement *mpeg2dec;
static GstElement *videoconvert;
static GstElement *x264enc;
static GstElement *mpegtsmux;
static GstElement *tcpserversink;


/* This function will be called by the pad-added signal */
static void tsdemux_pad_added_handler (GstElement *src, GstPad *new_pad, GstElement *data) {

  GstPad *sink_pad = gst_element_get_static_pad (data, "sink");  // input pad to what we are linking to.
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;

  g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

  /* If our "filter" is already linked, we have nothing to do here */
  if (gst_pad_is_linked (sink_pad)) {
    g_print ("We are already linked. Ignoring.\n");
    goto exit;
  }

  /* Check the new pad's type */
  new_pad_caps = gst_pad_get_current_caps (new_pad);
  new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
  new_pad_type = gst_structure_get_name (new_pad_struct);
  if (!g_str_has_prefix (new_pad_type, "video/mpeg")) {
    g_print ("It has type '%s' which is not video/mpeg. Ignoring.\n", new_pad_type);
    // Link the element and exit.
    goto exit;
  }
  // We want to link the video/mpeg capability.  
  if(g_str_has_prefix( new_pad_type, "video/mpeg")){
	
  	/* Attempt the link */
  	ret = gst_pad_link (new_pad, sink_pad); // link the tsdemux pad to the sink pad.
  	if (GST_PAD_LINK_FAILED (ret)) {
    		g_print ("Type is '%s' but link failed.\n", new_pad_type);
  	} else {
    		g_print ("Link succeeded (type '%s').\n", new_pad_type);
  	}
  }
exit:
  if (new_pad_caps != NULL)
    gst_caps_unref (new_pad_caps);

  /* Unreference the sink pad */
  gst_object_unref (sink_pad);
}

/*
static void handle_message (CustomData *data, GstMessage *msg) {
  GError *err;
  gchar *debug_info;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (msg, &err, &debug_info);
      g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
      g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
      g_clear_error (&err);
      g_free (debug_info);
      data->terminate = TRUE;
      break;
    case GST_MESSAGE_EOS:
      g_print ("\nEnd-Of-Stream reached.\n");
      data->terminate = TRUE;
      break;
    case GST_MESSAGE_DURATION:
      data->duration = GST_CLOCK_TIME_NONE;
      break;
      case GST_MESSAGE_STATE_CHANGED: {
      GstState old_state, new_state, pending_state;
      gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
      if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->playbin)) {
        g_print ("Pipeline state changed from %s to %s:\n",
            gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));

        data->playing = (new_state == GST_STATE_PLAYING);

        if (data->playing) {
          GstQuery *query;
          gint64 start, end;
          query = gst_query_new_seeking (GST_FORMAT_TIME);
          if (gst_element_query (data->playbin, query)) {
            gst_query_parse_seeking (query, NULL, &data->seek_enabled, &start, &end);
            if (data->seek_enabled) {
              g_print ("Seeking is ENABLED from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT "\n",
                  GST_TIME_ARGS (start), GST_TIME_ARGS (end));
            } else {
              g_print ("Seeking is DISABLED for this stream.\n");
            }
          }
          else {
            g_printerr ("Seeking query failed.");
          }
	  gst_query_unref (query);
        }
      }
    } break;
    default:
      g_printerr ("Unexpected message received.\n");
      break;
  }
  gst_message_unref (msg);
}
*/

static GstElement *create_pipeline(char *file_name, int port, char *host_name){

    pipeline             = gst_pipeline_new("pipeline");
    filesrc              = gst_element_factory_make("filesrc", "filesrc");
    tsdemux              = gst_element_factory_make("tsdemux", "tsdemux");
    queue                = gst_element_factory_make("queue", "queue");
    parse                = gst_element_factory_make("mpegvideoparse","mpegvideoparse");
    mpeg2dec             = gst_element_factory_make("mpeg2dec", "mpeg2dec");
    videoconvert         = gst_element_factory_make("videoconvert", "videoconvert");
    x264enc              = gst_element_factory_make("x264enc", "x264enc");
    mpegtsmux            = gst_element_factory_make("mpegtsmux", "mpegtsmux");
    tcpserversink        = gst_element_factory_make("tcpserversink", "tcpserversink");

    g_object_set( G_OBJECT(filesrc), "location", file_name, NULL);
    g_object_set( G_OBJECT(tcpserversink), "host", host_name, "port", port, NULL);

                  gst_bin_add_many( GST_BIN(pipeline), filesrc,tsdemux,queue,mpeg2dec,x264enc,mpegtsmux,tcpserversink, NULL);
		  gst_element_link(filesrc, tsdemux); 
                  g_signal_connect(tsdemux, "pad-added", G_CALLBACK (tsdemux_pad_added_handler), queue);
		                
              if( gst_element_link_many(queue,mpeg2dec,x264enc,mpegtsmux,tcpserversink, NULL) != TRUE){
	          g_error("Could not link all elements.");
                  exit(-1);	
              }

    return pipeline;
}
int main(int argc, char **argv) {


    gst_init(&argc, &argv);

    GstElement *local_pipeline;
    GstBus *bus;
    GstMessage *msg;
    GMainLoop *loop;
    gboolean terminate = FALSE;
    GstStateChangeReturn ret;

    if(argc != 4){
	g_error("usage is: ./weyland_tcp_streamer <file_name> <port_to_stream_on> <ip address>");
	exit(-1);

    }
    local_pipeline = create_pipeline(argv[1], atoi(argv[2]), argv[3]);

    ret = gst_element_set_state( local_pipeline, GST_STATE_PLAYING);
    if( ret == GST_STATE_CHANGE_FAILURE) {
     g_printerr("Unable to set the pipeline to playing state.\n");
     gst_object_unref(local_pipeline);
     return -1;

    } else {

    g_print("[*] Set state to playing...\n");
     
    bus = gst_element_get_bus( local_pipeline ); 

    do {
       
        msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
        GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Parse message */
    if (msg != NULL) {
      GError *err;
      gchar *debug_info;
 
       switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
          gst_message_parse_error (msg, &err, &debug_info);
          g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
          g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
          g_clear_error (&err);
          g_free (debug_info);
          terminate = TRUE;
          break;
        case GST_MESSAGE_EOS:
          g_print ("End-Of-Stream reached.\n");
          terminate = TRUE;
          break;
        case GST_MESSAGE_STATE_CHANGED:
          /* We are only interested in state-changed messages from the pipeline */
          if (GST_MESSAGE_SRC (msg) == GST_OBJECT (local_pipeline)) {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
            g_print ("Pipeline state changed from %s to %s:\n",
                gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
          }
          break;
        default:
          /* We should not reach here */
          g_printerr ("Unexpected message received.\n");
          break;
      }
      gst_message_unref (msg);
    }
    } while (!terminate);
    }
  return 0;
}
