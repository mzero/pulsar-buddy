#ifndef _INCLUDE_CRITICAL_H
#define _INCLUDE_CRITICAL_H

#include <Print.h>

class Critical : public Print {
public:
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buffer, size_t size);

    enum State {
      inactive,
      active,
      closed
    };

    State update();
};

extern Critical critical;

#endif // _INCLUDE_CRITICAL_H
