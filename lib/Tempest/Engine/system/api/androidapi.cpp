#include "androidapi.h"

#ifdef __ANDROID__

#include <Tempest/Event>
#include <Tempest/Window>
#include <Tempest/Log>

#include <android/configuration.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/window.h>
#include <android_native_app_glue.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace Tempest;

static android_app*     gApp         = nullptr;
static Tempest::Window* gOwner       = nullptr;
static std::atomic_bool gIsExit{false};
static bool             gWindowReady = false;
static int              gWndProxy    = 0;

// touch input state: OpenGothic-specific gesture handling
struct TouchPointer {
  bool active   = false;
  bool joystick = false;
  bool ui       = false;
  bool drag     = false;
  bool raw      = false;
  int  startX   = 0;
  int  startY   = 0;
  int  prevX    = 0;
  int  prevY    = 0;
  uint8_t keys  = 0;
  };
static TouchPointer gTouch[10];
static int32_t gDispW = 0;
static int32_t gDispH = 0;
static int32_t gBufW  = 0;
static int32_t gBufH  = 0;
static int32_t gRenderHeight = 720;
static int32_t gUiScale      = 100;
typedef void (*JoystickFn)(bool,int,int,int,int);
static JoystickFn gJoyCb    = nullptr;
static bool       gGameplay = false;

void AndroidApi::setRenderHeight(int32_t h) {
  gRenderHeight = h;
  }

void AndroidApi::setUiScale(int32_t percent) {
  if(percent<50)
    percent = 50;
  if(percent>200)
    percent = 200;
  gUiScale = percent;
  }

void AndroidApi::setJoystickCallback(void (*cb)(bool,int,int,int,int)) {
  gJoyCb = cb;
  }

void AndroidApi::setGameplayMode(bool gameplay) {
  gGameplay = gameplay;
  }

int32_t AndroidApi::renderHeightHint() {
  return gRenderHeight;
  }

// joystick zones: bit 1 = W, 2 = S, 4 = A, 8 = D
static uint8_t joyKeysOf(int dx, int dy, int thr) {
  uint8_t k = 0;
  if(dy < -thr) k = uint8_t(k|1);
  if(dy >  thr) k = uint8_t(k|2);
  if(dx < -thr) k = uint8_t(k|4);
  if(dx >  thr) k = uint8_t(k|8);
  return k;
  }

static void appCmdProxy(android_app* /*app*/, int32_t cmd) {
  AndroidApi::handleAppCmd(cmd);
  }

static int32_t inputProxy(android_app* /*app*/, AInputEvent* event) {
  return AndroidApi::handleInput(event);
  }

static void pumpAndroidEvents() {
  if(gApp==nullptr)
    return;
  while(true) {
    int events = 0;
    android_poll_source* source = nullptr;
    const int rc = ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source));
    if(rc<0)
      break;
    if(source!=nullptr)
      source->process(gApp, source);
    if(gApp->destroyRequested!=0) {
      gIsExit.store(true);
      break;
      }
    }
  }

void AndroidApi::setAndroidApp(void* app) {
  gApp = reinterpret_cast<android_app*>(app);
  gApp->onAppCmd     = appCmdProxy;
  gApp->onInputEvent = inputProxy;
  }

void* AndroidApi::nativeWindow() {
  return gApp!=nullptr ? gApp->window : nullptr;
  }

AndroidApi::AndroidApi() {
  }

void AndroidApi::handleAppCmd(int32_t cmd) {
  switch(cmd) {
    case APP_CMD_INIT_WINDOW:
      gWindowReady = (gApp!=nullptr && gApp->window!=nullptr);
      if(gApp!=nullptr && gApp->activity!=nullptr)
        ANativeActivity_setWindowFlags(gApp->activity, AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_FULLSCREEN, 0);
      if(gWindowReady) {
        const int32_t dw = ANativeWindow_getWidth (gApp->window);
        const int32_t dh = ANativeWindow_getHeight(gApp->window);
        if(dw>0 && dh>0) {
          gDispW = dw;
          gDispH = dh;
          const int32_t targetH = gRenderHeight;
          if(targetH>0 && dh>targetH) {
            gBufH = targetH;
            gBufW = int32_t((int64_t(dw)*targetH)/dh) & ~1;
            ANativeWindow_setBuffersGeometry(gApp->window, gBufW, gBufH, 0);
            } else {
            gBufW = dw;
            gBufH = dh;
            }
          }
        }
      dispatchResizeEvent();
      break;
    case APP_CMD_TERM_WINDOW:
      gWindowReady = false;
      break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
      dispatchResizeEvent();
      break;
    case APP_CMD_GAINED_FOCUS:
      dispatchFocusEvent(true);
      break;
    case APP_CMD_LOST_FOCUS:
      dispatchFocusEvent(false);
      break;
    case APP_CMD_DESTROY:
      gIsExit.store(true);
      break;
    default:
      break;
    }
  }

