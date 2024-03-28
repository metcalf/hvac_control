#include "ui_utils.h"

void objSetFlag(bool on, lv_obj_t *obj, lv_obj_flag_t f) {
    if (on) {
        lv_obj_add_flag(obj, f);
    } else {
        lv_obj_clear_flag(obj, f);
    }
}

void objSetVisibility(bool visible, lv_obj_t *obj) {
    objSetFlag(!visible, obj, LV_OBJ_FLAG_HIDDEN);
}
