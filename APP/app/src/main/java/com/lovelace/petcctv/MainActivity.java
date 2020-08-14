package com.lovelace.petcctv;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;

import com.amazonaws.auth.AWSCredentials;
import com.amazonaws.regions.Region;
import com.amazonaws.regions.Regions;
import com.amazonaws.services.kinesisvideo.AWSKinesisVideoClient;
import com.amazonaws.services.kinesisvideo.model.APIName;
import com.amazonaws.services.kinesisvideo.model.GetDataEndpointRequest;
import com.amazonaws.services.kinesisvideo.model.GetDataEndpointResult;
import com.amazonaws.services.kinesisvideoarchivedmedia.AWSKinesisVideoArchivedMediaClient;
import com.amazonaws.services.kinesisvideoarchivedmedia.model.DiscontinuityMode;
import com.amazonaws.services.kinesisvideoarchivedmedia.model.GetHLSStreamingSessionURLRequest;
import com.amazonaws.services.kinesisvideoarchivedmedia.model.GetHLSStreamingSessionURLResult;
import com.amazonaws.services.kinesisvideoarchivedmedia.model.HLSFragmentSelector;
import com.amazonaws.services.kinesisvideoarchivedmedia.model.HLSFragmentSelectorType;
import com.amazonaws.services.kinesisvideoarchivedmedia.model.PlaybackMode;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 1. credentials setting
        AWSCredentials awsCredentials = new AWSCredentials() {
            @Override
            public String getAWSAccessKeyId() {
                return null;
            }

            @Override
            public String getAWSSecretKey() {
                return null;
            }
        };

        AWSKinesisVideoClient videoClient = new AWSKinesisVideoClient(awsCredentials);
        videoClient.setRegion(Region.getRegion(Regions.US_WEST_2));

        AWSKinesisVideoArchivedMediaClient client = new AWSKinesisVideoArchivedMediaClient(awsCredentials);
        client.setRegion(Region.getRegion(Regions.US_WEST_2));

        // 2. get data end point
        GetDataEndpointRequest getDataEndpointRequest = new GetDataEndpointRequest();
        getDataEndpointRequest.setStreamName("arten");
        getDataEndpointRequest.setAPIName(APIName.GET_HLS_STREAMING_SESSION_URL);
        GetDataEndpointResult dataEndpointResult = videoClient.getDataEndpoint(getDataEndpointRequest);

        client.setEndpoint(dataEndpointResult.getDataEndpoint());

        // 3. get HLS URL
        GetHLSStreamingSessionURLRequest getHLSStreamingSessionURLRequest = new GetHLSStreamingSessionURLRequest();
        getHLSStreamingSessionURLRequest.setStreamName("arten");
        getHLSStreamingSessionURLRequest.setPlaybackMode(PlaybackMode.LIVE);
        HLSFragmentSelector hlsFragmentSelector = new HLSFragmentSelector();
        hlsFragmentSelector.setFragmentSelectorType(HLSFragmentSelectorType.SERVER_TIMESTAMP);
//        hlsFragmentSelector.setTimestampRange(null);
        getHLSStreamingSessionURLRequest.setHLSFragmentSelector(hlsFragmentSelector);
        getHLSStreamingSessionURLRequest.setDiscontinuityMode(DiscontinuityMode.ALWAYS);
//        getHLSStreamingSessionURLRequest.setMaxMediaPlaylistFragmentResults(null);
//        getHLSStreamingSessionURLRequest.setExpires(null);
        GetHLSStreamingSessionURLResult getHLSStreamingSessionURLResult = client.getHLSStreamingSessionURL(getHLSStreamingSessionURLRequest);

        // 4. set video
        String url = getHLSStreamingSessionURLResult.getHLSStreamingSessionURL();

    }
}