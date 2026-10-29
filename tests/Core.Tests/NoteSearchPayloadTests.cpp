#include <catch2/catch_test_macros.hpp>

#include "Core/Note/NotePayloadBuilder.h"
#include "Core/Note/NoteSearch.h"

using namespace F4DH::Core;

TEST_CASE("note search is case-insensitive across titles and bodies")
{
	REQUIRE(NoteSearch::Matches("Dead Drop Note", "", "dead drop"));
	REQUIRE(NoteSearch::Matches("Note", "Sandy Coves Convalescent Home", "sandy COVES"));
	REQUIRE(!NoteSearch::Matches("Note", "body", "missing"));
}

TEST_CASE("note search whitespace restores all and missing bodies do not match")
{
	const std::vector<NoteSearchDocument> documents{ { 1, "One", "" }, { 2, "Two", "needle" } };
	REQUIRE(NoteSearch::Filter(documents, " \t ").size() == 2);
	const auto filtered = NoteSearch::Filter(documents, "needle");
	REQUIRE(filtered.size() == 1);
	REQUIRE(filtered[0] == 2);
	REQUIRE(!NoteSearch::Matches("title", "", "body"));
}

TEST_CASE("note list payload carries ids titles and sequence with escaping")
{
	const auto json = NotePayloadBuilder::BuildList({ { 7, "A \"note\"\\title", 3 } });
	REQUIRE(json == "[{\"id\":7,\"title\":\"A \\\"note\\\"\\\\title\",\"sequence\":3}]");
}

TEST_CASE("note body payload carries status body plugin and first encounter")
{
	NoteBodyResult result;
	result.entryId = 9;
	result.status = NoteBodyResult::Status::Ready;
	result.title = "Note";
	result.body = "line\nnext";
	result.pluginName = "Test.esp";
	result.firstEncounter = EncounterMetadata{ EncounterKind::Pickup, { 2287, 10, 29, 9, 30 }, "Diamond City" };
	const auto json = NotePayloadBuilder::BuildBody(result);
	REQUIRE(json.find("\"id\":9") != std::string::npos);
	REQUIRE(json.find("\"status\":\"ready\"") != std::string::npos);
	REQUIRE(json.find("line\\nnext") != std::string::npos);
	REQUIRE(json.find("\"kind\":\"pickup\"") != std::string::npos);
	REQUIRE(json.find("\"location\":\"Diamond City\"") != std::string::npos);
}

TEST_CASE("note body payload exposes missing and empty states")
{
	NoteBodyResult missing;
	missing.status = NoteBodyResult::Status::MissingPlugin;
	missing.pluginName = "Missing.esp";
	REQUIRE(NotePayloadBuilder::BuildBody(missing).find("\"status\":\"missingPlugin\"") != std::string::npos);
	NoteBodyResult empty;
	empty.status = NoteBodyResult::Status::Empty;
	REQUIRE(NotePayloadBuilder::BuildBody(empty).find("\"status\":\"empty\"") != std::string::npos);
}

TEST_CASE("note payload escapes control characters")
{
	REQUIRE(NotePayloadBuilder::Escape("a\bb") == "a\\u0008b");
	REQUIRE(NotePayloadBuilder::Escape("a\tb") == "a\\tb");
}
