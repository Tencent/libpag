package org.libpag.reporter;

import java.util.HashMap;

public class PAGReporter {

    public interface PAGReporterListener {
        /**
         * Notifies PAG statistics report data. When external monitoring, PAG will not report again
         */
        void onReport(String event, String reportKey, HashMap<String, String> data);
    }

    private static PAGReporterListener mListener;
    private static final String APP_KEY = "0DOU0WKP4I47KH3I";
    private static final String REPORT_EVET = "pag_monitor";

    /**
     * Set up statistics report data monitoring
     * Once transferred to other application scenarios, you need to reset when returning
     */
    public static void SetReportListener(PAGReporterListener listener) {
        mListener = listener;
    }

    private static void OnReportData(HashMap<String, String> data) {
        if (data == null || data.isEmpty()) {
            return;
        }
        if (mListener != null) {
            mListener.onReport(REPORT_EVET, APP_KEY, data);
        } else {
            AVReportCenter.getInstance().commit(REPORT_EVET, APP_KEY, data);
        }

    }
}


