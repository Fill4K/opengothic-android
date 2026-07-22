#include "touchinput.h"

#include <Tempest/Painter>

#include <algorithm>
#include <cmath>

#include "game/playercontrol.h"
#include "resources.h"
#include "utils/gthfont.h"

using namespace Tempest;

TouchInput* TouchInput::instance         = nullptr;
int         TouchInput::uiScalePercent   = 100;
int         TouchInput::uiOpacityPercent = 30;
bool        TouchInput::joystickVisible  = true;

const TouchInput::Button TouchInput::buttons[TouchInput::buttonCount] = {
  {"MENU", Event::K_ESCAPE},
  {"INV",  Event::K_Tab},
  {"WPN",  Event::K_Space},
  {"JUMP", Event::K_LAlt},
  {"USE",  Event::K_LControl},
  {"LOG",  Event::K_N},
  {"STAT", Event::K_B},
  };

TouchInput::TouchInput(PlayerControl& ctrl) : ctrl(ctrl) {
  instance = this;
  }

void TouchInput::setKeyCallback(std::function<void(Tempest::Event::KeyType,bool)> cb) {
  keyCb = std::move(cb);
  }

void TouchInput::setUiConfig(int scalePercent, int opacityPercent, bool joyVisible) {
  uiScalePercent   = std::min(200, std::max(50, scalePercent));
  uiOpacityPercent = std::min(90,  std::max(5,  opacityPercent));
  joystickVisible  = joyVisible;
  }

void TouchInput::setGameplayMode(bool g) {
  if(gameplay==g)
    return;
  gameplay = g;
  if(!gameplay && pressed>=0) {
    // release a button that was held when a menu opened, to avoid a stuck key
    if(keyCb)
      keyCb(buttons[size_t(pressed)].key, false);
    pressed = -1;
    }
  update();
  }

void TouchInput::joystickState(bool active, int cx, int cy, int dx, int dy) {
  if(instance!=nullptr)
    instance->setJoystickState(active, cx, cy, dx, dy);
  }

void TouchInput::setJoystickState(bool active, int cx, int cy, int dx, int dy) {
  joyActive = active;
  joyCx = cx;
  joyCy = cy;
  joyDx = dx;
  joyDy = dy;
  update();
  }

Rect TouchInput::buttonRect(size_t id) const {
  const int base = std::max(56, h()/9);
  const int size = (base*uiScalePercent)/100;
  const int pad  = size/4;
  if(id==0 || id==5 || id==6) {
    // top-right row, right to left: MENU, LOG, STAT
    const int n = (id==0) ? 0 : int(id-4);
    return Rect(w()-(n+1)*(size+pad), pad, size, size);
    }
  // right column, bottom-up: USE, JUMP, WPN, INV
  const int slot = int(5-id);
  return Rect(w()-size-pad, h()-slot*(size+pad), size, size);
  }

void TouchInput::drawIcon(Painter& p, size_t id, const Rect& r) const {
  const int m  = r.w/4;
  const int x0 = r.x+m;
  const int y0 = r.y+m;
  const int x1 = r.x+r.w-m;
  const int y1 = r.y+r.h-m;
  const int cx = (x0+x1)/2;
  const int cy = (y0+y1)/2;
  const int t  = std::max(2, r.w/24);
  p.setPen  (Pen  (Color(1,1,1,0.85f), Painter::Alpha, float(t)));
  p.setBrush(Brush(Color(1,1,1,0.85f), Painter::Alpha));
  switch(id) {
    case 0: { // menu: three bars
      p.drawLine(x0, y0, x1, y0);
      p.drawLine(x0, cy, x1, cy);
      p.drawLine(x0, y1, x1, y1);
      break;
      }
    case 1: { // inventory: bag
      const int by = y0+(y1-y0)/3;
      p.drawLine(x0, by, x1, by);
      p.drawLine(x0, by, x0, y1);
      p.drawLine(x1, by, x1, y1);
      p.drawLine(x0, y1, x1, y1);
      p.drawLine(x0+(x1-x0)/4, by, cx, y0);
      p.drawLine(cx, y0, x1-(x1-x0)/4, by);
      break;
      }
    case 2: { // weapon: sword
      p.drawLine(x0, y1, x1, y0);
      const int gx = x0+(x1-x0)/4;
      const int gy = y1-(y1-y0)/4;
      const int g  = (x1-x0)/5;
      p.drawLine(gx-g, gy-g, gx+g, gy+g);
      break;
      }
    case 3: { // jump: up arrow
      p.drawLine(cx, y1, cx, y0);
      p.drawLine(cx, y0, x0, cy);
      p.drawLine(cx, y0, x1, cy);
      break;
      }
    case 4: { // use: door with knob
      p.drawLine(x0, y0, x1, y0);
      p.drawLine(x0, y1, x1, y1);
      p.drawLine(x0, y0, x0, y1);
      p.drawLine(x1, y0, x1, y1);
      p.drawRect(x1-(x1-x0)/4-t, cy-t, 2*t, 2*t);
      break;
      }
    case 5: { // log: book
      p.drawLine(x0, y0, x1, y0);
      p.drawLine(x0, y1, x1, y1);
      p.drawLine(x0, y0, x0, y1);
      p.drawLine(x1, y0, x1, y1);
      p.drawLine(cx, y0, cx, y1);
      break;
      }
    case 6: { // stats: bar chart
      const int bw = (x1-x0)/4;
      p.drawRect(x0,      cy,           bw, y1-cy);
      p.drawRect(cx-bw/2, y0+(y1-y0)/4, bw, y1-(y0+(y1-y0)/4));
      p.drawRect(x1-bw,   y0,           bw, y1-y0);
      break;
      }
    }
  }

