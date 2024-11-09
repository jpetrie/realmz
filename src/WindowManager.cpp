#include "WindowManager.h"

#include "FileManager.hpp"
#include "StringConvert.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <phosg/Strings.hh>
#include <resource_file/ResourceFile.hh>

#include "MemoryManager.hpp"
#include "QuickDraw.hpp"

using ResourceDASM::ResourceFile;

static phosg::PrefixedLogger wm_log("[WindowManager] ");
static std::unordered_map<int16_t, TTF_Font*> fonts_by_id;

using DialogItemType = ResourceDASM::ResourceFile::DecodedDialogItem::Type;

int16_t macos_dialog_item_type_for_resource_dasm_type(DialogItemType type) {
  switch (type) {
    case DialogItemType::BUTTON:
      return 4;
    case DialogItemType::CHECKBOX:
      return 5;
    case DialogItemType::RADIO_BUTTON:
      return 6;
    case DialogItemType::RESOURCE_CONTROL:
      return 7;
    case DialogItemType::TEXT:
      return 8;
    case DialogItemType::EDIT_TEXT:
      return 16;
    case DialogItemType::ICON:
      return 32;
    case DialogItemType::PICTURE:
      return 64;
    case DialogItemType::CUSTOM:
      return 0;
    default:
      throw std::logic_error("Unknown dialog item type");
  }
}

// This structure is "private" (not accessible in C) because it isn't directly
// used there: Realmz only interacts with dialog items through syscalls and
// handles, so we can use C++ types here without breaking anything.
struct DialogItem {
  int16_t ditl_resource_id;
  size_t item_id;
  DialogItemType type;
  std::string text;
  int16_t resource_id;
  Rect rect;
  bool enabled;

  DialogItem(int16_t ditl_res_id, size_t item_id, const ResourceDASM::ResourceFile::DecodedDialogItem& def)
      : ditl_resource_id(ditl_res_id),
        item_id(item_id),
        type(def.type),
        text(def.text),
        resource_id(def.resource_id),
        enabled(def.enabled) {
    this->rect.left = def.bounds.x1;
    this->rect.right = def.bounds.x2;
    this->rect.top = def.bounds.y1;
    this->rect.bottom = def.bounds.y2;
  }

  static std::vector<DialogItem> from_DITL(int16_t ditl_resource_id) {
    auto data_handle = GetResource(ResourceDASM::RESOURCE_TYPE_DITL, ditl_resource_id);
    auto defs = ResourceDASM::ResourceFile::decode_DITL(*data_handle, GetHandleSize(data_handle));

    std::vector<DialogItem> ret;
    for (const auto& def : defs) {
      size_t item_id = ret.size() + 1;
      ret.emplace_back(ditl_resource_id, item_id, def);
    }
    return ret;
  }
};

class WindowManager {
public:
  class Window {
  public:
    Window() = default;
    Window(std::string title, const Rect& bounds, SDL_WindowFlags flags, std::shared_ptr<std::vector<DialogItem>> dialog_items)
        : title{title},
          bounds{bounds},
          flags{flags},
          dialog_items{dialog_items} {
      w = bounds.right - bounds.left;
      h = bounds.bottom - bounds.top;
      sdlWindow = SDL_CreateWindow(title.c_str(), w, h, flags);

      if (sdlWindow == NULL) {
        throw std::runtime_error(phosg::string_printf("Could not create window: %s\n", SDL_GetError()));
      }

      sdlRenderer = SDL_CreateRenderer(sdlWindow, "opengl");

      if (sdlRenderer == NULL) {
        throw std::runtime_error(phosg::string_printf("Could not create window renderer: %s\n", SDL_GetError()));
      }

      // We'll use this texture as our own backbuffer, see
      // https://stackoverflow.com/questions/63759688/sdl-renderpresent-implementation
      // The SDL wiki also shows this technique of drawing to an in-memory texture buffer
      // before rendering to the backbuffer: https://wiki.libsdl.org/SDL3/SDL_CreateTexture
      sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32,
          SDL_TEXTUREACCESS_TARGET, w, h);

      if (sdlTexture == NULL) {
        throw std::runtime_error(phosg::string_printf("Could not create window texture: %s\n", SDL_GetError()));
      }

      // Default to rendering all draw calls to the intermediate texture buffer
      SDL_SetRenderTarget(sdlRenderer, sdlTexture);
      SDL_RenderClear(sdlRenderer);
    }

