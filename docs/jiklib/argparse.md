# argparse

Command-line argument parser.

## Types

### `ArgumentOption`

A long option, optional short alias, and its value specifications.

---

### `Parser`

Configured parser.

---

### `ParseResult`

Values returned by `parse`.

**Notes**

- `positionals` is keyed by positional name. `options` is keyed by the
  configured long option spelling, including its `--` prefix.
- Each option value is keyed by its declared name. A present flag has an empty
  value dictionary.

## Functions

### `add_option(parser: Parser, name: String, short_name: String, help: String) -> ArgumentOption`

Add an option and return it.

**Parameters**

1. `parser: Parser` - Parser to configure.
2. `name: String` - Long option name, such as `--verbose`. Foreign parameter.
3. `short_name: String` - Short option name, such as `-v`, or an empty string
   for a long-only option. Foreign parameter.
4. `help: String` - Help text. Foreign parameter.

**Returns**

- Configured option handle.

**Notes**

- An option with no declared values acts as a flag.
- Long names must begin with `--`. Short names are optional; when provided,
  they must be `-` followed by one character. Duplicate option names terminate
  through `std::panic`.

---

### `add_option_positional(option: ArgumentOption, name: String, help: String)`

Add a positional value consumed by an option.

**Parameters**

1. `option: ArgumentOption` - Option returned by `add_option`.
2. `name: String` - Value name. Foreign parameter.
3. `help: String` - Help text. Foreign parameter.

**Notes**

- Value names must be unique within an option because they become keys in the
  parse result.

---

### `add_positional(parser: Parser, name: String, help: String)`

Add a required positional argument.

**Parameters**

1. `parser: Parser` - Parser to configure.
2. `name: String` - Positional argument name. Foreign parameter.
3. `help: String` - Help text. Foreign parameter.

---

### `format_help(parser: Parser, r: Region) -> String`

Format generated parser help text.

**Parameters**

1. `parser: Parser` - Configured parser. Foreign parameter.
2. `r: Region` - Allocation region for the returned string.

**Returns**

- Help text.

---

### `new(name: String, r: Region) -> Parser`

Create an empty argument parser with a program name for generated help.

**Parameters**

1. `name: String` - Program name shown in the usage line. Foreign parameter.
2. `r: Region` - Allocation region for the parser and all configured data.

**Returns**

- `Parser`

---

### `throws parse(parser: Parser, args: Vec[String], r: Region) -> ParseResult`

Parse command-line arguments into a result.

**Parameters**

1. `parser: Parser` - Configured parser. Foreign parameter.
2. `args: Vec[String]` - Command-line tokens after the executable name.
   Foreign parameter.
3. `r: Region` - Allocation region for the returned result.

**Returns**

- `ParseResult`

**Notes**

- `args` must not include the executable name. The parsed dictionaries are
  allocated in `r`. Options may be written with their long or short names.
- `--` ends option parsing. Repeating an option is a parse error.
- Supported forms are `--long` and `-s`; grouped short options and inline
  values such as `--output=file` are not supported.
- Option values are retrieved by name, such as
  `result.options["--output"]?["file"]?`.
- Unknown or repeated options, missing option values, and missing or extra
  positional arguments return an `argparse:` error.
