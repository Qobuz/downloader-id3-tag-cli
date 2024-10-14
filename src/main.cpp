#include <CLI/CLI.hpp>

#include <iostream>
#include <optional>

#include "platform.hpp"

TAGLIB_HEADERS_BEGIN
  #include <taglib/tag.h>
  #include <taglib/fileref.h>
  #include <taglib/tfilestream.h>
  #include <taglib/tpropertymap.h>
  #include <taglib/asftag.h>
  #include <taglib/id3v2tag.h>
  #include <taglib/id3v2frame.h>
  #include <taglib/attachedpictureframe.h>
  #include <taglib/mp4tag.h>
  #include <taglib/xiphcomment.h>
  #include "tagunion.h"

  #include <taglib/aifffile.h>
  #include <taglib/flacfile.h>
  #include <taglib/mpegfile.h>
  #include <taglib/wavfile.h>
TAGLIB_HEADERS_END

#include "arguments.hpp"
#include "help.hpp"

// SetF is void(TagLib::Tag::* setter)(T), for some T.
// ConvF is T(*convert)(std::string) for SetF's T.
template<typename SetF, typename ValueF, typename ConvF>
bool process_field(TagLib::Tag& tag, SetF setter, const std::pair<bool, ValueF>& info, ConvF convert)
{
    auto& [valid, data] = info;
    if (valid)
    {
        (tag.*setter)(convert(data));
        return true;
    }
    return false;
}

void checkForRejectedProperties(const TagLib::PropertyMap &tags)
{
    if(tags.size() > 0)
    {
        unsigned int longest = 0;
        for(TagLib::PropertyMap::ConstIterator i = tags.begin(); i != tags.end(); ++i) {
            if(i->first.size() > longest) {
                longest = i->first.size();
            }
        }
        std::cout << "-- rejected TAGs (properties) --" << std::endl;
        for(TagLib::PropertyMap::ConstIterator i = tags.begin(); i != tags.end(); ++i)
            for(TagLib::StringList::ConstIterator j = i->second.begin(); j != i->second.end(); ++j)
                std::cout << std::left << std::setw(longest) << i->first << " - " << '"' << *j << '"' << std::endl;
    }
}

TagLib::ByteVector loadFile(const std::string& filename)
{
    if (filename.empty())
        return TagLib::ByteVector();

    TagLib::FileStream in(filename.c_str(), true);
    return in.readBlock(in.length());
}

// Additional tags from https://docs.mp3tag.de/mapping/
// https://taglib.org/api/classTagLib_1_1PropertyMap.html
template<typename ValueF, typename ConvF>
bool process_field(TagLib::File* file, TagLib::String propertyName, const std::pair<bool, ValueF>& info, ConvF convert)
{
    auto& [valid, data] = info;
    if (valid)
    {
        TagLib::PropertyMap map = file->properties();
        TagLib::String value = convert(data);
        map.replace(propertyName, value);
        checkForRejectedProperties(file->setProperties(map));
        return true;
    }
    return false;
}

template<typename T>
void print_field(const std::string& field_name, const T& field_value)
{
    std::cout << field_name << ": " << field_value << std::endl;
}

