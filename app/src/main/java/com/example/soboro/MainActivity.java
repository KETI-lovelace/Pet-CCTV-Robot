package com.example.soboro;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;

import android.os.AsyncTask;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.google.firebase.database.DataSnapshot;
import com.google.firebase.database.DatabaseError;
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;
import com.google.firebase.database.Query;
import com.google.firebase.database.ValueEventListener;
import com.google.firebase.iid.FirebaseInstanceId;

import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import androidx.appcompat.app.AppCompatActivity;

import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import android.widget.VideoView;

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
import com.google.android.exoplayer2.DefaultLoadControl;
import com.google.android.exoplayer2.DefaultRenderersFactory;
import com.google.android.exoplayer2.ExoPlaybackException;
import com.google.android.exoplayer2.ExoPlayerFactory;
import com.google.android.exoplayer2.Player;
import com.google.android.exoplayer2.SimpleExoPlayer;
import com.google.android.exoplayer2.source.ExtractorMediaSource;
import com.google.android.exoplayer2.source.MediaSource;
import com.google.android.exoplayer2.source.hls.HlsMediaSource;
import com.google.android.exoplayer2.trackselection.DefaultTrackSelector;
import com.google.android.exoplayer2.ui.PlayerView;
import com.google.android.exoplayer2.upstream.DefaultDataSourceFactory;
import com.google.android.exoplayer2.upstream.DefaultHttpDataSourceFactory;
import com.google.android.exoplayer2.util.Util;
import com.example.soboro.AWS.AWSManager;


public class MainActivity extends AppCompatActivity {

    private static final String TAG = "MainActivity";
    private String myToken;

    CheckBox chSubscribe;
    Button btSubscribe;
    public TextView tvResult;
    EditText etName;
    EditText etEmail;

    FirebaseDatabase mdatabase;
    DatabaseReference myRef;
    private String userId;

    ////////////////streaming////////////////
    AWSManager awsManager;

    Button startBtn;

    //For video
    private PlayerView videoView;
    private SimpleExoPlayer player;

    private Boolean playWhenReady = true;
    private int currentWindow = 0;
    private Long playbackPosition = 0L;

    boolean isPlaying = false;
    //////////////////////////////////////////

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

//        //실시간 앱 영상 스트리밍
//        WebView webView = (WebView)findViewById(R.id.webView);
//        webView.setWebViewClient(new WebViewClient());
//        webView.setBackgroundColor(255);
//        //영상을 폭에 꽉 차게 할려고 했지만 먹히지 않음???
//        webView.getSettings().setLoadWithOverviewMode(true);
//        webView.getSettings().setUseWideViewPort(true);
//        //이건 최신 버전에서는 사용하지 않게됨
//        //webView.getSettings().setDefaultZoom(WebSettings.ZoomDensity.FAR);
//
//        WebSettings webSettings = webView.getSettings();
//        webSettings.setJavaScriptEnabled(true);
//
//        //영상을 폭을 꽉 차게 하기 위해 직접 html태그로 작성함.
////        webView.loadData("<html><head><style type='text/css'>body{margin:auto auto;text-align:center;} img{width:100%25;} div{overflow: hidden;} </style></head><body><div><img src='http://172.20.10.4:8080/stream'/></div></body></html>" ,"text/html",  "UTF-8");
////        webView.loadUrl("http://172.20.10.4:8080/stream/video.mjpeg");
//        webView.loadUrl("http://192.168.137.38:8080/stream");

        /////////////streaming////////////
        startBtn = findViewById(R.id.startBtn);

        videoView = findViewById(R.id.videoView);

        awsManager = new AWSManager();

        startBtn.setOnClickListener(view -> {
            if(!isPlaying) {
                String url = awsManager.getVideoURL();
                initializePlayer(url);
                startBtn.setText("STOP");
                isPlaying = true;
            } else {
                releasePlayer();
                startBtn.setText("START");
                isPlaying = false;
            }
        });
        ////////////////////////////////////


        chSubscribe = (CheckBox) findViewById(R.id.ch_checkbox);
        btSubscribe = (Button) findViewById(R.id.bt_subscribe);
        tvResult = (TextView) findViewById(R.id.tv_result);
        etName = (EditText) findViewById(R.id.et_name);
        etEmail = (EditText) findViewById(R.id.et_email);

        myToken = FirebaseInstanceId.getInstance().getToken();
        if (myToken != null) {
            tvResult.setText(myToken.substring(0, 70));
        }

        mdatabase = FirebaseDatabase.getInstance();
        myRef = mdatabase.getReference("users");

        //구독 체크박스 확인해서 버튼 글자를 변경함
        if (chSubscribe.isChecked()) {
            btSubscribe.setText("구독합니다.");
        } else {
            btSubscribe.setText("해지합니다.");
        }

        //database에서 token으로 검색해서 데이터를 가져오기
        Query myQuery = myRef.orderByChild("token").equalTo(myToken);
        myQuery.addValueEventListener(new ValueEventListener() {
            @Override
            public void onDataChange(DataSnapshot dataSnapshot) {
                for (DataSnapshot userSnapshot : dataSnapshot.getChildren()) {
                    User user = userSnapshot.getValue(User.class);
                    String name = user.getName().toString();
                    String email = user.getEmail().toString();
                    tvResult.setText("이름: " + name + "\n" + "Email: " + email + " 으로 구독중입니다.");
                    etName.setText(name);
                    etEmail.setText(email);
                }
            }

            @Override
            public void onCancelled(DatabaseError databaseError) {
                //
                Log.d(TAG, "Failed to read value", databaseError.toException());
            }
        });

