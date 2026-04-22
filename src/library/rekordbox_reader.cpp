#ifdef _WIN32
#ifndef KS_STR_ENCODING_WIN32API
#define KS_STR_ENCODING_WIN32API
#endif
#endif

#include "library/rekordbox_reader.h"
#include "../../lib/rekordbox-metadata/rekordbox_pdb.h"
#include "../../lib/rekordbox-metadata/rekordbox_anlz.h"
#include "kaitai/kaitaistream.h"
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <iostream>

// Helper to safely get string from RB device string
static std::string RB_GetString(rekordbox_pdb_t::device_sql_string_t* rbs) {
    if (!rbs || !rbs->body()) return "";
    
    // The library handles different encodings. 
    // For RB PDB, it's usually UTF-16LE or ASCII.
    // Body can be device_sql_long_utf16le_t, device_sql_long_ascii_t, or device_sql_short_ascii_t.
    
    auto body = rbs->body();
    
    // We can try to cast to the specific types or rely on the fact that rekordbox_pdb.cpp
    // already calls bytes_to_str which (if KS_STR_ENCODING_WIN32API is defined)
    // converts everything to UTF-8.
    
    // However, the generated code for device_sql_string_t doesn't have a generic "text()" method.
    // It's in the body.
    
    if (auto b = dynamic_cast<rekordbox_pdb_t::device_sql_long_utf16le_t*>(body)) return b->text();
    if (auto b = dynamic_cast<rekordbox_pdb_t::device_sql_long_ascii_t*>(body)) return b->text();
    if (auto b = dynamic_cast<rekordbox_pdb_t::device_sql_short_ascii_t*>(body)) return b->text();
    
    return "";
}

