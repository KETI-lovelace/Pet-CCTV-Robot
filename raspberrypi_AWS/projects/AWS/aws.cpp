#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <string.h>
#include <chrono>
#include <vector>
#include <stdlib.h>
#include <mutex>
#include <credential-providers/IotCertCredentialProvider.h>
#include <KinesisVideoProducer.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

using namespace std;
using namespace com::amazonaws::kinesis::video;

#ifdef __cplusplus
extern "C" {
#endif

    int gstreamer_init(int, char**);

#ifdef __cplusplus
}
#endif

#define DEFAULT_RETENTION_PERIOD_HOURS 2
#define DEFAULT_KMS_KEY_ID ""
#define DEFAULT_STREAMING_TYPE STREAMING_TYPE_REALTIME
#define DEFAULT_CONTENT_TYPE "video/h264"
#define DEFAULT_MAX_LATENCY_SECONDS 60
#define DEFAULT_FRAGMENT_DURATION_MILLISECONDS 2000
#define DEFAULT_TIMECODE_SCALE_MILLISECONDS 1
#define DEFAULT_KEY_FRAME_FRAGMENTATION TRUE
#define DEFAULT_FRAME_TIMECODES TRUE
#define DEFAULT_ABSOLUTE_FRAGMENT_TIMES TRUE
#define DEFAULT_FRAGMENT_ACKS TRUE
#define DEFAULT_RESTART_ON_ERROR TRUE
#define DEFAULT_RECALCULATE_METRICS TRUE
#define DEFAULT_STREAM_FRAMERATE 25
#define DEFAULT_AVG_BANDWIDTH_BPS (4 * 1024 * 1024)
#define DEFAULT_BUFFER_DURATION_SECONDS 120
#define DEFAULT_REPLAY_DURATION_SECONDS 40
#define DEFAULT_CONNECTION_STALENESS_SECONDS 60
#define DEFAULT_CODEC_ID "V_MPEG4/ISO/AVC"
#define DEFAULT_TRACKNAME "kinesis_video"
#define DEFAULT_FRAME_DURATION_MS 1
#define DEFAULT_CREDENTIAL_ROTATION_SECONDS 3600
#define DEFAULT_CREDENTIAL_EXPIRATION_SECONDS 180

bool isFirst = false;

typedef enum _StreamSource {
    FILE_SOURCE,
    LIVE_SOURCE,
    RTSP_SOURCE
} StreamSource;

typedef struct _FileInfo {
    _FileInfo() :
        path(""),
        last_fragment_ts(0) {}
    string path;
    uint64_t last_fragment_ts;
} FileInfo;

typedef struct _CustomData {

    _CustomData() :
        streamSource(LIVE_SOURCE),
        h264_stream_supported(false),
        synthetic_dts(0),
        last_unpersisted_file_idx(0),
        stream_status(STATUS_SUCCESS),
        base_pts(0),
        max_frame_pts(0),
        key_frame_pts(0),
        main_loop(NULL),
        first_pts(GST_CLOCK_TIME_NONE),
        use_absolute_fragment_times(true) {
        producer_start_time = chrono::duration_cast<nanoseconds>(systemCurrentTime().time_since_epoch()).count();
    }

    GMainLoop* main_loop;
    unique_ptr<KinesisVideoProducer> kinesis_video_producer;
    shared_ptr<KinesisVideoStream> kinesis_video_stream;
    bool stream_started;
    bool h264_stream_supported;
    char* stream_name;
    mutex file_list_mtx;

    // list of files to upload.
    vector<FileInfo> file_list;

    // index of file in file_list that application is currently trying to upload.
    uint32_t current_file_idx;

    // index of last file in file_list that haven't been persisted.
    atomic_uint last_unpersisted_file_idx;

    // stores any error status code reported by StreamErrorCallback.
    atomic_uint stream_status;

    // Since each file's timestamp start at 0, need to add all subsequent file's timestamp to base_pts starting from the
    // second file to avoid fragment overlapping. When starting a new putMedia session, this should be set to 0.
    // Unit: ns
    uint64_t base_pts;

    // Max pts in a file. This will be added to the base_pts for the next file. When starting a new putMedia session,
    // this should be set to 0.
    // Unit: ns
    uint64_t max_frame_pts;

    // When uploading file, store the pts of frames that has flag FRAME_FLAG_KEY_FRAME. When the entire file has been uploaded,
    // key_frame_pts contains the timetamp of the last fragment in the file. key_frame_pts is then stored into last_fragment_ts
    // of the file.
    // Unit: ns
    uint64_t key_frame_pts;

    // Used in file uploading only. Assuming frame timestamp are relative. Add producer_start_time to each frame's
    // timestamp to convert them to absolute timestamp. This way fragments dont overlap after token rotation when doing
    // file uploading.
    uint64_t producer_start_time;

    volatile StreamSource streamSource;

    string rtsp_url;

    unique_ptr<Credentials> credential;

    uint64_t synthetic_dts;

    bool use_absolute_fragment_times;

    // Pts of first video frame
    uint64_t first_pts;
} CustomData;

