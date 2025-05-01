#pragma once

class CallBase {
public:
    virtual void Proceed(bool ok) = 0;
    virtual ~CallBase() = default;
};
