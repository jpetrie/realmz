#include "EventManager.h"

#include <queue>

static std::queue<EventRecord> event_queue;

void PushMenuEvent(int16_t menu_id, int16_t item_id) {
  Point where = {static_cast<int16_t>(-menu_id), static_cast<int16_t>(-item_id)};
  EventRecord event = {
      mouseDown,
      0,
      0,
      where,
      0};
  event_queue.push(event);
}

Boolean GetNextEvent(uint16_t eventMask, EventRecord* theEvent) {
  if (event_queue.empty()) {
    theEvent->what = nullEvent;
    return false;
  }

  *theEvent = event_queue.front();
  event_queue.pop();
  return true;
}