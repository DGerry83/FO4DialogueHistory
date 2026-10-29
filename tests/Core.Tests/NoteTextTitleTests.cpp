#include <catch2/catch_test_macros.hpp>

#include "Core/Note/NoteClassifier.h"
#include "Core/Note/NoteMetadata.h"
#include "Core/Note/NoteTextFormatter.h"
#include "Core/Note/NoteTitleGenerator.h"

using namespace F4DH::Core;

TEST_CASE("note classifier rejects teaching magazine and texture-only records")
{
	REQUIRE(NoteClassifier::IsDocument({}));
	REQUIRE(!NoteClassifier::IsDocument({ .teaches = true }));
	REQUIRE(!NoteClassifier::IsDocument({ .hasMagazineScript = true }));
	REQUIRE(!NoteClassifier::IsDocument({ .hasPerkMagKeyword = true }));
	REQUIRE(!NoteClassifier::IsDocument({ .textureOnly = true }));
}

TEST_CASE("note formatter maps markup pagebreaks images and entities to plain text")
{
	const auto plain = NoteTextFormatter::ToPlainText(
		"<p align='center'>Title<br>Line &amp; more</p>[pagebreak]<p><i>Next</i><img src='x'> end &lt;ok&gt;</p>");
	REQUIRE(plain.find("Title\nLine & more") != std::string::npos);
	REQUIRE(plain.find("--- Page Break ---") != std::string::npos);
	REQUIRE(plain.find("Next") != std::string::npos);
	REQUIRE(plain.find("[Image omitted]") != std::string::npos);
	REQUIRE(plain.find("end <ok>") != std::string::npos);
	REQUIRE(plain.find("<p") == std::string::npos);
}

TEST_CASE("note formatter normalizes carriage returns and opening whitespace")
{
	const auto plain = NoteTextFormatter::ToPlainText("First\r\n\r\nSecond\tline");
	REQUIRE(plain == "First\n\nSecond line");
	REQUIRE(NoteTextFormatter::NormalizeOpening("  One\n two\tthree  ") == "One two three");
}

TEST_CASE("note formatter detects unresolved aliases before markup stripping")
{
	REQUIRE(NoteTextFormatter::ContainsUnresolvedAlias("<Alias=Player>"));
	REQUIRE(NoteTextFormatter::ContainsUnresolvedAlias("<Alias.Pronoun=Player>"));
	REQUIRE(!NoteTextFormatter::ContainsUnresolvedAlias("Ordinary text"));
}

TEST_CASE("note title keeps a unique non-generic original")
{
	REQUIRE(NoteTitleGenerator::Generate("Dead Drop Note", "body", {}) == "Dead Drop Note");
}

TEST_CASE("note title generic and duplicate labels use a resolved opening")
{
	REQUIRE(NoteTitleGenerator::Generate("Note", "Billy, we found the Pipe", {}) == "Note: Billy, we found the Pipe");
	REQUIRE(NoteTitleGenerator::Generate("Dead Drop Note", "opening words", { "Dead Drop Note" }) ==
		"Dead Drop Note: opening words");
}

TEST_CASE("note title remaining collisions receive first-read numeric suffixes")
{
	const std::vector<std::string> existing{ "Note: opening", "Note: opening (2)" };
	REQUIRE(NoteTitleGenerator::Generate("Note", "opening", existing) == "Note: opening (3)");
}

TEST_CASE("note title empty original and empty body use safe fallbacks")
{
	REQUIRE(NoteTitleGenerator::Generate("", "", {}) == "Untitled note");
	REQUIRE(NoteTitleGenerator::Generate("Note", "", { "Note" }) == "Note (2)");
}

TEST_CASE("note title opening respects the 64 code-point limit and word boundary")
{
	const auto generated = NoteTitleGenerator::Generate("Note",
		"This is a long opening with enough words to exercise the bounded snippet without cutting a word in half", {});
	REQUIRE(generated.find("Note: ") == 0);
	REQUIRE(static_cast<unsigned char>(generated.back()) == 0xA6);  // final byte of UTF-8 ellipsis U+2026
	REQUIRE(generated.find("without") == std::string::npos);
}

TEST_CASE("note title matching is case-insensitive")
{
	REQUIRE(NoteTitleGenerator::Generate("note", "body", {}) == "note: body");
	REQUIRE(NoteTitleGenerator::Generate("Dead Drop Note", "body", { "dead drop note" }) == "Dead Drop Note: body");
}

TEST_CASE("note title never exposes an unresolved alias")
{
	const auto title = NoteTitleGenerator::Generate("Super Mutants in <Alias=Dungeon>!", "Open notice to anyone", {});
	REQUIRE(title == "Note: Open notice to anyone");
	REQUIRE(title.find("<Alias=") == std::string::npos);
}

TEST_CASE("note display year includes the 2000 epoch")
{
	REQUIRE(DisplayGameYear(288) == 2288);
	REQUIRE(DisplayGameYear(287) == 2287);
}
