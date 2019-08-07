#pragma once
#include <string>
#include <optional>
#include <exception>

#include <getopt.h>

#include "help.hpp"

struct arguments_parse_exception final : std::runtime_error
{
    explicit arguments_parse_exception(const std::string& msg)
        : runtime_error(msg)
    {
    }

    explicit arguments_parse_exception(const char* const msg)
        : runtime_error(msg)
    {
    }
};

class arguments
{
    arguments(bool help, bool version)
        : m_help(help), m_version(version)
    {
    }

public:
    static arguments parse_args(int argc, std::vector<std::string>& arg_vector)
    {
        std::vector<char*> argv;
        for (int i = 0; i < argc; ++i)
            argv.push_back(arg_vector[i].data());

        arguments args{false, false};
        for (;;)
        {
            static option options[] =
            {
                {    "help",       no_argument, nullptr, 'h' },
                { "version",       no_argument, nullptr, 'v' },
                {  "artist", required_argument, nullptr, 'a' },
                {   "title", required_argument, nullptr, 't' },
                {   "album", required_argument, nullptr, 'A' },
                {    "year", required_argument, nullptr, 'y' },
                {   "track", required_argument, nullptr, 'T' },
                {   "genre", required_argument, nullptr, 'g' },
                {   nullptr,                 0, nullptr,  0  }
            };

            int option_index = 0;
            int c = getopt_long(argc, argv.data(), "hva:t:A:y:Y:g:", options, &option_index);
            if (c == -1)
                break;

            switch (c)
            {
            case 'h':
                return {true, false};
            case 'v':
                return {false, true};
            case 'a':
                args.m_artist = optarg;
                break;
            case 't':
                args.m_title = optarg;
                break;
            case 'A':
                args.m_album = optarg;
                break;
            case 'y':
                try
                {
                    args.m_year = std::stoi(optarg);
                }
                catch (const std::invalid_argument&)
                {
                }
                break;
            case 'T':
                try
                {
                    args.m_year = std::stoi(optarg);
                }
                catch (const std::invalid_argument&)
                {
                }
                break;
            case 'g':
                args.m_genre = optarg;
                break;
            }
        }
        return args;
    }

    bool is_help() const
    {
        return m_help;
    }

    bool is_version() const
    {
        return m_version;
    }

    const std::string& file_name() const
    {
        return m_file_name;
    }

    std::pair<bool, std::string> artist() const
    {
        return { m_artist.has_value(), m_artist.value_or("") };
    }

    std::pair<bool, std::string> title() const
    {
        return { m_title.has_value(), m_title.value_or("") };
    }

    std::pair<bool, std::string> album() const
    {
        return { m_album.has_value(), m_album.value_or("") };
    }

    std::pair<bool, int> year() const
    {
        return { m_year.has_value(), m_year.value_or(0) };
    }

    std::pair<bool, int> track() const
    {
        return { m_track.has_value(), m_track.value_or(0) };
    }

    std::pair<bool, std::string> genre() const
    {
        return { m_genre.has_value(), m_genre.value_or("") };
    }

private:
    bool m_help;
    bool m_version;
    std::string m_file_name;
    std::optional<std::string> m_artist;
    std::optional<std::string> m_title;
    std::optional<std::string> m_album;
    std::optional<int> m_year;
    std::optional<int> m_track;
    std::optional<std::string> m_genre;
};
