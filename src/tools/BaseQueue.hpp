/*
    如果用户想使用自己定义的队列类，只需要继承这个类，并实现 enqueue、dequeue 和 empty 方法即可。
    这样就可以在 AsyncLogger 中使用用户自定义的队列类。
*/
#pragma once
template <typename T>
class BaseQueue {
public:
    using value_type = T;
    virtual ~BaseQueue() = default;
    virtual void enqueue(const T& value) = 0;
    virtual bool dequeue(T& value) = 0;
    virtual bool empty() const = 0;
};