bool setPicture(TagLib::Tag* tag, const TagLib::ByteVector& data)
{
    if (tag == nullptr)
        return false;

    if (typeid(*tag) == typeid(TagLib::ID3v2::Tag))
    {
        TagLib::ID3v2::Tag* t = dynamic_cast<TagLib::ID3v2::Tag*>(tag);
        t->removeFrames("APIC");

        if (!data.isEmpty())
        {
            TagLib::ID3v2::AttachedPictureFrame* frame = new TagLib::ID3v2::AttachedPictureFrame();
            frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
            frame->setMimeType("image/jpeg");
            frame->setPicture(data);
            t->addFrame(frame);
        }
    }
    if (typeid(*tag) == typeid(TagLib::MP4::Tag))
    {
        TagLib::MP4::Tag* t = dynamic_cast<TagLib::MP4::Tag*>(tag);
        t->removeItem("covr");

        if (!data.isEmpty())
        {
            TagLib::MP4::CoverArt ca(TagLib::MP4::CoverArt::JPEG, data);
            TagLib::MP4::CoverArtList cal;
            cal.append(ca);
            TagLib::MP4::Item item(cal);
            t->setItem("covr", item);
        }
    }
    if (typeid(*tag) == typeid(TagLib::ASF::Tag))
    {
        TagLib::ASF::Tag* t = dynamic_cast<TagLib::ASF::Tag*>(tag);
        t->removeItem("WM/Picture");

        if (!data.isEmpty())
        {
            TagLib::ASF::Picture picture;
            picture.setMimeType("image/jpeg");
            picture.setType(TagLib::ASF::Picture::Type::FrontCover);
            picture.setPicture(data);
            t->setAttribute("WM/Picture", picture);
        }
    }
    if (typeid(*tag) == typeid(TagLib::Ogg::XiphComment))
    {
        TagLib::Ogg::XiphComment* t = dynamic_cast<TagLib::Ogg::XiphComment*>(tag);
        t->removeAllPictures();
    }
    if (typeid(*tag) == typeid(TagLib::TagUnion))
    {
        TagLib::TagUnion* t = dynamic_cast<TagLib::TagUnion*>(tag);
        setPicture((*t)[0], data);
        setPicture((*t)[1], data);
        setPicture((*t)[2], data);
    }

    return true;
}

bool setPicture(TagLib::File* file, const TagLib::ByteVector& data)
{
    if (file == nullptr)
        return false;

    // For FLAC, add picture as a new block
    if (typeid(*file) == typeid(TagLib::FLAC::File))
    {
        TagLib::FLAC::File* ff = dynamic_cast<TagLib::FLAC::File*>(file);
        ff->removePictures();

        if (!data.isEmpty())
        {
            TagLib::FLAC::Picture* pic = new TagLib::FLAC::Picture();
            pic->setType(TagLib::FLAC::Picture::FrontCover);
            pic->setMimeType("image/jpeg");
            pic->setColorDepth(24);
            pic->setData(data);
            ff->addPicture(pic);
        }
    }

    return setPicture(file->tag(), data);
}

void print_tag_items(TagLib::Tag* tag)
{
    if (tag == nullptr)
        return;

    std::cout << typeid(*tag).name() << std::endl;

    if (typeid(*tag) == typeid(TagLib::ID3v2::Tag))
    {
        TagLib::ID3v2::Tag* t = dynamic_cast<TagLib::ID3v2::Tag*>(tag);
        const TagLib::ID3v2::FrameListMap& flm = t->frameListMap();
        for(TagLib::ID3v2::FrameListMap::ConstIterator i = flm.begin(); i != flm.end(); ++i)
        {
            std::cout << i->first << std::endl;
            for(TagLib::ID3v2::FrameList::ConstIterator j = i->second.begin(); j != i->second.end(); ++j)
                std::cout << "   " << (*j)->toString().to8Bit(true) << std::endl;
        }
    }
    if (typeid(*tag) == typeid(TagLib::MP4::Tag))
    {
        TagLib::MP4::Tag* t = dynamic_cast<TagLib::MP4::Tag*>(tag);
        const TagLib::MP4::ItemMap& im = t->itemMap();
        for(TagLib::MP4::ItemMap::ConstIterator i = im.begin(); i != im.end(); ++i)
            std::cout << i->first << std::endl;
    }
    if (typeid(*tag) == typeid(TagLib::ASF::Tag))
    {
        TagLib::ASF::Tag* t = dynamic_cast<TagLib::ASF::Tag*>(tag);
        TagLib::ASF::AttributeListMap& alm = t->attributeListMap();
        for(TagLib::ASF::AttributeListMap::ConstIterator i = alm.begin(); i != alm.end(); ++i)
            std::cout << i->first << std::endl;
    }
    if (typeid(*tag) == typeid(TagLib::Ogg::XiphComment))
    {
        TagLib::Ogg::XiphComment* t = dynamic_cast<TagLib::Ogg::XiphComment*>(tag);
        const TagLib::Ogg::FieldListMap& flm = t->fieldListMap();
        for(TagLib::Ogg::FieldListMap::ConstIterator i = flm.begin(); i != flm.end(); ++i)
        {
            std::cout << i->first << std::endl;
            for(TagLib::StringList::ConstIterator j = i->second.begin(); j != i->second.end(); ++j)
                std::cout << "   " << j->to8Bit(true) << std::endl;
        }

        TagLib::List<TagLib::FLAC::Picture*> pl = t->pictureList();
        std::cout << "Num pictures: " << pl.size() << std::endl;
    }
    if (typeid(*tag) == typeid(TagLib::TagUnion))
    {
        TagLib::TagUnion* t = dynamic_cast<TagLib::TagUnion*>(tag);
        print_tag_items((*t)[0]);
        print_tag_items((*t)[1]);
        print_tag_items((*t)[2]);
    }
}