    ~Window() {
      SDL_DestroyTexture(this->sdlTexture);
      SDL_DestroyRenderer(this->sdlRenderer);
      SDL_DestroyWindow(this->sdlWindow);
    }

    void sync(void) {
      SDL_SetRenderTarget(sdlRenderer, NULL);
      SDL_RenderTexture(sdlRenderer, sdlTexture, NULL, NULL);
      SDL_RenderPresent(sdlRenderer);
      SDL_SetRenderTarget(sdlRenderer, sdlTexture);
      SDL_SyncWindow(sdlWindow);
    }

    void draw_rgba_picture(void* pixels, int w, int h, const Rect& dispRect) {
      SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, pixels, 4 * w);

      if (s == NULL) {
        wm_log.error("Could not create surface: %s\n", SDL_GetError());
        return;
      }

      SDL_Texture* t = SDL_CreateTextureFromSurface(sdlRenderer, s);
      if (t == NULL) {
        wm_log.error("Could not create texture: %s\n", SDL_GetError());
        return;
      }
      SDL_DestroySurface(s);

      SDL_FRect dest;
      dest.x = dispRect.left;
      dest.y = dispRect.top;
      dest.w = dispRect.right - dispRect.left;
      dest.h = dispRect.bottom - dispRect.top;

      SDL_RenderTexture(sdlRenderer, t, NULL, &dest);
    }

    bool draw_text(const std::string& text, const Rect& dispRect) {
      // TTF_Font* font = fonts_by_id.at(port->txFont);
      TTF_Font* font = fonts_by_id.at(1601);
      RGBColor fore_color;
      GetForeColor(&fore_color);
      SDL_Surface* text_surface = TTF_RenderText_Blended_Wrapped(
          font,
          text.data(),
          text.size(),
          SDL_Color{
              static_cast<uint8_t>(fore_color.red / 0x0101),
              static_cast<uint8_t>(fore_color.green / 0x0101),
              static_cast<uint8_t>(fore_color.blue / 0x0101),
              255},
          dispRect.right - dispRect.left);
      if (!text_surface) {
        return false;
      }
      SDL_FRect text_dest;
      text_dest.x = dispRect.left;
      text_dest.y = dispRect.top;
      text_dest.w = std::min(dispRect.right - dispRect.left, text_surface->w);
      text_dest.h = std::min(dispRect.bottom - dispRect.top, text_surface->h);
      SDL_Texture* text_texture = SDL_CreateTextureFromSurface(sdlRenderer, text_surface);
      if (!text_texture || !SDL_RenderTexture(sdlRenderer, text_texture, NULL, &text_dest)) {
        return false;
      }
      return true;
    }

    void draw_background(PixPatHandle bkPixPat) {
      PixMapHandle pmap = (*bkPixPat)->patMap;
      Rect bounds = (*pmap)->bounds;
      int w = bounds.right - bounds.left;
      int h = bounds.bottom - bounds.top;
      SDL_Surface* s = SDL_CreateSurfaceFrom(
          w,
          h,
          SDL_PIXELFORMAT_RGB24,
          (*(*bkPixPat)->patData),
          3 * w);

      if (s == NULL) {
        wm_log.error("Could not create surface: %s\n", SDL_GetError());
        return;
      }

      SDL_Texture* t = SDL_CreateTextureFromSurface(sdlRenderer, s);
      if (t == NULL) {
        wm_log.error("Could not create texture: %s\n", SDL_GetError());
        return;
      }
      SDL_DestroySurface(s);
      if (!SDL_RenderTextureTiled(sdlRenderer, t, NULL, 1.0, NULL)) {
        wm_log.error("Could not render background texture: %s", SDL_GetError());
      }
    }

    void move(int hGlobal, int vGlobal) {
      SDL_SetWindowPosition(sdlWindow, hGlobal, vGlobal);
      SDL_SyncWindow(sdlWindow);
    }

    SDL_WindowID sdl_window_id() const {
      return SDL_GetWindowID(this->sdlWindow);
    }

    std::shared_ptr<std::vector<DialogItem>> get_dialog_items() {
      return this->dialog_items;
    }

  private:
    std::string title;
    Rect bounds;
    int w;
    int h;
    SDL_WindowFlags flags;
    std::shared_ptr<std::vector<DialogItem>> dialog_items;
    SDL_Window* sdlWindow;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* sdlTexture; // Use a texture to hold the window's rendered state
  };

  WindowManager() = default;
  ~WindowManager() = default;

  WindowPtr create_window(
      const std::string& title,
      const Rect& bounds,
      bool visible,
      bool go_away,
      int16_t proc_id,
      uint32_t ref_con,
      std::shared_ptr<std::vector<DialogItem>> dialog_items,
      SDL_WindowFlags flags) {
    CGrafPort port{};
    port.portRect = bounds;
    CWindowRecord* wr = new CWindowRecord();
    wr->port = port;

    // Note: Realmz doesn't actually use any of the following fields; we also
    // don't use numItems and dItems internally here (we instead use the vector
    // in the Window struct)
    wr->visible = visible;
    wr->goAwayFlag = go_away;
    wr->windowKind = proc_id;
    wr->refCon = ref_con;
    if (dialog_items) {
      wr->numItems = dialog_items->size();
      wr->dItems = dialog_items->data();
    } else {
      wr->numItems = 0;
      wr->dItems = nullptr;
    }

    std::shared_ptr<Window> window = std::make_shared<Window>(title, bounds, flags, dialog_items);
    record_to_window.emplace(&wr->port, window);
    sdl_window_id_to_window.emplace(window->sdl_window_id(), window);

    return &wr->port;
  }

  void destroy_window(WindowPtr record) {
    auto window_it = record_to_window.find(record);
    if (window_it == record_to_window.end()) {
      throw std::logic_error("Attempted to delete nonexistent window");
    }
    sdl_window_id_to_window.erase(window_it->second->sdl_window_id());
    record_to_window.erase(window_it);
    CWindowRecord* const window = reinterpret_cast<CWindowRecord*>(record);
    delete window;
  }

  std::shared_ptr<Window> window_for_record(WindowPtr record) {
    return this->record_to_window.at(record);
  }
  std::shared_ptr<Window> window_for_sdl_window_id(SDL_WindowID window_id) {
    return this->sdl_window_id_to_window.at(window_id);
  }

