#include <Tempest/Platform>

#ifdef __ANDROID__

#include <android/log.h>
#include <android_native_app_glue.h>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "ui/touchinput.h"

#include "system/api/androidapi.h"

extern int mainImpl(int argc, const char** argv);

static bool fileExists(const char* path) {
  return access(path, F_OK)==0;
  }

// Gothic 1 ships OU.BIN, Gothic 2 ships OU.DAT; use that to pick the game mode
static const char* detectGameFlag() {
  if(fileExists("/sdcard/Gothic/_work/Data/Scripts/content/CUTSCENE/OU.DAT") ||
     fileExists("/sdcard/Gothic/_work/DATA/scripts/content/CUTSCENE/OU.DAT"))
    return "-g2";
  if(fileExists("/sdcard/Gothic/_work/Data/Scripts/content/CUTSCENE/OU.BIN") ||
     fileExists("/sdcard/Gothic/_work/DATA/scripts/content/CUTSCENE/OU.BIN"))
    return "-g1";
  // default: Gothic 2 Night of the Raven
  return "-g2";
  }

extern "C" void android_main(struct android_app* app) {
  Tempest::AndroidApi::setAndroidApp(app);

  if(app->activity!=nullptr && app->activity->internalDataPath!=nullptr) {
    if(chdir(app->activity->internalDataPath)!=0)
      __android_log_print(ANDROID_LOG_WARN, "OpenGothic", "unable to chdir to internal data path");
    }

  // launcher settings, written by LauncherActivity
  int renderHeight  = 720;
  int buttonScale   = 100;
  int buttonOpacity = 30;
  int joyVisible    = 1;
  if(FILE* f = fopen("launcher.cfg", "r")) {
    char line[160];
    while(fgets(line, sizeof(line), f)!=nullptr) {
      int v = 0;
      if(sscanf(line, "renderHeight=%d", &v)==1)
        renderHeight = v;
      if(sscanf(line, "buttonScale=%d", &v)==1)
        buttonScale = v;
      if(sscanf(line, "buttonOpacity=%d", &v)==1)
        buttonOpacity = v;
      if(sscanf(line, "joystickVisible=%d", &v)==1)
        joyVisible = v;
      }
    fclose(f);
    }
  Tempest::AndroidApi::setRenderHeight(renderHeight);
  Tempest::AndroidApi::setUiScale(buttonScale);
  TouchInput::setUiConfig(buttonScale, buttonOpacity, joyVisible!=0);
  __android_log_print(ANDROID_LOG_INFO, "OpenGothic", "launcher cfg: renderHeight=%d buttonScale=%d buttonOpacity=%d joystick=%d", renderHeight, buttonScale, buttonOpacity, joyVisible);

  // -rt 0 / -gi 0: mobile Adreno RT drivers are unstable; disable ray-query by default
  const char* gameFlag = detectGameFlag();
  __android_log_print(ANDROID_LOG_INFO, "OpenGothic", "starting in %s mode", gameFlag);
  const char* argv[] = { "Gothic2Notr", "-g", "/sdcard/Gothic", gameFlag, "-rt", "0", "-gi", "0", nullptr };
  const int ret = mainImpl(8, argv);
  __android_log_print(ANDROID_LOG_INFO, "OpenGothic", "game exited with code %d", ret);

  ANativeActivity_finish(app->activity);
  // make sure the process fully restarts on the next launch
  std::exit(0);
  }

#endif