namespace com {
    namespace amazonaws {
        namespace kinesis {
            namespace video {

                class SampleClientCallbackProvider : public ClientCallbackProvider {
                public:

                    UINT64 getCallbackCustomData() override {
                        return reinterpret_cast<UINT64> (this);
                    }

                    StorageOverflowPressureFunc getStorageOverflowPressureCallback() override {
                        return storageOverflowPressure;
                    }

                    static STATUS storageOverflowPressure(UINT64 custom_handle, UINT64 remaining_bytes);
                };

                class SampleStreamCallbackProvider : public StreamCallbackProvider {
                    UINT64 custom_data_;
                public:
                    SampleStreamCallbackProvider(UINT64 custom_data) : custom_data_(custom_data) {}

                    UINT64 getCallbackCustomData() override {
                        return custom_data_;
                    }

                    StreamConnectionStaleFunc getStreamConnectionStaleCallback() override {
                        return streamConnectionStaleHandler;
                    };

                    StreamErrorReportFunc getStreamErrorReportCallback() override {
                        return streamErrorReportHandler;
                    };

                    DroppedFrameReportFunc getDroppedFrameReportCallback() override {
                        return droppedFrameReportHandler;
                    };

                    FragmentAckReceivedFunc getFragmentAckReceivedCallback() override {
                        return fragmentAckReceivedHandler;
                    };

                private:
                    static STATUS
                        streamConnectionStaleHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,
                            UINT64 last_buffering_ack);

                    static STATUS
                        streamErrorReportHandler(UINT64 custom_data, STREAM_HANDLE stream_handle, UPLOAD_HANDLE upload_handle, UINT64 errored_timecode,
                            STATUS status_code);

                    static STATUS
                        droppedFrameReportHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,
                            UINT64 dropped_frame_timecode);

                    static STATUS
                        fragmentAckReceivedHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,
                            UPLOAD_HANDLE upload_handle, PFragmentAck pFragmentAck);
                };

                class SampleCredentialProvider : public StaticCredentialProvider {
                    // Test rotation period is 40 second for the grace period.
                    const std::chrono::duration<uint64_t> ROTATION_PERIOD = std::chrono::seconds(DEFAULT_CREDENTIAL_ROTATION_SECONDS);
                public:
                    SampleCredentialProvider(const Credentials& credentials) :
                        StaticCredentialProvider(credentials) {}

                    void updateCredentials(Credentials& credentials) override {
                        // Copy the stored creds forward
                        credentials = credentials_;

                        // Update only the expiration
                        auto now_time = std::chrono::duration_cast<std::chrono::seconds>(
                            systemCurrentTime().time_since_epoch());
                        auto expiration_seconds = now_time + ROTATION_PERIOD;
                        credentials.setExpiration(std::chrono::seconds(expiration_seconds.count()));
                    }
                };

                class SampleDeviceInfoProvider : public DefaultDeviceInfoProvider {
                public:
                    device_info_t getDeviceInfo() override {
                        auto device_info = DefaultDeviceInfoProvider::getDeviceInfo();
                        // Set the storage size to 128mb
                        device_info.storageInfo.storageSize = 128 * 1024 * 1024;
                        return device_info;
                    }
                };

                STATUS
                    SampleClientCallbackProvider::storageOverflowPressure(UINT64 custom_handle, UINT64 remaining_bytes) {
                    UNUSED_PARAM(custom_handle);
                    return STATUS_SUCCESS;
                }

                STATUS SampleStreamCallbackProvider::streamConnectionStaleHandler(UINT64 custom_data,
                    STREAM_HANDLE stream_handle,
                    UINT64 last_buffering_ack) {
                    return STATUS_SUCCESS;
                }

