# compress_bz2

Compresses a stream of bytes using bz2 compression.

```tql
compress_bz2 [level=int]
```

## Description

The `compress_bz2` operator compresses bytes in a pipeline incrementally.

### `level = int (optional)`

The compression level to use. The supported values depend on the codec used. If
omitted, the default level for the codec is used.

## Examples

### Export all events in a Bzip2-compressed NDJSON file

```tql
export
write_ndjson
compress_bz2
save_file "/tmp/backup.json.bz2"
```

### Recompress a Bzip2-compressed file at a different compression level

```tql
load_file "in.bz2"
decompress_bz2
compress_bz2 level=18
save_file "out.bz2"
```