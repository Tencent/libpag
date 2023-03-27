package libpag.pagviewer;

import android.graphics.Matrix;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.Choreographer;
import android.view.MotionEvent;
import android.view.ViewGroup;

import org.libpag.PAGComposition;
import org.libpag.PAGFile;
import org.libpag.PAGImageView;
import org.libpag.PAGScaleMode;

/**
 * Created by liamcli on 2022/9/1.
 **/
public class PAGImageViewTestActivity extends AppCompatActivity implements Choreographer.FrameCallback {
    boolean cacheEnabled = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        cacheEnabled = getIntent().getBooleanExtra("closeCache", true);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_pagimageview);
        callFire();
//        Choreographer.getInstance().postFrameCallback(this);
    }

    void firepagView(PAGImageView view, String path) {
        PAGFile pagFile = PAGFile.Load(this.getAssets(), path.substring(9));
//        PAGComposition composition = PAGComposition.Make(pagFile.width(), pagFile.height());
//        composition.addLayer(pagFile);
//        view.setComposition(composition);

//        view.setPath(path);
//        view.setUrl("https://pag.art/pag/PAG_LOGO.pag");
        view.setRepeatCount(-1);
        Matrix matrix = new Matrix();
        matrix.setScale(0.2f, 0.2f);
        view.setMatrix(matrix);
        view.setScaleMode(PAGScaleMode.LetterBox);
//        view.setRenderScale(0.2f);
//        view.enableMemoryCache(true);
        view.play();
    }

    boolean hasFire;

    void callFire() {
        int ids[] = {R.id.pagView1, R.id.pagView2, R.id.pagView3, R.id.pagView4, R.id.pagView5,
                R.id.pagView6, R.id.pagView7, R.id.pagView8, R.id.pagView9, R.id.pagView10,
                R.id.pagView11, R.id.pagView12, R.id.pagView13, R.id.pagView14, R.id.pagView15,
                R.id.pagView16, R.id.pagView17, R.id.pagView18, R.id.pagView19, R.id.pagView20,
                R.id.pagView21, R.id.pagView22, R.id.pagView23};
        for (int i = 0; i < ids.length; i++) {
//            for (int i = 0; i < 1; i++) {
            firepagView((PAGImageView) findViewById(ids[i]), "assets://" + (i + 1) + ".pag");
        }

//        firepagView(this.<PAGImageView>findViewById(R.id.pagView23), "assets://alpha.pag");
//        PAGImageView imageView = new PAGImageView(this);
//        imageView.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
//                ViewGroup.LayoutParams.MATCH_PARENT));
//        firepagView(imageView, "assets://1.pag");
//        ((ViewGroup)findViewById(R.id.root)).removeAllViews();
//        ((ViewGroup)findViewById(R.id.root)).addView(imageView);
//        imageView.play();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_DOWN && !hasFire) {
            hasFire = true;
//            callFire();
            return super.dispatchTouchEvent(ev);
        }
        return super.dispatchTouchEvent(ev);
    }

    @Override
    public void doFrame(long l) {
//        Choreographer.getInstance().postFrameCallback(this);
//        Log.d("doFrame", "onAnimationUpdate: needsUpdateView  Start!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
        finish();
    }
}
