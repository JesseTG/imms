/*
 IMMS: Intelligent Multimedia Management System
 Copyright (C) 2001-2009 Michael Grigoriev

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#include "songinfo.h"
#include "strmanip.h"

#include <stdio.h>

#ifdef WITH_ID3LIB
# include <id3/tag.h>

class Mp3Info : public InfoSlave
{
public:
    Mp3Info(const string &filename)
    {
        id3tag.Clear();
        id3tag.Link(filename.c_str());
    }
    virtual string get_artist()
        { return get_text_frame(ID3FID_LEADARTIST); }
    virtual string get_title()
        { return get_text_frame(ID3FID_TITLE); }
    virtual string get_album()
        { return get_text_frame(ID3FID_ALBUM); }
    virtual time_t get_length() { return 0; }
protected:
    string get_text_frame(ID3_FrameID id);
    ID3_Tag id3tag;
};

string Mp3Info::get_text_frame(ID3_FrameID id)
{
    static char buffer[1024];
    ID3_Frame *myFrame = id3tag.Find(id);
    if (myFrame)
    {
        myFrame->Field(ID3FN_TEXT).Get(buffer, 1024);
        return buffer;
    }
    return "";
}
#endif

#ifdef WITH_VORBIS
# include <vorbis/vorbisfile.h>
# include <vorbis/codec.h>

class OggInfo : public InfoSlave
{
public:
    OggInfo(const string &filename);

    virtual string get_artist()
        { return get_comment("artist"); }
    virtual string get_title()
        { return get_comment("title"); }
    virtual string get_album()
        { return get_comment("album"); }
    virtual time_t get_length() { return 0; }

    ~OggInfo();
private:
    string get_comment(const string &id);

    OggVorbis_File vf;
    vorbis_comment *comment;
};

// OggInfo
OggInfo::OggInfo(const string &filename)
{
    comment = 0;

    FILE *fh = fopen(filename.c_str(), "r");
    if (!fh)
        return;

    if (ov_open(fh, &vf, NULL, 0))
        return;

    comment = ov_comment(&vf, -1);
}

OggInfo::~OggInfo()
{
    vorbis_comment_clear(comment);
    ov_clear(&vf);
}

string OggInfo::get_comment(const string &id)
{
    // Why exactly does vorbis_comment_query take a non-const char* ??
    char *content = 0, *tag = const_cast<char*>(id.c_str());
    return (comment &&
            (content = vorbis_comment_query(comment, tag, 0))) ?  content : "";
}
#endif

#ifdef WITH_TAGLIB

#include <fileref.h>
#include <tag.h>

using namespace TagLib;

class TagInfo : public InfoSlave
{
    public:
        TagInfo(const string &filename)
            : fileref(filename.c_str(), false) {}

        virtual string get_artist()
        {
            return !fileref.isNull() && fileref.tag() ? 
                    fileref.tag()->artist().toCString() : "";
        }
        virtual string get_title()
        {
            return !fileref.isNull() && fileref.tag() ? 
                    fileref.tag()->title().toCString() : "";
        }
        virtual string get_album()
        {
            return !fileref.isNull() && fileref.tag() ? 
                    fileref.tag()->album().toCString() : "";
        }
        virtual time_t get_length()
        {
            return !fileref.isNull() && fileref.audioProperties()
                ? fileref.audioProperties()->length() : 0;
        }
        virtual int get_track_num()
        {
            return !fileref.isNull() && fileref.tag() ? 
                    fileref.tag()->track() : 0;
        }
    private:
        FileRef fileref;
};
#endif

// SongInfo
void SongInfo::link(const string &_filename)
{
    if (filename == _filename)
        return;

    filename = _filename;

    delete myslave;
    myslave = 0;

    if (filename.length() > 3)
    {
        string ext = string_tolower(path_get_extension(filename));

        if (false) {}
#ifdef WITH_TAGLIB
        else if (1)
            myslave = new TagInfo(filename);
#endif
#ifdef WITH_ID3LIB
        else if (ext == "mp3")
            myslave = new Mp3Info(filename);
#endif
#ifdef WITH_VORBIS
        else if (ext == "ogg")
            myslave = new OggInfo(filename);
#endif
    }

    if (!myslave)
        myslave = new InfoSlave();
}

string SongInfo::get_artist()
    { return trim(myslave->get_artist()); }

string SongInfo::get_title()
    { return trim(myslave->get_title()); }

string SongInfo::get_album()
    { return trim(myslave->get_album()); }

time_t SongInfo::get_length()
    { return myslave->get_length(); }

int SongInfo::get_track_num()
    { return myslave->get_track_num(); }
