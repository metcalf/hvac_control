#include "MessageManager.h"
#include "ui_utils.h"

#include "esp_log.h"

#define MSG_SCROLL_MS 5 * 1000

static const char *TAG = "UI";

void pauseTimer(lv_event_t *e) { lv_timer_pause((lv_timer_t *)e->user_data); }

void messageTimerCb(lv_timer_t *timer) { ((MessageManager *)timer->user_data)->scrollNext(); }

void msgClickCb(lv_event_t *e) { ((MessageManager *)lv_event_get_user_data(e))->onMessageClick(); }
void msgCancelCb(lv_event_t *e) { (*(__intCancelCbFn_t *)lv_event_get_user_data(e))(); }

MessageManager::MessageManager(size_t nMsgIds, lv_obj_t *msgsContainer,
                               const lv_font_t *closeSymbolFont, cancelCbFn_t *cancelCb)
    : msgsContainer_(msgsContainer), cancelCb_(cancelCb) {
    nMsgIds_ = nMsgIds;
    messages_ = new MessageContainer *[nMsgIds_];

    for (int i = 0; i < nMsgIds; i++) {
        messages_[i] =
            new MessageContainer(this, msgsContainer_, closeSymbolFont,
                                 new __intCancelCbFn_t([this, i]() { this->onCancelMessage(i); }));
    }

    lv_obj_set_scroll_snap_y(msgsContainer_, LV_SCROLL_SNAP_START);
    msgHeight_ = messages_[0]->getHeight();
    msgTimer_ = lv_timer_create(messageTimerCb, MSG_SCROLL_MS, this);
}

void MessageManager::setMessage(uint8_t msgID, bool allowCancel, const char *msg) {
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
}

void MessageManager::clearMessage(uint8_t msgID) { messages_[msgID]->setVisibility(false); }

void MessageManager::onMessageClick() {
    lv_timer_reset(msgTimer_);
    lv_timer_resume(msgTimer_);
    scrollNext();
}

void MessageManager::scrollNext() {
    // TODO(future): It'd be nicer to make this a circular scroll
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
}

MessageManager::MessageContainer *MessageManager::focusedMessage() {
    for (int i = 0; i < nMsgIds_; i++) {
        if (messages_[i]->isFocused()) {
            return messages_[i];
        }
    }

    return nullptr;
}

void MessageManager::onCancelMessage(uint8_t msgID) {
    ESP_LOGD(TAG, "cancel msg: %d", msgID);
    clearMessage(msgID);
    if (cancelCb_ != nullptr) {
        (*cancelCb_)(msgID);
    }
}

MessageManager::MessageContainer::MessageContainer(MessageManager *manager, lv_obj_t *parent,
                                                   const lv_font_t *closeSymbolFont,
                                                   __intCancelCbFn_t *cancelCb)
    : parent_(parent), cancelCb_(cancelCb) {

    // BEGIN copy-paste from ui_Home.c
    container_ = lv_obj_create(parent);
    lv_obj_remove_style_all(container_);
    lv_obj_set_width(container_, lv_pct(100));
    lv_obj_set_height(container_, lv_pct(100));
    lv_obj_set_align(container_, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    // NB: This diverages from SquareLine since I added a container around
    // the cancel button and text but don't have a license to add more widgets in
    // SquareLine
    cancel_ = lv_obj_create(container_);
    lv_obj_remove_style_all(cancel_);
    lv_obj_set_width(cancel_, 45);
    lv_obj_set_height(cancel_, lv_pct(100));
    lv_obj_set_align(cancel_, LV_ALIGN_CENTER);
    lv_obj_add_flag(cancel_, LV_OBJ_FLAG_CLICKABLE); /// Flags
    lv_obj_clear_flag(cancel_, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                                   LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE); /// Flags
    textContainer_ = lv_obj_create(container_);
    lv_obj_remove_style_all(textContainer_);
    lv_obj_set_width(textContainer_, LV_SIZE_CONTENT);
    lv_obj_set_height(textContainer_, lv_pct(100));
    lv_obj_set_align(textContainer_, LV_ALIGN_CENTER);
    lv_obj_add_flag(textContainer_, LV_OBJ_FLAG_CLICKABLE); /// Flags
    lv_obj_clear_flag(textContainer_, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                                          LV_OBJ_FLAG_SCROLLABLE |
                                          LV_OBJ_FLAG_GESTURE_BUBBLE); /// Flags

    lv_obj_t *label = lv_label_create(cancel_);
    lv_obj_set_width(label, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(label, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    lv_label_set_text(label, "");
    lv_obj_set_style_text_font(label, closeSymbolFont, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(label, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(label, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    text_ = lv_label_create(textContainer_);
    lv_obj_set_width(text_, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(text_, LV_SIZE_CONTENT); /// 100
    lv_obj_set_align(text_, LV_ALIGN_CENTER);
    lv_label_set_text(text_, "MESSAGE");

    lv_obj_add_event_cb(cancel_, msgCancelCb, LV_EVENT_CLICKED, (void *)cancelCb_);
    lv_obj_add_event_cb(textContainer_, msgClickCb, LV_EVENT_CLICKED, (void *)manager);

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
