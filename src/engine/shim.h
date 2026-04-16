#ifndef ENGINE_SHIM_H
#define ENGINE_SHIM_H

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <memory>
#include <algorithm>
#include <cmath>
#include <span>

// Qt Types
typedef std::string QString;
inline QString QStringLiteral(const char* s) { return std::string(s); }

#define Q_OBJECT 
#define Q_DECLARE_FLAGS(name, type) typedef int name;
#define Q_UNUSED(x) (void)x
#define slots
#define signals public
#define tr(x) x

template<typename T> using QList = std::vector<T>;
template<typename T> using QVector = std::vector<T>;
template<typename T, int N> using QVarLengthArray = std::vector<T>;
template<typename T> using QScopedPointer = std::unique_ptr<T>;
template<typename T> using QSharedPointer = std::shared_ptr<T>;

typedef std::mutex QMutex;
typedef std::lock_guard<std::mutex> QMutexLocker;
typedef std::atomic<int> QAtomicInt;
template<typename T> using QAtomicPointer = std::atomic<T*>;

#define UNX_DEBUG_ASSERT(x) 
#define DEBUG_ASSERT(x)
#define VERIFY_OR_DEBUG_ASSERT(x) if(!(x))
#define FRIEND_TEST(x, y) 

// Fake QObject for signal/slots (minimal)
class QObject {
public:
    virtual ~QObject() {}
    template<typename T, typename Func>
    static void connect(T* sender, Func signal, void* receiver, Func slot) {}
};

// Math / Types Aliases
typedef float CSAMPLE;
typedef float CSAMPLE_GAIN;
typedef double SINT;

namespace unx {
    typedef float Sample;
    typedef std::vector<float> SampleBuffer;
    
    namespace audio {
        typedef int ChannelCount;
        typedef int SampleRate;
        struct FramePos {
            double value;
            FramePos(double v = 0) : value(v) {}
            operator double() const { return value; }
        };
        const FramePos kInvalidFramePos = FramePos(-1);
    }
}

namespace Engine = unx;

#endif