                STATUS
                    SampleStreamCallbackProvider::streamErrorReportHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,
                        UPLOAD_HANDLE upload_handle, UINT64 errored_timecode, STATUS status_code) {
                    CustomData* data = reinterpret_cast<CustomData*>(custom_data);
                    bool terminate_pipeline = false;

                    // Terminate pipeline if error is not retriable or if error is retriable but we are streaming file.
                    // When streaming file, we choose to terminate the pipeline on error because the easiest way to recover
                    // is to stream the file from the beginning again.
                    // In realtime streaming, retriable error can be handled underneath. Otherwise terminate pipeline
                    // and store error status if error is fatal.
                    if ((IS_RETRIABLE_ERROR(status_code) && data->streamSource == FILE_SOURCE) ||
                        (!IS_RETRIABLE_ERROR(status_code) && !IS_RECOVERABLE_ERROR(status_code))) {
                        data->stream_status = status_code;
                        terminate_pipeline = true;
                    }

                    if (terminate_pipeline && data->main_loop != NULL) {
                        g_main_loop_quit(data->main_loop);
                    }

                    return STATUS_SUCCESS;
                }

                STATUS
                    SampleStreamCallbackProvider::droppedFrameReportHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,
                        UINT64 dropped_frame_timecode) {
                    return STATUS_SUCCESS;
                }

                STATUS
                    SampleStreamCallbackProvider::fragmentAckReceivedHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,
                        UPLOAD_HANDLE upload_handle, PFragmentAck pFragmentAck) {
                    CustomData* data = reinterpret_cast<CustomData*>(custom_data);
                    if (data->streamSource == FILE_SOURCE && pFragmentAck->ackType == FRAGMENT_ACK_TYPE_PERSISTED) {
                        std::unique_lock<std::mutex> lk(data->file_list_mtx);
                        uint32_t last_unpersisted_file_idx = data->last_unpersisted_file_idx.load();
                        uint64_t last_frag_ts = data->file_list.at(last_unpersisted_file_idx).last_fragment_ts /
                            duration_cast<nanoseconds>(milliseconds(DEFAULT_TIMECODE_SCALE_MILLISECONDS)).count();
                        if (last_frag_ts != 0 && last_frag_ts == pFragmentAck->timestamp) {
                            data->last_unpersisted_file_idx = last_unpersisted_file_idx + 1;
                        }
                    }
                    return STATUS_SUCCESS;
                }

            }  // namespace video
        }  // namespace kinesis
    }  // namespace amazonaws
}  // namespace com;

void create_kinesis_video_frame(Frame* frame, const nanoseconds& pts, const nanoseconds& dts, FRAME_FLAGS flags,
    void* data, size_t len) {
    frame->flags = flags;
    frame->decodingTs = static_cast<UINT64>(dts.count()) / DEFAULT_TIME_UNIT_IN_NANOS;
    frame->presentationTs = static_cast<UINT64>(pts.count()) / DEFAULT_TIME_UNIT_IN_NANOS;
    // set duration to 0 due to potential high spew from rtsp streams
    frame->duration = 0;
    frame->size = static_cast<UINT32>(len);
    frame->frameData = reinterpret_cast<PBYTE>(data);
    frame->trackId = DEFAULT_TRACK_ID;
}

bool put_frame(shared_ptr<KinesisVideoStream> kinesis_video_stream, void* data, size_t len, const nanoseconds& pts, const nanoseconds& dts, FRAME_FLAGS flags) {
    Frame frame;
    create_kinesis_video_frame(&frame, pts, dts, flags, data, len);
    return kinesis_video_stream->putFrame(frame);
}

