# pagx

Command-line tool for working with [PAGX](https://github.com/libpag/libpag) files.

## Install

```bash
npm install -g pagx
```

## Commands

| Command | Description |
|---------|-------------|
| `pagx validate` | Validate PAGX structure against the specification |
| `pagx render` | Render PAGX to an image file (PNG, WebP, or JPEG) |
| `pagx bounds` | Query the precise bounds of a node or layer |
| `pagx font info` | Query font metrics from a font file or system font |
| `pagx font embed` | Embed fonts into a PAGX file |
| `pagx format` | Format a PAGX file (indentation and attribute ordering) |
| `pagx optimize` | Validate, optimize, and format in one step |

## Usage

```bash
# Validate a PAGX file
pagx validate input.pagx

# Render to PNG
pagx render input.pagx

# Render with options
pagx render --format webp --scale 2 input.pagx

# Optimize (validate + optimize + format)
pagx optimize -o output.pagx input.pagx

# Query bounds
pagx bounds --xpath "//Layer[@id='btn']" input.pagx

# Query font metrics
pagx font info --name "Arial" --size 24

# Embed fonts
pagx font embed --file CustomFont.ttf input.pagx
```

## Supported Platforms

- macOS (Apple Silicon and Intel)
- Linux (x64)
- Windows (x64)

## License

Apache-2.0
