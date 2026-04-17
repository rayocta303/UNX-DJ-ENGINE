#ifndef RAMPINGVALUE_H
#define RAMPINGVALUE_H

template <typename T>
class RampingValue {
public:
    RampingValue(T start, T end, int frames) {
        m_value = start;
        if (frames > 0) {
            m_step = (end - start) / (T)frames;
        } else {
            m_step = 0;
        }
    }

    T getNth(int n) const {
        return m_value + m_step * (T)n;
    }

private:
    T m_value;
    T m_step;
};

#endif
