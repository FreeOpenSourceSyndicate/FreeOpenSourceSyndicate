/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <initializer_list>
#include <string>
#include <utility>

namespace fossynd::game
{

// Replaces every `{name}` token in `text` with the matching argument, in one
// left-to-right pass so a substituted value is never rescanned. Keeps the
// localised format strings plain data (ADR-0016).
inline std::string substitute(std::string text,
                              std::initializer_list<std::pair<const char*, std::string>> values)
{
    std::size_t pos = 0;
    while (pos < text.size())
    {
        const std::size_t open = text.find('{', pos);
        if (open == std::string::npos)
        {
            break;
        }
        const std::size_t close = text.find('}', open + 1);
        if (close == std::string::npos)
        {
            break;
        }
        const std::string key = text.substr(open + 1, close - open - 1);
        bool replaced = false;
        for (const auto& value : values)
        {
            if (key == value.first)
            {
                text.replace(open, close - open + 1, value.second);
                pos = open + value.second.size();
                replaced = true;
                break;
            }
        }
        if (!replaced)
        {
            pos = close + 1;
        }
    }
    return text;
}

} // namespace fossynd::game