private:
  std::unordered_map<WindowPtr, std::shared_ptr<Window>> record_to_window;
  std::unordered_map<SDL_WindowID, std::shared_ptr<Window>> sdl_window_id_to_window;
};

static WindowManager wm;

static void
load_fonts(void) {
  auto font_filename = host_filename_for_mac_filename(":Black Chancery.ttf", true);
  fonts_by_id[1601] = TTF_OpenFont(font_filename.c_str(), 16);
}

static void SDL_snprintfcat(SDL_OUT_Z_CAP(maxlen) char* text, size_t maxlen, SDL_PRINTF_FORMAT_STRING const char* fmt, ...) {
  size_t length = SDL_strlen(text);
  va_list ap;

  va_start(ap, fmt);
  text += length;
  maxlen -= length;
  (void)SDL_vsnprintf(text, maxlen, fmt, ap);
  va_end(ap);
}

static void PrintDebugInfo(void) {
  int i, n;
  char text[1024];

  SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
  n = SDL_GetNumVideoDrivers();
  if (n == 0) {
    SDL_Log("No built-in video drivers\n");
  } else {
    (void)SDL_snprintf(text, sizeof(text), "Built-in video drivers:");
    for (i = 0; i < n; ++i) {
      if (i > 0) {
        SDL_snprintfcat(text, sizeof(text), ",");
      }
      SDL_snprintfcat(text, sizeof(text), " %s", SDL_GetVideoDriver(i));
    }
    SDL_Log("%s\n", text);
  }

  SDL_Log("Video driver: %s\n", SDL_GetCurrentVideoDriver());

  n = SDL_GetNumRenderDrivers();
  if (n == 0) {
    SDL_Log("No built-in render drivers\n");
  } else {
    SDL_snprintf(text, sizeof(text), "Built-in render drivers:\n");
    for (i = 0; i < n; ++i) {
      SDL_snprintfcat(text, sizeof(text), "  %s\n", SDL_GetRenderDriver(i));
    }
    SDL_Log("%s\n", text);
  }

  SDL_DisplayID dispID = SDL_GetPrimaryDisplay();
  if (dispID == 0) {
    SDL_Log("No primary display found\n");
  } else {
    SDL_snprintf(text, sizeof(text), "Primary display info:\n");
    SDL_snprintfcat(text, sizeof(text), "  Name:\t\t\t%s\n", SDL_GetDisplayName(dispID));
    const SDL_DisplayMode* dispMode = SDL_GetCurrentDisplayMode(dispID);
    SDL_snprintfcat(text, sizeof(text), "  Pixel Format:\t\t%x\n", dispMode->format);
    SDL_snprintfcat(text, sizeof(text), "  Width:\t\t%d\n", dispMode->w);
    SDL_snprintfcat(text, sizeof(text), "  Height:\t\t%d\n", dispMode->h);
    SDL_snprintfcat(text, sizeof(text), "  Pixel Density:\t%f\n", dispMode->pixel_density);
    SDL_snprintfcat(text, sizeof(text), "  Refresh Rate:\t\t%f\n", dispMode->refresh_rate);
    SDL_Log("%s\n", text);
  }
}