static GstFlowReturn on_new_sample(GstElement* sink, CustomData* data) {
    printf("on_new_sample\n");
    GstBuffer* buffer;
    bool isDroppable, isHeader, delta;
    size_t buffer_size;
    GstFlowReturn ret = GST_FLOW_OK;
    STATUS curr_stream_status = data->stream_status.load();
    GstSample* sample = nullptr;
    GstMapInfo info;

    if (STATUS_FAILED(curr_stream_status)) {
        ret = GST_FLOW_ERROR;
        goto CleanUp;
    }



    info.data = nullptr;
    sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));

    /////////////////////////////////////
    //GstCaps* caps;
    //GstStructure* s;
    //gchar* format;

    //caps = gst_sample_get_caps(sample);
    //s = gst_caps_get_structure(caps, 0);

    ///* we need to get the final caps on the buffer to get the size */
    //printf("format : %s\n", gst_structure_get_string(s, "format"));
    //////////////////////////////////

    // capture cpd at the first frame
    if (!data->stream_started) {
        data->stream_started = true;
        GstCaps* gstcaps = (GstCaps*)gst_sample_get_caps(sample);
        GstStructure* gststructforcaps = gst_caps_get_structure(gstcaps, 0);
        const GValue* gstStreamFormat = gst_structure_get_value(gststructforcaps, "codec_data");
        gchar* cpd = gst_value_serialize(gstStreamFormat);
        data->kinesis_video_stream->start(std::string(cpd));
        g_free(cpd);
    }

    buffer = gst_sample_get_buffer(sample);

    isHeader = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_HEADER);
    isDroppable = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_CORRUPTED) ||
        GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DECODE_ONLY) ||
        (GST_BUFFER_FLAGS(buffer) == GST_BUFFER_FLAG_DISCONT) ||
        (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DISCONT) && GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT)) ||
        // drop if buffer contains header only and has invalid timestamp
        (isHeader && (!GST_BUFFER_PTS_IS_VALID(buffer) || !GST_BUFFER_DTS_IS_VALID(buffer)));

    if (!isDroppable) {

        delta = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

        FRAME_FLAGS kinesis_video_flags = delta ? FRAME_FLAG_NONE : FRAME_FLAG_KEY_FRAME;

        // Always synthesize dts for file sources because file sources dont have meaningful dts.
        // For some rtsp sources the dts is invalid, therefore synthesize.
        if (data->streamSource == FILE_SOURCE || !GST_BUFFER_DTS_IS_VALID(buffer)) {
            data->synthetic_dts += DEFAULT_FRAME_DURATION_MS * HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_TIME_UNIT_IN_NANOS;
            buffer->dts = data->synthetic_dts;
        }
        else if (GST_BUFFER_DTS_IS_VALID(buffer)) {
            data->synthetic_dts = buffer->dts;
        }

        if (data->streamSource == FILE_SOURCE) {
            data->max_frame_pts = MAX(data->max_frame_pts, buffer->pts);

            // make sure the timestamp is continuous across multiple files.
            buffer->pts += data->base_pts + data->producer_start_time;

            if (CHECK_FRAME_FLAG_KEY_FRAME(kinesis_video_flags)) {
                data->key_frame_pts = buffer->pts;
            }
        }
        else if (data->use_absolute_fragment_times) {
            if (data->first_pts == GST_CLOCK_TIME_NONE) {
                data->first_pts = buffer->pts;
            }
            buffer->pts += data->producer_start_time - data->first_pts;
        }

        if (!gst_buffer_map(buffer, &info, GST_MAP_READ)) {
            goto CleanUp;
        }

        put_frame(data->kinesis_video_stream, info.data, info.size, std::chrono::nanoseconds(buffer->pts),
            std::chrono::nanoseconds(buffer->dts), kinesis_video_flags);
    }

CleanUp:

    if (info.data != nullptr) {
        gst_buffer_unmap(buffer, &info);
    }

    if (sample != nullptr) {
        gst_sample_unref(sample);
    }

    return ret;
}

static GstFlowReturn on_new_sample_raw(GstElement* sink, CustomData* data) {
    printf("on_raw\n");
    GstBuffer* buffer;
    GstFlowReturn ret = GST_FLOW_OK;
    GstSample* sample = nullptr;
    GstMapInfo map;
    GError* error = NULL;
    GdkPixbuf* pixbuf;
    GstCaps* caps;
    GstStructure* s;
    gboolean res;
    gint width, height;

    /* get the snapshot buffer format now. We set the caps on the appsink so
     * that it can only be an rgb buffer. The only thing we have not specified
     * on the caps is the height, which is dependant on the pixel-aspect-ratio
     * of the source material */
    printf("1\n");
    caps = gst_sample_get_caps(sample);
    if (!caps) {
        g_print("could not get snapshot format\n");
        exit(-1);
    }
    s = gst_caps_get_structure(caps, 0);
    printf("2\n");

    /* we need to get the final caps on the buffer to get the size */
    res = gst_structure_get_int(s, "width", &width);
    res |= gst_structure_get_int(s, "height", &height);
    if (!res) {
        g_print("could not get snapshot dimension\n");
        exit(-1);
    }

    printf("3\n");

    /* create pixmap from buffer and save, gstreamer video buffers have a stride
     * that is rounded up to the nearest multiple of 4 */
    buffer = gst_sample_get_buffer(sample);
    /* Mapping a buffer can fail (non-readable) */
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        pixbuf = gdk_pixbuf_new_from_data(map.data,
            GDK_COLORSPACE_RGB, FALSE, 8, width, height,
            GST_ROUND_UP_4(width * 3), NULL, NULL);

        printf("4\n");

        /* save the pixbuf */
        gdk_pixbuf_save(pixbuf, "snapshot.png", "png", &error, NULL);
        printf("5\n");
    }

    if (map.data != nullptr) {
        gst_buffer_unmap(buffer, &map);
    }

    if (sample != nullptr) {
        gst_sample_unref(sample);
    }

    return ret;
}

