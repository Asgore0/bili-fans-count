package com.example.bupfans;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;

final class LiquidGlassDrawable extends Drawable {
    private final boolean dark;
    private final float radius;
    private final float density;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint strokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint wavePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect = new RectF();
    private final Path clipPath = new Path();
    private final Path wavePath = new Path();
    private int masterAlpha = 255;

    LiquidGlassDrawable(int radius, boolean dark, float density) {
        this.radius = radius;
        this.dark = dark;
        this.density = density;
        strokePaint.setStyle(Paint.Style.STROKE);
        strokePaint.setStrokeCap(Paint.Cap.ROUND);
        wavePaint.setStyle(Paint.Style.STROKE);
        wavePaint.setStrokeCap(Paint.Cap.ROUND);
        wavePaint.setStrokeJoin(Paint.Join.ROUND);
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();
        if (bounds.isEmpty()) {
            return;
        }

        rect.set(bounds.left, bounds.top, bounds.right, bounds.bottom);
        float width = rect.width();
        float height = rect.height();

        clipPath.reset();
        clipPath.addRoundRect(rect, radius, radius, Path.Direction.CW);
        int save = canvas.save();
        canvas.clipPath(clipPath);

        drawBody(canvas, width, height);
        drawRefractionFields(canvas, width, height);
        drawCausticWaves(canvas, width, height);

        canvas.restoreToCount(save);
        drawRim(canvas, width, height);
    }

    private void drawBody(Canvas canvas, float width, float height) {
        int[] colors = dark
                ? new int[]{
                        color(34, 20, 32, 46),
                        color(16, 19, 52, 68),
                        color(17, 64, 38, 68)
                }
                : new int[]{
                        color(42, 255, 255, 255),
                        color(14, 236, 253, 255),
                        color(12, 255, 235, 228)
                };
        paint.setShader(new LinearGradient(
                rect.left,
                rect.top,
                rect.right,
                rect.bottom,
                colors,
                null,
                Shader.TileMode.CLAMP
        ));
        paint.setStyle(Paint.Style.FILL);
        canvas.drawRoundRect(rect, radius, radius, paint);

        paint.setShader(new LinearGradient(
                rect.left,
                rect.top,
                rect.left,
                rect.top + height * 0.58f,
                new int[]{
                        dark ? color(13, 255, 255, 255) : color(18, 255, 255, 255),
                        dark ? color(4, 255, 255, 255) : color(5, 255, 255, 255),
                        Color.TRANSPARENT
                },
                new float[]{0f, 0.42f, 1f},
                Shader.TileMode.CLAMP
        ));
        RectF topPane = new RectF(rect.left, rect.top, rect.right, rect.top + height * 0.36f);
        canvas.drawRoundRect(topPane, radius, radius, paint);
        paint.setShader(null);
    }

    private void drawRefractionFields(Canvas canvas, float width, float height) {
        drawRadial(canvas,
                rect.left + width * 0.18f,
                rect.top + height * 0.12f,
                width * 0.54f,
                dark ? color(9, 255, 255, 255) : color(11, 255, 255, 255));
        drawRadial(canvas,
                rect.left + width * 0.82f,
                rect.top + height * 0.82f,
                width * 0.48f,
                dark ? color(8, 53, 221, 255) : color(6, 47, 221, 255));
        drawRadial(canvas,
                rect.left + width * 0.76f,
                rect.top + height * 0.16f,
                width * 0.34f,
                dark ? color(6, 255, 118, 164) : color(5, 255, 118, 164));
    }

    private void drawRadial(Canvas canvas, float cx, float cy, float radius, int centerColor) {
        paint.setShader(new RadialGradient(
                cx,
                cy,
                radius,
                new int[]{centerColor, Color.TRANSPARENT},
                new float[]{0f, 1f},
                Shader.TileMode.CLAMP
        ));
        paint.setStyle(Paint.Style.FILL);
        canvas.drawCircle(cx, cy, radius, paint);
        paint.setShader(null);
    }

    private void drawCausticWaves(Canvas canvas, float width, float height) {
        wavePaint.setStrokeWidth(dp(0.85f));
        wavePaint.setColor(dark ? color(28, 186, 238, 255) : color(24, 255, 255, 255));
        wavePath.reset();
        wavePath.moveTo(rect.left + width * 0.06f, rect.top + height * 0.26f);
        wavePath.cubicTo(
                rect.left + width * 0.28f, rect.top + height * 0.12f,
                rect.left + width * 0.45f, rect.top + height * 0.34f,
                rect.left + width * 0.66f, rect.top + height * 0.22f
        );
        wavePath.cubicTo(
                rect.left + width * 0.78f, rect.top + height * 0.15f,
                rect.left + width * 0.91f, rect.top + height * 0.20f,
                rect.left + width * 0.98f, rect.top + height * 0.16f
        );
        canvas.drawPath(wavePath, wavePaint);

        wavePaint.setStrokeWidth(dp(0.65f));
        wavePaint.setColor(dark ? color(13, 255, 118, 164) : color(10, 47, 221, 255));
        wavePath.reset();
        wavePath.moveTo(rect.left + width * 0.10f, rect.top + height * 0.68f);
        wavePath.cubicTo(
                rect.left + width * 0.30f, rect.top + height * 0.76f,
                rect.left + width * 0.52f, rect.top + height * 0.58f,
                rect.left + width * 0.74f, rect.top + height * 0.66f
        );
        wavePath.cubicTo(
                rect.left + width * 0.84f, rect.top + height * 0.70f,
                rect.left + width * 0.91f, rect.top + height * 0.64f,
                rect.left + width * 0.96f, rect.top + height * 0.68f
        );
        canvas.drawPath(wavePath, wavePaint);
    }

    private void drawRim(Canvas canvas, float width, float height) {
        strokePaint.setStrokeWidth(dp(0.75f));
        strokePaint.setColor(dark ? color(44, 186, 238, 255) : color(48, 255, 255, 255));
        RectF outer = new RectF(rect);
        outer.inset(dp(0.5f), dp(0.5f));
        canvas.drawRoundRect(outer, radius, radius, strokePaint);

        strokePaint.setStrokeWidth(dp(0.65f));
        strokePaint.setColor(dark ? color(20, 255, 255, 255) : color(26, 255, 255, 255));
        RectF inner = new RectF(rect);
        inner.inset(dp(3f), dp(3f));
        canvas.drawArc(inner, 196, 120, false, strokePaint);
        canvas.drawArc(inner, 292, 42, false, strokePaint);

        strokePaint.setStrokeWidth(dp(0.55f));
        strokePaint.setColor(dark ? color(18, 255, 118, 164) : color(14, 47, 221, 255));
        RectF prism = new RectF(rect);
        prism.inset(dp(2f), dp(2f));
        canvas.drawArc(prism, 38, 52, false, strokePaint);
        canvas.drawArc(prism, 224, 44, false, strokePaint);
    }

    @Override
    public void setAlpha(int alpha) {
        masterAlpha = alpha;
        invalidateSelf();
    }

    @Override
    public void setColorFilter(ColorFilter colorFilter) {
        paint.setColorFilter(colorFilter);
        strokePaint.setColorFilter(colorFilter);
        wavePaint.setColorFilter(colorFilter);
    }

    @Override
    @SuppressWarnings("deprecation")
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    private int color(int alpha, int red, int green, int blue) {
        return Color.argb(alpha * masterAlpha / 255, red, green, blue);
    }

    private float dp(float value) {
        return value * density;
    }
}