void TouchInput::paintEvent(PaintEvent& e) {
  if(!gameplay)
    return;
  Painter p(e);

  // virtual joystick visual
  if(joystickVisible && joyActive) {
    const int R = std::max(48, h()/7);
    p.setPen(Pen(Color(1,1,1,0.35f), Painter::Alpha, 3.f));
    int px = joyCx+R;
    int py = joyCy;
    for(int s=1; s<=24; ++s) {
      const float a  = float(s)*6.2831853f/24.f;
      const int   nx = joyCx+int(std::cos(a)*float(R));
      const int   ny = joyCy+int(std::sin(a)*float(R));
      p.drawLine(px, py, nx, ny);
      px = nx;
      py = ny;
      }
    int dx = joyDx;
    int dy = joyDy;
    const float len = std::sqrt(float(dx*dx+dy*dy));
    if(len>float(R)) {
      dx = int(float(dx)*float(R)/len);
      dy = int(float(dy)*float(R)/len);
      }
    const int ks = std::max(16, R/2);
    p.setBrush(Brush(Color(1,1,1,0.30f), Painter::Alpha));
    p.drawRect(joyCx+dx-ks/2, joyCy+dy-ks/2, ks, ks);
    }

  const float fill = float(uiOpacityPercent)/100.f;
  auto& fnt = Resources::font(1.f);
  for(size_t i=0; i<buttonCount; ++i) {
    const Rect r = buttonRect(i);
    p.setBrush(Brush(Color(0,0,0, pressed==int(i) ? std::min(0.9f, fill+0.25f) : fill), Painter::Alpha));
    p.drawRect(r.x, r.y, r.w, r.h);
    p.setBrush(Brush(Color(1,1,1, std::min(0.9f, fill*0.85f)), Painter::Alpha));
    p.drawRect(r.x,       r.y,       r.w, 2);
    p.drawRect(r.x,       r.y+r.h-2, r.w, 2);
    p.drawRect(r.x,       r.y,       2,   r.h);
    p.drawRect(r.x+r.w-2, r.y,       2,   r.h);
    const auto sz = fnt.textSize(buttons[i].label);
    drawIcon(p, i, Rect(r.x, r.y, r.w, r.h-sz.h-4));
    fnt.drawText(p, r.x+(r.w-sz.w)/2, r.y+r.h-4, buttons[i].label);
    }
  }

void TouchInput::mouseDownEvent(MouseEvent& e) {
  if(!gameplay) {
    e.ignore();
    return;
    }
  for(size_t i=0; i<buttonCount; ++i) {
    if(buttonRect(i).contains(e.pos())) {
      pressed = int(i);
      if(keyCb)
        keyCb(buttons[i].key, true);
      update();
      return;
      }
    }
  e.ignore();
  }

void TouchInput::mouseDragEvent(MouseEvent& e) {
  e.ignore();
  }

void TouchInput::mouseUpEvent(MouseEvent& e) {
  if(!gameplay && pressed<0) {
    e.ignore();
    return;
    }
  int hit = -1;
  for(size_t i=0; i<buttonCount; ++i) {
    if(buttonRect(i).contains(e.pos())) {
      hit = int(i);
      break;
      }
    }
  if(hit<0)
    hit = pressed;
  if(hit>=0 && keyCb)
    keyCb(buttons[size_t(hit)].key, false);
  pressed = -1;
  update();
  }
