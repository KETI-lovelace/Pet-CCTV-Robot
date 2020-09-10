package com.example.soboroaws.AWS;

import android.annotation.SuppressLint;
import android.os.AsyncTask;
import android.util.Log;

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

import java.util.concurrent.ExecutionException;

public class AWSManager {
    String TAG = "AWSManager";

    private AWSCredentials awsCredentials;

    private AWSKinesisVideoClient videoClient;
    private AWSKinesisVideoArchivedMediaClient archivedMediaClient;

    //AWS setting
    //private String accessKeyId = "";
    //private String secretKey = "";

    //For request
    private String streamName = "arten";
    private APIName apiName = APIName.GET_HLS_STREAMING_SESSION_URL;

    private PlaybackMode playbackMode = PlaybackMode.LIVE;
    private DiscontinuityMode discontinuityMode = DiscontinuityMode.ALWAYS;
    private HLSFragmentSelector hlsFragmentSelector = new HLSFragmentSelector();
    private HLSFragmentSelectorType hlsFragmentSelectorType = HLSFragmentSelectorType.SERVER_TIMESTAMP;

    public AWSManager() {
        setUpAWS();
    }

    public void setUpAWS() {
        awsCredentials = new AWSCredentials() {
            @Override
            public String getAWSAccessKeyId() {
                return accessKeyId;
            }

            @Override
            public String getAWSSecretKey() {
                return secretKey;
            }
        };

        videoClient = new AWSKinesisVideoClient(awsCredentials);
        videoClient.setRegion(Region.getRegion(Regions.US_WEST_2));

        archivedMediaClient = new AWSKinesisVideoArchivedMediaClient(awsCredentials);
        archivedMediaClient.setRegion(Region.getRegion(Regions.US_WEST_2));
    }

    private GetDataEndpointRequest newEndpointRequest() {
        GetDataEndpointRequest newRequest = new GetDataEndpointRequest();
        newRequest.setStreamName(streamName);
        newRequest.setAPIName(apiName);

        return newRequest;
    }

    private String getDataEndPoint() {
        GetDataEndpointRequest endpointRequest = newEndpointRequest();
        GetDataEndpointResult endpointResult = videoClient.getDataEndpoint(endpointRequest);

        return endpointResult.getDataEndpoint();
    }

    private GetHLSStreamingSessionURLRequest newHLSURLRequest() {
        GetHLSStreamingSessionURLRequest newRequest = new GetHLSStreamingSessionURLRequest();
        newRequest.setStreamName(streamName);
        newRequest.setPlaybackMode(playbackMode);
        newRequest.setDiscontinuityMode(discontinuityMode);

        hlsFragmentSelector.setFragmentSelectorType(hlsFragmentSelectorType);
        newRequest.setHLSFragmentSelector(hlsFragmentSelector);

        return newRequest;
    }

    private String getHLSURL() {
        GetHLSStreamingSessionURLRequest HLSurlRequest = newHLSURLRequest();
        GetHLSStreamingSessionURLResult HLSurlResult = archivedMediaClient.getHLSStreamingSessionURL(HLSurlRequest);

        return HLSurlResult.getHLSStreamingSessionURL();
    }

    public String getVideoURL() {
        String url="";

        @SuppressLint("StaticFieldLeak")
        AsyncTask<Void, Void, String> asyncTask = new AsyncTask() {
            @Override
            protected Object doInBackground(Object[] objects) {
                String dataEndPoint = getDataEndPoint();
                archivedMediaClient.setEndpoint(dataEndPoint);

                return getHLSURL();
            }
        };

        try{
            url = asyncTask.execute().get();
        } catch(ExecutionException | InterruptedException e){
            Log.d(TAG, "asyncTask exception : "+e.toString());
            e.printStackTrace();
        }

        return url;
    }
}
