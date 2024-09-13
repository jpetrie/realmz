#import "MenuController.h"
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <cstddef>
#include <objc/NSObject.h>
#include <resource_file/ResourceFile.hh>

NSMenu* MCCreateMenu(const MenuList& menuList);
NSMenu* MCCreateSubMenu(NSString* title, const Menu& menuRes);

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
      NSMenu* subMenu = MCCreateSubMenu(title, *menu);
      [newMenu setSubmenu:subMenu forItem:menuItem];
    }
  }

  return newMenu;
}

NSMenu* MCCreateSubMenu(NSString* title, const Menu& menu) {
  NSMenu* subMenu = [[NSMenu alloc] initWithTitle:title];
  [subMenu setAutoenablesItems:NO];

  int i = 1;
  for (auto& subMenuItemRes : menu.items) {
    if (subMenuItemRes.name == "-") {
      [subMenu addItem:[NSMenuItem separatorItem]];
    } else {
      NSString* name = [NSString stringWithCString:subMenuItemRes.name.c_str() encoding:NSMacOSRomanStringEncoding];
      if (name != nullptr) {
        auto s = std::string(&subMenuItemRes.key_equivalent, 1);
        NSString* key = [NSString stringWithUTF8String:s.c_str()];
        NSMenuItem* subMenuItem = [subMenu addItemWithTitle:name action:NULL keyEquivalent:@""];
        subMenuItem.enabled = subMenuItemRes.enabled;
        if (subMenuItemRes.checked) {
          subMenuItem.state = NSControlStateValueOn;
        }
      }
    }
    i++;
  }

  return subMenu;
}