#ifndef SERATO_PARSER_H
#define SERATO_PARSER_H

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>

namespace serato {

enum class FieldId : uint32_t {
    Version = 0x7672736e,        // vrsn
    Track = 0x6f74726b,          // otrk
    FileType = 0x74747970,       // ttyp
    FilePath = 0x7066696c,       // pfil
    SongTitle = 0x74736e67,      // tsng
    Artist = 0x74617274,         // tart
    Album = 0x74616c62,          // talb
    Genre = 0x7467656e,          // tgen
    Comment = 0x74636f6d,        // tcom
    Grouping = 0x74677270,       // tgrp
    Label = 0x746c626c,          // tlbl
    Year = 0x74747972,           // ttyr
    Length = 0x746c656e,         // tlen
    Bitrate = 0x74626974,        // tbit
    SampleRate = 0x74736d70,     // tsmp
    Bpm = 0x7462706d,            // tbpm
    DateAddedText = 0x74616464,  // tadd
    DateAdded = 0x75616464,      // uadd
    Key = 0x746b6579,            // tkey
    BeatgridLocked = 0x6262676c, // bbgl
    FileTime = 0x75746d65,       // utme
    Missing = 0x626d6973,        // bmis
    TrackPath = 0x7074726b,      // ptrk
};

struct Track {
    std::string filetype;
    std::string location;
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string comment;
    std::string grouping;
    std::string label;
    int year = -1;
    int duration = 0;
    std::string bitrate;
    std::string samplerate;
    double bpm = -1.0;
    std::string key;
    bool beatgridlocked = false;
    bool missing = false;
    uint32_t filetime = 0;
    uint32_t datetimeadded = 0;
};

struct Crate {
    std::string name;
    std::vector<std::string> trackPaths;
};

class Parser {
public:
    static std::vector<Track> parseDatabase(const std::string& path);
    static Crate parseCrate(const std::string& path);

private:
    static std::string readUtf16be(std::ifstream& is, uint32_t size);
    static uint32_t readUint32be(std::ifstream& is);
    static bool parseTrack(std::ifstream& is, uint32_t size, Track& track);
};

} // namespace serato

#endif
