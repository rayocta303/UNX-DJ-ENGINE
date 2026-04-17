#pragma once
#include <memory>
class Track {};
typedef std::shared_ptr<Track> TrackPointer;
enum class StemChannelSelection { None = 0, All = 0xFF };