void copy_rect(Rect& dst, ResourceDASM::Rect& src) {
  dst.top = src.y1;
  dst.left = src.x1;
  dst.bottom = src.y2;
  dst.right = src.x2;
}

void WindowManager_Init(void) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    wm_log.error("Couldn't initialize video driver: %s\n", SDL_GetError());
    return;
  }

  PrintDebugInfo();

  TTF_Init();
  load_fonts();
}

WindowPtr WindowManager_CreateNewWindow(int16_t res_id, bool is_dialog, WindowPtr behind) {
  Rect bounds;
  int16_t proc_id;
  std::string title;
  bool visible;
  bool go_away;
  uint32_t ref_con;
  size_t num_dialog_items;
  std::shared_ptr<std::vector<DialogItem>> dialog_items;

  if (is_dialog) {
    auto data_handle = GetResource(ResourceDASM::RESOURCE_TYPE_DLOG, res_id);
    auto dlog = ResourceDASM::ResourceFile::decode_DLOG(*data_handle, GetHandleSize(data_handle));
    bounds.left = dlog.bounds.x1;
    bounds.right = dlog.bounds.x2;
    bounds.top = dlog.bounds.y1;
    bounds.bottom = dlog.bounds.y2;
    proc_id = dlog.proc_id;
    title = dlog.title;
    visible = dlog.visible;
    go_away = dlog.go_away;
    ref_con = dlog.ref_con;
    dialog_items = make_shared<std::vector<DialogItem>>(DialogItem::from_DITL(dlog.items_id));

  } else {
    auto data_handle = GetResource(ResourceDASM::RESOURCE_TYPE_WIND, res_id);
    auto wind = ResourceDASM::ResourceFile::decode_WIND(*data_handle, GetHandleSize(data_handle));
    bounds.left = wind.bounds.x1;
    bounds.right = wind.bounds.x2;
    bounds.top = wind.bounds.y1;
    bounds.bottom = wind.bounds.y2;
    proc_id = wind.proc_id;
    title = wind.title;
    visible = wind.visible;
    go_away = wind.go_away;
    ref_con = wind.ref_con;
  }

  SDL_WindowFlags flags{};

  if (proc_id == plainDBox) {
    flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_UTILITY;
  }

  return wm.create_window(
      title,
      bounds,
      visible,
      go_away,
      proc_id,
      ref_con,
      dialog_items,
      flags);
}

void WindowManager_DrawDialog(WindowPtr theWindow) {
  CWindowRecord* const windowRecord = reinterpret_cast<CWindowRecord*>(theWindow);
  auto window = wm.window_for_record(theWindow);

  CGrafPtr port;
  GetPort(reinterpret_cast<GrafPtr*>(&port));
  if (port->bkPixPat) {
    window->draw_background(port->bkPixPat);
  }

  auto items = window->get_dialog_items();
  if (items) {
    for (const auto& di : *items) {
      switch (di.type) {
        case DialogItemType::PICTURE: {
          auto pict = **GetPicture(di.resource_id);
          Rect r = pict.picFrame;
          uint32_t w = r.right - r.left;
          uint32_t h = r.bottom - r.top;
          window->draw_rgba_picture(*pict.data, w, h, di.rect);
          break;
        }
        case DialogItemType::TEXT: {
          if (di.text.empty()) {
            continue;
          }
          CGrafPtr port;
          GetPort(reinterpret_cast<GrafPtr*>(&port));
          if (!window->draw_text(di.text, di.rect)) {
            wm_log.error("Error when rendering text item %d: %s", di.resource_id, SDL_GetError());
          }
          break;
        }
        default:
          // TODO: Render other DITL types
          break;
      }
    }
  }

  window->sync();
}

