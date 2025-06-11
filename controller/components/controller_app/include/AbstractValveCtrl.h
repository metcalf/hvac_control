#pragma once

class AbstractValveCtrl {
  public:
    virtual ~AbstractValveCtrl() {}
    virtual void set(bool heatVlv, bool coolVlv) = 0;

    void setMode(bool cool, bool on) {
        // Always turn off the valve we're not setting (e.g. turn off the heat valve if
        // `cool` is true).
        bool heatVlv = !cool && on;
        bool coolVlv = cool && on;
        set(heatVlv, coolVlv);
    };
};
