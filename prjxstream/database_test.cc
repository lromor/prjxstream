#include "prjxstream/database.h"

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace prjxstream {
namespace {
constexpr absl::string_view kSampleTileGrid = R"({
  "TILE_A": {
    "bits": {
      "CLB_IO_CLK": {
        "alias": {
          "sites": {},
          "start_offset": 0,
          "type": "HCLK_L"
        },
        "baseaddr": "0x00020E00",
        "frames": 26,
        "offset": 50,
        "words": 1
      }
    },
    "grid_x": 72,
    "grid_y": 26,
    "pin_functions": {},
    "prohibited_sites": [],
    "sites": {},
    "type": "HCLK_L_BOT_UTURN"
  },
  "TILE_B": {
    "bits": {
      "CLB_IO_CLK": {
        "alias": {
          "sites": {
            "IOB33_Y0": "IOB33_Y0"
          },
          "start_offset": 2,
          "type": "LIOB33"
        },
        "baseaddr": "0x00400000",
        "frames": 42,
        "offset": 0,
        "words": 2
      }
    },
    "clock_region": "X0Y0",
    "grid_x": 0,
    "grid_y": 155,
    "pin_functions": {
      "IOB_X0Y0": "IO_25_14"
    },
    "prohibited_sites": [],
    "sites": {
      "IOB_X0Y0": "IOB33"
    },
    "type": "LIOB33_SING"
  }
})";

// Test some basic expectaions for a sample tilegrid.json
TEST(TileGridParser, SampleTileGrid) {
  absl::StatusOr<TileGrid> tile_grid_result =
    ParseTileGridJSON(kSampleTileGrid);
  EXPECT_TRUE(tile_grid_result.ok());
  const TileGrid &tile_grid = tile_grid_result.value();
  EXPECT_EQ(tile_grid.size(), 2);
  EXPECT_EQ(tile_grid.count("TILE_A"), 1);
  EXPECT_EQ(tile_grid.count("TILE_B"), 1);

  const Tile &tile_a = tile_grid.at("TILE_A");
  EXPECT_EQ(tile_a.coord.x, 72);
  EXPECT_EQ(tile_a.coord.y, 26);

  // Check bits block.
  EXPECT_TRUE(tile_a.bits.has_value());
  EXPECT_EQ(tile_a.pin_functions.size(), 0);

  const Tile &tile_b = tile_grid.at("TILE_B");
  EXPECT_EQ(tile_b.pin_functions.size(), 1);
  EXPECT_EQ(tile_b.bits.value().count(FrameBlockType::kCLBIOCLK), 1);

  const BitsBlock &block = tile_b.bits.value().at(FrameBlockType::kCLBIOCLK);
  EXPECT_TRUE(block.alias.has_value());
  EXPECT_EQ(block.alias.value().sites.size(), 1);
  EXPECT_EQ(block.base_address, 4194304);  // 0x00400000
}

using TileGridParserTestCaseArray = std::initializer_list<const char *>;

// Inputs that should fail gracefully.
TEST(TileGridParser, EmptyTileGrid) {
  constexpr absl::string_view kExpectFailTests[] = {"",     "[]", "  ",
                                                    "\n\n", "32", "asd",
                                                    R"({
  "TILE_A": {
    "bits": {},
    "grid_x": 72,
    "grid_y": 26,
    "pin_functions": {},
    "prohibited_sites": [],
    "type": "HCLK_L_BOT_UTURN"
  },
})"};
  for (const auto &v : kExpectFailTests) {
    const absl::StatusOr<TileGrid> tile_grid_result = ParseTileGridJSON(v);
    EXPECT_FALSE(tile_grid_result.ok());
  }
}

struct PseudoPIPsParserTestCase {
  absl::string_view db;
  TileTypePseudoPIPs expected_ppips;
  bool expected_success;
};

TEST(PseudoPIPsParser, CanParseSimpleDatabases) {
  const struct PseudoPIPsParserTestCase kTestCases[] = {
    {.db = "Palways", .expected_ppips = {}, .expected_success = false},
    {.db = "P    always",
     .expected_ppips = {{"P", PseudoPIPType::kAlways}},
     .expected_success = true},
    {.db = "P  always   \n",
     .expected_ppips = {{"P", PseudoPIPType::kAlways}},
     .expected_success = true},
    {.db = "P always",
     .expected_ppips = {{"P", PseudoPIPType::kAlways}},
     .expected_success = true},
    {.db = "P default",
     .expected_ppips = {{"P", PseudoPIPType::kDefault}},
     .expected_success = true},
    {.db = "P hint",
     .expected_ppips = {{"P", PseudoPIPType::kHint}},
     .expected_success = true},
    {.db = "P  always   \n  A   default \n",
     .expected_ppips = {{"P", PseudoPIPType::kAlways},
                        {"A", PseudoPIPType::kDefault}},
     .expected_success = true},
  };
  for (const auto &test : kTestCases) {
    const absl::StatusOr<TileTypePseudoPIPs> res =
      ParsePseudoPIPsDatabase(test.db);
    if (!test.expected_success) {
      EXPECT_FALSE(res.ok());
    } else {
      ASSERT_TRUE(res.ok()) << absl::StrFormat("for \"%s\"", test.db);
      EXPECT_EQ(res.value(), test.expected_ppips);
    }
  }
}

MATCHER(SegmentBitEquals, "segment bits are equal") {
  const SegmentBit &actual = std::get<0>(arg);
  const SegmentBit &expected = std::get<1>(arg);
  EXPECT_THAT(
    actual, ::testing::AllOf(
              ::testing::Field(&SegmentBit::word_column, expected.word_column),
              ::testing::Field(&SegmentBit::word_bit, expected.word_bit),
              ::testing::Field(&SegmentBit::is_set, expected.is_set)));
  return true;
}

struct SegmentBitsParserTestCase {
  absl::string_view db;
  TileTypeSegmentsBits expected_segbits;
  bool expected_success;
};

TEST(SegmentsBitsParser, CanParseSimpleDatabases) {
  const struct SegmentBitsParserTestCase kTestCases[] = {
    {"FOO 28_519 !29_519",
     {{"FOO", {{28, 519, true}, {29, 519, false}}}},
     true},
    {"BAR !1_23", {{"BAR", {{1, 23, false}}}}, true},
    {"\n BAZ  42_42 33_93\n QUX !0_1 \n  ",
     {{"BAZ", {{42, 42, true}, {33, 93, true}}}, {"QUX", {{0, 1, false}}}},
     true},
  };
  for (const auto &test : kTestCases) {
    const absl::StatusOr<TileTypeSegmentsBits> res =
      ParseSegmentsBitsDatabase(test.db);
    if (!test.expected_success) {
      EXPECT_FALSE(res.ok());
    } else {
      ASSERT_TRUE(res.ok())
        << absl::StrFormat("for:\n\"%s\"\n%s", test.db, res.status().message());
      const TileTypeSegmentsBits &actual = res.value();
      for (const auto &pair : test.expected_segbits) {
        ASSERT_EQ(actual.count(pair.first), 1)
          << absl::StrFormat("expected key \"%s\" not found", pair.first);
        EXPECT_THAT(actual.at(pair.first),
                    ::testing::Pointwise(SegmentBitEquals(), pair.second));
      }
    }
  }
}
}  // namespace
}  // namespace prjxstream
