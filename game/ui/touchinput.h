#pragma once

#include <Tempest/Widget>
#include <Tempest/Event>

#include <functional>

class PlayerControl;

namespace Tempest {
class Painter;
}

class TouchInput : public Tempest::Widget {
  public:
    TouchInput(PlayerControl& ctrl);

    void setKeyCallback(std::function<void(Tempest::Event::KeyType,bool)> cb);

    // observer for the virtual joystick visual
    void setJoystickState(bool active, int cx, int cy, int dx, int dy);
    // bridge for AndroidApi: single instance lives in MainWindow
    static void joystickState(bool active, int cx, int cy, int dx, int dy);
    // set from android_main before ui is created
    static void setUiConfig(int scalePercent, int opacityPercent, bool joystickVisible);
    // called by MainWindow every frame: buttons only show during gameplay
    void setGameplayMode(bool gameplay);

  protected:
    void paintEvent    (Tempest::PaintEvent&  e) override;
    void mouseDownEvent(Tempest::MouseEvent&  e) override;
    void mouseDragEvent(Tempest::MouseEvent&  e) override;
    void mouseUpEvent  (Tempest::MouseEvent&  e) override;

  private:
    struct Button {
      const char*             label;
      Tempest::Event::KeyType key;
      };

    Tempest::Rect buttonRect(size_t id) const;
    void          drawIcon(Tempest::Painter& p, size_t id, const Tempest::Rect& r) const;

    PlayerControl& ctrl;
    std::function<void(Tempest::Event::KeyType,bool)> keyCb;
    int  pressed  = -1;
    bool gameplay = false;

    bool joyActive = false;
    int  joyCx = 0;
    int  joyCy = 0;
    int  joyDx = 0;
    int  joyDy = 0;

    static TouchInput* instance;
    static int         uiScalePercent;
    static int         uiOpacityPercent;
    static bool        joystickVisible;

    static const size_t buttonCount = 7;
    static const Button buttons[buttonCount];
  };
