# testing

Testing utilities.

## Types

### `TestSuite`

Test suite state.

Fields:

1. `assertions: int` - Total assertion count.
2. `failed: int` - Failed assertion count.
3. `passed: int` - Passed assertion count.

## Functions

### `suite_new(r: Region) -> TestSuite`

Create a new test suite.

**Parameters**
1. `r: Region` - Allocation region for the test suite

---

### `suite_finish(suite: TestSuite) -> void`

Print suite summary and exit non-zero if any test failed.

**Parameters**
1. `suite: TestSuite` - Test suite

---

### `suite_assert(suite: TestSuite, cond: bool, s: Site) -> void`

Test suite assertion function.

**Parameters**
1. `suite: TestSuite` - Test suite to update.
2. `cond: bool` - Assertion condition.
3. `s: Site` - Source location captured for failure reporting.
