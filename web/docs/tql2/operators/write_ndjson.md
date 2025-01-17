# write_ndjson

Transforms the input event stream to a Newline-Delimited JSON byte stream.

```tql
write_ndjson [color=bool]
```

## Description

Transforms the input event stream to a Newline-Delimited JSON byte stream.

### `color = bool (optional)`

Colorize the output.

## Examples

### Convert a YAML stream into a JSON file

```tql
load_file "input.yaml"
read_yaml
write_ndjson
save_file "output.json"
```