static bool format_supported_by_source(GstCaps* src_caps, GstCaps* query_caps, int width, int height, int framerate) {
    gst_caps_set_simple(query_caps,
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION, framerate, 1,
        NULL);
    bool is_match = gst_caps_can_intersect(query_caps, src_caps);

    // in case the camera has fps as 10000000/333333
    if (!is_match) {
        gst_caps_set_simple(query_caps,
            "framerate", GST_TYPE_FRACTION_RANGE, framerate, 1, framerate + 1, 1,
            NULL);
        is_match = gst_caps_can_intersect(query_caps, src_caps);
    }

    return is_match;
}

static bool resolution_supported(GstCaps* src_caps, GstCaps* query_caps_h264, CustomData& data, int width, int height, int framerate) {
    if (query_caps_h264 && format_supported_by_source(src_caps, query_caps_h264, width, height, framerate)) {
        data.h264_stream_supported = true;
        //data.h264_stream_supported = false;
    }
    else {
        return false;
    }
    return true;
}

static bool resolution_supported_raw(GstCaps* src_caps, GstCaps* query_caps_raw, CustomData& data, int width, int height, int framerate) {
    if (query_caps_raw && format_supported_by_source(src_caps, query_caps_raw, width, height, framerate)) {
        //data.h264_stream_supported = false;
        printf("raw ok\n");
    }
    else {
        return false;
    }
    return true;
}

/* This function is called when an error message is posted on the bus */
static void error_cb(GstBus* bus, GstMessage* msg, CustomData* data) {
    GError* err;
    gchar* debug_info;

    /* Print error details on the screen */
    gst_message_parse_error(msg, &err, &debug_info);
    g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
    g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);

    g_main_loop_quit(data->main_loop);
}

void kinesis_video_init(CustomData* data) {
    unique_ptr<DeviceInfoProvider> device_info_provider(new SampleDeviceInfoProvider());
    unique_ptr<ClientCallbackProvider> client_callback_provider(new SampleClientCallbackProvider());
    unique_ptr<StreamCallbackProvider> stream_callback_provider(new SampleStreamCallbackProvider(
        reinterpret_cast<UINT64>(data)));

    char const* accessKey;
    char const* secretKey;
    char const* sessionToken;
    char const* defaultRegion;
    string defaultRegionStr;
    string sessionTokenStr;

    char const* iot_get_credential_endpoint;
    char const* cert_path;
    char const* private_key_path;
    char const* role_alias;
    char const* ca_cert_path;

    unique_ptr<CredentialProvider> credential_provider;

    if (nullptr == (defaultRegion = getenv(DEFAULT_REGION_ENV_VAR))) {
        defaultRegionStr = DEFAULT_AWS_REGION;
    }
    else {
        defaultRegionStr = string(defaultRegion);
    }

    if (nullptr != (accessKey = "AKIAYQTQW44UHUFBH6WJ") &&
        nullptr != (secretKey = "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")) {

        if (nullptr != (sessionToken = getenv(SESSION_TOKEN_ENV_VAR))) {
            sessionTokenStr = string(sessionToken);
        }
        else {
            sessionTokenStr = "";
        }

        data->credential.reset(new Credentials(string(accessKey),
            string(secretKey),
            sessionTokenStr,
            std::chrono::seconds(DEFAULT_CREDENTIAL_EXPIRATION_SECONDS)));
        credential_provider.reset(new SampleCredentialProvider(*data->credential.get()));
        printf("credential success\n");

    }
    else if (nullptr != (iot_get_credential_endpoint = getenv("IOT_GET_CREDENTIAL_ENDPOINT")) &&
        nullptr != (cert_path = getenv("CERT_PATH")) &&
        nullptr != (private_key_path = getenv("PRIVATE_KEY_PATH")) &&
        nullptr != (role_alias = getenv("ROLE_ALIAS")) &&
        nullptr != (ca_cert_path = getenv("CA_CERT_PATH"))) {
        credential_provider.reset(new IotCertCredentialProvider(iot_get_credential_endpoint,
            cert_path,
            private_key_path,
            role_alias,
            ca_cert_path,
            data->stream_name));

    }
    else {
        printf("another else\n");
    }

    data->kinesis_video_producer = KinesisVideoProducer::createSync(move(device_info_provider),
        move(client_callback_provider),
        move(stream_callback_provider),
        move(credential_provider),
        defaultRegionStr);

}