extern "C" RBDatabase* RB_LoadDatabase(const char* rootPath) {
    std::string pdbPath = std::string(rootPath) + "/PIONEER/rekordbox/export.pdb";
    std::ifstream is(pdbPath, std::ios::binary);
    if (!is.is_open()) {
        std::cerr << "[RB] Failed to open " << pdbPath << std::endl;
        return nullptr;
    }

    try {
        kaitai::kstream ks(&is);
        rekordbox_pdb_t pdb(&ks);

        std::map<uint32_t, std::string> artists;
        std::map<uint32_t, std::string> albums;
        std::map<uint32_t, std::string> genres;
        std::map<uint32_t, std::string> keys;
        std::map<uint32_t, std::string> artworks;
        std::map<uint32_t, std::string> labels;

        std::vector<RBTrack> rbTracks;
        std::vector<RBPlaylist> rbPlaylists;
        std::map<uint32_t, std::vector<uint32_t>> playlistTracks;

        for (auto table : *pdb.tables()) {
            auto page_ref = table->first_page();
            while (page_ref) {
                auto page = page_ref->body();
                if (page->is_data_page() && page->type() == table->type()) {
                    for (auto group : *page->row_groups()) {
                        for (auto row : *group->rows()) {
                            if (!row->present()) continue;
                            auto body = row->body();

                            switch (table->type()) {
                                case rekordbox_pdb_t::PAGE_TYPE_ARTISTS: {
                                    auto r = static_cast<rekordbox_pdb_t::artist_row_t*>(body);
                                    artists[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_ALBUMS: {
                                    auto r = static_cast<rekordbox_pdb_t::album_row_t*>(body);
                                    albums[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_GENRES: {
                                    auto r = static_cast<rekordbox_pdb_t::genre_row_t*>(body);
                                    genres[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_KEYS: {
                                    auto r = static_cast<rekordbox_pdb_t::key_row_t*>(body);
                                    keys[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_ARTWORK: {
                                    auto r = static_cast<rekordbox_pdb_t::artwork_row_t*>(body);
                                    artworks[r->id()] = RB_GetString(r->path());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_LABELS: {
                                    auto r = static_cast<rekordbox_pdb_t::label_row_t*>(body);
                                    labels[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_TRACKS: {
                                    auto r = static_cast<rekordbox_pdb_t::track_row_t*>(body);
                                    RBTrack t;
                                    memset(&t, 0, sizeof(RBTrack));
                                    t.ID = r->id();
                                    strncpy(t.Title, RB_GetString(r->title()).c_str(), 255);
                                    strncpy(t.FilePath, RB_GetString(r->file_path()).c_str(), 511);
                                    t.BPM = (float)r->tempo() / 100.0f;
                                    t.Duration = r->duration();
                                    
                                    // Map IDs
                                    if (artists.count(r->artist_id())) strncpy(t.Artist, artists[r->artist_id()].c_str(), 255);
                                    if (albums.count(r->album_id())) strncpy(t.Album, albums[r->album_id()].c_str(), 255);
                                    if (genres.count(r->genre_id())) strncpy(t.Genre, genres[r->genre_id()].c_str(), 255);
                                    if (keys.count(r->key_id())) strncpy(t.Key, keys[r->key_id()].c_str(), 31);
                                    if (artworks.count(r->artwork_id())) strncpy(t.ArtworkPath, artworks[r->artwork_id()].c_str(), 511);
                                    if (labels.count(r->label_id())) strncpy(t.Label, labels[r->label_id()].c_str(), 127);
                                    
                                    if (artists.count(r->remixer_id())) strncpy(t.Remixer, artists[r->remixer_id()].c_str(), 127);
                                    if (artists.count(r->composer_id())) strncpy(t.Composer, artists[r->composer_id()].c_str(), 127);
                                    
                                    strncpy(t.MixName, RB_GetString(r->mix_name()).c_str(), 127);
                                    strncpy(t.Comment, RB_GetString(r->comment()).c_str(), 255);
                                    strncpy(t.DateAdded, RB_GetString(r->date_added()).c_str(), 31);
                                    strncpy(t.ReleaseDate, RB_GetString(r->release_date()).c_str(), 31);
                                    strncpy(t.ISRC, RB_GetString(r->isrc()).c_str(), 31);
                                    
                                    t.Rating = r->rating();
                                    t.Year = r->year();
                                    t.Bitrate = r->bitrate();
                                    t.SampleRate = r->sample_rate();
                                    t.PlayCount = r->play_count();
                                    t.ColorID = r->color_id();
                                    t.TrackNumber = r->track_number();
                                    t.DiscNumber = r->disc_number();
                                    
                                    strncpy(t.AnalyzePath, RB_GetString(r->analyze_path()).c_str(), 511);

                                    rbTracks.push_back(t);
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_PLAYLIST_TREE: {
                                    auto r = static_cast<rekordbox_pdb_t::playlist_tree_row_t*>(body);
                                    RBPlaylist pl;
                                    memset(&pl, 0, sizeof(RBPlaylist));
                                    pl.ID = r->id();
                                    pl.ParentID = r->parent_id();
                                    pl.IsFolder = (r->raw_is_folder() != 0);
                                    strncpy(pl.Name, RB_GetString(r->name()).c_str(), 255);
                                    rbPlaylists.push_back(pl);
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_PLAYLIST_ENTRIES: {
                                    auto r = static_cast<rekordbox_pdb_t::playlist_entry_row_t*>(body);
                                    playlistTracks[r->playlist_id()].push_back(r->track_id());
                                    break;
                                }
                                default: break;
                            }
                        }
                    }
                }
                if (page_ref->index() == table->last_page()->index()) break;
                page_ref = page->next_page();
            }
        }

        RBDatabase* db = new RBDatabase();
        db->TrackCount = (uint32_t)rbTracks.size();
        db->Tracks = new RBTrack[db->TrackCount];
        for (size_t i = 0; i < rbTracks.size(); i++) db->Tracks[i] = rbTracks[i];
        
        db->PlaylistCount = (uint32_t)rbPlaylists.size();
        db->Playlists = new RBPlaylist[db->PlaylistCount];
        for (size_t i = 0; i < rbPlaylists.size(); i++) {
            db->Playlists[i] = rbPlaylists[i];
            auto& tids = playlistTracks[db->Playlists[i].ID];
            db->Playlists[i].TrackCount = (uint32_t)tids.size();
            if (db->Playlists[i].TrackCount > 0) {
                db->Playlists[i].TrackIDs = new uint32_t[db->Playlists[i].TrackCount];
                for (size_t j = 0; j < tids.size(); j++) db->Playlists[i].TrackIDs[j] = tids[j];
            } else {
                db->Playlists[i].TrackIDs = nullptr;
            }
        }

        return db;

    } catch (const std::exception& e) {
        std::cerr << "[RB] Error parsing database: " << e.what() << std::endl;
        return nullptr;
    }
}

extern "C" void RB_FreeDatabase(RBDatabase* db) {
    if (!db) return;
    if (db->Tracks) {
        for (uint32_t i = 0; i < db->TrackCount; i++) {
            if (db->Tracks[i].Cues) delete[] db->Tracks[i].Cues;
            if (db->Tracks[i].Phrases) delete[] db->Tracks[i].Phrases;
            if (db->Tracks[i].DynamicWaveform) delete[] db->Tracks[i].DynamicWaveform;
        }
        delete[] db->Tracks;
    }
    if (db->Playlists) {
        for (uint32_t i = 0; i < db->PlaylistCount; i++) {
            if (db->Playlists[i].TrackIDs) delete[] db->Playlists[i].TrackIDs;
        }
        delete[] db->Playlists;
    }
    delete db;
}

static void RB_ParseAnlz(const std::string& path, RBTrack* track) {
    std::ifstream is(path, std::ios::binary);
    if (!is.is_open()) return;

    try {
        kaitai::kstream ks(&is);
        rekordbox_anlz_t anlz(&ks);
        int currentStaticPrio = 0;

        for (auto& section : *anlz.sections()) {
            auto tag = section->fourcc();
            if (tag == rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID) {
                auto bg = static_cast<rekordbox_anlz_t::beat_grid_tag_t*>(section->body());
                track->BeatGridCount = bg->num_beats();
                for (uint32_t i = 0; i < bg->num_beats() && i < 1024; i++) {
                    auto b = (*bg->beats())[i].get();
                    track->BeatGrid[i].Time = b->time();
                    track->BeatGrid[i].BPM = b->tempo();
                    track->BeatGrid[i].BeatNumber = b->beat_number();
                }
            } else if (tag == rekordbox_anlz_t::SECTION_TAGS_CUES || tag == rekordbox_anlz_t::SECTION_TAGS_CUES_2) {
                std::vector<RBCue> found;
                if (tag == rekordbox_anlz_t::SECTION_TAGS_CUES) {
                    auto ct = static_cast<rekordbox_anlz_t::cue_tag_t*>(section->body());
                    for (auto& entry : *ct->cues()) {
                        RBCue rc;
                        memset(&rc, 0, sizeof(RBCue));
                        rc.Time = entry->time();
                        rc.ID = (uint16_t)entry->hot_cue();
                        rc.Type = (uint16_t)entry->type();
                        found.push_back(rc);
                    }
                } else {
                    auto ct = static_cast<rekordbox_anlz_t::cue_extended_tag_t*>(section->body());
                    for (auto& entry : *ct->cues()) {
                        RBCue rc;
                        memset(&rc, 0, sizeof(RBCue));
                        rc.Time = entry->time();
                        rc.ID = (uint16_t)entry->hot_cue();
                        rc.Type = (uint16_t)entry->type();
                        std::string comment = entry->comment();
                        strncpy(rc.Comment, comment.c_str(), 63);
                        
                        if (!entry->_is_null_color_red()) {
                            rc.Color[0] = entry->color_red();
                            rc.Color[1] = entry->color_green();
                            rc.Color[2] = entry->color_blue();
                        }
                        
                        found.push_back(rc);
                    }
                }

                for (auto& f : found) {
                    bool exists = false;
                    for (uint32_t i=0; i<track->CueCount; i++) {
                        if (track->Cues[i].Time == f.Time && track->Cues[i].ID == f.ID) {
                            exists = true; 
                            break;
                        }
                    }
                    if (!exists) {
                        RBCue* next = new RBCue[track->CueCount + 1];
                        if (track->Cues) {
                            memcpy(next, track->Cues, sizeof(RBCue) * track->CueCount);
                            delete[] track->Cues;
                        }
                        next[track->CueCount] = f;
                        track->Cues = next;
                        track->CueCount++;
                    }
                }
            } else if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_TINY || tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_PREVIEW || 
                       tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_PREVIEW || tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_PREVIEW) {
                std::string data;
                int prio = 0; // 1=blue, 2=color, 3=3band
                if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_PREVIEW) {
                    auto w3 = static_cast<rekordbox_anlz_t::wave_3band_preview_tag_t*>(section->body());
                    data = w3->entries();
                    prio = 3;
                } else if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_PREVIEW) {
                    auto wc = static_cast<rekordbox_anlz_t::wave_color_preview_tag_t*>(section->body());
                    data = wc->entries();
                    prio = 2;
                } else {
                    auto wp = static_cast<rekordbox_anlz_t::wave_preview_tag_t*>(section->body());
                    data = wp->data();
                    prio = 1;
                }

                if (prio > currentStaticPrio) {
                    track->StaticWaveformLen = data.length() > 8192 ? 8192 : data.length();
                    track->StaticWaveformType = prio;
                    memcpy(track->StaticWaveform, data.data(), track->StaticWaveformLen);
                    currentStaticPrio = prio;
                }
            } else if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_SCROLL || tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_SCROLL || tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_SCROLL) {
                std::string data;
                int type = 0;
                if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_SCROLL) {
                    auto ws = static_cast<rekordbox_anlz_t::wave_scroll_tag_t*>(section->body());
                    data = ws->entries();
                    type = 1;
                } else if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_SCROLL) {
                    auto wc = static_cast<rekordbox_anlz_t::wave_color_scroll_tag_t*>(section->body());
                    data = wc->entries();
                    type = 2;
                } else {
                    auto w3 = static_cast<rekordbox_anlz_t::wave_3band_scroll_tag_t*>(section->body());
                    data = w3->entries();
                    type = 3;
                }
                
                if (track->DynamicWaveform) delete[] track->DynamicWaveform;
                track->DynamicWaveformLen = data.length();
                track->WaveformType = type;
                if (track->DynamicWaveformLen > 0) {
                    track->DynamicWaveform = new unsigned char[track->DynamicWaveformLen];
                    memcpy(track->DynamicWaveform, data.data(), track->DynamicWaveformLen);
                }
            } else if (tag == rekordbox_anlz_t::SECTION_TAGS_SONG_STRUCTURE) {
                auto ss = static_cast<rekordbox_anlz_t::song_structure_tag_t*>(section->body());
                if (ss && ss->body() && ss->body()->entries()) {
                    auto entries = ss->body()->entries();
                    if (!track->Phrases) {
                        track->PhraseCount = entries->size();
                        track->Phrases = new RBPhrase[track->PhraseCount];
                        for (size_t i = 0; i < track->PhraseCount; i++) {
                            auto entry = (*entries)[i].get();
                            track->Phrases[i].Index = entry->index();
                            track->Phrases[i].Beat = entry->beat();
                            track->Phrases[i].KindID = 0;
                            strcpy(track->Phrases[i].Kind, "Unknown");
                            
                            auto kind = entry->kind();
                            if (auto kh = dynamic_cast<rekordbox_anlz_t::phrase_high_t*>(kind)) {
                                track->Phrases[i].KindID = kh->id();
                                switch (kh->id()) {
                                    case rekordbox_anlz_t::MOOD_HIGH_PHRASE_INTRO: strcpy(track->Phrases[i].Kind, "Intro"); break;
                                    case rekordbox_anlz_t::MOOD_HIGH_PHRASE_UP: strcpy(track->Phrases[i].Kind, "Up"); break;
                                    case rekordbox_anlz_t::MOOD_HIGH_PHRASE_DOWN: strcpy(track->Phrases[i].Kind, "Down"); break;
                                    case rekordbox_anlz_t::MOOD_HIGH_PHRASE_CHORUS: strcpy(track->Phrases[i].Kind, "Chorus"); break;
                                    case rekordbox_anlz_t::MOOD_HIGH_PHRASE_OUTRO: strcpy(track->Phrases[i].Kind, "Outro"); break;
                                }
                            } else if (auto km = dynamic_cast<rekordbox_anlz_t::phrase_mid_t*>(kind)) {
                                track->Phrases[i].KindID = km->id();
                                switch (km->id()) {
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_INTRO: strcpy(track->Phrases[i].Kind, "Intro"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_1: strcpy(track->Phrases[i].Kind, "Verse 1"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_2: strcpy(track->Phrases[i].Kind, "Verse 2"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_3: strcpy(track->Phrases[i].Kind, "Verse 3"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_4: strcpy(track->Phrases[i].Kind, "Verse 4"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_5: strcpy(track->Phrases[i].Kind, "Verse 5"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_6: strcpy(track->Phrases[i].Kind, "Verse 6"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_BRIDGE: strcpy(track->Phrases[i].Kind, "Bridge"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_CHORUS: strcpy(track->Phrases[i].Kind, "Chorus"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_OUTRO: strcpy(track->Phrases[i].Kind, "Outro"); break;
                                }
                            } else if (auto kl = dynamic_cast<rekordbox_anlz_t::phrase_low_t*>(kind)) {
                                track->Phrases[i].KindID = kl->id();
                                switch(kl->id()) {
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_INTRO: strcpy(track->Phrases[i].Kind, "Intro"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_1: strcpy(track->Phrases[i].Kind, "Verse 1"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_1B: strcpy(track->Phrases[i].Kind, "Verse 1B"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_1C: strcpy(track->Phrases[i].Kind, "Verse 1C"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_2: strcpy(track->Phrases[i].Kind, "Verse 2"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_2B: strcpy(track->Phrases[i].Kind, "Verse 2B"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_2C: strcpy(track->Phrases[i].Kind, "Verse 2C"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_BRIDGE: strcpy(track->Phrases[i].Kind, "Bridge"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_CHORUS: strcpy(track->Phrases[i].Kind, "Chorus"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_OUTRO: strcpy(track->Phrases[i].Kind, "Outro"); break;
                                }
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {}
}

extern "C" void RB_LoadTrackData(RBTrack* track, const char* rootPath) {
    if (!track || track->AnalyzePath[0] == '\0') return;

    std::string datPath = std::string(rootPath) + "/" + track->AnalyzePath;
    RB_ParseAnlz(datPath, track);

    // Also look for .EXT
    std::string extPath = datPath;
    if (extPath.size() > 4) {
        extPath.replace(extPath.size() - 3, 3, "EXT");
        RB_ParseAnlz(extPath, track);
    }
}