int32_t AndroidApi::handleInput(void* e) {
  if(gOwner==nullptr || e==nullptr)
    return 0;
  AInputEvent* event = reinterpret_cast<AInputEvent*>(e);
  const int32_t type = AInputEvent_getType(event);

  if(type==AINPUT_EVENT_TYPE_MOTION) {
    const int32_t action = AMotionEvent_getAction(event);
    const int32_t code   = action & AMOTION_EVENT_ACTION_MASK;
    const size_t  pIndex = size_t((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

    int32_t winW = 0, winH = 0;
    if(gApp!=nullptr && gApp->window!=nullptr) {
      winW = ANativeWindow_getWidth (gApp->window);
      winH = ANativeWindow_getHeight(gApp->window);
      }
    const int jThr   = (winH/12>48) ? winH/12 : 48;
    const int tapThr = (winH/40>24) ? winH/40 : 24;

    // remap touch coordinates into the (possibly downscaled) buffer space
    auto getX = [&](size_t i) {
      int x = int(AMotionEvent_getX(event, i));
      if(gDispW>0 && gBufW>0 && gBufW!=gDispW)
        x = int((int64_t(x)*gBufW)/gDispW);
      return x;
      };
    auto getY = [&](size_t i) {
      int y = int(AMotionEvent_getY(event, i));
      if(gDispH>0 && gBufH>0 && gBufH!=gDispH)
        y = int((int64_t(y)*gBufH)/gDispH);
      return y;
      };

    // ui hit area on the right edge: mirror TouchInput::buttonRect layout
    auto isUiArea = [&](int x, int y) {
      if(winW<=0 || winH<=0)
        return false;
      const int base = (winH/9>56) ? winH/9 : 56;
      const int size = int((int64_t(base)*gUiScale)/100);
      const int pad  = size/4;
      const bool rightCol = (x >= winW-(size+2*pad));
      const bool topRow   = (y <= size+2*pad && x >= winW-3*(size+pad)-pad);
      return rightCol || topRow;
      };

    auto applyJoyKeys = [&](TouchPointer& p, uint8_t keys) {
      static const struct { uint8_t bit; Event::KeyType key; } bind[] = {
        {1, Event::K_W}, {2, Event::K_S}, {4, Event::K_A}, {8, Event::K_D},
        };
      for(auto& b : bind) {
        const bool was = (p.keys & b.bit)!=0;
        const bool now = (keys   & b.bit)!=0;
        if(was==now)
          continue;
        KeyEvent ev(b.key, Event::M_NoModifier, now ? Event::KeyDown : Event::KeyUp);
        if(now)
          SystemApi::dispatchKeyDown(*gOwner, ev, uint32_t(b.key)); else
          SystemApi::dispatchKeyUp  (*gOwner, ev, uint32_t(b.key));
        }
      p.keys = keys;
      };

    switch(code) {
      case AMOTION_EVENT_ACTION_DOWN:
      case AMOTION_EVENT_ACTION_POINTER_DOWN: {
        const int x  = getX(pIndex);
        const int y  = getY(pIndex);
        const int id = int(AMotionEvent_getPointerId(event, pIndex));
        if(id<0 || id>=10)
          return 1;
        auto& p = gTouch[id];
        p.active   = true;
        p.raw      = !gGameplay;
        p.joystick = !p.raw && winW>0 && x<(winW*2)/5;
        p.ui       = !p.raw && !p.joystick && isUiArea(x, y);
        p.drag     = false;
        p.keys     = 0;
        p.startX   = x;
        p.startY   = y;
        p.prevX    = x;
        p.prevY    = y;
        if(p.joystick && gJoyCb!=nullptr)
          gJoyCb(true, x, y, 0, 0);
        if(p.ui || p.raw) {
          MouseEvent ev(x, y, Event::ButtonLeft, Event::M_NoModifier, 0, id, Event::MouseDown);
          SystemApi::dispatchMouseDown(*gOwner, ev);
          }
        return 1;
        }
      case AMOTION_EVENT_ACTION_MOVE: {
        const size_t cnt = AMotionEvent_getPointerCount(event);
        for(size_t i=0; i<cnt; ++i) {
          const int x  = getX(i);
          const int y  = getY(i);
          const int id = int(AMotionEvent_getPointerId(event, i));
          if(id<0 || id>=10)
            continue;
          auto& p = gTouch[id];
          if(!p.active)
            continue;
          if(p.raw) {
            MouseEvent ev(x, y, Event::ButtonNone, Event::M_NoModifier, 0, id, Event::MouseMove);
            SystemApi::dispatchMouseMove(*gOwner, ev);
            }
          else if(p.joystick) {
            applyJoyKeys(p, joyKeysOf(x-p.startX, y-p.startY, jThr));
            if(gJoyCb!=nullptr)
              gJoyCb(true, p.startX, p.startY, x-p.startX, y-p.startY);
            }
          else if(!p.ui) {
            const int dx = x-p.startX;
            const int dy = y-p.startY;
            if(!p.drag && dx*dx+dy*dy > tapThr*tapThr)
              p.drag = true;
            if(p.drag) {
              MouseEvent ev(winW/2+(x-p.prevX)*3, winH/2+(y-p.prevY)*3, Event::ButtonNone, Event::M_NoModifier, 0, id, Event::MouseMove);
              SystemApi::dispatchMouseMove(*gOwner, ev);
              }
            }
          p.prevX = x;
          p.prevY = y;
          }
        return 1;
        }
      case AMOTION_EVENT_ACTION_UP:
      case AMOTION_EVENT_ACTION_POINTER_UP:
      case AMOTION_EVENT_ACTION_CANCEL: {
        const int x  = getX(pIndex);
        const int y  = getY(pIndex);
        const int id = int(AMotionEvent_getPointerId(event, pIndex));
        if(id<0 || id>=10)
          return 1;
        auto& p = gTouch[id];
        if(p.raw) {
          MouseEvent ev(x, y, Event::ButtonLeft, Event::M_NoModifier, 0, id, Event::MouseUp);
          SystemApi::dispatchMouseUp(*gOwner, ev);
          }
        else if(p.joystick) {
          applyJoyKeys(p, 0);
          if(gJoyCb!=nullptr)
            gJoyCb(false, 0, 0, 0, 0);
          }
        else if(p.ui) {
          MouseEvent ev(x, y, Event::ButtonLeft, Event::M_NoModifier, 0, id, Event::MouseUp);
          SystemApi::dispatchMouseUp(*gOwner, ev);
          }
        else if(!p.drag && code!=AMOTION_EVENT_ACTION_CANCEL) {
          // tap: synthesize a click at the touch position
          MouseEvent evD(x, y, Event::ButtonLeft, Event::M_NoModifier, 0, id, Event::MouseDown);
          SystemApi::dispatchMouseDown(*gOwner, evD);
          MouseEvent evU(x, y, Event::ButtonLeft, Event::M_NoModifier, 0, id, Event::MouseUp);
          SystemApi::dispatchMouseUp(*gOwner, evU);
          }
        p.active = false;
        p.ui     = false;
        p.drag   = false;
        p.raw    = false;
        p.keys   = 0;
        return 1;
        }
      default:
        break;
      }
    return 0;
    }

  if(type==AINPUT_EVENT_TYPE_KEY) {
    const int32_t keyCode = AKeyEvent_getKeyCode(event);
    const int32_t action  = AKeyEvent_getAction(event);
    if(keyCode==AKEYCODE_BACK) {
      // map hardware/gesture "back" to Escape, to open in-game menu
      if(action==AKEY_EVENT_ACTION_DOWN) {
        KeyEvent ev(Event::K_ESCAPE, Event::M_NoModifier, Event::KeyDown);
        SystemApi::dispatchKeyDown(*gOwner, ev, uint32_t(keyCode));
        } else
      if(action==AKEY_EVENT_ACTION_UP) {
        KeyEvent ev(Event::K_ESCAPE, Event::M_NoModifier, Event::KeyUp);
        SystemApi::dispatchKeyUp(*gOwner, ev, uint32_t(keyCode));
        }
      return 1;
      }
    }
  return 0;
  }

void AndroidApi::dispatchResizeEvent() {
  if(gOwner==nullptr || gApp==nullptr || gApp->window==nullptr)
    return;
  const int32_t w = ANativeWindow_getWidth (gApp->window);
  const int32_t h = ANativeWindow_getHeight(gApp->window);
  if(w<=0 || h<=0)
    return;
  SizeEvent e(static_cast<int>(w), static_cast<int>(h));
  SystemApi::dispatchResize(*gOwner, e);
  }

void AndroidApi::dispatchFocusEvent(bool gain) {
  if(gOwner==nullptr)
    return;
  FocusEvent e(gain, Event::UnknownReason);
  SystemApi::dispatchFocus(*gOwner, e);
  }

void AndroidApi::waitForNativeWindow() {
  while(gApp!=nullptr && gApp->window==nullptr && !gIsExit.load()) {
    int events = 0;
    android_poll_source* source = nullptr;
    (void)ALooper_pollOnce(250, nullptr, &events, reinterpret_cast<void**>(&source));
    if(source!=nullptr)
      source->process(gApp, source);
    if(gApp->destroyRequested!=0) {
      gIsExit.store(true);
      break;
      }
    }
  gWindowReady = (gApp!=nullptr && gApp->window!=nullptr);
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, uint32_t /*width*/, uint32_t /*height*/) {
  gOwner = owner;
  waitForNativeWindow();
  return reinterpret_cast<SystemApi::Window*>(&gWndProxy);
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, ShowMode /*sm*/) {
  return implCreateWindow(owner, uint32_t(0), uint32_t(0));
  }

void AndroidApi::implDestroyWindow(Window* /*w*/) {
  gOwner = nullptr;
  }

void AndroidApi::implExit() {
  gIsExit.store(true);
  if(gApp!=nullptr && gApp->activity!=nullptr)
    ANativeActivity_finish(gApp->activity);
  }

Rect AndroidApi::implWindowClientRect(SystemApi::Window* /*w*/) {
  if(gApp!=nullptr && gApp->window!=nullptr) {
    const int32_t w = ANativeWindow_getWidth (gApp->window);
    const int32_t h = ANativeWindow_getHeight(gApp->window);
    return Rect(0, 0, int(w), int(h));
    }
  return Rect();
  }

bool AndroidApi::implSetAsFullscreen(SystemApi::Window* /*w*/, bool /*fullScreen*/) {
  return true;
  }

bool AndroidApi::implIsFullscreen(SystemApi::Window* /*w*/) {
  return true;
  }

float AndroidApi::implUiScale(SystemApi::Window* /*w*/) {
  if(gApp!=nullptr && gApp->config!=nullptr) {
    const int32_t d = AConfiguration_getDensity(gApp->config);
    if(d>0) {
      float s = float(d)/float(ACONFIGURATION_DENSITY_MEDIUM);
      if(s<1.f)
        s = 1.f;
      if(s>3.f)
        s = 3.f;
      return s;
      }
    }
  return 2.f;
  }

void AndroidApi::implSetWindowTitle(SystemApi::Window* /*w*/, const char* /*utf8*/) {
  }

void AndroidApi::implSetCursorPosition(SystemApi::Window* /*w*/, int /*x*/, int /*y*/) {
  }

void AndroidApi::implShowCursor(SystemApi::Window* /*w*/, CursorShape /*cursor*/) {
  }

bool AndroidApi::implIsRunning() {
  return !gIsExit.load();
  }

int AndroidApi::implExec(AppCallBack& cb) {
  while(!gIsExit.load()) {
    implProcessEvents(cb);
    }
  return 0;
  }

void AndroidApi::implProcessEvents(AppCallBack& cb) {
  pumpAndroidEvents();
  if(gIsExit.load())
    return;
  if(!gWindowReady || gApp==nullptr || gApp->window==nullptr) {
    // window is gone (app in background) - do not render
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return;
    }
  if(cb.onTimer()==0)
    std::this_thread::yield();
  if(gOwner!=nullptr)
    SystemApi::dispatchRender(*gOwner);
  }

#endif
