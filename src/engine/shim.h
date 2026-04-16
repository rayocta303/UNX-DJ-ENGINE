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
#include <map>

// Qt Types
typedef std::string QString;
inline QString QStringLiteral(const char* s) { return std::string(s); }

namespace Qt {
    enum ConnectionType {
        AutoConnection, 
        DirectConnection, 
        QueuedConnection, 
        BlockingQueuedConnection, 
        UniqueConnection = 0x80
    };
}


#define Q_OBJECT 
#define Q_DECLARE_FLAGS(name, type) typedef int name;
#define Q_UNUSED(x) (void)x
#define Q_DECLARE_METATYPE(x)
#define Q_DECLARE_TYPEINFO(x, y)
#define slots
#define signals public
#define tr(x) x

class QDebug {
public:
    template<typename T>
    QDebug& operator<<(const T& t) { return *this; }
};

class QEvent {
public:
    virtual ~QEvent() {}
};



template<typename T> using QList = std::vector<T>;
template<typename T> using QVector = std::vector<T>;
template<typename T, int N> class QVarLengthArray : public std::vector<T> {
public:
    using std::vector<T>::vector;
    void append(const T& t) { this->push_back(t); }
    T& operator[](int i) { return std::vector<T>::operator[](i); }
    const T& operator[](int i) const { return std::vector<T>::operator[](i); }
    bool isEmpty() const { return this->empty(); }
};
template<typename K, typename V> class QHash : public std::map<K, V> {
public:
    V value(const K& k, const V& defaultV = V()) const {
        auto it = this->find(k);
        return (it != this->end()) ? it->second : defaultV;
    }
    bool contains(const K& k) const { return this->find(k) != this->end(); }
    void insert(const K& k, const V& v) { (*this)[k] = v; }
};

template<typename T> struct QTypeInfo {
    static const bool isComplex = true;
};

template<typename T> using QScopedPointer = std::unique_ptr<T>;
template<typename T> using QSharedPointer = std::shared_ptr<T>;

typedef std::mutex QMutex;

typedef std::lock_guard<std::mutex> QMutexLocker;
typedef std::atomic<int> QAtomicInt;
typedef size_t qhash_seed_t;
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

#endif
