#include <catch2/catch_test_macros.hpp>
#include "GrainFreeze/UpdateChecker.h"

TEST_CASE ("UpdateChecker::isNewer correctly compares dotted versions", "[update]")
{
    using gf::UpdateChecker;

    // Strict-greater semantics: never flag the same version.
    CHECK_FALSE (UpdateChecker::isNewer ("2.3.0", "2.3.0"));
    CHECK_FALSE (UpdateChecker::isNewer ("v2.3.0", "2.3.0"));
    CHECK_FALSE (UpdateChecker::isNewer ("v2.2.0", "2.3.0"));
    CHECK_FALSE (UpdateChecker::isNewer ("v2.2",   "2.3.0"));

    // Newer minor, patch, major.
    CHECK (UpdateChecker::isNewer ("v2.4.0", "2.3.0"));
    CHECK (UpdateChecker::isNewer ("v2.3.1", "2.3.0"));
    CHECK (UpdateChecker::isNewer ("v3.0.0", "2.9.9"));
    CHECK (UpdateChecker::isNewer ("2.4",    "v2.3.0"));    // mixed prefixes
    CHECK (UpdateChecker::isNewer ("v10.0",  "v2.9.9"));    // double-digit major (string-compare would mis-rank this)
}
