package jp.atomneopixel.ble;

import android.Manifest;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class LauncherActivity extends Activity {
    private final Handler main = new Handler(Looper.getMainLooper());
    private final Runnable update = this::updateScreen;
    private TextView message;
    private Button startButton;
    private Button stopButton;
    private boolean openWhenReady;
    private boolean chromeMissing;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        if (saved != null) openWhenReady = saved.getBoolean("openWhenReady");
        LinearLayout screen = new LinearLayout(this);
        screen.setOrientation(LinearLayout.VERTICAL);
        screen.setFitsSystemWindows(true);
        int padding = Math.round(24 * getResources().getDisplayMetrics().density);
        screen.setPadding(padding, padding, padding, padding);

        TextView title = new TextView(this);
        title.setTextSize(26);
        title.setText("Atom NeoPixel");
        screen.addView(title);
        TextView help = new TextView(this);
        help.setTextSize(16);
        help.setText("Atom Lite を通常起動し、本体ボタンを約1.5秒押します。"
                + "内蔵 LED が水色になったら、Chrome の「BLE 接続」で機器を選びます。");
        screen.addView(help);

        startButton = new Button(this);
        startButton.setText("サーバーを起動して Chrome を開く");
        startButton.setAllCaps(false);
        startButton.setOnClickListener(this::start);
        screen.addView(startButton);

        stopButton = new Button(this);
        stopButton.setText("サーバーを停止");
        stopButton.setOnClickListener(this::stop);
        screen.addView(stopButton);
        message = new TextView(this);
        message.setTextSize(16);
        screen.addView(message);
        setContentView(screen);
    }

    private void start(View view) {
        chromeMissing = false;
        openWhenReady = true;
        LoopbackService.start(this);
        if (Build.VERSION.SDK_INT >= 33
                && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)
                    != PackageManager.PERMISSION_GRANTED
                && !getPreferences(MODE_PRIVATE).getBoolean("askedNotifications", false)) {
            getPreferences(MODE_PRIVATE).edit().putBoolean("askedNotifications", true).apply();
            requestPermissions(new String[] { Manifest.permission.POST_NOTIFICATIONS }, 1);
        }
    }

    private void stop(View view) {
        openWhenReady = false;
        chromeMissing = false;
        stopService(new Intent(this, LoopbackService.class));
    }

    private void updateScreen() {
        LoopbackService.Status status = LoopbackService.status();
        startButton.setEnabled(status != LoopbackService.Status.STARTING);
        stopButton.setEnabled(status == LoopbackService.Status.STARTING
                || status == LoopbackService.Status.RUNNING);
        switch (status) {
            case STARTING:
                message.setText("サーバーを起動中です。");
                break;
            case RUNNING:
                message.setText(chromeMissing ? "Google Chrome をインストールしてください。"
                        : "配信中  " + LoopbackServer.URL);
                if (openWhenReady) {
                    openWhenReady = false;
                    try {
                        startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(LoopbackServer.URL))
                                .setPackage("com.android.chrome"));
                    } catch (ActivityNotFoundException error) {
                        chromeMissing = true;
                    }
                }
                break;
            case PORT_BUSY:
                openWhenReady = false;
                message.setText("8080 番ポートを使用中のアプリを停止して再試行してください。");
                break;
            case FAILED:
                openWhenReady = false;
                message.setText("サーバーを起動できませんでした。");
                break;
            default:
                openWhenReady = false;
                message.setText("サーバーは停止中です。");
        }
        main.postDelayed(update, 200);
    }

    @Override
    protected void onResume() {
        super.onResume();
        main.post(update);
    }

    @Override
    protected void onPause() {
        main.removeCallbacks(update);
        super.onPause();
    }

    @Override
    protected void onSaveInstanceState(Bundle state) {
        state.putBoolean("openWhenReady", openWhenReady);
        super.onSaveInstanceState(state);
    }
}