bool process_file(arguments&& args)
{
    platform::string file_name = platform::convert::to_platform(args.file_name());
    TagLib::FileRef file(file_name.c_str());

    if (file.isNull())
        return false;

    TagLib::File* f = file.file();
    if (f == nullptr)
        return false;
    TagLib::Tag& tag = *file.tag();

    auto utf8string = [](const std::string& str) -> TagLib::String
    {
        if (str.empty())
            return TagLib::String();
        return TagLib::String(str, TagLib::String::Type::UTF8);
    };

    auto itoi = [](const int& i)
    {
        return i;
    };

    auto itos = [](const int& i) -> std::string
    {
        return std::to_string(i);
    };

    bool processed = false;
    processed |= process_field(tag, &TagLib::Tag::setArtist, args.artist(), utf8string);
    processed |= process_field(tag, &TagLib::Tag::setTitle, args.title(), utf8string);
    processed |= process_field(tag, &TagLib::Tag::setAlbum, args.album(), utf8string);
    processed |= process_field(tag, &TagLib::Tag::setYear, args.year(), itoi);
    processed |= process_field(tag, &TagLib::Tag::setTrack, args.track(), itoi);
    processed |= process_field(tag, &TagLib::Tag::setGenre, args.genre(), utf8string);
    processed |= process_field(tag, &TagLib::Tag::setComment, args.comment(), utf8string);

    processed |= process_field(f, "SUBTITLE", args.work(), utf8string);
    processed |= process_field(f, "ALBUMARTIST", args.albumArtist(), utf8string);
    processed |= process_field(f, "DISCNUMBER", args.discnumber(), itos);
    processed |= process_field(f, "DISCTOTAL", args.disctotal(), itos);
    processed |= process_field(f, "TRACKNUMBER", args.tracknumber(), itos);
    processed |= process_field(f, "TRACKTOTAL", args.tracktotal(), itos);
    processed |= process_field(f, "COMPOSER", args.composer(), utf8string);
    processed |= process_field(f, "COPYRIGHT", args.copyright(), utf8string);
    processed |= process_field(f, "OWNER", args.license(), utf8string);
    processed |= process_field(f, "ENCODEDBY", args.encodedby(), utf8string);
    processed |= process_field(f, "ISRC", args.isrc(), utf8string);
    processed |= process_field(f, "QBZ:TID", args.trackId(), utf8string);

    // Add picture
    auto [picture_valid, picture] = args.picture();
    if (picture_valid)
    {
        TagLib::ByteVector data = loadFile(picture);
        processed |= setPicture(f, data);
    }

    if (processed)
    {
        // Use ID3v2::v3 for more compatibility
        if (typeid(*f) == typeid(TagLib::MPEG::File))
        {
            TagLib::MPEG::File* ff = dynamic_cast<TagLib::MPEG::File*>(f);
            ff->save(TagLib::MPEG::File::AllTags, TagLib::File::StripOthers, TagLib::ID3v2::v3);
        }
        else if (typeid(*f) == typeid(TagLib::RIFF::WAV::File))
        {
            TagLib::RIFF::WAV::File* ff = dynamic_cast<TagLib::RIFF::WAV::File*>(f);
            ff->save(TagLib::RIFF::WAV::File::AllTags, TagLib::File::StripOthers, TagLib::ID3v2::v3);
        }
        else if (typeid(*f) == typeid(TagLib::RIFF::AIFF::File))
        {
            TagLib::RIFF::AIFF::File* ff = dynamic_cast<TagLib::RIFF::AIFF::File*>(f);
            ff->save(TagLib::ID3v2::v3);
        }
        else
            file.save();
        return true;
    }

    if (!file.isNull() && file.tag())
    {
        std::cout << typeid(*f).name() << std::endl;
        std::cout << "Information for file " << file_name << std::endl;
        print_field(" Artist", tag.artist().to8Bit(true));
        print_field("  Title", tag.title().to8Bit(true));
        print_field("  Album", tag.album().to8Bit(true));
        print_field("   Year", tag.year());
        print_field("  Track", tag.track());
        print_field("  Genre", tag.genre().to8Bit(true));
        print_field("Comment", tag.comment().to8Bit(true));

        TagLib::PropertyMap tags = f->properties();

        unsigned int longest = 0;
        for(TagLib::PropertyMap::ConstIterator i = tags.begin(); i != tags.end(); ++i)
            if (i->first.size() > longest)
                longest = i->first.size();

        std::cout << "-- TAG (properties) --" << std::endl;
        for(TagLib::PropertyMap::ConstIterator i = tags.begin(); i != tags.end(); ++i)
            for(TagLib::StringList::ConstIterator j = i->second.begin(); j != i->second.end(); ++j)
                std::cout << std::left << std::setw(longest) << i->first << " - " << '"' << j->to8Bit(true) << '"' << std::endl;

        std::cout << "-- TAG (format specific names) --" << std::endl;
        print_tag_items(file.tag());
    }

    if (!file.isNull() && file.audioProperties())
    {
        TagLib::AudioProperties *properties = file.audioProperties();

        int duration = properties->lengthInMilliseconds() / 1000;
        int seconds = duration % 60;
        int minutes = (duration - seconds) / 60;

        std::cout << "-- AUDIO --" << std::endl;
        std::cout << "bitrate     - " << properties->bitrate() << std::endl;
        std::cout << "sample rate - " << properties->sampleRate() << std::endl;
        std::cout << "channels    - " << properties->channels() << std::endl;
        std::cout << "length      - " << minutes << ":" << std::setfill('0') << std::setw(2) << seconds << std::endl;
    }

    return true;
}

