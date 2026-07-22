#pragma once

#include <Tempest/AbstractTextInput>

namespace Tempest {

class TextEdit : public Tempest::AbstractTextInput {
  public:
    TextEdit();

    void  setText(const char* text) override;
    using AbstractTextInput::setText;
    using AbstractTextInput::text;

    using AbstractTextInput::setFont;
    using AbstractTextInput::font;

    using AbstractTextInput::setTextColor;
    using AbstractTextInput::textColor;
  };

}
