/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/Json.h"

using namespace foss::core;

FOSS_TEST(Json, ParsesScalars)
{
    std::string error;
    const Json root = Json::parse(R"({"n": 12.5, "b": true, "s": "hi", "z": null})", error);
    FOSS_CHECK(error.empty());
    FOSS_CHECK(root.isObject());
    FOSS_CHECK_NEAR(root["n"].asFloat(), 12.5f);
    FOSS_CHECK(root["b"].asBool());
    FOSS_CHECK_EQ(root["s"].asString(), std::string("hi"));
    FOSS_CHECK(root["z"].isNull());
}

FOSS_TEST(Json, ParsesNestedArraysAndObjects)
{
    std::string error;
    const Json root =
        Json::parse(R"({"agents": [{"name": "A", "hp": 3}, {"name": "B", "hp": 7}]})", error);
    FOSS_CHECK(error.empty());
    FOSS_CHECK_EQ(root["agents"].size(), static_cast<std::size_t>(2));
    FOSS_CHECK_EQ(root["agents"][0]["name"].asString(), std::string("A"));
    FOSS_CHECK_EQ(root["agents"][1]["hp"].asInt(), 7);
}

FOSS_TEST(Json, MissingKeysYieldNullWithoutCrashing)
{
    std::string error;
    const Json root = Json::parse(R"({"a": 1})", error);
    FOSS_CHECK(error.empty());
    // Chained lookups on missing keys must be safe and fall back.
    FOSS_CHECK(root["nope"].isNull());
    FOSS_CHECK(root["nope"]["deeper"].isNull());
    FOSS_CHECK_NEAR(root["nope"]["deeper"].asFloat(42.0f), 42.0f);
    FOSS_CHECK(root["a"].asInt(99) == 1);
}

FOSS_TEST(Json, HandlesEscapesAndUnicode)
{
    std::string error;
    const Json root = Json::parse(R"({"s": "line\nbreak\t\"q\" \u0041"})", error);
    FOSS_CHECK(error.empty());
    FOSS_CHECK_EQ(root["s"].asString(), std::string("line\nbreak\t\"q\" A"));
}

FOSS_TEST(Json, RejectsMalformedInput)
{
    std::string error;

    // Trailing comma is not valid JSON and must be rejected.
    Json a = Json::parse(R"({"a": 1,})", error);
    FOSS_CHECK(!error.empty());
    FOSS_CHECK(a.isNull());

    // Unterminated string.
    error.clear();
    Json b = Json::parse(R"({"a": "oops)", error);
    FOSS_CHECK(!error.empty());

    // Trailing garbage.
    error.clear();
    Json c = Json::parse(R"({"a": 1} extra)", error);
    FOSS_CHECK(!error.empty());

    // Empty input is not a value.
    error.clear();
    Json d = Json::parse("", error);
    FOSS_CHECK(!error.empty());
}

FOSS_TEST(Json, ParseFileReportsMissingFile)
{
    Json root;
    std::string error;
    FOSS_CHECK(!Json::parseFile("this/file/does/not/exist.json", root, error));
    FOSS_CHECK(!error.empty());
}
