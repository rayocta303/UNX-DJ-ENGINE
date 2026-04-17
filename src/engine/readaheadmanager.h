#ifndef READAHEADMANAGER_H
#define READAHEADMANAGER_H

#include <list>
#include "engine/shim.h"
#include "engine/cachingreader/simplecachingreader.h"
#include "audio/types.h"
#include "engine/util/sample.h"

// Trigger constants
const double kNoTrigger = -1.0;

class ReadAheadManager {
public:
    ReadAheadManager(SimpleCachingReader* pReader);
    virtual ~ReadAheadManager();

    // Ported from Mixxx: Gets samples from reader, handles loops and cues triggers
    SINT getNextSamples(double dRate,
                         CSAMPLE* pOutput,
                         SINT requested_samples,
                         unx::audio::ChannelCount channelCount);

    void notifySeek(double seekPosition);

    void setLoopPoints(double start, double end, bool enabled) {
        m_loopStart = start;
        m_loopEnd = end;
        m_loopEnabled = enabled;
    }

private:
    SimpleCachingReader* m_pReader;
    double m_currentPosition;
    
    // Simplified loop/cue state for XDJ-UNX-C
    double m_loopStart;
    double m_loopEnd;
    bool m_loopEnabled;

    CSAMPLE* m_pCrossFadeBuffer;
    int m_cacheMissCount;
    bool m_cacheMissExpected;

    // Read ahead log (for tracking virtual playposition)
    struct ReadLogEntry {
        double start;
        double end;
        ReadLogEntry(double s, double e) : start(s), end(e) {}
    };
    std::list<ReadLogEntry> m_readAheadLog;
};

#endif // READAHEADMANAGER_H
