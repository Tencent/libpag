package org.libpag.reporter;

import android.util.Log;

import com.tencent.beacon.event.open.BeaconEvent;
import com.tencent.beacon.event.open.BeaconReport;
import com.tencent.beacon.event.open.EventResult;

import org.extra.tools.LibraryLoadUtils;

import java.util.Map;

public class AVReportCenter {
    private static final String TAG = "AVReportCenter" + "-" + Integer.toHexString(AVReportCenter.class.hashCode());

    private static boolean beaconEnable = false;
    private static final AVReportCenter ourInstance = new AVReportCenter();
    private boolean enable = true;

    private AVReportCenter() {
        init();
    }

    public static AVReportCenter getInstance() {
        return ourInstance;
    }

    public void init() {
        if (LibraryLoadUtils.getAppContext() != null) {
            try {
                Class.forName("com.tencent.beacon.event.open.BeaconReport");
                beaconEnable = true;
            } catch (ClassNotFoundException e) {
                Log.e(TAG, "init: " + e);
            }
        }
    }

    public void commit(String reportEvent, String reportKey, final Map<String, String> data) {
        if (!beaconEnable || !enable || data == null) {
            return;
        }
        // 不同版本的接口可能不一样，加上 try catch，保护一下
        try {
            BeaconEvent event = BeaconEvent.builder().withAppKey(reportKey).withCode(reportEvent).withParams(data).build();
            EventResult result = BeaconReport.getInstance().report(event);
            if (!result.isSuccess()) {
                Log.e(TAG, result.errMsg);
            }
        } catch (Exception e) {
            beaconEnable = false;
            Log.e(TAG, "report: " + e);
        }
    }

    public boolean isEnable() {
        return enable;
    }

    public void setEnable(boolean enable) {
        this.enable = enable;
    }
}
