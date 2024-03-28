#include "MessageManager.h"
#include "ui_utils.h"

#define MSG_SCROLL_MS 5 * 1000

void pauseTimer(lv_event_t *e) { lv_timer_pause((lv_timer_t *)e->user_data); }

void resetAndResumeTimer(lv_event_t *e) {
  lv_timer_reset((lv_timer_t *)e->user_data);
  lv_timer_resume((lv_timer_t *)e->user_data);
}

void messageTimerCb(lv_timer_t *timer) {
  ((MessageManager *)timer->user_data)->onMessageTimer();
}

MessageManager::MessageManager(size_t nMsgIds, lv_obj_t *msgsContainer,
                               const lv_font_t *closeSymbolFont)
    : msgsContainer_(msgsContainer) {
  nMsgIds_ = nMsgIds;
  messages_ = new MessageContainer *[nMsgIds_];

  for (int i = 0; i < nMsgIds; i++) {
    messages_[i] = new MessageContainer(msgsContainer_, closeSymbolFont);
  }

  lv_obj_set_scroll_snap_y(msgsContainer_, LV_SCROLL_SNAP_START);
  msgHeight_ = messages_[0]->getHeight();
  msgTimer_ = lv_timer_create(messageTimerCb, MSG_SCROLL_MS, this);

  lv_obj_add_event_cb(msgsContainer_, pauseTimer, LV_EVENT_SCROLL_BEGIN,
                      msgTimer_);
  lv_obj_add_event_cb(msgsContainer_, resetAndResumeTimer, LV_EVENT_SCROLL_END,
                      msgTimer_);
}

void MessageManager::setMessage(uint8_t msgID, bool allowCancel,
                                const char *msg) {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  MessageContainer *msgContainer = messages_[msgID];

  msgContainer->setText(msg);
  msgContainer->setCancelable(allowCancel);

  MessageContainer *focused = focusedMessage();
  if (focused == nullptr) {
    // Either there was no message or we're in the middle of scrolling
    // so just add to the end.
    msgContainer->setIndex(-1);
  } else if (msgContainer != focused) {
    msgContainer->setIndex(focused->getIndex() + 1);
  }

  msgContainer->setVisibility(true);
  xSemaphoreGive(mutex_);
}

void MessageManager::clearMessage(uint8_t msgID) {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  messages_[msgID]->setVisibility(false);
  xSemaphoreGive(mutex_);
}

// TODO: Mutex?
void MessageManager::onMessageTimer() {
  // TODO(future): It'd be nicer to make this a circular scroll
  xSemaphoreTake(mutex_, portMAX_DELAY);
  lv_coord_t currY = lv_obj_get_scroll_y(msgsContainer_);

  int nVisible = 0;
  for (int i = 0; i < nMsgIds_; i++) {
    if (messages_[i]->isVisible()) {
      nVisible++;
    }
  }

  lv_coord_t newY = currY + msgHeight_;
  if (newY > (nVisible - 1) * msgHeight_) {
    newY = 0;
  }

  lv_obj_scroll_to_y(msgsContainer_, newY, LV_ANIM_ON);
  xSemaphoreGive(mutex_);
}

// TODO: Mutex?
MessageManager::MessageContainer *MessageManager::focusedMessage() {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  for (int i = 0; i < nMsgIds_; i++) {
    if (messages_[i]->isFocused()) {
      xSemaphoreGive(mutex_);
      return messages_[i];
    }
  }

  xSemaphoreGive(mutex_);
  return nullptr;
}

MessageManager::MessageContainer::MessageContainer(
    lv_obj_t *parent, const lv_font_t *closeSymbolFont) {
  lv_obj_t *ui_message_container, *ui_message_close, *ui_message_text;
  lv_obj_t *ui_Footer = parent; // Gross but allows for copy-paste

  // BEGIN copy-paste from ui_Home.c
  ui_message_container = lv_obj_create(ui_Footer);
  lv_obj_remove_style_all(ui_message_container);
  lv_obj_set_width(ui_message_container, lv_pct(100));
  lv_obj_set_height(ui_message_container, lv_pct(100));
  lv_obj_set_align(ui_message_container, LV_ALIGN_CENTER);
  lv_obj_set_flex_flow(ui_message_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ui_message_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(ui_message_container, LV_OBJ_FLAG_SCROLLABLE); /// Flags

  ui_message_close = lv_label_create(ui_message_container);
  lv_obj_set_width(ui_message_close, LV_SIZE_CONTENT);  /// 1
  lv_obj_set_height(ui_message_close, LV_SIZE_CONTENT); /// 1
  lv_obj_set_align(ui_message_close, LV_ALIGN_CENTER);
  lv_label_set_text(ui_message_close, "");
  lv_obj_set_style_text_font(ui_message_close, closeSymbolFont,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(ui_message_close, 0,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(ui_message_close, 3,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(ui_message_close, 1,
                           LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(ui_message_close, 0,
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_message_text = lv_label_create(ui_message_container);
  lv_obj_set_width(ui_message_text, LV_SIZE_CONTENT);  /// 1
  lv_obj_set_height(ui_message_text, LV_SIZE_CONTENT); /// 100
  lv_obj_set_align(ui_message_text, LV_ALIGN_CENTER);
  lv_label_set_text(ui_message_text, "MESSAGE");
  // END copy-paste from ui_Home.c

  // NB: This is a bit awkard but allows directly copy-pasting from the
  // Squareline generated code
  container_ = ui_message_container;
  cancel_ = ui_message_close;
  text_ = ui_message_text;
  parent_ = parent;

  setVisibility(false);
}

void MessageManager::MessageContainer::setVisibility(bool visible) {
  if (visible_ == visible) {
    return;
  }

  objSetVisibility(visible, container_);
  visible_ = visible;
}

void MessageManager::MessageContainer::setCancelable(bool cancelable) {
  if (cancelable_ == cancelable) {
    return;
  }

  objSetVisibility(cancelable, cancel_);

  cancelable_ = cancelable;
}

void MessageManager::MessageContainer::setText(const char *str) {
  if (strcmp(str, lv_label_get_text(text_)) == 0) {
    // Strings are the same, don't cause an invalidation
    return;
  }

  lv_label_set_text(text_, str);
}

bool MessageManager::MessageContainer::isFocused() {
  if (!visible_) {
    return false;
  }

  return lv_obj_get_scroll_y(parent_) == lv_obj_get_y(container_);
}
