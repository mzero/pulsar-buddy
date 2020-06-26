#ifndef _INCLUDE_UI_LAYOUT_H_
#define _INCLUDE_UI_LAYOUT_H_

#include <initializer_list>

#include "ui_field.h"


class Layout : public Field {
public:
  Layout(const std::initializer_list<Field*>& f, int initialSelectedIndex = 0)
    : Field(0, 0, 128, 32), // FIXME: hack!! compute from field list
      fields(f), selectedIndex(initialSelectedIndex), focus(focusNone)
    { }

  bool render(bool refresh);
  void select(bool s);

  void enter(bool alternate);
  void exit();

  bool click(Button::State s);
  void update(Encoder::Update);

protected:
  const std::initializer_list<Field*>& fields;
  int selectedIndex;

  Field* selectedField() const;

  enum Focus { focusNone, focusNavigate, focusField };
  Focus focus;

  void redraw();
};

#endif // _INCLUDE_UI_LAYOUT_H_