void kinesis_video_stream_init(CustomData* data) {
    /* create a test stream */
    map<string, string> tags;
    char tag_name[MAX_TAG_NAME_LEN];
    char tag_val[MAX_TAG_VALUE_LEN];
    SPRINTF(tag_name, "piTag");
    SPRINTF(tag_val, "piValue");

    STREAMING_TYPE streaming_type = DEFAULT_STREAMING_TYPE;
    data->use_absolute_fragment_times = DEFAULT_ABSOLUTE_FRAGMENT_TIMES;

    if (data->streamSource == FILE_SOURCE) {
        streaming_type = STREAMING_TYPE_OFFLINE;
        data->use_absolute_fragment_times = true;
    }

    unique_ptr<StreamDefinition> stream_definition(new StreamDefinition(
        data->stream_name,
        hours(DEFAULT_RETENTION_PERIOD_HOURS),
        &tags,
        DEFAULT_KMS_KEY_ID,
        streaming_type,
        DEFAULT_CONTENT_TYPE,
        duration_cast<milliseconds> (seconds(DEFAULT_MAX_LATENCY_SECONDS)),
        milliseconds(DEFAULT_FRAGMENT_DURATION_MILLISECONDS),
        milliseconds(DEFAULT_TIMECODE_SCALE_MILLISECONDS),
        DEFAULT_KEY_FRAME_FRAGMENTATION,
        DEFAULT_FRAME_TIMECODES,
        data->use_absolute_fragment_times,
        DEFAULT_FRAGMENT_ACKS,
        DEFAULT_RESTART_ON_ERROR,
        DEFAULT_RECALCULATE_METRICS,
        0,
        DEFAULT_STREAM_FRAMERATE,
        DEFAULT_AVG_BANDWIDTH_BPS,
        seconds(DEFAULT_BUFFER_DURATION_SECONDS),
        seconds(DEFAULT_REPLAY_DURATION_SECONDS),
        seconds(DEFAULT_CONNECTION_STALENESS_SECONDS),
        DEFAULT_CODEC_ID,
        DEFAULT_TRACKNAME,
        nullptr,
        0));
    data->kinesis_video_stream = data->kinesis_video_producer->createStreamSync(move(stream_definition));

    // reset state
    data->stream_status = STATUS_SUCCESS;
    data->stream_started = false;

    // since we are starting new putMedia, timestamp need not be padded.
    if (data->streamSource == FILE_SOURCE) {
        data->base_pts = 0;
        data->max_frame_pts = 0;
    }

}

