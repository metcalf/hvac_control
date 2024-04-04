#pragma once

#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "lvgl/lvgl.h"

typedef std::function<void(uint8_t)> cancelCbFn_t;

typedef std::function<void()> __intCancelCbFn_t;

class MessageManager {
public:
  typedef void (*cancelCb_t)(uint8_t);

  MessageManager(size_t nMsgIds, lv_obj_t *msgsContainer,
                 const lv_font_t *closeSymbolFont, cancelCbFn_t *cancelCb);
  ~MessageManager() {
    lv_timer_del(msgTimer_);
    for (int i = 0; i < nMsgIds_; i++) {
      delete messages_[i];
    }
    delete messages_;
  }

  void setMessage(uint8_t msgID, bool allowCancel, const char *msg);
  void clearMessage(uint8_t msgID);

  void startScrollTimer() {
    lv_timer_reset(msgTimer_);
    lv_timer_resume(msgTimer_);
  }
  void pauseScrollTimer() { lv_timer_pause(msgTimer_); }

  void onMessageTimer();

private:
  class MessageContainer {
  public:
    MessageContainer(lv_obj_t *parent, const lv_font_t *closeSymbolFont,
                     __intCancelCbFn_t *cancelCb);
    ~MessageContainer() {
      lv_obj_del(container_);
      delete cancelCb_;
    }

    void setVisibility(bool visible);
    void setCancelable(bool cancelable);
    void setText(const char *str);

    bool isVisible() { return visible_; }
    bool isFocused();
    uint32_t getIndex() { return lv_obj_get_index(container_); }
    void setIndex(uint32_t idx) { lv_obj_move_to_index(container_, idx); }
    lv_coord_t getHeight() {
      lv_obj_update_layout(container_);
      return lv_obj_get_height(container_);
    }

  private:
    lv_obj_t *parent_;
    __intCancelCbFn_t *cancelCb_;

    lv_obj_t *container_, *cancel_, *text_;
    bool cancelable_ = true, visible_ = true;
  };

  lv_obj_t *msgsContainer_;
  cancelCbFn_t *cancelCb_;

  MessageContainer **messages_;
  size_t nMsgIds_;
  lv_timer_t *msgTimer_;
  lv_coord_t msgHeight_;

  MessageContainer *focusedMessage();
  void onCancelMessage(uint8_t msgID);
};
