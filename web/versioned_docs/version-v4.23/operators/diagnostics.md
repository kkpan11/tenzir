---
sidebar_custom_props:
  operator:
    source: true
---

# diagnostics

Retrieves diagnostic events from a Tenzir node.

## Synopsis

```
diagnostics [--live] [--retro]
```

## Description

The `diagnostics` operator retrieves diagnostic events from a Tenzir
node.

### `--live`

Work on all diagnostic events as they are generated in real-time instead of on
diagnostic events persisted at a Tenzir node.

### `--retro`

Work on persisted diagnostic events (first), even when `--live` is given.

See [`export` operator](export.md#--retro) for more details.

## Schemas

Tenzir emits diagnostic information with the following schema:

### `tenzir.diagnostic`

Contains detailed information about the diagnostic.

|Field|Type|Description|
|:-|:-|:-|
|`pipeline_id`|`string`|The ID of the pipeline that created the diagnostic.|
|`run`|`uint64`|The number of the run, starting at 1 for the first run.|
|`timestamp`|`time`|The exact timestamp of the diagnostic creation.|
|`message`|`string`|The diagnostic message.|
|`severity`|`string`|The diagnostic severity.|
|`notes`|`list<record>`|The diagnostic notes. Can be empty.|
|`annotations`|`list<record>`|The diagnostic annotations. Can be empty.|
|`rendered`|`string`|The rendered diagnostic, as printed on the command-line.|

The record `notes` has the following schema:

|Field|Type|Description|
|:-|:-|:-|
|`kind`|`string`|The kind of note, which is `note`, `usage`, `hint` or `docs`.|
|`message`|`string`|The message of this note.|

The record `annotations` has the following schema:

|Field|Type|Description|
|:-|:-|:-|
|`primary`|`bool`|True if the `source` represents the underlying reason for the diagnostic, false if it is only related to it.|
|`text`|`string`|A message for explanations. Can be empty.|
|`source`|`string`|The character range in the pipeline string that this annotation is associated to.|

## Examples

View all diagnostics generated in the past five minutes.

```
diagnostics
| where timestamp > 5 minutes ago
```

Only show diagnostics that contain the `error` severity.

```
diagnostics
| where severity == "error"
```
