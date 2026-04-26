# testing

Testing utilities.

## Types

### `TestSuite`

Test suite state.

## Functions

### `suite_finish(suite: TestSuite) -> void`

Print suite summary and exit non-zero if any test failed.

**Parameters**
1. `suite: TestSuite` - Test suite

---

### `suite_assert(suite: TestSuite, cond: bool, s: Site) -> void`

Test suite assertion function.

**Parameters**
1. `suite: TestSuite` - Test suite
2. `cond: bool` - Assertion expression, boolean
3. `s: Site` - Site instance
