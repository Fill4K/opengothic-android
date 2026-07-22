#pragma once

#include <Tempest/SystemApi>

#ifdef __ANDROID__

namespace Tempest {

class AndroidApi final: SystemApi {
  public:
    // called from android_main, before Application object is created
    static void    setAndroidApp(void* app);
    static void*   nativeWindow();

    // native_app_glue callbacks forward here
    static void    handleAppCmd(int32_t cmd);
    static int32_t handleInput (void* event);

    // called from android_main: preferred internal rendering height in pixels (0 = native)
    static void    setRenderHeight(int32_t h);
    // ui scale percent for the on-screen button hit area (default 100)
    static void    setUiScale(int32_t percent);
    // observer for the virtual joystick: (active, centerX, centerY, dx, dy) in buffer coords
    static void    setJoystickCallback(void (*cb)(bool,int,int,int,int));
    // when false (menus, dialogs), touches are dispatched as plain mouse clicks
    static void    setGameplayMode(bool gameplay);
    // preferred internal rendering height in pixels (0 = native)
    static int32_t renderHeightHint();

  private:
    AndroidApi();

    Window*  implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height) override;
    Window*  implCreateWindow(Tempest::Window *owner, ShowMode sm) override;
    void     implDestroyWindow(Window* w) override;
    void     implExit() override;

    Rect     implWindowClientRect(SystemApi::Window *w) override;

    bool     implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) override;
    bool     implIsFullscreen(SystemApi::Window *w) override;
    float    implUiScale(SystemApi::Window* w) override;

    void     implSetWindowTitle(SystemApi::Window *w, const char* utf8) override;

    void     implSetCursorPosition(SystemApi::Window *w, int x, int y) override;
    void     implShowCursor(SystemApi::Window *w, CursorShape cursor) override;

    bool     implIsRunning() override;
    int      implExec(AppCallBack& cb) override;
    void     implProcessEvents(AppCallBack& cb) override;

    static void dispatchResizeEvent();
    static void dispatchFocusEvent(bool gain);
    static void waitForNativeWindow();

  friend class SystemApi;
  };

}

#endif
