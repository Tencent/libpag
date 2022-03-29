package libpag.pagviewer;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.RelativeLayout;

import org.libpag.VideoDecoder;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.UnsupportedEncodingException;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {
    RelativeLayout containerView;
    Button btPlayFirst;
    Button btPlaySecond;

    PAGPlayerView firstPlayer = null;
    PAGPlayerView secondPlayer = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_main);
        containerView = (RelativeLayout) findViewById(R.id.container_view);
        btPlayFirst = (Button) findViewById(R.id.play_first);
        if (btPlayFirst == null) {
            return;
        }
        btPlayFirst.setOnClickListener(this);
        btPlaySecond = (Button) findViewById(R.id.play_second);
        if (btPlaySecond == null) {
            return;
        }
        btPlaySecond.setOnClickListener(this);

        firstPlayer = createPlayerView();
        secondPlayer = createPlayerView();

        activatedView(btPlayFirst.getId());
        if (firstPlayer != null) {
            play(firstPlayer, btPlayFirst.getText().toString());
        }
    }

    private PAGPlayerView createPlayerView() {
        PAGPlayerView pagView = new PAGPlayerView();
        pagView.setRepeatCount(-1);
        return pagView;
    }

    private void play(final PAGPlayerView player, String pagPath) {
        releasePagView();
        containerView.removeAllViews();
        BackgroundView backgroundView = new BackgroundView(this);
        backgroundView.setLayoutParams(new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        containerView.addView(backgroundView);

        View pagView = player.createView(this, pagPath);
        pagView.setOnTouchListener(new View.OnTouchListener() {
            boolean isPlay = true;

            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_UP:
                        if (isPlay) {
                            isPlay = false;
                            stopPlayPAG(player);
                        } else {
                            isPlay = true;
                            resumePlayPAG(player);
                        }
                        break;
                    default:
                        break;
                }
                return true;
            }
        });
        containerView.addView(pagView);
        player.play();
    }

    private void stopPlayPAG(PAGPlayerView player) {
        if (player != null) {
            player.stop();
        }
    }

    private void resumePlayPAG(PAGPlayerView player) {
        if (player != null) {
            player.play();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        releasePagView();
    }

    private void releasePagView() {
        if (firstPlayer != null) {
            firstPlayer.onRelease();
        }
        if(secondPlayer != null){
            secondPlayer.onRelease();
        }
    }

    public boolean CheckStoragePermissions(Activity activity) {
        // Check if we have write permission

        String[] PERMISSIONS_STORAGE = {
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE};

        int REQUEST_PERMISSION_CODE = 1;

        int checkStoragePermissions = ActivityCompat.checkSelfPermission(activity,
                Manifest.permission.ACCESS_FINE_LOCATION);
        if (checkStoragePermissions != PackageManager.PERMISSION_GRANTED) {
            // We don't have permission so prompt the user
            ActivityCompat.requestPermissions(activity, PERMISSIONS_STORAGE, REQUEST_PERMISSION_CODE);
            return false;
        } else {
            Log.i("test", "CheckStoragePermissions: Granted");
            return true;
        }
    }

    private void activatedView(int viewId) {
        switch (viewId) {
            case R.id.play_first:
                btPlayFirst.setActivated(true);
                btPlaySecond.setActivated(false);
                break;
            case R.id.play_second:
                btPlayFirst.setActivated(false);
                btPlaySecond.setActivated(true);
                break;
            default:
                break;
        }
    }

    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.play_second) {
            VideoDecoder.SetMaxHardwareDecoderCount(0);
            play(secondPlayer, btPlaySecond.getText().toString());
            activatedView(R.id.play_second);
        } else {
            VideoDecoder.SetMaxHardwareDecoderCount(15);
            play(firstPlayer, btPlayFirst.getText().toString());
            activatedView(R.id.play_first);
        }
    }

    public String readToString(String fileName) {
        File file = new File(fileName);
        Long filelength = file.length();
        byte[] filecontent = new byte[filelength.intValue()];
        try {
            FileInputStream in = new FileInputStream(file);
            in.read(filecontent);
            in.close();
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
        try {
            return new String(filecontent, "utf-8");
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
            return null;
        }
    }
}
