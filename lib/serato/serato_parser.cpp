#include "serato_parser.h"
#include <iostream>
#include <algorithm>

namespace serato {

static uint32_t swap32(uint32_t val) {
    return ((val << 24) & 0xff000000) |
           ((val << 8)  & 0x00ff0000) |
           ((val >> 8)  & 0x0000ff00) |
           ((val >> 24) & 0x000000ff);
}

uint32_t Parser::readUint32be(std::ifstream& is) {
    uint32_t val;
    is.read(reinterpret_cast<char*>(&val), 4);
    return swap32(val);
}

std::string Parser::readUtf16be(std::ifstream& is, uint32_t size) {
    if (size == 0) return "";
    std::vector<char16_t> buffer(size / 2);
    is.read(reinterpret_cast<char*>(buffer.data()), size);
    
    std::string result;
    for (auto& c : buffer) {
        // Swap bytes for BE to LE
        uint16_t val = (uint16_t)c;
        uint16_t le = ((val << 8) & 0xff00) | ((val >> 8) & 0x00ff);
        
        // Very basic UTF-16 to UTF-8 conversion for ASCII range
        // For full support, a proper converter should be used.
        if (le < 128) result += (char)le;
        else result += '?'; 
    }
    return result;
}

bool Parser::parseTrack(std::ifstream& is, uint32_t totalSize, Track& track) {
    uint32_t read = 0;
    while (read < totalSize) {
        uint32_t fieldId = readUint32be(is);
        uint32_t fieldSize = readUint32be(is);
        read += 8;

        if (read + fieldSize > totalSize) break;

        switch (static_cast<FieldId>(fieldId)) {
            case FieldId::FileType: track.filetype = readUtf16be(is, fieldSize); break;
            case FieldId::FilePath: track.location = readUtf16be(is, fieldSize); break;
            case FieldId::SongTitle: track.title = readUtf16be(is, fieldSize); break;
            case FieldId::Artist: track.artist = readUtf16be(is, fieldSize); break;
            case FieldId::Album: track.album = readUtf16be(is, fieldSize); break;
            case FieldId::Genre: track.genre = readUtf16be(is, fieldSize); break;
            case FieldId::Comment: track.comment = readUtf16be(is, fieldSize); break;
            case FieldId::Grouping: track.grouping = readUtf16be(is, fieldSize); break;
            case FieldId::Label: track.label = readUtf16be(is, fieldSize); break;
            case FieldId::Key: track.key = readUtf16be(is, fieldSize); break;
            case FieldId::Bitrate: track.bitrate = readUtf16be(is, fieldSize); break;
            case FieldId::SampleRate: track.samplerate = readUtf16be(is, fieldSize); break;
            case FieldId::Length: {
                std::string s = readUtf16be(is, fieldSize);
                try { track.duration = std::stoi(s); } catch(...) {}
                break;
            }
            case FieldId::Year: {
                std::string s = readUtf16be(is, fieldSize);
                try { track.year = std::stoi(s); } catch(...) {}
                break;
            }
            case FieldId::Bpm: {
                std::string s = readUtf16be(is, fieldSize);
                try { track.bpm = std::stod(s); } catch(...) {}
                break;
            }
            case FieldId::BeatgridLocked: {
                char b; is.read(&b, 1); track.beatgridlocked = (b != 0);
                if (fieldSize > 1) is.ignore(fieldSize - 1);
                break;
            }
            case FieldId::Missing: {
                char b; is.read(&b, 1); track.missing = (b != 0);
                if (fieldSize > 1) is.ignore(fieldSize - 1);
                break;
            }
            case FieldId::FileTime:
            case FieldId::DateAdded: {
                uint32_t val = readUint32be(is);
                if (fieldId == (uint32_t)FieldId::FileTime) track.filetime = val;
                else track.datetimeadded = val;
                if (fieldSize > 4) is.ignore(fieldSize - 4);
                break;
            }
            default:
                is.ignore(fieldSize);
                break;
        }
        read += fieldSize;
    }
    return !track.location.empty();
}

std::vector<Track> Parser::parseDatabase(const std::string& path) {
    std::ifstream is(path, std::ios::binary);
    std::vector<Track> tracks;
    if (!is.is_open()) return tracks;

    while (is.peek() != EOF) {
        uint32_t fieldId = readUint32be(is);
        uint32_t fieldSize = readUint32be(is);

        if (fieldId == (uint32_t)FieldId::Track) {
            Track t;
            if (parseTrack(is, fieldSize, t)) {
                tracks.push_back(t);
            }
        } else {
            is.ignore(fieldSize);
        }
    }
    return tracks;
}

Crate Parser::parseCrate(const std::string& path) {
    std::ifstream is(path, std::ios::binary);
    Crate crate;
    if (!is.is_open()) return crate;

    // Crate name from filename
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) crate.name = path.substr(lastSlash + 1);
    else crate.name = path;
    if (crate.name.size() > 6 && crate.name.substr(crate.name.size() - 6) == ".crate") {
        crate.name = crate.name.substr(0, crate.name.size() - 6);
    }

    while (is.peek() != EOF) {
        uint32_t fieldId = readUint32be(is);
        uint32_t fieldSize = readUint32be(is);

        if (fieldId == (uint32_t)FieldId::Track) {
            // Track entry in crate is another TLV block
            uint32_t subId = readUint32be(is);
            uint32_t subSize = readUint32be(is);
            if (subId == (uint32_t)FieldId::TrackPath) {
                crate.trackPaths.push_back(readUtf16be(is, subSize));
                if (fieldSize > subSize + 8) is.ignore(fieldSize - (subSize + 8));
            } else {
                is.ignore(fieldSize);
            }
        } else {
            is.ignore(fieldSize);
        }
    }
    return crate;
}

} // namespace serato