void WindowManager_MoveWindow(WindowPtr theWindow, uint16_t hGlobal, uint16_t vGlobal, bool front) {
  if (theWindow == nullptr) {
    return;
  }

  auto window = wm.window_for_record(theWindow);
  window->move(hGlobal, vGlobal);
}

void WindowManager_DisposeWindow(WindowPtr theWindow) {
  if (theWindow == nullptr) {
    return;
  }

  wm.destroy_window(theWindow);
}

DisplayProperties WindowManager_GetPrimaryDisplayProperties(void) {
  auto displayID = SDL_GetPrimaryDisplay();

  if (displayID == 0) {
    wm_log.error("Could not get primary display: %s", SDL_GetError());
    return {};
  }

  SDL_Rect bounds{};
  if (!SDL_GetDisplayBounds(displayID, &bounds)) {
    wm_log.error("Could not get display bounds: %s", SDL_GetError());
    return {};
  }

  return DisplayProperties{
      bounds.w,
      bounds.h};
}

OSErr PlotCIcon(const Rect* theRect, CIconHandle theIcon) {
  GrafPtr port;
  GetPort(&port);
  auto window = wm.window_for_record(reinterpret_cast<CGrafPort*>(port));
  auto bounds = (*theIcon)->iconBMap.bounds;
  int w = bounds.right - bounds.left;
  int h = bounds.bottom - bounds.top;
  window->draw_rgba_picture(
      *((*theIcon)->iconData),
      w, h, *theRect);
  window->sync();

  return noErr;
}

void GetDialogItem(DialogPtr dialog, short item_id, short* item_type, Handle* item_handle, Rect* box) {
  auto window = wm.window_for_record(reinterpret_cast<WindowPtr>(dialog));
  auto items = window->get_dialog_items();
  if (!items) {
    throw std::logic_error("GetDialogItem called on non-dialog window");
  }

  try {
    auto& item = items->at(item_id - 1);
    // Realmz doesn't use the handle directly; it only passes the handle to other
    // Classic Mac OS API functions. So, we can just return the DialogItem
    // pointer directly instead of a real Handle.
    *item_type = macos_dialog_item_type_for_resource_dasm_type(item.type);
    *item_handle = reinterpret_cast<Handle>(&item);
    *box = item.rect;
  } catch (const std::out_of_range&) {
    wm_log.warning("GetDialogItem called with invalid item_id %hd (there are only %zu items)", item_id, items->size());
  }
}

void GetDialogItemText(Handle item_handle, Str255 text) {
  // See comment in GetDialogItem about why this isn't a real Handle
  auto* item = reinterpret_cast<DialogItem*>(item_handle);
  pstr_for_string<256>(text, item->text);
}

void SetDialogItemText(Handle item_handle, ConstStr255Param text) {
  // See comment in GetDialogItem about why this isn't a real Handle
  auto* item = reinterpret_cast<DialogItem*>(item_handle);
  item->text = string_for_pstr<256>(text);
}

int16_t StringWidth(ConstStr255Param s) {
  return s[0];
}

Boolean IsDialogEvent(const EventRecord* ev) {
  if (ev->what != nullEvent) {
    fprintf(stderr, "Non-null event\n");
  }
  try {
    auto window = wm.window_for_sdl_window_id(ev->sdl_window_id);
    return (window->get_dialog_items() != nullptr);
  } catch (const std::out_of_range&) {
    return false;
  }
}

Boolean DialogSelect(const EventRecord* ev, DialogPtr* dialog, short* item_hit) {
  try {
    auto window = wm.window_for_sdl_window_id(ev->sdl_window_id);
    auto items = window->get_dialog_items();
    if (!items) {
      throw std::logic_error("DialogSelect called on non-dialog window");
    }
    for (size_t z = 0; z < items->size(); z++) {
      const auto& item = items->at(z);
      if (item.enabled && PtInRect(ev->where, &item.rect)) {
        *item_hit = item.item_id;
        return true;
      }
    }
  } catch (const std::out_of_range&) {
  }
  return false;
}

void SystemClick(const EventRecord* theEvent, WindowPtr theWindow) {
  // This is used for handling events in windows belonging to the system, other
  // applications, or desk accessories. On modern systems we never see these
  // events, so we can just do nothing here.
}