        //체크박스 클릭 이벤트 - 체크 체인지 리스너
        chSubscribe.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean isChecked) {
                if (isChecked) {
                    //
                    btSubscribe.setText("구독합니다.");
                } else {
                    btSubscribe.setText("해지합니다.");
                }
            }
        });

        //구독버튼 클릭 이벤트
        btSubscribe.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {

                if (etName.getText().toString().equals("")) {
                    Toast.makeText(getApplicationContext(), "이름을 입력해 주세요.", Toast.LENGTH_SHORT).show();
                    return;
                }
                if (etEmail.getText().toString().equals("")) {
                    Toast.makeText(getApplicationContext(), "메일을 입력해 주세요.", Toast.LENGTH_SHORT).show();
                    return;
                }


                if (chSubscribe.isChecked()) {
                    //구독신청합니다.
                    //FirebaseMessaging.getInstance().subscribeToTopic("news");
                    Log.d(TAG, "구독: Yes");

                    //기존 저장된 데이터가 있는지 없는지 체크해서 새로 만들든지 업데이트 한다.
                    //그런데 userIda말고 토큰으로 해야 할듯 한데... 일단 작동하니 이대로 하자.
                    if (TextUtils.isEmpty(userId)) {
                        createUser(etName.getText().toString(), etEmail.getText().toString(), myToken);
                    } else {
                        updateUser(etName.getText().toString(), etEmail.getText().toString(), myToken);
                    }

                } else {
                    //데이터베이스에서 완전 삭제하는 것으로 처리
                    deleteUser(myToken);
                    Log.d(TAG, "구독: Cancel");
                }
            }
        });


    }

    //streaming
    private void initializePlayer(String url) {
        if (player == null) {

            player = ExoPlayerFactory.newSimpleInstance(this.getApplicationContext());

            //플레이어 연결
            videoView.setPlayer(player);

        }

        MediaSource mediaSource = buildMediaSource(Uri.parse(url));

        //prepare
        player.prepare(mediaSource, true, false);

        //start,stop
        player.setPlayWhenReady(playWhenReady);

        player.addListener(new Player.EventListener() {
            @Override
            public void onPlayerError(ExoPlaybackException error) {
                player.prepare(mediaSource, false, true);
            }
        });
    }

    //streaming
    private MediaSource buildMediaSource(Uri uri) {

        String userAgent = Util.getUserAgent(this, "blackJin");

        if (uri.getLastPathSegment().contains("mp3") || uri.getLastPathSegment().contains("mp4")) {

            return new ExtractorMediaSource.Factory(new DefaultHttpDataSourceFactory(userAgent))
                    .createMediaSource(uri);

        } else if (uri.getLastPathSegment().contains("m3u8")) {

            //com.google.android.exoplayer:exoplayer-hls 확장 라이브러리를 빌드 해야 합니다.
            return new HlsMediaSource.Factory(new DefaultHttpDataSourceFactory(userAgent))
                    .createMediaSource(uri);

        } else {

            return new ExtractorMediaSource.Factory(new DefaultDataSourceFactory(this, userAgent))
                    .createMediaSource(uri);
        }

    }

    //streaming
    private void releasePlayer() {
        if (player != null) {
            playbackPosition = player.getCurrentPosition();
            currentWindow = player.getCurrentWindowIndex();
            playWhenReady = player.getPlayWhenReady();

            videoView.setPlayer(null);
            player.release();
            player = null;
        }
    }


    //"users"노드 아래에 새로운 유저를 만든다.
    private void createUser(String name, String email, String token) {
        //TODO 등록여부에 따라 분기
        ///토큰을 검색해서 같은 토큰이 없으면 새로운 User를 생성한다.
        //기존에 유저아이디가 없으면 생성한다.
        Query myQuery = myRef.orderByChild("token").equalTo(myToken);
        //어떻게 체크하지???
        if(TextUtils.isEmpty(userId)){
            userId = myRef.push().getKey();
        }
        User user = new User(name, email, token);
        myRef.child(userId).setValue(user);
    }

    //이미 userId가 있는 경우 값을 업데이트만 한다.
    private void updateUser(String name, String email, String token){
        if (!TextUtils.isEmpty(name)){
            myRef.child(userId).child("name").setValue(name);
        }
        if (!TextUtils.isEmpty(email)){
            myRef.child(userId).child("email").setValue(email);
        }
        if (!TextUtils.isEmpty(token)){
            myRef.child(userId).child("token").setValue(token);
        }
    }

    //삭제
    private void deleteUser(final String token){
        //데이터베이스에서 삭제하는 것으로 처리하자.
        Query myQuery = myRef.orderByChild("token").equalTo(token);
        myQuery.addListenerForSingleValueEvent(new ValueEventListener() {
            @Override
            public void onDataChange(DataSnapshot dataSnapshot) {
                for(DataSnapshot snapshot : dataSnapshot.getChildren()){
                    snapshot.getRef().removeValue();
                    Toast.makeText(getApplicationContext(), "삭제완료!", Toast.LENGTH_SHORT).show();
                    if(myToken!=null) {
                        tvResult.setText(myToken.substring(0, 100));
                    }
                }
            }

            @Override
            public void onCancelled(DatabaseError databaseError) {
                Log.d(TAG, "Failed to read value", databaseError.toException());
            }
        });
    }

}