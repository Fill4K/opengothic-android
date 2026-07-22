package org.opengothic.gothic;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

public class LauncherActivity extends Activity {

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    refreshUi();
  }

  @Override
  protected void onResume() {
    super.onResume();
    refreshUi();
  }

  private void refreshUi() {
    if(!Environment.isExternalStorageManager()) {
      showPermissionScreen();
      return;
    }
    if(!gameDataPresent()) {
      showDataMissingScreen();
      return;
    }
    showSettings();
  }

  private boolean gameDataPresent() {
    return new File("/storage/emulated/0/Gothic/Data").isDirectory();
  }

  private String detectedGame() {
    String[] dirs = {
      "/storage/emulated/0/Gothic/_work/Data/Scripts/content/CUTSCENE/",
      "/storage/emulated/0/Gothic/_work/DATA/scripts/content/CUTSCENE/"
    };
    for(String d : dirs) {
      if(new File(d, "OU.DAT").exists())
        return "Gothic II: Night of the Raven";
    }
    for(String d : dirs) {
      if(new File(d, "OU.BIN").exists())
        return "Gothic";
    }
    return "unknown (Gothic II mode will be used)";
  }

  private int dp(int v) {
    return (int)(getResources().getDisplayMetrics().density * v);
  }

  private LinearLayout baseLayout() {
    LinearLayout l = new LinearLayout(this);
    l.setOrientation(LinearLayout.VERTICAL);
    l.setBackgroundColor(Color.rgb(24, 22, 20));
    int p = dp(24);
    l.setPadding(p, p, p, p);
    return l;
  }

  private ScrollView wrap(View content) {
    ScrollView sv = new ScrollView(this);
    sv.setBackgroundColor(Color.rgb(24, 22, 20));
    sv.addView(content);
    return sv;
  }

  private TextView title() {
    TextView t = new TextView(this);
    t.setText("OpenGothic");
    t.setTextSize(32);
    t.setTextColor(Color.rgb(230, 220, 200));
    t.setPadding(0, 0, 0, dp(12));
    return t;
  }

  private TextView text(String s) {
    TextView t = new TextView(this);
    t.setText(s);
    t.setTextSize(16);
    t.setTextColor(Color.rgb(210, 200, 185));
    t.setPadding(0, dp(4), 0, dp(4));
    return t;
  }

  private TextView sectionLabel(String s) {
    TextView t = new TextView(this);
    t.setText(s);
    t.setTextSize(18);
    t.setTextColor(Color.rgb(230, 220, 200));
    t.setPadding(0, dp(14), 0, dp(4));
    return t;
  }

  private RadioGroup radioRow(int idBase, String[] labels, int[] values, int current) {
    RadioGroup g = new RadioGroup(this);
    g.setOrientation(RadioGroup.HORIZONTAL);
    for(int i = 0; i < labels.length; ++i) {
      RadioButton rb = new RadioButton(this);
      rb.setId(idBase + i);
      rb.setText(labels[i]);
      rb.setTextColor(Color.rgb(210, 200, 185));
      rb.setChecked(values[i] == current);
      g.addView(rb);
    }
    return g;
  }

  private static int groupValue(RadioGroup g, int idBase, int[] values, int fallback) {
    int id = g.getCheckedRadioButtonId();
    if(id < idBase || id >= idBase + values.length)
      return fallback;
    return values[id - idBase];
  }

  private void showPermissionScreen() {
    LinearLayout root = baseLayout();
    root.addView(title());
    root.addView(text("OpenGothic needs access to your storage to read the game files from /storage/emulated/0/Gothic."));
    Button b = new Button(this);
    b.setText("Grant storage access");
    b.setOnClickListener(new View.OnClickListener() {
      @Override public void onClick(View v) {
        try {
          Intent it = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + getPackageName()));
          startActivity(it);
        } catch(Exception e) {
          startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
        }
      }
    });
    root.addView(b);
    setContentView(wrap(root));
  }

  private void showDataMissingScreen() {
    LinearLayout root = baseLayout();
    root.addView(title());
    root.addView(text("Game files were not found."));
    root.addView(text("Copy the folders from your PC installation of Gothic or Gothic II (Data, _work, system) into:"));
    TextView path = text("/storage/emulated/0/Gothic");
    path.setTextColor(Color.rgb(240, 230, 150));
    root.addView(path);
    root.addView(text("Then return to this screen."));
    setContentView(wrap(root));
  }

  private void showSettings() {
    SharedPreferences prefs = getSharedPreferences("launcher", MODE_PRIVATE);
    LinearLayout root = baseLayout();
    root.addView(title());
    root.addView(text("Detected game: " + detectedGame()));

    final int[] resValues = {720, 1080, 0};
    root.addView(sectionLabel("Resolution:"));
    final RadioGroup resGroup = new RadioGroup(this);
    resGroup.setOrientation(RadioGroup.VERTICAL);
    String[] resLabels = {"720p - performance (recommended)", "1080p - balanced", "Native - quality (may lag)"};
    int curRes = prefs.getInt("renderHeight", 720);
    for(int i = 0; i < resLabels.length; ++i) {
      RadioButton rb = new RadioButton(this);
      rb.setId(1000 + i);
      rb.setText(resLabels[i]);
      rb.setTextColor(Color.rgb(210, 200, 185));
      rb.setChecked(resValues[i] == curRes);
      resGroup.addView(rb);
    }
    root.addView(resGroup);

    final int[] scaleValues = {75, 100, 130};
    root.addView(sectionLabel("Button size:"));
    final RadioGroup scaleGroup = radioRow(2000, new String[]{"Small", "Normal", "Large"}, scaleValues, prefs.getInt("buttonScale", 100));
    root.addView(scaleGroup);

    final int[] opValues = {15, 30, 50};
    root.addView(sectionLabel("Button opacity:"));
    final RadioGroup opGroup = radioRow(3000, new String[]{"Subtle", "Normal", "Strong"}, opValues, prefs.getInt("buttonOpacity", 30));
    root.addView(opGroup);

    final CheckBox joy = new CheckBox(this);
    joy.setText("Show virtual joystick");
    joy.setTextColor(Color.rgb(210, 200, 185));
    joy.setChecked(prefs.getBoolean("joyVisible", true));
    joy.setPadding(0, dp(10), 0, dp(10));
    root.addView(joy);

    Button play = new Button(this);
    play.setText("PLAY");
    play.setTextSize(20);
    play.setOnClickListener(new View.OnClickListener() {
      @Override public void onClick(View v) {
        int h      = groupValue(resGroup, 1000, resValues, 720);
        int bScale = groupValue(scaleGroup, 2000, scaleValues, 100);
        int bOp    = groupValue(opGroup, 3000, opValues, 30);
        boolean jv = joy.isChecked();
        SharedPreferences.Editor ed = getSharedPreferences("launcher", MODE_PRIVATE).edit();
        ed.putInt("renderHeight", h);
        ed.putInt("buttonScale", bScale);
        ed.putInt("buttonOpacity", bOp);
        ed.putBoolean("joyVisible", jv);
        ed.apply();
        writeConfig(h, bScale, bOp, jv);
        startActivity(new Intent(LauncherActivity.this, GameActivity.class));
        finish();
      }
    });
    LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
    lp.topMargin = dp(16);
    root.addView(play, lp);

    setContentView(wrap(root));
  }

  private void writeConfig(int h, int bScale, int bOp, boolean jv) {
    try {
      File cfg = new File(getFilesDir(), "launcher.cfg");
      FileWriter w = new FileWriter(cfg);
      w.write("renderHeight=" + h + "\n");
      w.write("buttonScale=" + bScale + "\n");
      w.write("buttonOpacity=" + bOp + "\n");
      w.write("joystickVisible=" + (jv ? 1 : 0) + "\n");
      w.close();
    } catch(IOException e) {
      // if the config cannot be written, the game will use defaults
    }
  }
}
