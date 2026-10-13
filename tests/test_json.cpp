/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/Json.h"

using namespace fossynd::core;

FOSSYND_TEST(Json, ParsesScalars)
{
    std::string error;
    const Json root = Json::parse(R"({"n": 12.5, "b": true, "s": "hi", "z": null})", error);
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(root.isObject());
    FOSSYND_CHECK_NEAR(root["n"].asFloat(), 12.5f);
    FOSSYND_CHECK(root["b"].asBool());
    FOSSYND_CHECK_EQ(root["s"].asString(), std::string("hi"));
    FOSSYND_CHECK(root["z"].isNull());
}

FOSSYND_TEST(Json, ParsesNestedArraysAndObjects)
{
    std::string error;
    const Json root =
        Json::parse(R"({"agents": [{"name": "A", "hp": 3}, {"name": "B", "hp": 7}]})", error);
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(root["agents"].size(), static_cast<std::size_t>(2));
    FOSSYND_CHECK_EQ(root["agents"][0]["name"].asString(), std::string("A"));
    FOSSYND_CHECK_EQ(root["agents"][1]["hp"].asInt(), 7);
}

FOSSYND_TEST(Json, MissingKeysYieldNullWithoutCrashing)
{
    std::string error;
    const Json root = Json::parse(R"({"a": 1})", error);
    FOSSYND_CHECK(error.empty());
    // Chained lookups on missing keys must be safe and fall back.
    FOSSYND_CHECK(root["nope"].isNull());
    FOSSYND_CHECK(root["nope"]["deeper"].isNull());
    FOSSYND_CHECK_NEAR(root["nope"]["deeper"].asFloat(42.0f), 42.0f);
    FOSSYND_CHECK(root["a"].asInt(99) == 1);
}

FOSSYND_TEST(Json, HandlesEscapesAndUnicode)
{
    std::string error;
    const Json root = Json::parse(R"({"s": "line\nbreak\t\"q\" \u0041"})", error);
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(root["s"].asString(), std::string("line\nbreak\t\"q\" A"));
}

FOSSYND_TEST(Json, RejectsMalformedInput)
{
    std::string error;

    // Trailing comma is not valid JSON and must be rejected.
    Json a = Json::parse(R"({"a": 1,})", error);
    FOSSYND_CHECK(!error.empty());
    FOSSYND_CHECK(a.isNull());

    // Unterminated string.
    error.clear();
    Json b = Json::parse(R"({"a": "oops)", error);
    FOSSYND_CHECK(!error.empty());

    // Trailing garbage.
    error.clear();
    Json c = Json::parse(R"({"a": 1} extra)", error);
    FOSSYND_CHECK(!error.empty());

    // Empty input is not a value.
    error.clear();
    Json d = Json::parse("", error);
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Json, ParseFileReportsMissingFile)
{
    Json root;
    std::string error;
    FOSSYND_CHECK(!Json::parseFile("this/file/does/not/exist.json", root, error));
    FOSSYND_CHECK(!error.empty());
}
