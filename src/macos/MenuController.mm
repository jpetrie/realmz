#import "MenuController.h"
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <cstddef>
#include <objc/NSObject.h>
#include <resource_file/ResourceFile.hh>

NSMenu* MCCreateMenu(const MenuList& menuList);
NSMenu* MCCreateSubMenu(NSString* title, const Menu& menuRes, const std::list<std::shared_ptr<Menu>> submenus);

void MCSync(std::shared_ptr<MenuList> menuList) {
  NSApplication* application = [NSApplication sharedApplication];

  id newMenu = MCCreateMenu(*menuList);

  application.mainMenu = newMenu;
}

NSMenu* MCCreateMenu(const MenuList& menuList) {
  NSMenu* newMenu = [[NSMenu alloc] initWithTitle:@"Realmz"];
  [newMenu setAutoenablesItems:NO];

  for (auto menu : menuList.menus) {
    NSString* title = [NSString stringWithUTF8String:menu->title.c_str()];
    NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:title action:NULL keyEquivalent:@""];
    menuItem.enabled = menu->enabled;
    [newMenu addItem:menuItem];
    if (menu->items.size()) {
      NSMenu* subMenu = MCCreateSubMenu(title, *menu, menuList.submenus);
      [newMenu setSubmenu:subMenu forItem:menuItem];
    }
  }

  return newMenu;
}

NSMenu* MCCreateSubMenu(NSString* title, const Menu& menu, const std::list<std::shared_ptr<Menu>> submenus) {
  NSMenu* newMenu = [[NSMenu alloc] initWithTitle:title];
  [newMenu setAutoenablesItems:NO];

  int i = 1;
  for (auto& subMenuItemRes : menu.items) {
    if (subMenuItemRes.name == "-") {
      [newMenu addItem:[NSMenuItem separatorItem]];
    } else {
      NSString* name = [NSString stringWithCString:subMenuItemRes.name.c_str() encoding:NSMacOSRomanStringEncoding];
      if (name != nullptr) {
        auto s = std::string(&subMenuItemRes.key_equivalent, 1);
        NSString* key = [NSString stringWithUTF8String:s.c_str()];
        NSMenuItem* subMenuItem = [newMenu addItemWithTitle:name action:NULL keyEquivalent:@""];
        subMenuItem.enabled = subMenuItemRes.enabled;
        if (subMenuItemRes.checked) {
          subMenuItem.state = NSControlStateValueOn;
        }

        // Submenu ids are specified by the itemMark field if the itemCmd field has
        // the value 0x1B
        // Macintosh Toolbox Essentials (3-96)
        if (subMenuItemRes.key_equivalent == 0x1B && subMenuItemRes.mark_character) {
          for (auto subMenuRes : submenus) {
            if (subMenuRes->menu_id == static_cast<uint8_t>(subMenuItemRes.mark_character)) {
              NSString* subMenuTitle = [NSString stringWithCString:subMenuRes->title.c_str() encoding:NSMacOSRomanStringEncoding];
              NSMenu* subMenu = MCCreateSubMenu(subMenuTitle, *subMenuRes, submenus);
              [newMenu setSubmenu:subMenu forItem:subMenuItem];

              break;
            }
          }
        }
      }
    }
    i++;
  }

  return newMenu;
}