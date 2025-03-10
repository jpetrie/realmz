#pragma once

#include <SDL3_ttf/SDL_ttf.h>
#include <resource_file/BitmapFontRenderer.hh>
#include <variant>

#define BLACK_CHANCERY_FONT_ID 1602
#define GENEVA_FONT_ID 0
#define CHICAGO_FONT_ID 1

typedef std::variant<TTF_Font*, ResourceDASM::BitmapFontRenderer> Font;

void init_fonts();
Font load_font(int16_t font_id);
void set_font_face(TTF_Font* font, int16_t face);
std::string replace_param_text(const std::string& text);