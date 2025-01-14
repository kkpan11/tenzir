# compress_gzip

Compresses a stream of bytes using gzip compression.

```tql
compress_gzip [level=int, window_bits=int, format=string]
```

## Description

The `compress_gzip` operator compresses bytes in a pipeline incrementally.

### `level = int (optional)`

The compression level to use. The supported values depend on the codec used. If
omitted, the default level for the codec is used.

### `window_bits = int (optional)`

A number representing the encoder window bits.

### `format = string (optional)`

A string representing the used format. Possible values are `zlib`, `deflate` and
`gzip`.

Defaults to `gzip`.

## Examples

### Export all events in a Gzip-compressed NDJSON file

```tql
export
write_ndjson
compress_gzip
save_file "/tmp/backup.json.gz"
```

### Compress using Gzip deflate

```tql
export
write_ndjson
compress_gzip format="deflate"
```

### Recompress a Gzip-compressed file at a different compression level

```tql
load_file "in.gzip"
decompress_gzip
compress_gzip level=18
save_file "out.gzip"
```