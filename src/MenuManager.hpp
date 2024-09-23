#pragma once

#include <list>
#include <resource_file/ResourceFile.hh>

struct Menu {
  struct Item {
    std::string name;
    uint8_t icon_number;
    char key_equivalent;
    char mark_character; // In MacRoman; use decode_mac_roman if needed
    uint8_t style_flags; // See TextStyleFlag
    bool enabled;
    bool checked;

    Item() = default;
    ~Item() = default;

    Item(ResourceDASM::ResourceFile::DecodedMenu::Item& item)
        : name{item.name},
          icon_number{item.icon_number},
          key_equivalent{item.key_equivalent},
          mark_character{item.mark_character},
          style_flags{item.style_flags},
          enabled{item.enabled},
          checked{false} {}
  };

  int16_t menu_id;
  int16_t proc_id;
  std::string title;
  bool enabled;
  std::vector<Item> items;

  Menu() = default;
  ~Menu() = default;

  Menu(ResourceDASM::ResourceFile::DecodedMenu& decoded_menu)
      : menu_id{decoded_menu.menu_id},
        proc_id{decoded_menu.proc_id},
        title(decoded_menu.title),
        enabled(decoded_menu.enabled) {
    for (auto& item : decoded_menu.items) {
      // Convert DecodedMenu::Item list to Menu::Item list
      this->items.emplace_back(item);
    }
  }
};

struct MenuList {
  std::list<std::shared_ptr<Menu>> menus;
  std::list<std::shared_ptr<Menu>> submenus;
};