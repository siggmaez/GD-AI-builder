package com.siggmaez.aibuilder.bridge;

import android.app.*;
import android.content.*;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import android.widget.*;
import org.json.JSONObject;

public final class MainActivity extends Activity {
    private final Handler handler = new Handler(Looper.getMainLooper());
    private TextView status, account, code, address, error;
    private String authUrl = "", authCode = "";
    private boolean openLoginWhenReady;
    private final Runnable refresh = new Runnable() { public void run() { render(); handler.postDelayed(this, 1000); } };

    private int dp(float value) { return Math.round(value * getResources().getDisplayMetrics().density); }
    private GradientDrawable background(int color) {
        var result = new GradientDrawable(); result.setColor(color); result.setCornerRadius(dp(16)); return result;
    }
    private TextView text(LinearLayout parent, String value, float size, int color) {
        var label = new TextView(this); label.setText(value); label.setTextSize(size); label.setTextColor(color);
        label.setPadding(0, dp(6), 0, dp(6)); parent.addView(label); return label;
    }
    private void button(LinearLayout parent, String title, boolean accent, View.OnClickListener callback) {
        var button = new Button(this); button.setText(title); button.setAllCaps(false);
        button.setTextColor(accent ? Color.rgb(17,20,29) : Color.WHITE);
        button.setBackground(background(accent ? Color.rgb(124,156,255) : Color.rgb(45,49,60)));
        var params = new LinearLayout.LayoutParams(-1, dp(50)); params.setMargins(0,dp(6),0,dp(6));
        parent.addView(button, params); button.setOnClickListener(callback);
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setStatusBarColor(Color.rgb(17,19,25)); getWindow().setNavigationBarColor(Color.rgb(17,19,25));
        var scroll = new ScrollView(this); scroll.setBackgroundColor(Color.rgb(17,19,25));
        var column = new LinearLayout(this); column.setOrientation(LinearLayout.VERTICAL); column.setPadding(dp(24),dp(20),dp(24),dp(32));
        scroll.addView(column); setContentView(scroll);
        var heading = text(column,"AI Builder Bridge",28,Color.WHITE); heading.setTypeface(null,Typeface.BOLD);
        text(column,"On this phone · No PC · No Termux",14,Color.rgb(160,167,185));
        status = text(column,"Bridge stopped",20,Color.rgb(124,156,255));
        address = text(column,"Bridge URL: http://127.0.0.1:8765",14,Color.WHITE);
        button(column,"Start bridge",true,v -> command(BridgeService.START));
        button(column,"Stop bridge",false,v -> command(BridgeService.STOP));
        text(column,"ChatGPT account",20,Color.WHITE);
        account = text(column,"Start the bridge, then sign in.",14,Color.LTGRAY);
        code = text(column,"",30,Color.WHITE); code.setTypeface(Typeface.MONOSPACE);
        button(column,"Sign in with ChatGPT",true,v -> {
            openLoginWhenReady=true;
            account.setText("Starting bridge and requesting a login code...");
            command(BridgeService.SIGN_IN);
        });
        button(column,"Copy code and open official sign-in",false,v -> {
            if (authCode.isEmpty()) { toast("No code yet. Press Sign in and check the status message."); return; }
            openOfficialLogin();
        });
        button(column,"Open ChatGPT security settings",false,v -> startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse("https://chatgpt.com/#settings/Security"))));
        button(column,"Copy bridge URL",false,v -> {
            ((ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("Bridge URL","http://127.0.0.1:8765"));
            toast("Copied. Paste into Geode > AI Builder > Bridge URL.");
        });
        button(column,"Return to Geometry Dash",true,v -> {
            Intent launch=getPackageManager().getLaunchIntentForPackage("com.geode.launcher");
            if (launch==null) launch=getPackageManager().getLaunchIntentForPackage("com.robtopx.geometryjump");
            if (launch!=null) startActivity(launch); else toast("Open Geometry Dash through your Geode launcher.");
        });
        button(column,"Android battery settings",false,v -> startActivity(new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS)));
        error = text(column,"",14,Color.rgb(255,145,145));
        text(column,"Keep the bridge notification active while playing. Android may stop the bridge if background work is restricted.\n\nOnly the official Codex account login is used. No API keys or browser cookies are requested.",13,Color.rgb(160,167,185));
        if (Build.VERSION.SDK_INT>=33 && checkSelfPermission("android.permission.POST_NOTIFICATIONS")!=getPackageManager().PERMISSION_GRANTED)
            requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"},1);
        handleIntent(getIntent());
    }
    private void command(String action) {
        try { startForegroundService(new Intent(this,BridgeService.class).setAction(action)); }
        catch (Exception exception) { toast("Android could not start the foreground bridge. Open this app and retry."); }
    }
    private void toast(String text) { Toast.makeText(this,text,Toast.LENGTH_LONG).show(); }
    private void openOfficialLogin() {
        if (!authUrl.startsWith("https://auth.openai.com/")) return;
        ((ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("Device code",authCode));
        try { startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse(authUrl))); }
        catch (ActivityNotFoundException exception) { toast("Install or enable a browser to open the official sign-in page."); }
    }
    private void render() {
        try {
            var data=new JSONObject(BridgeService.snapshot); boolean running=data.optBoolean("running");
            status.setText(running ? "Bridge online · on this phone" : "Bridge stopped");
            var auth=data.optJSONObject("account");
            if(auth!=null) {
                account.setText(auth.optString("message","Sign in with your ChatGPT account."));
                authCode=auth.optString("code","");authUrl=auth.optString("url","");code.setText(authCode);
                if (openLoginWhenReady && !authCode.isEmpty()) {
                    openLoginWhenReady=false;
                    openOfficialLogin();
                }
                if ("error".equals(auth.optString("state")) || "signed_in".equals(auth.optString("state"))) openLoginWhenReady=false;
            }
            error.setText(BridgeService.lastError);
        } catch(Exception ignored) { error.setText("Unable to read bridge status."); }
    }
    private void handleIntent(Intent intent) {
        Uri uri=intent.getData();
        if(uri!=null && "aibuilder".equals(uri.getScheme()) && "bridge".equals(uri.getHost()) && "/start".equals(uri.getPath())) command(BridgeService.START);
    }
    @Override public void onNewIntent(Intent intent) { super.onNewIntent(intent);setIntent(intent);handleIntent(intent); }
    @Override public void onResume() { super.onResume();handler.post(refresh); }
    @Override public void onPause() { handler.removeCallbacks(refresh);super.onPause(); }
}