int gstreamer_live_source_init(int argc, char* argv[], CustomData* data, GstElement* pipeline) {
    bool isOnRpi = false;

    /* init stream format */
    int width = 0, height = 0, framerate = 25, bitrateInKBPS = 512;
    // index 1 is stream name which is already processed
    for (int i = 2; i < argc; i++) {
        if (i < argc) {
            if ((0 == STRCMPI(argv[i], "-w")) ||
                (0 == STRCMPI(argv[i], "/w")) ||
                (0 == STRCMPI(argv[i], "--w"))) {
                // process the width
                if (STATUS_FAILED(STRTOI32(argv[i + 1], NULL, 10, &width))) {
                    return 1;
                }
            }
            else if ((0 == STRCMPI(argv[i], "-h")) ||
                (0 == STRCMPI(argv[i], "/h")) ||
                (0 == STRCMPI(argv[i], "--h"))) {
                // process the width
                if (STATUS_FAILED(STRTOI32(argv[i + 1], NULL, 10, &height))) {
                    return 1;
                }
            }
            else if ((0 == STRCMPI(argv[i], "-f")) ||
                (0 == STRCMPI(argv[i], "/f")) ||
                (0 == STRCMPI(argv[i], "--f"))) {
                // process the width
                if (STATUS_FAILED(STRTOI32(argv[i + 1], NULL, 10, &framerate))) {
                    return 1;
                }
            }
            else if ((0 == STRCMPI(argv[i], "-b")) ||
                (0 == STRCMPI(argv[i], "/b")) ||
                (0 == STRCMPI(argv[i], "--b"))) {
                // process the width
                if (STATUS_FAILED(STRTOI32(argv[i + 1], NULL, 10, &bitrateInKBPS))) {
                    return 1;
                }
            }
            // skip the index
            i++;
        }
        else if (0 == STRCMPI(argv[i], "-?") ||
            0 == STRCMPI(argv[i], "--?") ||
            0 == STRCMPI(argv[i], "--help")) {
            g_printerr("Invalid arguments\n");
            return 1;
        }
        else if (argv[i][0] == '/' ||
            argv[i][0] == '-') {
            // Unknown option
            g_printerr("Invalid arguments\n");
            return 1;
        }
    }

    if ((width == 0 && height != 0) || (width != 0 && height == 0)) {
        g_printerr("Invalid resolution\n");
        return 1;
    }

    GstElement* source_filter, * filter, * appsink, * h264parse, * encoder, * source, * video_convert;
    GstElement* tee, *queue_image, *queue_stream, *queue_app, *source_filter_image, *appsink_image;
    
    tee = gst_element_factory_make("tee", "tee");
    queue_image = gst_element_factory_make("queue", "queue_image");
    queue_stream = gst_element_factory_make("queue", "queue_stream");
    queue_app = gst_element_factory_make("queue", "queue_app");
    source_filter_image = gst_element_factory_make("capsfilter", "source_filter_image");
    video_convert = gst_element_factory_make("videoconvert", "video_convert");
    appsink_image = gst_element_factory_make("appsink", "appsink_image");

    /* create the elemnents */
    /*
       gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,format=I420,width=1280,height=720,framerate=15/1 ! x264enc pass=quant bframes=0 ! video/x-h264,profile=baseline,format=I420,width=1280,height=720,framerate=15/1 ! matroskamux ! filesink location=test.mkv
     */
    source_filter = gst_element_factory_make("capsfilter", "source_filter");
    filter = gst_element_factory_make("capsfilter", "encoder_filter");
    appsink = gst_element_factory_make("appsink", "appsink");
    h264parse = gst_element_factory_make("h264parse", "h264parse"); // needed to enforce avc stream format

    source = gst_element_factory_make("v4l2src", "source");

    if (!pipeline || !source || !source_filter || !filter || !appsink || !h264parse || !source_filter_image) {
        g_printerr("Not all elements could be created.\n");
        return 1;
    }

    /* configure source */
    g_object_set(G_OBJECT(source), "do-timestamp", TRUE, "device", "/dev/video0", NULL);

    /* Determine whether device supports h264 encoding and select a streaming resolution supported by the device*/
    if (GST_STATE_CHANGE_FAILURE == gst_element_set_state(source, GST_STATE_READY)) {
        g_printerr("Unable to set the source to ready state.\n");
        return 1;
    }

    GstPad* srcpad = gst_element_get_static_pad(source, "src");
    GstCaps* src_caps = gst_pad_query_caps(srcpad, NULL);
    gst_element_set_state(source, GST_STATE_NULL);

    GstCaps* query_caps_raw = gst_caps_new_simple("video/x-raw",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        NULL);
    GstCaps* query_caps_h264 = gst_caps_new_simple("video/x-h264",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        NULL);

    vector<int> res_width = { 640, 1280, 1920 };
    vector<int> res_height = { 480, 720, 1080 };
    vector<int> fps = { 30, 25, 20 };
    bool found_resolution = false;
    for (int i = 0; i < res_width.size(); i++) {
        width = res_width[i];
        height = res_height[i];
        for (int j = 0; j < fps.size(); j++) {
            framerate = fps[j];
            if (resolution_supported(src_caps, query_caps_h264, *data, width, height, framerate)) {
                found_resolution = true;
                break;
            }
        }
        if (found_resolution) {
            break;
        }
    }
    if (!found_resolution) {
        g_printerr("Default list of resolutions (1920x1080, 1280x720, 640x480) are not supported by video source\n");
        return 1;
    }

    found_resolution = false;
    for (int i = 0; i < res_width.size(); i++) {
        width = res_width[i];
        height = res_height[i];
        for (int j = 0; j < fps.size(); j++) {
            framerate = fps[j];
            if (resolution_supported_raw(src_caps, query_caps_raw, *data, width, height, framerate)) {
                found_resolution = true;
                break;
            }
        }
        if (found_resolution) {
            break;
        }
    }
    if (!found_resolution) {
        g_printerr("Default list of resolutions (1920x1080, 1280x720, 640x480) are not supported by video source\n");
        return 1;
    }

    gst_caps_unref(src_caps);
    gst_object_unref(srcpad);

    /* source filter */
    gst_caps_set_simple(query_caps_h264,
        "stream-format", G_TYPE_STRING, "byte-stream",
        "alignment", G_TYPE_STRING, "au",
        NULL);
    g_object_set(G_OBJECT(source_filter), "caps", query_caps_h264, NULL);

    gst_caps_set_simple(query_caps_raw,
        "format", G_TYPE_STRING, "RGB",
        NULL);
    g_object_set(G_OBJECT(source_filter_image), "caps", query_caps_raw, NULL);

    printf("after filter_image\n");

    gst_caps_unref(query_caps_h264);
    gst_caps_unref(query_caps_raw);

    /* configure filter */
    GstCaps* h264_caps = gst_caps_new_simple("video/x-h264",
        "stream-format", G_TYPE_STRING, "avc",
        "alignment", G_TYPE_STRING, "au",
        NULL);

    g_object_set(G_OBJECT(filter), "caps", h264_caps, NULL);
    gst_caps_unref(h264_caps);

    /* configure appsink */
    g_object_set(G_OBJECT(appsink), "emit-signals", TRUE, "sync", FALSE, NULL);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), data);

    g_object_set(G_OBJECT(appsink_image), "emit-signals", TRUE, "sync", FALSE, NULL);
    g_signal_connect(appsink_image, "new-sample", G_CALLBACK(on_new_sample_raw), data);

    printf("after appsink_image\n");

    /* build the pipeline */

    gst_bin_add_many(GST_BIN(pipeline), source, tee, queue_stream, source_filter, h264parse, filter, appsink, queue_image, source_filter_image, video_convert, appsink_image, NULL);
    printf("after bin add\n");
    
    if (!gst_element_link_many(source, tee, NULL)) {
        g_printerr("Failed to link elements\n");
            gst_object_unref(pipeline);
            return -1;
    }

    if (!gst_element_link_many(tee, queue_stream, source_filter, h264parse, filter, appsink, NULL)) {
        g_printerr("Failed to link elements2\n");
        gst_object_unref(pipeline);
        return -1;
    }

    if (!gst_element_link_many(tee, queue_image, video_convert, source_filter_image, appsink_image, NULL)) {
        g_printerr("Failed to link elements3\n");
        gst_object_unref(pipeline);
        return -1;
    }

    return 0;
}

