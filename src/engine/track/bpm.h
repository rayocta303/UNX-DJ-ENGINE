#pragma once
namespace unx {
class Bpm {
public:
    Bpm(double value = 0.0) : m_value(value) {}
    double value() const { return m_value; }
    bool isValid() const { return m_value > 0; }
private:
    double m_value;
};
}
