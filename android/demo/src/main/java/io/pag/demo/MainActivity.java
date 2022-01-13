package io.pag.demo;

import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.WindowManager;

public class MainActivity extends AppCompatActivity implements SimpleListAdapter.ItemClickListener {

    private static final String[] items = new String[]{
            "基础使用",
            "替换文字",
            "替换图片",
            "多个PAGFile在同一个Surface中使用",
            "纹理ID创建PAGSurface"
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_main);
        initView();
    }

    private void initView() {
        RecyclerView rv = findViewById(R.id.rv_);
        LinearLayoutManager linearLayoutManager = new LinearLayoutManager(this);
        linearLayoutManager.setOrientation(LinearLayoutManager.VERTICAL);
        rv.setLayoutManager(linearLayoutManager);
        rv.setAdapter(new SimpleListAdapter(items, this));
    }

    @Override
    public void onItemClick(int position) {
        switch (position) {
            case 0:
            case 1:
            case 2:
            case 3:
                goToAPIsDetail(position);
                break;
            case 4:
                goToTestDetail(position);
                break;

            default:
                break;
        }
    }

    private void goToAPIsDetail(int position) {
        Intent intent = new Intent(MainActivity.this, APIsDetailActivity.class);
        intent.putExtra("API_TYPE", position);
        startActivity(intent);
    }

    private void goToTestDetail(int position) {
        Intent intent = new Intent(MainActivity.this, TextureDemoActivity.class);
        intent.putExtra("API_TYPE", position);
        startActivity(intent);
    }
}