int gstreamer_init(int argc, char* argv[], CustomData* data) {

    /* init GStreamer */
    gst_init(&argc, &argv);

    GstElement* pipeline;
    int ret;
    GstStateChangeReturn gst_ret;

    // Reset first frame pts
    data->first_pts = GST_CLOCK_TIME_NONE;

    pipeline = gst_pipeline_new("live-kinesis-pipeline");
    ret = gstreamer_live_source_init(argc, argv, data, pipeline);

    if (ret != 0) {
        return ret;
    }

    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, data);
    gst_object_unref(bus);

    /* start streaming */
    gst_ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (gst_ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return 1;
    }

    printf("before stream\n");

    data->main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(data->main_loop);

    /* free resources */
    gst_bus_remove_signal_watch(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(data->main_loop);
    data->main_loop = NULL;
    return 0;
}

int main(int argc, char* argv[]) {

    //if (argc < 2) {
    //    printf("nope\n");
    //    return 1;
    //}

    CustomData data;
    char stream_name[MAX_STREAM_NAME_LEN + 1];
    STATUS stream_status = STATUS_SUCCESS;

    STRNCPY(stream_name, "ARTEN", MAX_STREAM_NAME_LEN);
    stream_name[MAX_STREAM_NAME_LEN] = '\0';
    data.stream_name = stream_name;

    data.streamSource = LIVE_SOURCE;

    /* init Kinesis Video */
    try {
        kinesis_video_init(&data);
        kinesis_video_stream_init(&data);
    }
    catch (runtime_error& err) {
        printf("error occured\n %s\n", err.what());
        return 1;
    }

    // non file uploading scenario
    gstreamer_init(argc, argv, &data);
    if (STATUS_SUCCEEDED(stream_status)) {
        // if stream_status is success after eos, send out remaining frames.
        data.kinesis_video_stream->stopSync();
    }
    else {
        data.kinesis_video_stream->stop();
    }

    // CleanUp
    data.kinesis_video_producer->freeStream(data.kinesis_video_stream);

    return 0;
}
