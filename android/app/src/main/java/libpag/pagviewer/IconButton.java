package libpag.pagviewer;

import android.content.Context;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.widget.Button;

public class IconButton extends Button {
    Rect bounds;

    public IconButton(Context context) {
        super(context);
        bounds = new Rect();
    }

    public IconButton(Context context, AttributeSet attrs) {
        super(context, attrs);
        bounds = new Rect();
    }

    public IconButton(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        bounds = new Rect();
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        Paint textPaint = getPaint();
        String text = getText().toString();
        textPaint.getTextBounds(text, 0, text.length(), bounds);

        int textWidth = bounds.width();
        int drawableWidth = (int) (24 * getContext().getResources().getDisplayMetrics().density);
        int iconPadding = 0;
        int contentWidth = drawableWidth + iconPadding + textWidth;
        int horizontalPadding = (int) ((getWidth() / 2.0) - (contentWidth / 2.0));
        setCompoundDrawablePadding(-horizontalPadding + iconPadding);
        setPadding(horizontalPadding, getPaddingTop(), 0, getPaddingBottom());
    }
}
