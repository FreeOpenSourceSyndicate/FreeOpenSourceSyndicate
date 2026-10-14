/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "core/data/AgentSprites.h"

#include <cctype>
#include <iterator>
#include <map>
#include <sstream>

namespace fossynd::core
{

const char* const kAgentFrameNames[4] = {"front", "frontStep", "back", "backStep"};

namespace
{

// A parsed, palette-independent frame: rows of pixel characters.
struct RawFrame
{
    std::vector<std::string> rows;
};

// Trims trailing whitespace/CR so CRLF files and hand-edited spacing behave.
std::string rtrim(const std::string& line)
{
    std::size_t end = line.size();
    while (end > 0 && (line[end - 1] == '\r' || line[end - 1] == ' ' || line[end - 1] == '\t'))
    {
        --end;
    }
    return line.substr(0, end);
}

std::vector<std::string> tokenize(const std::string& line)
{
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token)
    {
        tokens.push_back(token);
    }
    return tokens;
}

bool parse(const std::string& text, std::map<std::string, RawFrame>& frames, std::string& error)
{
    std::istringstream stream(text);
    std::string line;
    std::string current;

    while (std::getline(stream, line))
    {
        const std::string trimmed = rtrim(line);
        if (trimmed.empty() || trimmed[0] == '#')
        {
            continue;
        }

        const std::vector<std::string> tokens = tokenize(trimmed);
        if (tokens[0] == "frame")
        {
            if (tokens.size() != 2)
            {
                error = "frame directive needs exactly one name";
                return false;
            }
            if (frames.count(tokens[1]) != 0)
            {
                error = "duplicate frame: " + tokens[1];
                return false;
            }
            frames.emplace(tokens[1], RawFrame{});
            current = tokens[1];
            continue;
        }
        if (tokens[0] == "end")
        {
            if (current.empty())
            {
                error = "end without a frame";
                return false;
            }
            current.clear();
            continue;
        }
        if (current.empty())
        {
            error = "pixel row outside a frame: " + trimmed;
            return false;
        }
        frames[current].rows.push_back(trimmed);
    }

    return true;
}

} // namespace

bool bakeSheet(const std::string& text, const SpritePalette& palette,
               const std::vector<std::string>& frameNames, BakedSheet& out, std::string& error,
               bool requireAnchor)
{
    std::map<std::string, RawFrame> frames;
    if (!parse(text, frames, error))
    {
        return false;
    }
    if (frameNames.empty())
    {
        error = "no frames requested";
        return false;
    }

    int width = 0;
    int height = 0;
    for (const std::string& name : frameNames)
    {
        const auto it = frames.find(name);
        if (it == frames.end() || it->second.rows.empty())
        {
            error = "missing frame: " + name;
            return false;
        }
        const RawFrame& raw = it->second;
        if (width == 0)
        {
            width = static_cast<int>(raw.rows[0].size());
            height = static_cast<int>(raw.rows.size());
        }
        if (static_cast<int>(raw.rows.size()) != height)
        {
            error = "frame " + name + " has a different height";
            return false;
        }
        for (const std::string& row : raw.rows)
        {
            if (static_cast<int>(row.size()) != width)
            {
                error = "frame " + name + " has a ragged row";
                return false;
            }
        }
    }

    out.frames.assign(frameNames.size(), BakedFrame{});

    for (std::size_t f = 0; f < frameNames.size(); ++f)
    {
        const RawFrame& raw = frames[frameNames[f]];
        BakedFrame& baked = out.frames[f];
        baked.width = width;
        baked.height = height;
        baked.rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0u);

        int anchors = 0;
        for (int y = 0; y < height; ++y)
        {
            const std::string& row = raw.rows[static_cast<std::size_t>(y)];
            for (int x = 0; x < width; ++x)
            {
                const char c = row[static_cast<std::size_t>(x)];
                const std::uint8_t* color = nullptr;
                switch (c)
                {
                case '.':
                    break;
                case 'A':
                    ++anchors;
                    baked.anchorX = x;
                    baked.anchorY = y;
                    break;
                case 'B':
                    color = palette.body;
                    break;
                case 'T':
                    color = palette.trim;
                    break;
                case 'S':
                    color = palette.skin;
                    break;
                case 'D':
                    color = palette.dark;
                    break;
                default:
                    error = std::string("unknown pixel character '") + c + "' in frame " +
                            frameNames[f];
                    return false;
                }
                if (color != nullptr)
                {
                    const std::size_t index =
                        (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                         static_cast<std::size_t>(x)) *
                        4u;
                    baked.rgba[index + 0] = color[0];
                    baked.rgba[index + 1] = color[1];
                    baked.rgba[index + 2] = color[2];
                    baked.rgba[index + 3] = 255;
                }
            }
        }

        if (anchors > 1 || (requireAnchor && anchors != 1))
        {
            error = "frame " + frameNames[f] +
                    (requireAnchor ? " must have exactly one anchor ('A')"
                                   : " must have at most one anchor ('A')");
            return false;
        }
        if (anchors == 0)
        {
            // Props with no explicit anchor rest on the bottom-centre of the tile.
            baked.anchorX = width / 2;
            baked.anchorY = height - 1;
        }
    }

    return true;
}

bool bakeAgentSprites(const std::string& text, const SpritePalette& palette,
                      AgentSpriteSheet& out, std::string& error)
{
    const std::vector<std::string> names(std::begin(kAgentFrameNames), std::end(kAgentFrameNames));
    return bakeSheet(text, palette, names, out.sheet, error, /*requireAnchor=*/true);
}

} // namespace fossynd::core