constexpr static const int RETURN_OK = 0;
constexpr static const int RETURN_ERROR = 1;

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
#ifdef _WIN32
    // Set console code page to UTF-8 so console known how to interpret string data
    SetConsoleOutputCP(CP_UTF8);

    // Enable buffering to prevent VS from chopping up UTF-8 byte sequences
    setvbuf(stdout, nullptr, _IOFBF, 1000);
#endif

    CLI::App app("id3-tag-cli");
    app.footer("If no argument is specified, information of given file is retrieved.\n"
               "If the option is not specified, the value is unchanged.\n"
               "If the argument is empty string (\"\") (for [TEXT]) or 0 (for [INT]) the value is cleared.\n"
               "\n"
               "Written by Zereges <https://github.com/Zereges/id3-tag-cli>");

    arguments args;
    try
    {
        arguments::parse_args(app, args);
    }
    catch (const CLI::Error& ex)
    {
        std::cerr << CLI::FailureMessage::help(&app, ex);
        return RETURN_ERROR;
    }

    CLI11_PARSE(app);

    if (app.get_option("--print")->as<bool>())
    {
        std::cout << app.config_to_str(true, true);
        return RETURN_OK;
    }

    if (!process_file(std::move(args)))
    {
        std::cerr << "Couldn't process given file " << args.file_name() << std::endl;
        return RETURN_ERROR;
    }
}
