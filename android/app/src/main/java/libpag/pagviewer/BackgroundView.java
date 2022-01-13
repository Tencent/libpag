package libpag.pagviewer;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RelativeLayout;

public class BackgroundView extends View {
    public BackgroundView(Context context) {
        super(context);
    }

    public BackgroundView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawColor(Color.WHITE);
        int width = getWidth();
        int height = getHeight();
        int tileSize = (int)(8 *  getContext().getResources().getDisplayMetrics().density);
        Path path = new Path();
        for (int y = 0; y < height; y += tileSize) {
            boolean draw = (y / tileSize) % 2 == 1;
            for (int x = 0; x < width; x += tileSize) {
                if (draw) {
                    path.addRect(x, y, x + tileSize, y + tileSize, Path.Direction.CCW);
                }
                draw = !draw;
            }
        }
        Paint paint = new Paint();
        paint.setColor(Color.argb(255, 204, 204, 204));
        canvas.drawPath(path, paint);
    }
}
